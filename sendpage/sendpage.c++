/*	$Header: /usr/people/sam/fax/./sendpage/RCS/sendpage.c++,v 1.15 1995/04/08 21:43:42 sam Rel $ */
/*
 * Copyright (c) 1994-1995 Sam Leffler
 * Copyright (c) 1994-1995 Silicon Graphics, Inc.
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
#include "FaxClient.h"
#include "StrArray.h"
#include "Dispatcher.h"
#include "DialRules.h"
#include "Sys.h"
#include "config.h"

#include <osfcn.h>
extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>		// XXX
#if HAS_LOCALE
#include <locale.h>
#endif
}

typedef unsigned int PageNotify;

class sendPageApp : public FaxClient {
public:
    enum {
	no_notice,
	when_done,
	when_requeued
    };
private:
    fxStr	destNumber;		// pager service provider dialstring
    fxStr	externalNumber;		// displayable version of destNumber
    fxBool	gotPermission;		// got response to checkPermission
    fxBool	permission;		// permission granted/denied
    fxBool	verbose;
    fxStr	killtime;		// job's time to be killed
    fxStr	sendtime;		// job's time to be sent
    int		maxRetries;		// max number times to try send
    int		maxDials;		// max number times to dial telephone
    int		priority;		// scheduling priority
    PageNotify	notify;
    fxBool	setup;			// if true, then ready to send
    DialStringRules* dialRules;		// dial string conversion database

    fxStr	mailbox;		// user@host return mail address
    fxStr	jobtag;			// user-specified job identifier
    fxStr	from;			// command line from information
    fxStr	senderName;		// sender's full name

    fxStr	appName;		// for error messages
    fxStrArray	pins;			// destination PINs
    fxStr	msgFile;		// file containing any text
protected:
    void copyToTemporary(int fin, fxStr& tmpl);
    void copyToTemporary(const fxStr& msg, fxStr& tmpl);

    void printError(const char* va_alist ...);
    void printWarning(const char* va_alist ...);
    void fatal(const char* fmt ...);
    void usage();

    void recvConf(const char* cmd, const char* tag);
    void recvEof();
    void recvError(const int err);
public:
    sendPageApp();
    ~sendPageApp();

    fxBool initialize(int argc, char** argv);

    virtual fxBool prepareSubmission();
    virtual fxBool submitJob();

    fxBool setupSenderIdentity(const fxStr&);
    const fxStr& getSenderName() const;

    void setMailbox(const char*);
    const fxStr& getMailbox() const;

    void setPriority(const char*);
};

sendPageApp::sendPageApp()
{
    dialRules = NULL;
    gotPermission = FALSE;
    permission = FALSE;
    verbose = FALSE;
    killtime = "now + 30 minutes";// default time to kill the job
    maxDials = FAX_REDIALS;
    maxRetries = FAX_RETRIES;
    priority = FAX_DEFPRIORITY;	// default priority
    notify = when_done;		// default notification
    setup = FALSE;
    verbose = 0;
}

sendPageApp::~sendPageApp()
{
    if (msgFile != "")
	Sys::unlink(msgFile);
    delete dialRules;
}

fxBool
sendPageApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    while ((c = Sys::getopt(argc, argv, "a:d:f:h:i:k:p:P:t:T:vDNR")) != -1)
	switch (c) {
	case 'a':			// time at which to transmit job
	    sendtime = optarg;
	    break;
	case 'D':			// notify when done
	    notify = when_done;
	    break;
	case 'd':			// pager service number
	    if (optarg[0] == '\0')
		fatal("No pager service number specified for -d option");
	    if (destNumber != "")
		fatal("Sorry, only one pager service number may be specified");
	    destNumber = optarg;
	    break;
	case 'f':			// sender's identity
	    from = optarg;
	    break;
	case 'h':			// server's host
	    setHost(optarg);
	    break;
	case 'i':			// user-specified job identifier
	    jobtag = optarg;
	    break;
	case 'k':			// time to kill job
	    killtime = optarg;
	    break;
	case 'N':			// no notification
	    notify = no_notice;
	    break;
	case 'p':			// PIN
	    pins.append(optarg);
	    break;
	case 'P':			// set scheduling priority
	    setPriority(optarg);
	    break;
	case 'R':			// notify when requeued or done
	    notify = when_requeued;
	    break;
	case 't':			// times to retry sending
	    maxRetries = ::atoi(optarg);
	    break;
	case 'T':			// times to dial telephone
	    maxDials = ::atoi(optarg);
	    break;
	case 'v':			// verbose mode
	    verbose++;
	    break;
	case '?':
	    usage();
	    /*NOTREACHED*/
	}
    if (destNumber == "") {
	printError("No pager service number specified");
	usage();
    }
    if (pins.length() == 0) {
	printError("No pager identification number (PIN) specified");
	usage();
    }

    if (verbose)
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    FaxClient::setVerbose(verbose > 0);	// protocol tracing

    if (optind < argc) {
	fxStr msg;
	for (; optind < argc; optind++) {
	    if (msg.length() > 0)
		msg.append(' ');
	    msg.append(argv[optind]);
	}
	if (msg.length() > 0)
	    copyToTemporary(msg, msgFile);
    } else
	copyToTemporary(fileno(stdin), msgFile);
    return (prepareSubmission() && submitJob());
}

