/*	$Header: /usr/people/sam/fax/./util/RCS/FaxClient.c++,v 1.53 1995/04/08 21:44:02 sam Rel $ */
/*
 * Copyright (c) 1990-1995 Sam Leffler
 * Copyright (c) 1991-1995 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */
#include <osfcn.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
}
#include <signal.h>
#include <errno.h>

#include "config.h"
#include "Dispatcher.h"
#include "FaxClient.h"

/*
 * These definitions have to agree with the
 * ones in the decoder in faxd.recv.  They
 * should probably just be in a common file.
 */
#define MAXCODE(n)	((1L<<(n))-1)
#define	BITS_MIN	9		// start with 9 bits
#define	BITS_MAX	13		// max of 13 bit strings
#define	HSIZE		9001L		// 91% occupancy
#define	HSHIFT		(13-8)

/* predefined codes */
#define	CODE_CLEAR	0		// code to clear string table
#define	CODE_EOI	1		// end-of-information code
#define CODE_FIRST	256		// first free code entry
#define	CODE_MAX	MAXCODE(BITS_MAX)

typedef u_short hcode_t;		// codes fit in 16 bits
struct hash_t {
    long	hash;
    hcode_t	code;
};

FaxClient::FaxClient()
{
    init();
}

FaxClient::FaxClient(const fxStr& hostarg)
{
    init();
    setupHostModem(hostarg);
}

FaxClient::FaxClient(const char* hostarg)
{
    init();
    setupHostModem(hostarg);
}

void
FaxClient::init()
{
    fd = -1;
    verbose = FALSE;
    running = FALSE;
    prevcc = 0;
    peerdied = FALSE;
    version = FAX_PROTOVERS;
    port = -1;
    proto = FAX_PROTONAME;
    hashtab = new hash_t[HSIZE];

    uid_t uid = ::getuid();
    if (uid != 0) {
	static const char* trustedUsers[] = { FAX_TRUSTED, NULL };
	passwd* pwd = ::getpwuid(uid);
	const char* name = pwd ? pwd->pw_name : "nobody";
	for (u_int i = 0; trustedUsers[i] != NULL; i++)
	    if (::strcmp(trustedUsers[i], name) == 0)
		break;
	trusted = (trustedUsers[i] != NULL);
    } else
	trusted = TRUE;
}

FaxClient::~FaxClient()
{
    (void) hangupServer();
    delete hashtab;
}

void
FaxClient::setupHostModem(const fxStr& s)
{
    u_int pos = s.next(0, '@');
    if (pos == s.length()) {		// no @, check for host:modem
	pos = s.next(0, ':');
	host = s.head(pos);
	if (pos == s.length())
	    modem = "";
	else
	    modem = s.tail(s.length() - (pos+1));
    } else {				// modem@host
	modem = s.head(pos);
	if (pos == s.length())
	    host = "";
	else
	    host = s.tail(s.length() - (pos+1));
    }
}

void
FaxClient::setupHostModem(const char* cp)
{
    setupHostModem(fxStr(cp));
}

void FaxClient::startRunning()			{ running = TRUE; }
void FaxClient::stopRunning()			{ running = FALSE; }

void FaxClient::setProtocolVersion(u_int v)	{ version = v; }
void FaxClient::setHost(const fxStr& hostarg)	{ setupHostModem(hostarg); }
void FaxClient::setHost(const char* hostarg)	{ setupHostModem(hostarg); }
void FaxClient::setModem(const fxStr& modemarg)	{ modem = modemarg; }
void FaxClient::setModem(const char* modemarg)	{ modem = modemarg; }
void FaxClient::setVerbose(fxBool b)		{ verbose = b; }

void
FaxClient::setPort(int p)
{
    if (p == FAX_DEFPORT || p == port || trusted)
	port = p;
    else
	printError("Cannot set port to %u; operation is priviledged.", p);
}
void
FaxClient::setProtoName(const char* s)
{
    if (s == proto || trusted)
	proto = s;
    else
	printError("Cannot set protocol to %s; operation is priviledged.", s);
}

fxBool
FaxClient::setupUserIdentity()
{
    struct passwd* pwd = NULL;
    char* name = ::cuserid(NULL);
    if (!name) {
	name = ::getlogin();
	if (name)
	    pwd = ::getpwnam(name);
    }
    if (!pwd)
	pwd = ::getpwuid(::getuid());
    if (!pwd) {
	printError("Can not determine your user name.");
	return (FALSE);
    }
    userName = pwd->pw_name;
    if (pwd->pw_gecos && pwd->pw_gecos[0] != '\0') {
	senderName = pwd->pw_gecos;
	senderName.resize(senderName.next(0, '('));	// strip SysV junk
	u_int l = senderName.next(0, '&');
	if (l < senderName.length()) {
	    /*
	     * Do the '&' substitution and raise the
	     * case of the first letter of the inserted
	     * string (the usual convention...)
	     */
	    senderName.remove(l);
	    senderName.insert(userName, l);
	    if (islower(senderName[l]))
		senderName[l] = toupper(senderName[l]);
	}
	senderName.resize(senderName.next(0,','));
    } else
	senderName = userName;
    if (senderName.length() == 0) {
	printError("Bad (null) user name.");
	return (FALSE);
    } else
	return (TRUE);
}

