/*	$Header: /usr/people/sam/fax/./recvfax/RCS/main.c,v 1.66 1995/04/08 21:43:09 sam Rel $ */
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
#include "defs.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

char*	SPOOLDIR = FAX_SPOOLDIR;

char**	dataFiles;
int*	fileTypes;
int	nDataFiles = 0;
int	nPollIDs = 0;
char**	pollIDs;
char	line[1024];		/* current input line */
char*	tag;			/* keyword: tag */
int	debug = 0;
Job*	jobList = 0;
int	version = 0;
char	userID[1024];
struct	tm now;			/* current time of day */
int	privileged = 0;
static struct sockaddr_in peeraddr;
int	peeraddrlen;

extern	int cvtFacility(const char*, int*);

void
done(int status, char* how)
{
    fflush(stdout);
    if (debug)
	syslog(LOG_DEBUG, how);
    exit(status);
}

static void
setUserID(const char* modemname, char* tag)
{
    (void) modemname;
    strncpy(userID, tag, sizeof (userID)-1);
}

static void
setProtoVersion(const char* modemname, char* tag)
{
    (void) modemname;
    version = atoi(tag);
    if (version > FAX_PROTOVERS) {
	protocolBotch(
	    "protocol version %u requested: only understand up to %u.",
	    version, FAX_PROTOVERS);
	done(1, "EXIT");
    }
}

static void
ackPermission(const char* modemname, char* tag)
{
    (void) modemname; (void) tag;
    sendClient("permission", "%s", "granted");
    fflush(stdout);
}

#define	TRUE	1
#define	FALSE	0

struct {
    const char* cmd;		/* command to match */
    int		check;		/* if true, checkPermission first */
    void	(*cmdFunc)(const char*, char*);
} cmds[] = {
    { "begin",			TRUE,	submitJob },
    { "checkPerm",		TRUE,	ackPermission },
    { "tiff",			TRUE,	getTIFFData },
    { "postscript",		TRUE,	getPostScriptData },
    { "zpostscript",		TRUE,	getZPostScriptData },
    { "opaque",			TRUE,	getOpaqueData },
    { "zopaque",		TRUE,	getZOpaqueData },
    { "data",			TRUE,	getDataOldWay },
    { "poll",			TRUE,	newPollID },
    { "userID",			FALSE,	setUserID },
    { "version",		FALSE,	setProtoVersion },
    { "serverStatus",		FALSE,	sendServerStatus },
    { "serverInfo",		FALSE,	sendServerInfo },
    { "allStatus",		FALSE,	sendAllStatus },
    { "userStatus",		FALSE,	sendUserStatus },
    { "jobStatus",		FALSE,	sendJobStatus },
    { "recvStatus",		FALSE,	sendRecvStatus },
    { "remove",			TRUE,	removeJob },
    { "removeGroup",		TRUE,	removeJobGroup },
    { "kill",			TRUE,	killJob },
    { "killGroup",		TRUE,	killJobGroup },
    { "alterTTS",		TRUE,	alterJobTTS },
    { "alterGroupTTS",		TRUE,	alterJobGroupTTS },
    { "alterKillTime",		TRUE,	alterJobKillTime },
    { "alterGroupKillTime",	TRUE,	alterJobGroupKillTime },
    { "alterMaxDials",		TRUE,	alterJobMaxDials },
    { "alterGroupMaxDials",	TRUE,	alterJobGroupMaxDials },
    { "alterNotify",		TRUE,	alterJobNotification },
    { "alterGroupNotify",	TRUE,	alterJobGroupNotification },
    { "alterModem",		TRUE,	alterJobModem },
    { "alterGroupModem",	TRUE,	alterJobGroupModem },
    { "alterPriority",		TRUE,	alterJobPriority },
    { "alterGroupPriority",	TRUE,	alterJobGroupPriority },
};
#define	NCMDS	(sizeof (cmds) / sizeof (cmds[0]))