void
sendPageApp::usage()
{
    fxFatal("usage: %s"
	" [-h host[:modem]]"
	" [-k kill-time]\n"
	" [-a time-to-send]"
	"    "
	" [-d destination]"
	" [-f from]"
	" [-i identifier]"
	" [-t max-tries]"
	" [-P priority]"
	" [-vDNR]"
	" [msgs...]",
	(char*) appName);
}

void
sendPageApp::setPriority(const char* pri)
{
    if (streq(pri, "default") || streq(pri, "normal"))
	priority = FAX_DEFPRIORITY;
    else if (streq(pri, "high"))
	priority = FAX_DEFPRIORITY - 4*16;
    else
	priority = atoi(pri);
}

/*
 * Copy data from fin to a temporary file.
 */
void
sendPageApp::copyToTemporary(int fin, fxStr& tmpl)
{
    tmpl = _PATH_TMP "sndpageXXXXXX";
    int fd = Sys::mkstemp(tmpl);
    if (fd < 0)
	fatal("%s: Can not create temporary file", (char*) tmpl);
    int cc, total = 0;
    char buf[16*1024];
    while ((cc = Sys::read(fin, buf, sizeof (buf))) > 0) {
	if (Sys::write(fd, buf, cc) != cc) {
	    Sys::unlink(tmpl);
	    fatal("%s: write error", (char*) tmpl);
	}
	total += cc;
    }
    ::close(fd);
    if (total == 0)
	Sys::unlink(tmpl), tmpl = "";
}

/*
 * Copy data from a string to a temporary file.
 */
void
sendPageApp::copyToTemporary(const fxStr& msg, fxStr& tmpl)
{
    tmpl = _PATH_TMP "sndpageXXXXXX";
    int fd = Sys::mkstemp(tmpl);
    if (fd < 0)
	fatal("%s: Can not create temporary file", (char*) tmpl);
    if (Sys::write(fd, msg, msg.length()) != msg.length()) {
	Sys::unlink(tmpl);
	fatal("%s: write error", (char*) tmpl);
	Sys::unlink(tmpl), tmpl = "";
    }
    ::close(fd);
}

fxBool
sendPageApp::prepareSubmission()
{
    if (!setupSenderIdentity(from))
	return (FALSE);
    dialRules = new DialStringRules(FAX_LIBDATA "/" FAX_DIALRULES);
    dialRules->setVerbose(verbose);
    if (!dialRules->parse() && verbose)		// NB: not fatal
	printWarning("unable to setup dialstring rules");
    /*
     * Convert dialstrings to a displayable format.  This
     * deals with problems like calling card access codes
     * getting displayed in status messages.
     */
    externalNumber = dialRules->displayNumber(destNumber);
    return (setup = TRUE);
}