fxBool
FaxClient::callServer()
{
    char* cp;
    if (host.length() == 0) {		// if host not specified by -h
	cp = ::getenv("FAXSERVER");
	if (cp && *cp != '\0') {
	    if (modem != "") {		// don't clobber specified modem
		fxStr m(modem);
		setupHostModem(cp);
		modem = m;
	    } else
		setupHostModem(cp);
	}
	if (host.length() == 0)
	    host = FAX_DEFHOST;
    }
    if (trusted && (cp = ::getenv("FAXSERVICE")) && *cp != '\0') {
	fxStr s(cp);
	u_int l = s.next(0,'/');
	port = (int) s.head(l);
	if (l < s.length())
	    proto = s.tail(s.length()-(l+1));
    }
    struct hostent* hp = ::gethostbyname((char*) host);
    if (!hp) {
	printError("%s: Unknown host", (char*) host);
	return (FALSE);
    }
    int protocol;
    struct protoent* pp = ::getprotobyname((char*) proto);
    if (!pp) {
	printWarning("%s: No protocol definition, using default.",
	    (char*) proto);
	protocol = 0;
    } else
	protocol = pp->p_proto;
    fd = ::socket(hp->h_addrtype, SOCK_STREAM, protocol);
    if (fd < 0) {
	printError("Can not create socket to connect to server.");
	return (FALSE);
    }
    struct sockaddr_in sin;
    ::memset(&sin, 0, sizeof (sin));
    sin.sin_family = hp->h_addrtype;
    if (port == -1) {
	struct servent* sp = ::getservbyname(FAX_SERVICE, (char*) proto);
	if (!sp) {
	    printWarning("No \"%s\" service definition, using default %u/%s.",
		FAX_SERVICE, FAX_DEFPORT, (char*) proto);
	    sin.sin_port = htons(FAX_DEFPORT);
	} else
	    sin.sin_port = sp->s_port;
    } else
	sin.sin_port = htons(port);
    for (char** cpp = hp->h_addr_list; *cpp; cpp++) {
	::memcpy(&sin.sin_addr, *cpp, hp->h_length);
	if (verbose)
	    ::printf("connect to %s (%s) at port %u\n",
		(char*) host, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
	if (::connect(fd, (struct sockaddr*) &sin, sizeof (sin)) >= 0) {
	    fdOut = fd;
	    ::signal(SIGPIPE, fxSIGHANDLER(SIG_IGN));
	    Dispatcher::instance().link(fd, Dispatcher::ReadMask, this);
	    if (version > 0) {
		sendLine("version", version);
		if (modem != "")
		    sendLine("modem", modem);
		if (userName == "")
		    setupUserIdentity();
		sendLine("userID", userName);
	    }
	    return (TRUE);
	}
    }
    printError("Can not reach \"%s\" service at host \"%s\".",
	FAX_SERVICE, (char*) host);
    ::close(fd), fd = -1;
    return (FALSE);
}

fxBool
FaxClient::hangupServer()
{
    if (fd != -1) {
	if (Dispatcher::instance().handler(fd, Dispatcher::ReadMask) == this)
	    Dispatcher::instance().unlink(fd);
	(void) ::close(fd);
	fd = -1;
    }
    return (TRUE);
}

void
FaxClient::setFds(const int in, const int out)
{
    fd = in;
    fdOut = out;
}

fxBool
FaxClient::sendLine(const char* cmd, int v)
{
    char num[20];
    ::sprintf(num, "%d", v);
    return sendLine(cmd, num);
}

fxBool
FaxClient::sendLine(const char* cmd, const char* tag)
{
    char line[2048];
    ::sprintf(line, "%s:%s\n", cmd, tag);
    char* cp = ::strchr(line, '\n');
    if (cp[1] != '\0') {
	if (::strchr(cmd, '\n'))
	    printError("Protocol botch, embedded newline in command \"%s\"",
		cmd);
	else
	    printError("Protocol botch, embedded newline in tag \"%s\"", tag);
	return (FALSE);
    } else
	return sendLine(line);
}

fxBool
FaxClient::sendLine(const char* cmd, const fxStr& s)
{
    return sendLine(cmd, (char*) s);
}

fxBool
FaxClient::sendLine(const char* line)
{
    if (peerdied)
	return (FALSE);
    if (verbose)
	printf("-> %s", line);
    u_int l = strlen(line);
    if (::write(fdOut, line, l) != l) {
	if (errno != EPIPE)
	    printError("Server write error; line was \"%s\".", line);
	else if (verbose)
	    ::printf("SEND peer died.\n");
	peerdied = TRUE;
	return (FALSE);
    } else
	return (TRUE);
}

fxBool
FaxClient::sendRawData(void* buf, int cc)
{
#ifdef __linux__
    /*
     * Linux kernel bug: can get short writes on
     * stream sockets when setup for blocking i/o.
     */
    u_char* bp = (u_char*) buf;
    for (int cnt, sent = 0; cc; sent += cnt, cc -= cnt) 
	if ((cnt = ::write(fdOut, bp + sent, cc)) <= 0) {
	    if (errno != EPIPE)
		printError("Protocol botch (server write error).");
	    else if (verbose)
		::printf("SEND DATA peer died.\n");
	    peerdied = TRUE;
	    return (FALSE);
	}
#else
    if (::write(fdOut, buf, cc) != cc) {
	if (errno != EPIPE)
	    printError("Protocol botch (server write error).");
	else if (verbose)
	    ::printf("SEND DATA peer died.\n");
	peerdied = TRUE;
	return (FALSE);
    }
#endif
    return (TRUE);
}

fxBool
FaxClient::sendData(const char* type, const char* filename)
{
    if (peerdied)
	return (FALSE);
    int tempfd = ::open(filename, O_RDONLY);
    if (tempfd < 0) {
	printError("%s: Can not open (sendData).", filename);
	return (FALSE);
    }
    struct stat sb;
    fstat(tempfd, &sb);
    int cc = (int) sb.st_size;
    if (verbose)
	::printf("SEND \"%s\" (%s:%d bytes)\n", filename, type, cc);
    sendLine(type, cc);
    while (cc > 0) {
	char buf[32*1024];
	int n = fxmin((size_t) cc, sizeof (buf));
	if (read(tempfd, buf, n) != n) {
	    printError("Protocol botch (data read).");
	    return (FALSE);
	}
	if (!sendRawData(buf, n))
	    return (FALSE);
	cc -= n;
    }
    ::close(tempfd);
    return (TRUE);
}

#define	putByte(b) {						\
    if (occ == sizeof (obuf)) {					\
	if (!sendRawData(obuf, sizeof (obuf)))			\
	    return (FALSE);					\
	ototal += sizeof (obuf);				\
	occ = 0;						\
    }								\
    obuf[occ++] = b;						\
}
#define	PutNextCode(c) {					\
    nextdata = (nextdata << nbits) | (c);			\
    if ((nextbits += nbits) >= 8) {				\
	putByte((u_char)(nextdata >> (nextbits-8)));		\
	if ((nextbits -= 8) >= 8) {				\
	    putByte((u_char)(nextdata >> (nextbits-8)));	\
	    nextbits -= 8;					\
	}							\
    }								\
}
#define	CLEARSTATE() {						\
    nbits = BITS_MIN;						\
    maxcode = MAXCODE(BITS_MIN);				\
    free_ent = CODE_FIRST;					\
    clearstate();						\
}

