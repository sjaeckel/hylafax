#ident $Header: /usr/people/sam/flexkit/fax/faxd/RCS/faxServerApp.c++,v 1.27 91/06/04 19:54:48 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "FaxServer.h"
#include "FaxRequest.h"
#include "FaxRecvInfo.h"
#include "faxServerApp.h"
#include "FIFOServer.h"
#include "config.h"

#include <getopt.h>
extern "C" {
#include <syslog.h>
#include <dirent.h>
int setreuid(int, int);
}
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <osfcn.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <pwd.h>

#include "flock.h"			// XXX

const fxStr faxServerApp::fifoName	= FAX_FIFO;
const fxStr faxServerApp::configName	= FAX_CONFIG;
const fxStr faxServerApp::sendDir	= FAX_SENDDIR;
const fxStr faxServerApp::recvDir	= FAX_RECVDIR;
const fxStr faxServerApp::mailCmd	= FAX_MAILCMD;
const fxStr faxServerApp::notifyCmd	= FAX_NOTIFYCMD;
const fxStr faxServerApp::ps2faxCmd	= FAX_PS2FAX;

static void s0(faxServerApp* o)			{ o->close(); }
static void s1(faxServerApp* o)			{ o->scanQueue(); }
static void s2(faxServerApp* o, char* cp)	{ o->notifySendComplete(cp); }
static void s3(faxServerApp* o, FaxRequest* r)	{ o->notifyJobComplete(r); }
static void s4(faxServerApp* o, FaxRecvInfo r)	{ o->notifyJobRecvd(r); }
static void s5(faxServerApp* o, char* cp)	{ o->logInfo(cp); }
static void s6(faxServerApp* o, char* cp)	{ o->fifoMessage(cp); }

static void sigAlarm();

fxAPPINIT(faxServerApp, 0);

faxServerApp::faxServerApp() :
    device("/dev/ttym2"),
    queueDir(FAX_SPOOLDIR)
{
    addInput("close",		fxDT_void,	this, (fxStubFunc) s0);
    addInput("::scanQueue",	fxDT_void,	this, (fxStubFunc) s1);
    addInput("::sendComplete",	fxDT_CharPtr,	this, (fxStubFunc) s2);
    addInput("::jobComplete",	fxDT_FaxRequest,this, (fxStubFunc) s3);
    addInput("::jobRecvd",	fxDT_FaxRecvd,	this, (fxStubFunc) s4);
    addInput("::traceStatus",	fxDT_CharPtr,	this, (fxStubFunc) s5);
    addInput("::fifoMessage",	fxDT_CharPtr,	this, (fxStubFunc) s6);

    sendChannel = addOutput("sendFax", fxDT_FaxRequest);

    scanChannel = addOutput("scanQueue", fxDT_void);
    connect("scanQueue", this, "::scanQueue");

    okToUse2D = TRUE;
    queue = 0;
    fifo = 0;
    devfifo = 0;
    currentTimeout = 0;
    requeueInterval = FAX_REQUEUE;
    server = 0;
    openlog("FaxServer", LOG_PID, LOG_DAEMON);
}

faxServerApp::~faxServerApp()
{
    if (server) delete server;
    if (fifo) delete fifo;
    if (devfifo) delete devfifo;
    closelog();
}

const char* faxServerApp::className() const { return ("faxServerApp"); }