#define	CHECK(x)	{ if (!(x)) return (FALSE); }
#define	CHECKSEND(x)	{ if (!(x)) goto failure; }

fxBool
sendPageApp::submitJob()
{
    u_int i;

    CHECK(setup && callServer())
    startRunning();
    /*
     * Explicitly check for permission to submit a job
     * before sending the input documents.  This way we
     * we avoid sending a load of stuff just to find out
     * that the user/host is not permitted to submit jobs.
     */
    CHECKSEND(sendLine("checkPerm", "send"))
    permission = gotPermission = FALSE;
    while (isRunning() && !getPeerDied() && !gotPermission)
	Dispatcher::instance().dispatch();
    CHECK(permission)

    /*
     * Transfer the text of the message.
     */
    if (msgFile != "")
	CHECKSEND(sendData("opaque", msgFile))

    CHECKSEND(sendLine("begin", 0))
    CHECKSEND(sendLine("jobtype", "pager"))
    if (sendtime != "")
	CHECKSEND(sendLine("sendAt", sendtime))
    if (maxDials >= 0)
	CHECKSEND(sendLine("maxdials", maxDials))
    if (maxRetries >= 0)
	CHECKSEND(sendLine("maxtries", maxRetries))
    if (killtime != "")
	CHECKSEND(sendLine("killtime", killtime))
    CHECKSEND(sendLine("priority", priority));
    /*
     * If the dialstring is different from the
     * displayable number then pass both.
     */
    if (destNumber != externalNumber)
	CHECKSEND(sendLine("external", externalNumber))
    CHECKSEND(sendLine("number", destNumber))
    CHECKSEND(sendLine("sender", senderName))
    CHECKSEND(sendLine("mailaddr", mailbox))
    if (jobtag != "")
	CHECKSEND(sendLine("jobtag", jobtag))
    if (notify == when_done)
	CHECKSEND(sendLine("notify", "when done"))
    else if (notify == when_requeued)
	CHECKSEND(sendLine("notify", "when requeued"))
    for (i = 0; i < pins.length(); i++) {
	CHECKSEND(sendLine("page", pins[i]))
	CHECKSEND(sendLine("!page", pins[i]))
    }
    CHECKSEND(sendLine("end", i))
    CHECKSEND(sendLine(".\n"));
    return (TRUE);
failure:
    if (getPeerDied()) {
	/*
	 * Look for the reason the peer died.
	 */
	while (isRunning())
	    Dispatcher::instance().dispatch();
    }
    return (FALSE);
}
#undef CHECKSEND
#undef CHECK

/*
 * Create the mail address for a local user.
 */
void
sendPageApp::setMailbox(const char* user)
{
    fxStr acct(user);
    if (acct.next(0, "@!") == acct.length()) {
	char hostname[64];
	(void) gethostname(hostname, sizeof (hostname));
	struct hostent* hp = gethostbyname(hostname);
	mailbox = acct | "@" | (hp ? hp->h_name : hostname);
    } else
	mailbox = acct;
}
const fxStr& sendPageApp::getMailbox() const		{ return mailbox; }

/*
 * Setup the sender's identity.
 */