/*
 * Uses an open addressing double hashing (no chaining) on the 
 * prefix code/next character combination.  We do a variant of
 * Knuth's algorithm D (vol. 3, sec. 6.4) along with G. Knott's
 * relatively-prime secondary probe.  Here, the modular division
 * first probe is replaced with a faster exclusive-or manipulation. 
 */
fxBool
FaxClient::sendLZWData(int fdIn, int cc)
{
    u_int nbits;		// # of bits/code
    u_int maxcode;		// maximum code for nbits
    u_int free_ent;		// next free entry in hash table
    u_long nextdata = 0;	// next bits of i/o
    long nextbits = 0;		// # of valid bits in nextdata
    u_char ibuf[32*1024];	// input buffer
    u_char* bp;			// next available input byte
    u_char obuf[16*1024];	// output buffer
    u_int occ = 0;		// # of bytes in obuf
    u_long itotal = cc;		// input data size
    u_long ototal = 0;		// output (encoded) data size

    CLEARSTATE();
    PutNextCode(CODE_CLEAR);
    int n = fxmin((size_t) cc, sizeof (ibuf));
    if (::read(fdIn, ibuf, n) != n) {
	printError("Protocol botch (data read).");
	return (FALSE);
    }
    bp = ibuf, cc -= n;
    hcode_t ent = *bp++; n--;
    for (;;) {
	if (n == 0) {
	    if (cc == 0)
		break;
	    n = fxmin((size_t) cc, sizeof (ibuf));
	    if (::read(fdIn, ibuf, n) != n) {
		printError("Protocol botch (data read).");
		return (FALSE);
	    }
	    bp = ibuf, cc -= n;
	}
	int c = *bp++; n--;
	long fcode = ((long)c << BITS_MAX) + ent;
	int h = (c << HSHIFT) ^ ent;	/* xor hashing */
	hash_t* hp = &hashtab[h];
	if (hp->hash == fcode) {
	    ent = hp->code;
	    continue;
	}
	if (hp->hash >= 0) {
	    /*
	     * Primary hash failed, check secondary hash.
	     */
	    long disp = (h == 0 ? 1 : HSIZE - h);
	    do {
		if ((hp -= disp) < hashtab)
		    hp += HSIZE;
		if (hp->hash == fcode) {
		    ent = hp->code;
		    goto hit;
		}
	    } while (hp->hash >= 0);
	}
	/*
	 * New entry, emit code and add to table.
	 */
	PutNextCode(ent);
	ent = c;
	hp->code = free_ent++;
	hp->hash = fcode;
	if (free_ent == CODE_MAX-1) {		// table is full, reset
	    PutNextCode(CODE_CLEAR);
	    CLEARSTATE();
	} else if (free_ent > maxcode) {	// increase code size
	    nbits++;
	    fxAssert(nbits <= BITS_MAX, "LZW encoder overflow");
	    maxcode = MAXCODE(nbits);
	}
hit:
	;
    }
    if (ent != (hcode_t) -1) {
	PutNextCode(ent);
    }
    free_ent++;
    if (free_ent == CODE_MAX-1) {		// table is full, reset
	PutNextCode(CODE_CLEAR);
	CLEARSTATE();
    } else if (free_ent > maxcode) {		// increase code size
	nbits++;
	fxAssert(nbits <= BITS_MAX, "LZW encoder overflow");
    }
    PutNextCode(CODE_EOI);
    if (nextbits > 0) {
	putByte((u_char)(nextdata<<(8-nextbits)));
	ototal++;
    }
    if (occ > 0) {
	sendRawData(obuf, occ);
	ototal += occ;
    }
    if (verbose)
	::printf("SEND LZW: %.1fx compression (%lu -> %lu)\n",
	    (float) itotal / (float) ototal, itotal, ototal);
    return (TRUE);
}