void
faxServerApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    // We not only want faxd to run effective uid == fax pseudo-user,
    // we also want it to fork "at" to run as fax pseudo-user.
    // Therefore, the real uid must be set to the effective uid.
    int uid = geteuid();
    if (uid == 0) {
	struct passwd* pwd = getpwnam(FAX_USER);
	if (!pwd)
	    fxFatal("No fax user \"%s\" defined on your system!\n"
		"This software is not installed properly!");
	(void) setreuid(pwd->pw_uid, pwd->pw_uid);
	(void) setgid(pwd->pw_gid);
    } else {
	struct passwd* pwd = getpwuid(uid);
	if (!pwd)
	    fxFatal("Can not figure out the identity of uid %d", uid);
	if (strcmp(pwd->pw_name, FAX_USER) != 0)
	    fxFatal("The fax server must run as the fax user \"%s\".\n"
		"Corect the ownership and/or mode of the server.");
	(void) setreuid(uid, -1);
    }

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    while ((c = getopt(argc, argv, "m:i:q:12")) != -1)
	switch (c) {
	case 'm':
	    device = optarg;
	    break;
	case 'i':
	    requeueInterval = atoi(optarg);
	    break;
	case 'q':
	    queueDir = optarg;
	    break;
	case '1':
	    okToUse2D = FALSE;
	    break;
	case '2':
	    okToUse2D = TRUE;
	    break;
	case '?':
	    fxFatal("usage: %s"
		" [-12]"
		" [-m modem-device]"
		" [-q queue-directory]"
		" [-i requeue-interval]",
		(char*) appName
		);
	}
    server = new FaxServer(device);
    assert(server);
    connect("sendFax", server, "sendFax");
    server->connect("jobComplete",	this, "::jobComplete");
    server->connect("sendComplete",	this, "::sendComplete");
    server->connect("jobRecvd",		this, "::jobRecvd");
    server->connect("trace",		this, "::traceStatus");

    server->setOkToReceive2D(okToUse2D);

    if (::chdir(queueDir) < 0)
	fxFatal("%s: Can not change directory", (char*) queueDir);

    /*
     * Configure server from configuration file.
     */
    l = device.length();
    const fxStr& dev = device.tokenR(l, '/');
    server->restoreState(configName | "." | dev);

    fifo = new FIFOServer(fifoName, 0600, TRUE);
    fifo->connect("message", this, "::fifoMessage");
    devfifo = new FIFOServer(fifoName | "." | dev, 0600, TRUE);
    devfifo->connect("message", this, "::fifoMessage");

    server->initialize(argc, argv);
}

void
faxServerApp::open()
{
    fxApplication::open();
    logInfo("OPEN \"%s\"", (char*) device);
    server->open();
    if (server->openSucceeded()) {
	scanQueueDirectory();
	scanQueue();
    } else
	fxFatal("ModemType in %s is missing or invalid.", (char*) configName);
}

void
faxServerApp::close()
{
    logInfo("CLOSE \"%s\"", (char*) device);
    server->close();
    fxApplication::close();
}

/*
 * Scan the spool directory for new queue files and
 * enter them in the queue of outgoing jobs.  We do
 * a flock on the queue directory while scanning to
 * avoid conflict between servers (and clients).
 * The queue is handled according to a first-come-first
 * served basis (i.e. using the file's mod-time).
 */
void
faxServerApp::scanQueueDirectory()
{
    DIR* dir = opendir(sendDir);
    if (dir) {
	fxStr prefix(sendDir | "/");
	(void) flock(dir->dd_fd, LOCK_SH);
	Job** last = &queue;
	for (dirent* dp = readdir(dir); dp; dp = readdir(dir)) {
	    if (dp->d_name[0] != 'q')
		continue;
	    struct stat sb;
	    fxStr file(prefix | dp->d_name);
	    if (stat(file, &sb) >= 0 && (sb.st_mode&S_IFMT) == S_IFREG) {
		Job* job = new Job(file, 0);
		job->next = 0;
		*last = job;
		last = &job->next;
	    }
	}
	(void) flock(dir->dd_fd, LOCK_UN);
	closedir(dir);
    } else
	logError("Could not scan queue directory");
    // XXX could do some intelligent queue processing
    // (e.g. coalesce jobs to the same destination)
}

/*
 * Process the next job in the queue.  The queue file
 * is locked to avoid conflict with other servers that
 * are processing the same spool area.  The queue file
 * associated with the job is read into an internal
 * format that's sent off to the fax server.  When the
 * fax server is done with the job (either sending the
 * files or failing), the transmitted request is returned,
 * triggering a call to notifyJobComplete.  Thus if
 * we fail to handle the entry at the front of the queue
 * we just simulate a server response by invoking the
 * method directly.
 */
