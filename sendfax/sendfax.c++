/*	$Header: /usr/people/sam/fax/./sendfax/RCS/sendfax.c++,v 1.79 1995/04/08 21:43:21 sam Rel $ */
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
#include "SendFaxClient.h"
#include "FaxDB.h"
#include "Dispatcher.h"
#include "config.h"
#include "Sys.h"

#include <pwd.h>
#include <osfcn.h>
#if HAS_LOCALE
extern "C" {
#include <locale.h>
}
#endif

class sendFaxApp : public SendFaxClient {
private:
    fxStr	appName;		// for error messages
    fxStr	stdinTemp;		// temp file for collecting from pipe
    FaxDB*	db;
    int		verbose;
    fxStr	company;
    fxStr	location;

    static fxStr dbName;

    void addDestination(const char* cp);
    void copyToTemporary(int fin, fxStr& tmpl);
    fxStr tildeExpand(const fxStr& filename);
    void printError(const char* va_alist ...);
    void printWarning(const char* va_alist ...);
    void fatal(const char* fmt ...);
    void usage();

    virtual void recvConf(const char* cmd, const char* tag);
public:
    sendFaxApp();
    ~sendFaxApp();

    fxBool initialize(int argc, char** argv);

    void setPriority(const char*);
};

fxStr sendFaxApp::dbName("~/.faxdb");

sendFaxApp::sendFaxApp()
{
    db = NULL;
    verbose = 0;
}

sendFaxApp::~sendFaxApp()
{
    if (stdinTemp != "")
	Sys::unlink(stdinTemp);
    delete db;
}

fxBool
sendFaxApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    db = new FaxDB(tildeExpand(dbName));
    while ((c = Sys::getopt(argc, argv, "a:c:d:f:h:i:k:P:r:s:t:T:x:y:lmnpvDNR")) != -1)
	switch (c) {
	case 'a':			// time at which to transmit job
	    setSendTime(optarg);
	    break;
	case 'c':			// cover sheet: comment field
	    setCoverComments(optarg);
	    break;
	case 'D':			// notify when done
	    setNotification(SendFaxClient::when_done);
	    break;
	case 'd':			// destination name and number
	    addDestination(optarg);
	    break;
	case 'f':			// sender's identity
	    setFromIdentity(optarg);
	    break;
	case 'h':			// server's host
	    setHost(optarg);
	    break;
	case 'i':			// user-specified job identifier
	    setJobTag(optarg);
	    break;
	case 'k':			// time to kill job
	    setKillTime(optarg);
	    break;
	case 'l':			// low resolution
	    setResolution(98.);
	    break;
	case 'm':			// medium resolution
	    setResolution(196.);
	    break;
	case 'n':			// no cover sheet
	    setCoverSheet(FALSE);
	    break;
	case 'N':			// no notification
	    setNotification(SendFaxClient::no_notice);
	    break;
	case 'p':			// submit polling request
	    setPollRequest(TRUE);
	    break;
	case 'P':			// set scheduling priority
	    setPriority(optarg);
	    break;
	case 'r':			// cover sheet: regarding field
	    setCoverRegarding(optarg);
	    break;
	case 'R':			// notify when requeued or done
	    setNotification(SendFaxClient::when_requeued);
	    break;
	case 's':			// set page size
	    setPageSize(optarg);
	    break;
	case 't':			// times to retry sending
	    setMaxRetries(atoi(optarg));
	    break;
	case 'T':			// times to dial telephone
	    setMaxDials(atoi(optarg));
	    break;
	case 'v':			// verbose mode
	    verbose++;
	    break;
	case 'x':			// cover page: to's company
	    company = optarg;
	    break;
	case 'y':			// cover page: to's location
	    location = optarg;
	    break;
	case '?':
	    usage();
	    /*NOTREACHED*/
	}
    if (getNumberOfDestinations() == 0) {
	fprintf(stderr, "%s: No destination specified.\n", (char*) appName);
	usage();
    }

    if (verbose)
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    SendFaxClient::setVerbose(verbose > 0);	// type rules & basic operation
    FaxClient::setVerbose(verbose > 1);		// protocol tracing

    if (optind < argc) {
	for (; optind < argc; optind++)
	    addFile(argv[optind]);
    } else if (!getPollRequest()) {
	copyToTemporary(fileno(stdin), stdinTemp);
	addFile(stdinTemp);
    }
    return (prepareSubmission() && submitJob());
}