void
FaxClient::clearstate(void)
{
    hash_t *hp = &hashtab[HSIZE-1];
    long i = HSIZE-8;
    do {
	i -= 8;
	hp[-7].hash = -1;
	hp[-6].hash = -1;
	hp[-5].hash = -1;
	hp[-4].hash = -1;
	hp[-3].hash = -1;
	hp[-2].hash = -1;
	hp[-1].hash = -1;
	hp[ 0].hash = -1;
	hp -= 8;
    } while (i >= 0);
    for (i += 8; i > 0; i--, hp--)
	hp->hash = -1;
}

fxBool
FaxClient::sendLZWData(const char* type, const char* filename)
{
    if (peerdied)
	return (FALSE);
    int tempfd = ::open(filename, O_RDONLY);
    if (tempfd < 0) {
	printError("%s: Can not open (sendLZWData).", filename);
	return (FALSE);
    }
    struct stat sb;
    ::fstat(tempfd, &sb);
    int cc = (int) sb.st_size;
    if (verbose)
	::printf("SEND LZW \"%s\" (%s:%d bytes)\n", filename, type, cc);
    sendLine(type, cc);
    fxBool status = sendLZWData(tempfd, cc); 
    ::close(tempfd);
    return (status);
}

int
FaxClient::inputReady(int)
{
    int n = ::read(fd, buf+prevcc, sizeof (buf) - prevcc - 1);
    if (n > 0) {
	n += prevcc;
	buf[n] = '\0';
	for (char *bp = buf; *bp;) {
	    char *cp = ::strchr(bp, '\n');
	    if (!cp) {
		prevcc = n - (bp - buf);
		::memmove(buf, bp, prevcc);
		goto done;
	    }
	    *cp++ = '\0';
	    if (verbose)
		::printf("<- %s\n", bp);
	    char* tag = ::strchr(bp, ':');
	    if (tag) {
		*tag++ = '\0';
		while (isspace(*tag))
		    tag++;
		recvConf(bp, tag);
	    } else if (strcmp(bp, ".") == 0) {
		recvConf(bp, "");
	    } else
		::fprintf(stderr,
		    "Malformed server message \"%s\" ignored.\n", bp);
	    bp = cp;
	}
	prevcc = 0;
    } else if (n == 0) {
	if (verbose)
	    ::printf("<- EOF\n");
	recvEof();
    } else {
	if (verbose)
	    ::printf("<- ERROR (errno = %d)\n", errno);
	if (!peerdied || errno != ECONNRESET)
	    recvError(errno);
    }
done:
    return (0);
}