void
faxServerApp::processJob(Job* job)
{
    logInfo("JOB \"%s\"", (char*) job->file);
    int fd = ::open(job->file, O_RDWR);
    if (fd > 0) {
	if (flock(fd, LOCK_EX|LOCK_NB) >= 0) {
	    request = readQFile(job->file, fd);
	    if (request) {
		time_t now = time(0);
		// check if job has expired
		struct stat sb;
		(void) fstat(fileno(request->fp), &sb);
		long tts = request->tts - now;
		if (tts > 0) {		// not time to transmit
		    if (server->getTracing() & FAXTRACE_SERVER)
			logInfo("SEND NOT READY: \"%s\" in %d seconds",
			    (char*) request->qfile, tts);
		    notifyJobComplete(request);
		} else
		    sendJob(request);
		return;
	    } else
		logError("Could not read q file for \"%s\"",
		    (char*) job->file);
	} else {
	    if (errno == EWOULDBLOCK) {
		logError("Job file \"%s\" already locked", (char*) job->file);
		insertJob(new Job(job));
	    } else
		logError("Could not lock job file \"%s\" (errno %d): %m",
		    (char*) job->file, errno);
	}
	::close(fd);
    } else
	logError("Could not open job file \"%s\" (errno %d)",
	    (char*) job->file, errno);
    notifyJobComplete();
}

/*
 * Process a ``send job''.
 *
 * Check the remote machine registry for the destination
 * machine's capabilities and use that info to convert
 * and preformat documents.  In particular, we use this
 * info to decide on the page size, whether or not high
 * res is permissible, and whether or not 2d encoding
 * facsimile can be sent.
 */
void
faxServerApp::sendJob(FaxRequest* request)
{
    FaxMachineInfo info;
    info.restore(server->canonicalizePhoneNumber(request->number));
    float res = request->resolution;
    u_int w = request->pagewidth;
    float l = request->pagelength;
    for (u_int i = 0, n = request->files.length(); i < n; i++) {
	const fxStr& file = request->files[i];
	if (request->formats[i]) {
	    // convert PostScript to facsimile
	    fxStr temp(file | "+");
	    JobStatus status = convertPostScript(file, temp, res, w, l, info);
	    if (status != Job::done) {
		// bail out
		(void) unlink(temp);
		deleteRequest(status, request, TRUE);
		notifyJobComplete();
		return;
	    }
	    (void) unlink(file);
	    request->files[i] = temp;
	    request->formats[i] = FALSE;
	} else {
	    // XXX verify format against machine capabilities
	}
    }
    // everything's converted and compatible; start job
    logInfo("SEND BEGIN: \"%s\" TO \"%s\" FROM \"%s\"",
	(char*) request->qfile,
	(char*) request->number,
	(char*) request->sender);
    jobStart = fileStart = time(0);
    sendData(sendChannel, new FaxRequestData(request));
}

/*
 * Invoke the PostScript interpreter to image
 * the document according to the capabilities
 * of the remote fax machine.
 */
JobStatus
faxServerApp::convertPostScript(const fxStr& inFile, const fxStr& outFile,
    float resolution, u_int pageWidth, float pageLength,
    const FaxMachineInfo& info)
{
    if (resolution > 150 && !info.supportsHighRes)
	resolution = 98;
    if (pageWidth > info.maxPageWidth)
	pageWidth = info.maxPageWidth;
    if (info.maxPageLength != -1 && pageLength > info.maxPageLength)
	pageLength = info.maxPageLength;
    if (pageLength == 297)	// for ISO A4 use guaranteed reproducible area
	pageLength = 280;
    // ps2fax:
    //   -o file		output (temp) file
    //   -r <res>		output resolution (dpi)
    //   -w <pagewidth>		output page width (pixels)
    //   -l <pagelength>	output page length (mm)
    //   [-1|-2]		1d or 2d encoding
    char buf[1024];
    int encoding = 1 + (okToUse2D && info.supports2DEncoding);
    sprintf(buf, " -r %g -w %d -l %g -%d ",
	resolution, pageWidth, pageLength, encoding);
    fxStr cmd(ps2faxCmd | " -o " | outFile | buf | inFile);
    logInfo("CONVERT POSTSCRIPT: \"%s\"", (char*) cmd);
    JobStatus status;
    request->notice.resize(0);
    FILE* fp = popen(cmd, "r");
    if (fp) {
	/*
	 * Collect the output from the interpreter
	 * in case there is an error -- this is sent
	 * back to the user that submitted the job.
	 */
	int n;
	while ((n = fread(buf, 1, sizeof (buf), fp)) > 0)
	    request->notice.append(buf, n);
	int exitstat = pclose(fp);
	if (exitstat != 0) {
	    status = Job::format_failed;
	    logError("Conversion \"%s\" failed (exit %d)",
		(char*) cmd, exitstat);
	} else
	    status = Job::done;
    } else {
	logError("Conversion \"%s\" failed (popen)", (char*) cmd);
	status = Job::no_formatter;
    }
    return (status);
}

