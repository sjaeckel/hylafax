/*	$Header: /usr/people/sam/fax/./faxd/RCS/faxApp.c++,v 1.13 1995/04/08 21:31:12 sam Rel $ */
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
#include "faxApp.h"
#include "config.h"

#include <errno.h>
#include <pwd.h>
#include <limits.h>
extern "C" {
#include <syslog.h>
#if HAS_LOCALE
#include <locale.h>
#endif
}

#include "Sys.h"

/*
 * Logging and error routines.
 */

void
vlogInfo(const char* fmt, va_list ap)
{
    ::vsyslog(LOG_INFO|faxApp::getLogFacility(), fmt, ap);
}

void
logInfo(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlogInfo(fmt, ap);
    va_end(ap);
}

void
vlogError(const char* fmt, va_list ap)
{
    ::vsyslog(LOG_ERR|faxApp::getLogFacility(), fmt, ap);
}

void
logError(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlogError(fmt, ap);
    va_end(ap);
}

void
fxFatal(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlogError(fmt, ap);
    va_end(ap);
    ::exit(-1);
}

/*
 * getopt Iterator Interface.
 */
extern int opterr, optind;
extern char* optarg;

GetoptIter::GetoptIter(int ac, char** av, const fxStr& s) : opts(s)
{
    argc = ac;
    argv = av;
    optind = 1;
    opterr = 0;
    c = Sys::getopt(argc, argv, opts);
}
GetoptIter::~GetoptIter() {}

void GetoptIter::operator++()		{ c = Sys::getopt(argc, argv, opts); }
void GetoptIter::operator++(int)	{ c = Sys::getopt(argc, argv, opts); }
const char* GetoptIter::optArg() const	{ return optarg; }
const char* GetoptIter::getArg()
   { return optind < argc ? argv[optind] : ""; }
const char* GetoptIter::nextArg()
   { return optind < argc ? argv[optind++] : ""; }

faxApp::faxApp()
{
    running = FALSE;
    setLogFacility(LOG_FAX);			// default
#ifdef LC_CTYPE
    setlocale(LC_CTYPE, "");			// for <ctype.h> calls
#endif
#ifdef LC_TIME
    setlocale(LC_TIME, "");			// for strftime calls
#endif
}
faxApp::~faxApp() {}

void
faxApp::initialize(int, char**)
{
    openFIFOs();
}
void faxApp::open(void) { running = TRUE; }
void faxApp::close(void) { running = FALSE; }

fxStr faxApp::getopts;
void faxApp::setOpts(const char* s) { getopts = s; }
const fxStr& faxApp::getOpts() { return getopts; }

int faxApp::facility = LOG_DAEMON;

void
faxApp::setupLogging(const char* appName)
{
    ::openlog(appName, LOG_PID|LOG_ODELAY, facility);
}

extern	"C" int cvtFacility(const char*, int*);

void
faxApp::setLogFacility(const char* fac)
{
    if (!cvtFacility(fac, &facility))
	logError("Unknown syslog facility name \"%s\"", fac);
}

/*
 * FIFO-related support.
 */

/*
 * Open the requisite FIFO special files.
 */
void
faxApp::openFIFOs(void)
{
}

void
faxApp::closeFIFOs(void)
{
}

/*
 * Open the specified FIFO file.
 */
int
faxApp::openFIFO(const char* fifoName, int mode, fxBool okToExist)
{
    if (Sys::mkfifo(fifoName, mode & 0777) < 0) {
	if (errno != EEXIST || !okToExist)
	    fxFatal("Could not create FIFO \"%s\".", fifoName);
    }
    int fd = Sys::open(fifoName, CONFIG_OPENFIFO|O_NDELAY, 0);
    if (fd == -1)
	fxFatal("Could not open FIFO file \"%s\"", fifoName);
    // open should set O_NDELAY, but just to be sure...
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NDELAY) < 0)
	logError("openFIFO: fcntl: %m");
    return (fd);
}

/*
 * Respond to input on a FIFO file descriptor.
 */