main(int argc, char** argv)
{
    extern char* optarg;
    char modemname[80];
    time_t t = time(0);
    struct sockaddr_in sin;
    int sinlen;
    int c;
    int facility = LOG_DAEMON;

    now = *localtime(&t);
    (void) cvtFacility(LOG_FAX, &facility);
    openlog(argv[0], LOG_PID, facility);
    umask(077);
    while ((c = getopt(argc, argv, "q:d")) != -1)
	switch (c) {
	case 'q':
	    SPOOLDIR = optarg;
	    break;
	case 'd':
	    debug = 1;
	    syslog(LOG_DEBUG, "BEGIN");
	    break;
	case '?':
	    syslog(LOG_ERR,
		"Bad option `%c'; usage: faxd.recv [-q queue-dir] [-d]", c);
	    done(-1, "EXIT");
	}
    if (chdir(SPOOLDIR) < 0) {
	syslog(LOG_ERR, "%s: chdir: %m", SPOOLDIR);
	sendError("Can not change to spooling directory.");
	done(-1, "EXIT");
    }
    if (debug) {
	char buf[82];
	syslog(LOG_DEBUG, "chdir to %s", getcwd(buf, 80));
    }
#if defined(SO_LINGER) && !defined(__linux__)
    { struct linger opt;
      opt.l_onoff = 1;
      opt.l_linger = 60;
      if (setsockopt(0, SOL_SOCKET, SO_LINGER, (char*)&opt, sizeof (opt)) < 0)
	syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
    }
#endif
    sinlen = sizeof (sin);
    if (getsockname(0, (struct sockaddr*) &sin, &sinlen) == 0) {
	privileged = (sin.sin_port == htons(FAX_DEFPORT+1));
	peeraddrlen = sizeof (peeraddr);
	getpeername(0, (struct sockaddr*) &peeraddr, &peeraddrlen);
	if (privileged)
	    syslog(LOG_NOTICE,
		"Connection to privileged fax service port from %s.",
		getClientIdentity());
    } else {
	syslog(LOG_WARNING, "getsockname: %m");
	memset(&peeraddr, 0, sizeof (peeraddr));
    }
    strcpy(modemname, MODEM_ANY);
    strcpy(userID, "");
    while (getCommandLine() && !isCmd(".")) {
	if (isCmd("modem")) {			/* select outgoing device */
	    int l = strlen(DEV_PREFIX);
	    char* cp;
	    /*
	     * Convert modem name to identifier form by stripping
	     * any leading device pathname prefix and by replacing
	     * '/'s with '_'s for SVR4 where terminal devices are
	     * in subdirectories.
	     */
	    if (strncmp(tag, DEV_PREFIX, l) == 0)
		tag += l;
	    for (cp = tag; cp = strchr(cp, '/'); *cp = '_')
		;
	    strncpy(modemname, tag, sizeof (modemname)-1);
	} else {
	    int i;
	    for (i = 0; i < NCMDS && !isCmd(cmds[i].cmd); i++)
		;
	    if (i == NCMDS) {
		protocolBotch("unrecognized cmd \"%s\".", line);
		done(1, "EXIT");
	    }
	    if (cmds[i].check) {
		checkPermission();
		if (!privileged)		/* bypass shutdown controls */
		    checkServerStatus(modemname);
	    }
	    (*cmds[i].cmdFunc)(modemname, tag);
	}
    }
    /* remove original files -- only links remain */
    { int i;
      for (i = 0; i <nDataFiles; i++)
	unlink(dataFiles[i]);
    }
    done(0, "END");
}

const char*
getClientIdentity()
{
    struct hostent* hp =
	gethostbyaddr(&peeraddr.sin_addr, peeraddrlen, peeraddr.sin_family);
    return (hp ? hp->h_name : inet_ntoa(peeraddr.sin_addr));
}