/*
 * Insert a job in the queue according to
 * the time-to-send criteria.  The queue
 * is maintained sorted with the tts values
 * delta-differenced.
 */
void
faxServerApp::insertJob(Job* job)
{
    Job* jp;
    Job** jpp = &queue;
    for (; (jp = *jpp) && jp->tts < job->tts; jpp = &jp->next)
	if (jp->tts > 0)
	    job->tts -= jp->tts;
    job->next = jp;
    *jpp = job;
    if (jp)
	jp->tts -= job->tts;
}

/*
 * Remove a job from the queue and update the time-to-send
 * values of any remaining jobs.  Also, if necessary, adjust
 * the current timeout.
 */
Job*
faxServerApp::removeJob(const fxStr& name)
{
    // XXX block alarm while manipulating queue
    Job* jp;
    for (Job** jpp = &queue; (jp = *jpp) && jp->file != name; jpp = &jp->next)
	;
    if (jp) {
	if (*jpp = jp->next) {
	    jp->next->tts += jp->tts;
	    if (jpp == &queue && currentTimeout != 0) {
		// adjust timeout
		unsigned timeRemaining = alarm(0);
		queue->tts -= currentTimeout - timeRemaining;
		assert(queue->tts > 0);	// should always be later
		signal(SIGALRM, (sig_type) sigAlarm);
		alarm(currentTimeout = (u_int) queue->tts);
	    }
	}
    }
    return (jp);
}

/*
 * Delete a queued job.
 */
void
faxServerApp::deleteJob(const fxStr& name)
{
    Job* jp = removeJob(name);
    if (jp)
	delete jp;
}

/*
 * Alter job parameters.
 */
void
faxServerApp::alterJob(const char* s)
{
    const char cmd = *s++;
    const char* cp = strchr(s, ' ');
    if (!cp) {
	logError("Malformed JOB request \"%s\"", s);
	return;
    }
    fxStr name(s, cp-s);
    // XXX block alarm while manipulating queue
    Job* jp = removeJob(name);
    if (!jp) {
	logError("JOB \"%s\" not found on the queue.", (char*) name);
	return;
    }
    while (isspace(*cp))
	cp++;
    switch (cmd) {
    case 'T':			// time-to-send
	jp->tts = atoi(cp);
	break;
    case 'P':			// change priority
	jp->pri = atoi(cp);
	break;
    default:
	logError("Invalid JOB request command \"%c\" ignored.", cmd);
	break;
    }
    insertJob(jp);		// place back on queue
    scanQueue();		// and rescan queue
}

/*
 * Rescan the spooling area after a timeout.
 */
void
faxServerApp::scanQueue()
{
    if (queue) {
	Job* job = queue;
	if (job->tts <= 0) {
	    /*
	     * The job at the head of the queue should
	     * be processed.  Remove it from the queue
	     * and initiate the work.  If the job must
	     * be requeued, a new ``job'' will be created
	     * (could be optimized).
	     */
	    queue = job->next;
	    processJob(job);
	    delete job;
	} else {
	    /*
	     * The next job to be processed is some time
	     * in the future.  Start/restart the timer.
	     */
	    unsigned timeRemaining = alarm(0);
	    if (timeRemaining == 0 ||
	      (job->tts -= currentTimeout - timeRemaining) > 0) {
		currentTimeout = (u_int) job->tts;
		signal(SIGALRM, (sig_type) sigAlarm);
		alarm(currentTimeout);
	    }
	}
    }
}

/*
 * Handle the completion of processing of a fax request.
 * We free up any resources associated with the request
 * and then process the next entry in the queue or restart
 * the scan timer.
 */