void
sendFaxApp::usage()
{
    fxFatal("usage: %s"
	" [-h host[:modem]]"
	" [-k kill-time]\n"
	"    "
	" [-a time-to-send]"
	" [-c comments]"
	" [-x company]"
	" [-y location]"
	" [-r regarding]\n"
	"    "
	" [-d destination]"
	" [-f from]"
	" [-i identifier]"
	" [-s pagesize]"
	" [-t max-tries]"
	" [-P priority]"
	" [-lmnpvDR]"
	" [files]",
	(char*) appName);
}

void
sendFaxApp::setPriority(const char* pri)
{
    int priority;

    if (streq(pri, "default") || streq(pri, "normal"))
	priority = FAX_DEFPRIORITY;
    else if (streq(pri, "bulk") || streq(pri, "junk"))
	priority = FAX_DEFPRIORITY + 4*16;
    else if (streq(pri, "high"))
	priority = FAX_DEFPRIORITY - 4*16;
    else
	priority = atoi(pri);
    SendFaxClient::setPriority(priority);
}

/*
 * Add a destination; parse ``person@number'' syntax.
 */
void
sendFaxApp::addDestination(const char* cp)
{
    fxStr recipient;
    const char* tp = strchr(cp, '@');
    if (tp) {
	recipient = fxStr(cp, tp-cp);
	cp = tp+1;
    } else
	recipient = "";
    fxStr dest(cp);
    if (db && dest.length() > 0) {
	fxStr name;
	FaxDBRecord* r = db->find(dest, &name);
	if (r) {
	    if (recipient == "")
		recipient = name;
	    dest = r->find(FaxDB::numberKey);
	}
    }
    if (dest.length() == 0)
	fatal("Null destination for \"%s\"", cp);
    if (!SendFaxClient::addDestination(recipient, dest, company, location))
	fatal("Could not add fax destination");
}

/*
 * Expand a filename that might have `~' in it.
 */
fxStr
sendFaxApp::tildeExpand(const fxStr& filename)
{
    fxStr path(filename);
    if (filename.length() > 1 && filename[0] == '~') {
	path.remove(0);
	char* cp = getenv("HOME");
	if (!cp || *cp == '\0') {
	    struct passwd* pwd = getpwuid(getuid());
	    if (!pwd)
		fatal("Can not figure out who you are");
	    cp = pwd->pw_dir;
	}
	path.insert(cp);
    }
    return (path);
}

/*
 * Copy data from fin to a temporary file.
 */
void
sendFaxApp::copyToTemporary(int fin, fxStr& tmpl)
{
    tmpl = _PATH_TMP "sndfaxXXXXXX";
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
    if (total == 0) {
	Sys::unlink(tmpl);
	tmpl = "";
	fatal("No input data; tranmission aborted");
    }
}

void
sendFaxApp::printError(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: ", (char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(-1);
}
#undef fmt

void
sendFaxApp::printWarning(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: Warning, ", (char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
}
#undef fmt

#define	isCmd(s)	(strcasecmp(s, cmd) == 0)

void
sendFaxApp::recvConf(const char* cmd, const char* tag)
{
    if (isCmd("job")) {
	u_int len = getNumberOfFiles();
	const char* id = ::strchr(tag, ':');
	if (id) {
	    printf("request id is %.*s (group id %s) for host %s (%u %s)\n",
		id-tag, tag, id+1,
		(char*) getHost(),
		len, len > 1 ? "files" : "file");
	} else
	    printf("request id is %s for host %s (%u %s)\n",
		tag, (char*) getHost(), len, len > 1 ? "files" : "file");
    } else if (isCmd("error")) {
	printf("%s\n", tag);
    } else
	SendFaxClient::recvConf(cmd, tag);
}

#include <signal.h>

static	sendFaxApp* app = NULL;

static void
cleanup()
{
    sendFaxApp* a = app;
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
    app = new sendFaxApp;
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
sendFaxApp::fatal(const char* va_alist ...)
#define	fmt va_alist
{
    fprintf(stderr, "%s: ", (char*) appName);
    va_list ap;
    va_start(ap, fmt);
    vfatal(stderr, fmt, ap);
    /*NOTTEACHED*/
}
#undef fmt
