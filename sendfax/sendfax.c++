/*	$Id$ */
/*
 * Copyright (c) 1990-1996 Sam Leffler
 * Copyright (c) 1991-1996 Silicon Graphics, Inc.
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
#include "Sys.h"
#include "config.h"

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

    static fxStr dbName;

    void addDestination(const char* cp);
    void copyToTemporary(int fin, fxStr& tmpl);
    void fatal(const char* fmt ...);
    void usage();
public:
    sendFaxApp();
    ~sendFaxApp();

    bool run(int argc, char** argv);
};

fxStr sendFaxApp::dbName("~/.faxdb");

sendFaxApp::sendFaxApp()
{
    db = NULL;
}

sendFaxApp::~sendFaxApp()
{
    if (stdinTemp != "") Sys::unlink(stdinTemp);
    delete db;
}

bool
sendFaxApp::run(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    resetConfig();
    readConfig(FAX_SYSCONF);
    readConfig(FAX_LIBDATA "/sendfax.conf");
    readConfig(FAX_USERCONF);

    bool waitForJob = false;
    int verbose = 0;
    SendFaxJob& proto = getProtoJob();
    db = new FaxDB(tildeExpand(dbName));
    while ((c = Sys::getopt(argc, argv, "a:b:B:c:C:d:f:F:h:i:I:k:M:P:r:s:t:T:V:x:y:12lmnpvwDENR")) != -1)
    switch (c) {
    case '1':			// restrict to 1D-encoded data
        proto.setDesiredDF(0);
        break;
    case '2':			// restrict to 2D-encoded data
        proto.setDesiredDF(1);
        break;
    case 'a':			// time at which to transmit job
        proto.setSendTime(optarg);
        break;
    case 'b':			// minimum transfer speed
        proto.setMinSpeed(optarg);
        break;
    case 'B':			// desired transmit speed
        proto.setDesiredSpeed(optarg);
        break;
    case 'C':			// cover page: template file
        proto.setCoverTemplate(optarg);
        break;
    case 'c':			// cover page: comment field
        proto.setCoverComments(optarg);
        break;
    case 'D':			// notify when done
        proto.setNotification("when done");
        break;
    case 'd':			// destination name and number
        addDestination(optarg);
        break;
    case 'E':			// disable use of ECM
        proto.setDesiredEC(false);
        break;
    case 'F':			// override tag line format string
        proto.setTagLineFormat(optarg);
        break;
    case 'f':			// sender's identity
        setFromIdentity(optarg);
        break;
    case 'h':			// server's host
        setHost(optarg);
        break;
    case 'I':			// fixed retry time
        proto.setRetryTime(optarg);
        break;
    case 'i':			// user-specified job identifier
        proto.setJobTag(optarg);
        break;
    case 'k':			// time to kill job
        proto.setKillTime(optarg);
        break;
    case 'l':			// low resolution
        proto.setVResolution(98.);
        break;
    case 'M':			// desired min-scanline time
        proto.setDesiredMST(optarg);
        break;
    case 'm':			// medium resolution
        proto.setVResolution(196.);
        break;
    case 'n':			// no cover sheet
        proto.setAutoCoverPage(false);
        break;
    case 'N':			// no notification
        proto.setNotification("none");
        break;
    case 'p':			// submit polling request
        addPollRequest();
        break;
    case 'P':			// set scheduling priority
        proto.setPriority(optarg);
        break;
    case 'r':			// cover sheet: regarding field
        proto.setCoverRegarding(optarg);
        break;
    case 'R':			// notify when requeued or done
        proto.setNotification("when requeued");
        break;
    case 's':			// set page size
        proto.setPageSize(optarg);
        break;
    case 't':			// times to retry sending
        proto.setMaxRetries(atoi(optarg));
        break;
    case 'T':			// times to dial telephone
        proto.setMaxDials(atoi(optarg));
        break;
    case 'v':			// verbose mode
        verbose++;
        setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
        SendFaxClient::setVerbose(true);	// type rules & basic operation
        FaxClient::setVerbose(verbose > 1);	// protocol tracing
        break;
    case 'V':			// cover sheet: voice number field
        proto.setCoverVoiceNumber(optarg);
        break;
    case 'w':			// wait for job to complete
        waitForJob = true;
        break;
    case 'x':			// cover page: to's company
        proto.setCoverCompany(optarg);
        break;
    case 'y':			// cover page: to's location
        proto.setCoverLocation(optarg);
        break;
    case '?':
        usage();
        /*NOTREACHED*/
    }
    if (getNumberOfJobs() == 0) {
        fprintf(stderr, "%s: No destination specified.\n",
            (const char*) appName);
        usage();
    }
    if (optind < argc) {
        for (; optind < argc; optind++) {
            addFile(argv[optind]);
        }
    } else if (getNumberOfPollRequests() == 0) {
        copyToTemporary(fileno(stdin), stdinTemp);
        addFile(stdinTemp);
    }
    bool status = false;
    fxStr emsg;
    if (callServer(emsg)) {
        status = login(NULL, emsg)
            && prepareForJobSubmissions(emsg)
            && submitJobs(emsg);
        if (status && waitForJob) {
            if (getNumberOfJobs() > 1) {
                printWarning("can only wait for one job (right now),"
                    " waiting for job %s.", (const char*) getCurrentJob());
            }
            jobWait(getCurrentJob());
        }
        hangupServer();
    }
    if (!status) printError(emsg);
    return (status);
}

void
sendFaxApp::usage()
{
    fxFatal("usage: %s [options] [files]\n"
        "(Read the manual page; it's too complicated)", (const char*) appName);
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
    } else {
        recipient = "";
    }
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
    if (dest.length() == 0) {
        fatal("Null destination for \"%s\"", cp);
    }
    SendFaxJob& job = addJob();
    job.setDialString(dest);
    job.setCoverName(recipient);
}

/*
 * Copy data from fin to a temporary file.
 */
void
sendFaxApp::copyToTemporary(int fin, fxStr& tmpl)
{
    char buff[128];
    sprintf(buff, "%s/sndfaxXXXXXX", _PATH_TMP);
    int fd = Sys::mkstemp(buff);
    tmpl = buff;
    if (fd < 0) {
        fatal("%s: Can not create temporary file", (const char*) tmpl);
    }
    int cc, total = 0;
    char buf[16*1024];
    while ((cc = Sys::read(fin, buf, sizeof (buf))) > 0) {
        if (Sys::write(fd, buf, cc) != cc) {
            Sys::unlink(tmpl);
            fatal("%s: write error", (const char*) tmpl);
        }
        total += cc;
    }
    Sys::close(fd);
    if (total == 0) {
        Sys::unlink(tmpl);
        tmpl = "";
        fatal("No input data; tranmission aborted");
    }
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
    signal(SIGCHLD, fxSIGHANDLER(SIG_DFL));     // by YC
    app = new sendFaxApp;
    if (!app->run(argc, argv)) sigDone(0);
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
    fprintf(stderr, "%s: ", (const char*) appName);
    va_list ap;
    va_start(ap, fmt);
    vfatal(stderr, fmt, ap);
    /*NOTTEACHED*/
}
#undef fmt