void
faxServerApp::notifyJobComplete(FaxRequest* req)
{
    if (req) {
	time_t now = time(0);
	float dt = (now - jobStart) / 60.;
	record("SEND", req->mailaddr, req->number, req->npages, dt);
	if (!req->status) {		// send failed
	    long tts = req->tts - now;
	    if (tts <= 0) {
		/*
		 * Send failed, bump it's ``time to send''
		 * and rewrite the queue file.  This causes
		 * the job to be rescheduled for tranmission
		 * at a future time.
		 */
		tts = requeueInterval;
		req->tts = now + tts;
		req->writeQFile();
		logInfo("REQUEUE \"%s\", reason \"%s\"",
		    (char*) req->qfile, (char*) req->notice);
		if (req->notify == FaxRequest::when_requeued)
		    notifySender(Job::requeued, req); 
	    }
	    insertJob(new Job(req->qfile, tts));
	} else {
	    logInfo("SEND DONE: %g minutes", dt);
	    if (req->notify != FaxRequest::no_notice)
		notifySender(Job::done, req);
	    if (req->killjob.length() > 0) {	// remove at job
		system(FAX_ATRM | req->killjob | ">/dev/null 2>&1");
		logInfo("REMOVE at job \"%s\"\n", (char*) req->killjob);
	    }
	    for (u_int i = 0; i < req->files.length(); i++)
		(void) unlink(req->files[i]);
	    (void) unlink(req->qfile);
	}
	delete req;				// implicit unlock of q file
    }
    scanQueue();
}

/*
 * Notify the sender of the facsimile that
 * something has happened -- either the job
 * has completed, or it's been requeued for
 * later transmission.
 */
void
faxServerApp::notifySender(JobStatus why, FaxRequest* req)
{
    logInfo("NOTIFY \"%s\"\n", (char*) req->mailaddr);
    FILE* fp = popen(mailCmd | " " | req->mailaddr, "w");
    if (fp) {
	fprintf(fp, "Your facsimile job \"%s\", to %s, ",
	    (char*) req->qfile, (char*) req->number);
	switch (why) {
	case Job::no_status:
	    break;
	case Job::done:
	    fprintf(fp, "was sent successfully.\n");
	    fprintf(fp, "Total transmission time was %g minutes.\n",
		(time(0) - jobStart) / 60.);
	    break;
	case Job::requeued:
	    fprintf(fp, "was not sent because:\n");
	    fprintf(fp, "    %s\n", (char*) req->notice);
	    { char buf[30];
	      strftime(buf, sizeof (buf), "%R", localtime(&req->tts));
	      fprintf(fp, "The job will be retransmitted at %s.\n", buf);
	    }
	    break;
	case Job::removed:
	    fprintf(fp, "was deleted from the queue.\n");
	    break;
	case Job::timedout:
	    fprintf(fp, "could not be sent after repeated attempts.\n");
	    break;
	case Job::format_failed:
	case Job::no_formatter:
	    fprintf(fp, "was not sent because\n"
	        "conversion of PostScript to facsimile failed.\n");
	    if (why == Job::format_failed) {
		fprintf(fp, "The output from \"%s\" was:\n\n%.*s\n",
		    (char*) ps2faxCmd,
		    request->notice.length(), (char*) request->notice);
		fprintf(fp, "Check your job for non-standard fonts\n"
		    "and invalid PostScript constructs.\n");
	    } else
		fprintf(fp, "The conversion program (\"%s\") was not found.\n",
		    (char*) ps2faxCmd);
	    break;
	}
	(void) pclose(fp);
    } else
	logError("popen for NOTIFY \"%s\" failed: %m", (char*) mailCmd);
}

void
faxServerApp::fifoMessage(const char* cp)
{
    switch (cp[0]) {
    case 'A':				// answer the phone
	logInfo("ANSWER");
	server->recvFax(TRUE);
	break;
    case 'M':				// modem control
	logInfo("MODEM \"%s\"", cp+1);
	server->restoreStateItem(cp+1);
	break;
    case 'J':				// alter job parameter(s)
	logInfo("JOB PARAMS \"%s\"", cp+1);
	alterJob(cp+1);
	break;
    case 'Q':				// quit
	logInfo("QUIT");
	faxServerApp::close();
	break;
    case 'R':				// remove job
	logInfo("REMOVE \"%s\"", cp+1);
	deleteJob(cp+1);
	// NOTE: actual removal normally done by faxd.recv
	int fd = ::open(cp+1, O_RDWR);
	if (fd > 0) {
	    if (flock(fd, LOCK_EX|LOCK_NB) >= 0) {
		FaxRequest* req = new FaxRequest(cp+1);
		if (req) {
		    (void) req->readQFile(fd);
		    deleteRequest(Job::removed, req);
		}
	    }
	    ::close(fd);
	}
	break;
    case 'S':				// send job
	logInfo("SEND \"%s\"", cp+1);
	insertJob(new Job(cp+1, 0));
	if (queue)
	    scanQueue();
	else
	    logError("SEND \"%s\" failed to insert job", cp+1);
	break;
    default:
	logInfo("bad fifo message \"%s\"", cp);
	break;
    }
}