fxBool
sendPageApp::setupSenderIdentity(const fxStr& from)
{
    FaxClient::setupUserIdentity();		// client identity

    if (from != "") {
	u_int l = from.next(0, '<');
	if (l == from.length()) {
	    l = from.next(0, '(');
	    if (l != from.length()) {		// joe@foobar (Joe Schmo)
		mailbox = from.head(l);
		l++; senderName = from.token(l, ')');
	    } else {				// joe
		setMailbox(from);
		if (from == getUserName())
		    senderName = FaxClient::getSenderName();
		else
		    senderName = "";
	    }
	} else {				// Joe Schmo <joe@foobar>
	    senderName = from.head(l);
	    l++; mailbox = from.token(l, '>');
	}
	if (senderName == "" && mailbox != "") {
	    /*
	     * Mail address, but no "real name"; construct one from
	     * the account name.  Do this by first stripping anything
	     * to the right of an '@' and then stripping any leading
	     * uucp patch (host!host!...!user).
	     */
	    senderName = mailbox;
	    senderName.resize(senderName.next(0, '@'));
	    senderName.remove(0, senderName.nextR(senderName.length(), '!'));
	}

	// strip and leading&trailing white space
	senderName.remove(0, senderName.skip(0, " \t"));
	senderName.resize(senderName.skipR(senderName.length(), " \t"));
	mailbox.remove(0, mailbox.skip(0, " \t"));
	mailbox.resize(mailbox.skipR(mailbox.length(), " \t"));
	if (senderName == "" || mailbox == "") {
	    printError("Malformed (null) sender name or mail address");
	    return (FALSE);
	}
    } else {
	senderName = FaxClient::getSenderName();
	setMailbox(getUserName());
    }
    return (TRUE);
}
const fxStr& sendPageApp::getSenderName() const	{ return senderName; }

#define	isCmd(s)	(::strcasecmp(s, cmd) == 0)

void
sendPageApp::recvConf(const char* cmd, const char* tag)
{
    if (isCmd("job")) {
	u_int len = pins.length();
	const char* id = ::strchr(tag, ':');
	if (id) {
	    printf("request id is %.*s (group id %s) for host %s (%u %s)\n",
		id-tag, tag, id+1,
		(char*) getHost(),
		len, len > 1 ? "msgs" : "msg");
	} else
	    printf("request id is %s for host %s (%u %s)\n",
		tag, (char*) getHost(), len, len > 1 ? "msgs" : "msg");
    } else if (isCmd("error")) {
	printf("%s\n", tag);
    } else if (isCmd("permission")) {
	gotPermission = TRUE;
	permission = (strcasecmp(tag, "granted") == 0);
    } else
	printError("Unknown status message received: \"%s:%s\"", cmd, tag);
}

void
sendPageApp::recvEof()
{
    stopRunning();
}

void
sendPageApp::recvError(const int err)
{
    printError("Socket read error: %s", strerror(err));
    stopRunning();
}

void
sendPageApp::printError(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: ", (const char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(-1);
}
#undef fmt

void
sendPageApp::printWarning(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: Warning, ", (const char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
}
#undef fmt

#include <signal.h>

static	sendPageApp* app = NULL;

static void
cleanup()
{
    sendPageApp* a = app;
    app = NULL;
    delete a;
}

static void
sigDone(int)
{
    cleanup();
    exit(-1);
}

int
main(int argc, char** argv)
{
#ifdef LC_CTYPE
    setlocale(LC_CTYPE, "");			// for <ctype.h> calls
#endif
#ifdef LC_TIME
    setlocale(LC_TIME, "");			// for strftime calls
#endif
    signal(SIGHUP, fxSIGHANDLER(sigDone));
    signal(SIGINT, fxSIGHANDLER(sigDone));
    signal(SIGTERM, fxSIGHANDLER(sigDone));
    app = new sendPageApp;
    if (!app->initialize(argc, argv))
	sigDone(0);
    while (app->isRunning())
	Dispatcher::instance().dispatch();
    signal(SIGHUP, fxSIGHANDLER(SIG_IGN));
    signal(SIGINT, fxSIGHANDLER(SIG_IGN));
    signal(SIGTERM, fxSIGHANDLER(SIG_IGN));
    cleanup();
    return (0);
}

static void
vfatal(FILE* fd, const char* fmt, va_list ap)
{
    vfprintf(fd, fmt, ap);
    va_end(ap);
    fputs(".\n", fd);
    sigDone(0);
}

void
fxFatal(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, fmt);
    vfatal(stderr, fmt, ap);
    /*NOTTEACHED*/
}
#undef fmt

void
sendPageApp::fatal(const char* va_alist ...)
#define	fmt va_alist
{
    fprintf(stderr, "%s: ", (const char*) appName);
    va_list ap;
    va_start(ap, fmt);
    vfatal(stderr, fmt, ap);
    /*NOTTEACHED*/
}
#undef fmt