int
getCommandLine()
{
    char* cp;

    if (!fgets(line, sizeof (line) - 1, stdin)) {
	protocolBotch("unexpected EOF.");
	return (0);
    }
    cp = strchr(line, '\0');
    if (cp > line && cp[-1] == '\n')
	*--cp = '\0';
    if (cp > line && cp[-1] == '\r')		/* for telnet users */
	*--cp = '\0';
    if (debug)
	syslog(LOG_DEBUG, "line \"%s\"", line);
    if (strcmp(line, ".") && strcmp(line, "..")) {
	tag = strchr(line, ':');
	if (!tag) {
	    protocolBotch("malformed line \"%s\".", line);
	    return (0);
	}
	*tag++ = '\0';
	while (isspace(*tag))
	    tag++;
    }
    return (1);
}

int
openFIFO(const char* fifoname)
{
    int fd;
#ifdef FIFOSELECTBUG
    /*
     * We try multiple times to open the appropriate FIFO
     * file because the system has a kernel bug that forces
     * the server to close+reopen the FIFO file descriptors
     * for each message received on the FIFO (yech!).
     */
    { int tries = 0;
      do {
	if (tries > 0)
	    sleep(1);
	fd = open(fifoname, O_WRONLY|O_NDELAY);
      } while (fd == -1 && errno == ENXIO && ++tries < 5);
    }
#else
    fd = open(fifoname, O_WRONLY|O_NDELAY);
#endif
    return (fd);
}

/*
 * Notify server of job parameter alteration.
 */
int
notifyServer(const char* modem, const char* fmt, ...)
{
    int fifo;

    if (debug)
	syslog(LOG_DEBUG, "notify server for \"%s\"", modem);
    fifo = openFIFO(FAX_FIFO);
    if (fifo != -1) {
	char buf[2048];
	int len, ok;
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	len = strlen(buf);
	if (debug)
	    syslog(LOG_DEBUG, "write \"%.*s\" to fifo", len, buf);
	/*
	 * Turn off O_NDELAY so that write will block if FIFO is full.
	 */
	if (fcntl(fifo, F_SETFL, fcntl(fifo, F_GETFL, 0) &~ O_NDELAY) <0)
	    syslog(LOG_ERR, "fcntl: %m");
	ok = (write(fifo, buf, len+1) == len+1);
	if (!ok)
	    syslog(LOG_ERR, "FIFO write failed: %m");
	close(fifo);
	return (ok);
    } else if (debug)
	syslog(LOG_INFO, "%s: Can not open for notification: %m", FAX_FIFO);
    return (0);
}

extern int parseAtSyntax(const char*, const struct tm*, struct tm*, char* emsg);

int
cvtTime(const char* spec, struct tm* ref, u_long* result, const char* what)
{
    char emsg[1024];
    struct tm when;
    if (!parseAtSyntax(spec, ref, &when, emsg)) {
	sendAndLogError("Error parsing %s \"%s\": %s.", what, spec, emsg);
	return (0);
    } else {
	*result = (u_long) mktime(&when);
	return (1);
    }
}

void
vsendClient(char* tag, char* fmt, va_list ap)
{
    fprintf(stdout, "%s:", tag);
    vfprintf(stdout, fmt, ap);
    fputc('\n', stdout);
    if (debug) {
	char buf[2048];
	sprintf(buf, "%s:", tag);
	vsprintf(buf+strlen(buf), fmt, ap);
	syslog(LOG_DEBUG, "%s", buf);
    }
}

void
sendClient(char* tag, char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsendClient(tag, fmt, ap);
    va_end(ap);
}

void
sendError(char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsendClient("error", fmt, ap);
    va_end(ap);
}

void
sendAndLogError(char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsendClient("error", fmt, ap);
    vsyslog(LOG_ERR, fmt, ap);
    va_end(ap);
}

void
protocolBotch(char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    sprintf(buf, "Protocol botch, %s", fmt);
    vsendClient("error", buf, ap);
    vsyslog(LOG_ERR, buf, ap);
    va_end(ap);
}