void
faxServerApp::deleteRequest(JobStatus why, FaxRequest* req, fxBool force)
{
    if (req->notify != FaxRequest::no_notice || force)
	notifySender(why, req);
    if (req->sendjob.length() > 0 && req->tts == 0) {
	system(FAX_ATRM | req->sendjob | ">/dev/null 2>&1");
	logInfo("REMOVE at job \"%s\"\n", (char*) req->sendjob);
    }
    if (req->killjob.length() > 0) {	// remove at job
	system(FAX_ATRM | req->killjob | ">/dev/null 2>&1");
	logInfo("REMOVE at job \"%s\"\n", (char*) req->killjob);
    }
    for (u_int i = 0, n = req->files.length(); i < n; i++)
	(void) unlink(req->files[i]);
    (void) unlink(req->qfile);
    delete req;
}

void
faxServerApp::notifySendComplete(const char* filename)
{
    time_t t = time(0);
    float dt = (t - fileStart) / 60.;
    fileStart = t;
    logInfo("SEND: FROM \"%s\" TO \"%s\" (\"%s\" sent in %4.2g minutes)",
	(char*) request->mailaddr, (char*) request->number, filename, dt);
}

void
faxServerApp::notifyJobRecvd(const FaxRecvInfo& req)
{
    char type[80];
    if (req.pagelength == 297 || req.pagelength == -1)
	strcpy(type, "A4");
    else if (req.pagelength == 364)
	strcpy(type, "B4");
    else
	sprintf(type, "(%d x %g)", req.pagewidth, req.pagelength);
    logInfo("RECV: \"%s\" from \"%s\", %d %s pages, %g dpi, %g minutes",
	(char*) req.qfile, (char*) req.sender,
	req.npages, type, req.resolution, req.time / 60.);
    // hand to delivery/notification command
    system(notifyCmd | " " | req.qfile);	// XXX more info
    record("RECV", req.sender, server->getModemNumber(), req.npages, req.time);
}

void
faxServerApp::timeout()
{
    if (queue) {
	queue->tts -= currentTimeout;
	sendVoid(scanChannel);
    }
}

FaxRequest*
faxServerApp::readQFile(const fxStr& filename, int fd)
{
    FaxRequest* req = new FaxRequest(filename);
    if (!req->readQFile(fd)) {
	delete req;
	return 0;
    } else
	return req;
}

extern "C" {
    void syslog(int, const char* ...);
}

void
faxServerApp::logInfo(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    vlogInfo(fmt, ap);
    va_end(ap);
}
#undef fmt

void
faxServerApp::record(const char* cmd,
    const char* from, const char* to, int npages, float duration)
{
    FILE* flog = fopen(FAX_XFERLOG, "a");
    if (flog != NULL) {
	flock(fileno(flog), LOCK_EX);
	time_t now = time(0);
	char buf[80];
	cftime(buf, "%D %R", &now);
	fprintf(flog, "%s %s \"%s\" \"%s\" %d %g\n",
	    buf, cmd, from, to, npages, duration);
	fclose(flog);
    } else
	logError("Can not open log file \"%s\"", FAX_XFERLOG);
}

void
faxServerApp::vlogInfo(const char* fmt, va_list ap)
{
    char buf[4096];
    vsprintf(buf, fmt, ap);
    syslog(LOG_INFO, buf);
}

void
faxServerApp::logError(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    vlogError(fmt, ap);
    va_end(ap);
}
#undef fmt

void
faxServerApp::vlogError(const char* fmt, va_list ap)
{
    char buf[4096];
    vsprintf(buf, fmt, ap);
    syslog(LOG_ERR, buf);
}

void
fxFatal(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    char buf[4096];
    vsprintf(buf, fmt, ap);
    va_end(ap);
    syslog(LOG_ERR, buf);
    exit(-1);
}
#undef fmt

static void
sigAlarm()
{
    faxServerAppAppInstance->timeout();
}