int
faxApp::FIFOInput(int fd)
{
    char buf[2048];
    int n;
    while ((n = ::read(fd, buf, sizeof (buf)-1)) > 0) {
	buf[n] = '\0';
	/*
	 * Break up '\0'-separated records and strip
	 * any trailing '\n' so that "echo mumble>FIFO"
	 * works (i.e. echo appends a '\n' character).
	 */
	char* bp = &buf[0];
	do {
	    char* cp = strchr(bp, '\0');
	    if (cp > bp) {
		if (cp[-1] == '\n')
		    cp[-1] = '\0';
		FIFOMessage(bp);
	    }
	    bp = cp+1;
	} while (bp < &buf[n]);
    }
#ifdef FIFOSELECTBUG
    /*
     * Solaris 2.x botch (and some versions of IRIX 5.x).
     *
     * A client close of an open FIFO causes an M_HANGUP to be
     * sent and results in the receiver's file descriptor being
     * marked ``hung up''.  This in turn causes select to
     * perpetually return true and if we're running as a realtime
     * process, brings the system to a halt.  The workaround for
     * Solaris 2.1 was to do a parallel reopen of the appropriate
     * FIFO so that the original descriptor is recycled.  This
     * apparently no longer works in Solaris 2.2 or later and we
     * are forced to close and reopen both FIFO descriptors (noone
     * appears capable of answering why this this is necessary and
     * I personally don't care...)
     */
    closeFIFOs(); openFIFOs();
#endif
    return (0);
}

/*
 * Process a message received through a FIFO.
 */
void
faxApp::FIFOMessage(const char* cp)
{
    logError("Bad fifo message \"%s\"", cp);
}

/*
 * Miscellaneous stuff.
 */

/*
 * Force the real uid+gid to be the same as
 * the effective ones.  Must temporarily
 * make the effective uid root in order to
 * do the real id manipulations.
 */
void
faxApp::setRealIDs(void)
{
    uid_t euid = ::geteuid();
    if (::seteuid(0) < 0)
	logError("seteuid(root): %m");
    if (::setgid(getegid()) < 0)
	logError("setgid: %m");
    if (::setuid(euid) < 0)
	logError("setuid: %m");
}

static void
detachIO(void)
{
    int fd = Sys::open(_PATH_DEVNULL, O_RDWR);
    ::dup2(fd, STDIN_FILENO);
    ::dup2(fd, STDOUT_FILENO);
    ::dup2(fd, STDERR_FILENO);
    for (fd = 0; fd < _POSIX_OPEN_MAX; fd++)
	if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
	    (void) ::close(fd);
}

/*
 * Run the specified shell command.  If changeIDs is
 * true, we set the real uid+gid to the effective; this
 * is so that programs like sendmail show an informative
 * from address.
 */
fxBool
faxApp::runCmd(const char* cmd, fxBool changeIDs)
{
    pid_t pid = ::fork();
    switch (pid) {
    case 0:
	if (changeIDs)
	    setRealIDs();
	detachIO();
	execl("/bin/sh", "sh", "-c", cmd, (char*) NULL);
	::sleep(1);			// XXX give parent time
	::_exit(127);
    case -1:
	logError("Can not fork for \"%s\"", cmd);
	return (FALSE);
    default:
	{ int status = 0;
	  Sys::waitpid(pid, status);
	  if (status != 0) {
	    logError("Bad exit status %#o for \"%s\"", status, cmd);
	    return (FALSE);
	  }
	}
	return (TRUE);
    }
}

/*
 * Setup server uid+gid.  Normally the server is started up
 * by root and then sets its effective uid+gid to reflect
 * those of the ``fax'' user.  This permits the server to
 * switch to ``root'' whenever it's necessary (in order to
 * gain access to a root-specific function such as starting
 * a getty process).  Alternatively the server may be run
 * setuid ``fax'' with the real uid of ``root'' (in order to
 * do priviledged operations).
 */ 
void
faxApp::setupPermissions(void)
{
    if (::getuid() != 0)
	fxFatal("The fax server must run with real uid root.\n");
    uid_t euid = ::geteuid();
    const passwd* pwd = ::getpwnam(FAX_USER);
    if (!pwd)
	fxFatal("No fax user \"%s\" defined on your system!\n"
	    "This software is not installed properly!", FAX_USER);
    if (euid == 0) {
	if (::setegid(pwd->pw_gid) < 0)
	    fxFatal("Can not setup permissions (gid)");
	if (::seteuid(pwd->pw_uid) < 0)
	    fxFatal("Can not setup permissions (uid)");
    } else {
	uid_t faxuid = pwd->pw_uid;
	::setpwent();
	pwd = ::getpwuid(euid);
	if (!pwd)
	    fxFatal("Can not figure out the identity of uid %u", euid);
	if (pwd->pw_uid != faxuid)
	    fxFatal("Configuration error; "
		"the fax server must run as the fax user \"%s\".", FAX_USER);
    }
    ::endpwent();
}

/*
 * Break the association with the controlling
 * tty if we can preserve it later with the
 * POSIX O_NOCTTY mechanism.
 */
void
faxApp::detachFromTTY(void)
{
#ifdef O_NOCTTY
    detachIO();
    switch (fork()) {
    case 0:	break;			// child, continue
    case -1:	::_exit(1);		// error
    default:	::_exit(0);		// parent, terminate
    }
    (void) ::setsid();
#endif
}
