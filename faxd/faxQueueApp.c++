/*	$Header: /usr/people/sam/fax/./faxd/RCS/faxQueueApp.c++,v 1.66 1995/04/08 21:33:34 sam Rel $ */
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
#include "Sys.h"

#include <ctype.h>
#include <osfcn.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <sys/file.h>
#include <tiffio.h>

#include "Dispatcher.h"

#include "FaxMachineInfo.h"
#include "FaxAcctInfo.h"
#include "FaxRequest.h"
#include "FaxTrace.h"
#include "Timeout.h"
#include "UUCPLock.h"
#include "DialRules.h"
#include "Modem.h"
#include "faxQueueApp.h"
#include "config.h"

/*
 * HylaFAX Spooling and Command Agent.
 */

const fxStr faxQueueApp::fifoName	= FAX_FIFO;
const fxStr faxQueueApp::sendDir	= FAX_SENDDIR;
const fxStr faxQueueApp::docDir		= FAX_DOCDIR;

extern	const char* fmtTime(time_t);
fxStr strTime(time_t t)	{ return fxStr(fmtTime(t)); }

#define	JOBHASH(pri)	(((pri) >> 4) & (NQHASH-1))

faxQueueApp::SchedTimeout::SchedTimeout() { started = FALSE; }
faxQueueApp::SchedTimeout::~SchedTimeout() {}

void
faxQueueApp::SchedTimeout::timerExpired(long, long)
{
    started = FALSE;
    faxQueueApp::instance().runScheduler();
}

void
faxQueueApp::SchedTimeout::start()
{
    if (!started) {
	Dispatcher::instance().startTimer(0,1, this);
	started = TRUE;
    }
}

faxQueueApp* faxQueueApp::_instance = NULL;

faxQueueApp::faxQueueApp()
    : configFile(FAX_CONFIG)
{
    fifo = -1;
    quit = FALSE;
    dialRules = NULL;
    setupConfig();

    fxAssert(_instance == NULL, "Cannot create multiple fxServerApp instances");
    _instance = this;
}

faxQueueApp::~faxQueueApp()
{
}

faxQueueApp& faxQueueApp::instance() { return *_instance; }

#include "version.h"

void
faxQueueApp::initialize(int argc, char** argv)
{
    TIFFSetErrorHandler(NULL);
    TIFFSetWarningHandler(NULL);
    updateConfig(configFile);		// read config file
    faxApp::initialize(argc, argv);

    logInfo("%s", VERSION);
    logInfo("%s", "Copyright (c) 1990-1995 Sam Leffler");
    logInfo("%s", "Copyright (c) 1991-1995 Silicon Graphics, Inc.");

    modemsOnCmdLine(argc, argv);
    scanForModems();
}

void
faxQueueApp::open()
{
    faxApp::open();
    scanQueueDirectory();
    Modem::broadcast("HELLO");		// announce queuer presence
    pokeScheduler();
}

static fxStr
cvtDev(const fxStr& id)
{
    fxStr devID(id);
    fxStr prefix(DEV_PREFIX);
    u_int l = prefix.length();
    if (devID.length() > l && devID.head(l) == prefix) {
	devID.remove(0, l);
	while ((l = devID.next(0, '/')) < devID.length())
	    devID[l] = '_';
    }
    return (devID);
}

void
faxQueueApp::modemsOnCmdLine(int argc, char**argv)
{
    for (GetoptIter iter(argc, argv, getOpts()); iter.notDone(); iter++)
	switch (iter.option()) {
	case 'm':
	    /*
	     * Beware of people supplying the device name
	     * instead of the device identifier; convert
	     * to a device id if it looks like a device name.
	     */
	    Modem::getModemByID(
		cvtDev(iter.optArg())).setCapabilities("Pffffffff");
	    break;
	}
}

/*
 * Scan the spool area for modems.  We can't be certain the
 * modems are actively working without probing them; this
 * is done simply to buildup the internal database and later
 * individual modems are enabled for use either based on
 * messages received through the FIFO or based on command
 * line options.
 */
void
faxQueueApp::scanForModems()
{
    DIR* dir = Sys::opendir(".");
    if (dir == NULL) {
	logError("Could not scan directory for modems");
	return;
    }
    fxStr fifoMatch(fifoName | ".");
    for (dirent* dp = readdir(dir); dp; dp = readdir(dir)) {
	if (dp->d_name[0] != fifoName[0])
	    continue;
	if (!strneq(dp->d_name, fifoMatch, fifoMatch.length()))
	    continue;
	if (Sys::isFIFOFile(dp->d_name)) {
	    fxStr devid(dp->d_name);
	    devid.remove(0, fifoMatch.length()-1);	// NB: leave "."
	    if (Sys::isRegularFile(FAX_CONFIG | devid)) {
		devid.remove(0);			// strip "."
		(void) Modem::getModemByID(devid);	// adds to list
	    }
	}
    }
    ::closedir(dir);
}

/*
 * Scan the spool directory for queue files and
 * enter them in the queues of outgoing jobs.
 */
void
faxQueueApp::scanQueueDirectory()
{
    DIR* dir = Sys::opendir(sendDir);
    if (dir == NULL) {
	logError("Could not scan queue directory");
	return;
    }
    for (dirent* dp = readdir(dir); dp; dp = readdir(dir)) {
	if (dp->d_name[0] == 'q')
	    submitJob(sendDir | "/" | dp->d_name);
    }
    ::closedir(dir);
}

#define	isOKToStartJobs(di, dci, n) \
    (di.getActive()+n <= dci.getMaxConcurrentJobs())

/*
 * Process a job and its associated request specification.
 * We check the remote machine registry for the destination's
 * current call status (rejection notice, the max number of
 * concurrent jobs that may be sent) and use that info to
 * decide if the job should be started, rejected, or put
 * to sleep waiting for resources.
 */
void
faxQueueApp::processJob(Job& job, FaxRequest* req)
{
    DestInfo& di = destJobs[job.dest];
    const DestControlInfo& dci = destCtrls[job.dest];
    /*
     * Constrain the maximum number of times the phone
     * will be dialed and/or the number of attempts that
     * will be made (and reject jobs accordingly).
     */
    if (dci.getMaxDials() < req->maxdials) {
	req->maxdials = dci.getMaxDials();
	if (req->totdials >= req->maxdials) {
	    rejectJob(job, *req,
		fxStr::format("REJECT: Too many attempts to dial (max %u)",
		req->maxdials));
	    deleteRequest(job, req, Job::rejected, TRUE);
	    return;
	}
    }
    if (dci.getMaxTries() < req->maxtries) {
	req->maxtries = dci.getMaxTries();
	if (req->tottries >= req->maxtries) {
	    rejectJob(job, *req,
		fxStr::format("REJECT: Too many attempts to transmit (max %u)",
		req->maxtries));
	    deleteRequest(job, req, Job::rejected, TRUE);
	    return;
	}
    }
    time_t now = Sys::now();
    time_t tts;
    if (dci.getRejectNotice() != "") {
	/*
	 * Calls to this destination are being rejected for
	 * a specified reason that we return to the sender.
	 */
	rejectJob(job, *req, "REJECT: " | dci.getRejectNotice());
	deleteRequest(job, req, Job::rejected, TRUE);
    } else if (!di.isActive(job) && !isOKToStartJobs(di, dci, 1)) {
	/*
	 * This job would exceed the max number of concurrent
	 * jobs that may be sent to this destination.  Put it
	 * on a ``blocked queue'' for the destination; the job
	 * will be made ready to run when one of the existing
	 * jobs terminates.
	 */
	blockJob(job, *req, "Blocked by concurrent jobs");
	di.block(job);				// place at tail of queue
	delete req;
    } else if ((tts = dci.nextTimeToSend(now)) != now) {
	/*
	 * This job may not be started now because of time-of-day
	 * restrictions.  Reschedule it for the next possible time.
	 */
	delayJob(job, *req, "Delayed by time-of-day restrictions", tts);
	delete req;
    } else {
	/*
	 * The job can go.  Prepare it for transmission and
	 * pass it on to the thread that does the actual
	 * transmission work.  The job is marked ``active to
	 * this destination'' prior to preparing it because
	 * preparation may involve asynchronous activities.
	 * The job is placed on the active list so that it
	 * can be located by filename if necessary.
	 */
	di.active(job);
	FaxMachineInfo& info = di.getInfo(job.dest);
	JobStatus status;
	setActive(job);				// place job on active list
	if (!prepareJobNeeded(job, *req, status)) {
	    if (status != Job::done) {
		deleteRequest(job, req, status, TRUE);
		setDead(job);
	    } else
		sendJobStart(job, req, dci);
	} else
	    prepareJobStart(job, req, info, dci);
    }
}

/*
 * Check if the job requires preparation that should
 * done in a fork'd copy of the server.  A sub-fork is
 * used if documents must be converted or a continuation
 * cover page must be crafted (i.e. the work may take
 * a while).
 */
fxBool
faxQueueApp::prepareJobNeeded(Job& job, FaxRequest& req, JobStatus& status)
{
    for (u_int i = 0, n = req.requests.length(); i < n; i++)
	switch (req.requests[i].op) {
	case FaxRequest::send_postscript:	// convert PostScript
	    return (TRUE);
	case FaxRequest::send_tiff:		// verify format
	    return (TRUE);
	case FaxRequest::send_poll:		// verify modem is capable
	    if (!job.modem->supportsPolling()) {
		req.notice = "Modem does not support polling";
		status = Job::rejected;
		jobError(job, "SEND REJECT: " | req.notice);
		return (FALSE);
	    }
	    break;
	}
    status = Job::done;
    return (req.cover != "");			// need continuation cover page
}

/*
 * Handler used by job preparation subprocess
 * to pass signal from parent queuer process.
 * We mark state so job preparation will be aborted
 * at the next safe point in the procedure.
 */
void
faxQueueApp::prepareCleanup(int s)
{
    logError("CAUGHT SIGNAL %d, ABORT JOB PREPARATION", s);
    faxQueueApp::instance().abortPrepare = TRUE;
}

/*
 * Start job preparation in a sub-fork.  The server process
 * forks and sets up a Dispatcher handler to reap the child
 * process.  The exit status from the child is actually the
 * return value from the prepareJob method; this and a
 * reference to the original Job are passed back into the
 * server thread at which point the transmit work is actually
 * initiated.
 */
void
faxQueueApp::prepareJobStart(Job& job, FaxRequest* req,
    FaxMachineInfo& info, const DestControlInfo& dci)
{
    traceQueue(job, "PREPARE START");
    abortPrepare = FALSE;
    pid_t pid = ::fork();
    switch (pid) {
    case 0:				// child, do work
	/*
	 * NB: There is a window here where the subprocess
	 * doing the job preparation can have the old signal
	 * handlers installed when a signal comes in.  This
	 * could be fixed by using the appropriate POSIX calls
	 * to block and unblock signals, but signal usage is
	 * quite tenuous (i.e. what is and is not supported
	 * on a system), so rather than depend on this
	 * functionality being supported, we'll just leave
	 * the (small) window in until it shows itself to
	 * be a problem.
	 */
	::signal(SIGTERM, fxSIGHANDLER(faxQueueApp::prepareCleanup));
	::signal(SIGINT, fxSIGHANDLER(faxQueueApp::prepareCleanup));
	::_exit(prepareJob(job, *req, info, dci));
	/*NOTREACHED*/
    case -1:				// fork failed, sleep and retry
	delayJob(job, *req, "Could not fork to prepare job for transmission",
	    Sys::now() + random() % requeueInterval);
	delete req;
	break;
    default:				// parent, setup handler to wait
	job.startPrepare(pid);
	delete req;			// must reread after preparation
	break;
    }
}

/*
 * Handle notification from the sub-fork that job preparation
 * is done.  The exit status is checked and interpreted as the
 * return value from prepareJob if it was passed via _exit.
 */
void
faxQueueApp::prepareJobDone(Job& job, int status)
{
    traceQueue(job, "PREPARE DONE");
    if (status&0xff) {
	logError("JOB %s: bad exit status %#x from sub-fork",
	    (char*) job.jobid, status);
	status = Job::failed;
    } else
	status >>= 8;
    FaxRequest* req = readRequest(job);
    if (!req) {
	// NB: no way to notify the user (XXX)
	logError("JOB %s: qfile vanished during preparation",
	    (char*) job.jobid);
	setDead(job);
    } else {
	if (job.abortPending) {
	    req->notice = "Job aborted by user";
	    status = Job::failed;
	}
	switch (status) {
	case Job::requeued:		// couldn't fork RIP
	    delayJob(job, *req, "Cannot fork to prepare job for transmission",
		Sys::now() + random() % requeueInterval);
	    delete req;
	    break;
	case Job::done:			// preparation completed successfully
	    sendJobStart(job, req, destCtrls[job.dest]);
	    break;
	default:			// problem preparing job
	    deleteRequest(job, req, status, TRUE);
	    setDead(job);
	    break;
	}
    }
}

/*
 * The server minimizes imaging operations by checking for the
 * existence of compatible previously imaged versions of documents.
 * This is done by selecting a common filename that reflect the
 * remote machine capabilities used for imaging.  This also depends
 * on the naming convention used by faxd.recv when creating document
 * files; each file is named as:
 *
 *	doc<docnum>.<jobid>
 *
 * where <docnum> is a unique document number that reflects a
 * document (potentially) used by multiple senders and <jobid>
 * is the unique identifier assigned to each outbound job.  Then,
 * each imaged document is named:
 *
 *	doc<docnum>;<encoded-capabilities>
 *
 * where <encoded-capabilities> is a string that encodes the
 * remote machine's capabilities.  Before imaging a document
 * we check to see if there is an existing one.  If so then we
 * just link to it.  Otherwise we create the document.  Once
 * the document has been transmitted we don't remove it until
 * the link count of the original source document goes to zero
 * (at which time we search for all possible imaged forms).
 */
static fxStr
mkdoc(const fxStr& file, const Class2Params& params)
{
    fxStr doc(file);
    u_int l = doc.nextR(doc.length(), '.');
    if (::strcmp(&doc[l], "cover"))
	doc.resize(l-1);
    return (doc | ";" | params.encode());
}

#include "class2.h"

/*
 * Prepare a job by converting any user-submitted documents
 * to a format suitable for transmission.
 */
JobStatus
faxQueueApp::prepareJob(Job& job, FaxRequest& req,
    const FaxMachineInfo& info, const DestControlInfo& dci)
{
    /*
     * Select imaging parameters according to requested
     * values, client capabilities, and modem capabilities.
     * Note that by this time we know the modem is capable
     * of certain requirements needed to transmit the document.
     */
    Class2Params params;
    params.setVerticalRes(req.resolution > 150 && !info.getSupportsHighRes() ?
	98 : req.resolution);
    params.setPageWidthInMM(
	fxmin((u_int) req.pagewidth, (u_int) info.getMaxPageWidthInMM()));
    params.setPageLengthInMM(
	fxmin((u_int) req.pagelength, (u_int) info.getMaxPageLengthInMM()));
    /*
     * Generate 2D-encoded facsimile if:
     * o the server is permitted to generate 2D-encoded data,
     * o the modem is capable of sending 2D-encoded data, and
     * o the remote side is known to be capable of it.
     */
    params.df = (use2D && job.modem->supports2D() &&
	info.getCalledBefore() && info.getSupports2DEncoding()) ?
	    DF_2DMR : DF_1DMR;
    /*
     * Check and process the documents to be sent
     * using the parameter selected above.
     */
    JobStatus status = Job::done;
    fxBool updateQFile = FALSE;
    fxStr tmp;		// NB: here to avoid compiler complaint
    u_int i = 0;
    while (i < req.requests.length() && status == Job::done && !abortPrepare) {
	faxRequest& freq = req.requests[i];
	switch (freq.op) {
	case FaxRequest::send_postscript:	// convert PostScript
	    tmp = mkdoc(freq.item, params);
	    status = convertPostScript(job, freq.item, tmp, params, req.notice);
	    if (status == Job::done) {
		/*
		 * Insert converted file into list and mark the
		 * PostScript document so that it's saved, but
		 * not processed again.  The converted file
		 * is sent, while the saved file is kept around
		 * in case it needs to be returned to the sender.
		 */
		freq.op = FaxRequest::send_postscript_saved;
		req.insertFax(i+1, tmp);
	    } else
		Sys::unlink(tmp);		// bail out
	    updateQFile = TRUE;
	    break;
	case FaxRequest::send_tiff:		// verify format
	    status = checkFileFormat(job, freq.item, info, req.notice);
	    if (status == Job::done) {
		freq.op = FaxRequest::send_tiff_saved;
		req.insertFax(i+1, freq.item);
	    }
	    updateQFile = TRUE;
	    break;
	}
	i++;
    }
    if (status == Job::done && !abortPrepare) {
	if (req.cover != "") {
	    /*
	     * Generate a continuation cover page if necessary.
	     * Note that a failure in doing this is not considered
	     * fatal; perhaps this should be configurable?
	     */
	    makeCoverPage(job, req, params);
	    updateQFile = TRUE;
	}
	if (req.pagehandling == "" && !abortPrepare) {
	    /*
	     * Calculate/recalculate the per-page session parameters
	     * and check the page count against the max pages.
	     */
	    if (!preparePageHandling(req, info, dci, req.notice)) {
		status = Job::rejected;		// XXX
		req.notice.insert("Document preparation failed: ");
	    }
	    updateQFile = TRUE;
	}    
    }
    if (updateQFile)
	updateRequest(req, job);
    return (status);
}

/*
 * Prepare the job for transmission by analysing
 * the page characteristics and determining whether
 * or not the page transfer parameters will have
 * to be renegotiated after the page is sent.  This
 * is done before the call is placed because it can
 * be slow and there can be timing problems if this
 * is done during transmission.
 */
fxBool
faxQueueApp::preparePageHandling(FaxRequest& req,
    const FaxMachineInfo& info, const DestControlInfo& dci, fxStr& emsg)
{
    u_int maxPages = dci.getMaxSendPages();
    /*
     * Scan the pages and figure out where session parameters
     * will need to be renegotiated.  Construct a string of
     * indicators to use when doing the actual transmission.
     *
     * NB: all files are coalesced into a single fax document
     *     if possible
     */
    Class2Params params;		// current parameters
    Class2Params next;			// parameters for ``next'' page
    TIFF* tif = NULL;			// current open TIFF image
    req.totpages = 0;
    for (u_int i = 0;;) {
	if (!tif || TIFFLastDirectory(tif)) {
	    /*
	     * Locate the next file to be sent.
	     */
	    if (tif)			// close previous file
		TIFFClose(tif), tif = NULL;
	    if (i >= req.requests.length())
		goto done;
	    i = req.findRequest(FaxRequest::send_fax, i);
	    if (i == fx_invalidArrayIndex)
		goto done;
	    const faxRequest& freq = req.requests[i];
	    tif = TIFFOpen(freq.item, "r");
	    if (tif == NULL) {
		emsg = "Can not open document file";
		goto bad;
	    }
	    if (freq.dirnum != 0 && !TIFFSetDirectory(tif, freq.dirnum)) {
		emsg = "Can not set directory in document file";
		goto bad;
	    }
	    i++;			// advance for next find
	} else {
	    /*
	     * Read the next TIFF directory.
	     */
	    if (!TIFFReadDirectory(tif)) {
		emsg = "Problem reading document directory";
		goto bad;
	    }
	}
	next = params;
	setupParams(tif, next, info);
	if (params.df != (u_int) -1) {
	    /*
	     * The pagehandling string has:
	     * 'M' = EOM, for when parameters must be renegotiated
	     * 'S' = MPS, for when next page uses the same parameters
	     * 'P' = EOP, for the last page to be transmitted
	     */
	    req.pagehandling.append(next == params ? 'S' : 'M');
	}
	/*
	 * Record the session parameters needed by each page
	 * so that we can set the initial session parameters
	 * as needed *before* dialing the telephone.  This is
	 * to cope with Class 2 modems that do not properly
	 * implement the +FDIS command.
	 */
	req.pagehandling.append(next.encode());
	params = next;
	if (++req.totpages > maxPages) {
	    emsg = fxStr::format("Too many pages in submission; max %u",
		maxPages);
	    goto bad;
	}
    }
done:
    req.pagehandling.append('P');		// EOP
    return (TRUE);
bad:
    if (tif)
	TIFFClose(tif);
    return (FALSE);
}

/*
 * Select session parameters according to the info
 * in the TIFF file.  We setup the encoding scheme,
 * page width & length, and vertical-resolution
 * parameters.
 */
void
faxQueueApp::setupParams(TIFF* tif, Class2Params& params, const FaxMachineInfo& info)
{
    uint32 g3opts = 0;
    TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS, &g3opts);
    params.df = (g3opts&GROUP3OPT_2DENCODING ? DF_2DMR : DF_1DMR);

    uint32 w;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    params.setPageWidthInPixels((u_int) w);

    /*
     * Try to deduce the vertical resolution of the image
     * image.  This can be problematical for arbitrary TIFF
     * images 'cuz vendors sometimes don't give the units.
     * We, however, can depend on the info in images that
     * we generate 'cuz we're careful to include valid info.
     */
    float yres;
    if (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)) {
	uint16 resunit;
	TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
	if (resunit == RESUNIT_CENTIMETER)
	    yres *= 25.4;
	params.setVerticalRes((u_int) yres);
    } else {
	/*
	 * No vertical resolution is specified, try
	 * to deduce one from the image length.
	 */
	uint32 l;
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &l);
	// B4 at 98 lpi is ~1400 lines
	params.setVerticalRes(l < 1450 ? 98 : 196);
    }

    /*
     * Select page length according to the image size and
     * vertical resolution.  Note that if the resolution
     * info is bogus, we may select the wrong page size.
     */
    if (info.getMaxPageLengthInMM() != -1) {
	uint32 h;
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
	params.setPageLengthInMM((u_int)(h / yres));
    } else
	params.ln = LN_INF;
}

/*
 * Convert a PostScript document into a form suitable
 * for transmission to the remote fax machine.
 */
JobStatus
faxQueueApp::convertPostScript(Job& job,
    const fxStr& inFile, const fxStr& outFile,
    const Class2Params& params,
    fxStr& emsg)
{
    JobStatus status;
    /*
     * Open/create the target file and lock it to guard against
     * concurrent jobs imaging the same document with the same
     * parameters.  The parent will hold the open file descriptor
     * for the duration of the imaging job.  Concurrent jobs will
     * block on flock and wait for the imaging to be completed.
     * Previously imaged documents will be flock'd immediately
     * and reused without delays.
     *
     * NB: There is a race condition here.  One process may create
     * the file but another may get the shared lock above before
     * the exclusive lock below is captured.  If this happens
     * then the exclusive lock will block temporarily, but the
     * process with the shared lock may attempt to send a document
     * before it's preparation is completed.  We could add a delay
     * before the shared lock but that would slow down the normal
     * case and the window is small--so let's leave it there for now.
     */
    int fd = Sys::open(outFile, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (fd == -1) {
	if (errno == EEXIST) {
	    /*
	     * The file already exist, flock it in case it's
	     * being created (we'll block until the imaging
	     * is completed).  Otherwise, the document imaging
	     * has already been completed and we can just use it.
	     */
	    fd = Sys::open(outFile, O_RDWR);	// NB: RDWR for flock emulations
	    if (fd != -1) {
		if (::flock(fd, LOCK_SH) == -1) {
		    status = Job::format_failed;
		    emsg = "Unable to lock shared document file";
		} else
		    status = Job::done;
		(void) ::close(fd);		// NB: implicit unlock
	    } else {
		/*
		 * This *can* happen if document preparation done
		 * by another job fails (e.g. because of a time
		 * limit or a malformed PostScript submission).
		 */
		status = Job::format_failed;
		emsg = "Unable to open shared document file";
	    }
	} else {
	    status = Job::format_failed;
	    emsg = "Unable to create document file";
	}
	/*
	 * We were unable to open, create, or flock
	 * the file.  This should not happen.
	 */
	if (status != Job::done)
	    jobError(job, "CONVERT POSTSCRIPT: " | emsg | ": %m");
    } else {
	(void) ::flock(fd, LOCK_EX);		// XXX check for errors?
	/*
	 * Imaged document does not exist, run the PostScript interpreter
	 * to generate it.  The interpreter is invoked according to:
	 *   -o file		output (temp) file
	 *   -r <res>		output resolution (dpi)
	 *   -w <pagewidth>	output page width (pixels)
	 *   -l <pagelength>	output page length (mm)
	 *   -1|-2		1d or 2d encoding
	 */
	char rbuf[20]; ::sprintf(rbuf, "%u", params.verticalRes());
	char wbuf[20]; ::sprintf(wbuf, "%u", params.pageWidth());
	char lbuf[20]; ::sprintf(lbuf, "%d", params.pageLength());
	char* argv[30];
	int ac = 0;
	argv[ac++] = ps2faxCmd;
	argv[ac++] = "-o"; argv[ac++] = outFile;
	argv[ac++] = "-r"; argv[ac++] = rbuf;
	argv[ac++] = "-w"; argv[ac++] = wbuf;
	argv[ac++] = "-l"; argv[ac++] = lbuf;
	argv[ac++] = params.df == DF_1DMR ? "-1" : "-2";
	argv[ac++] = inFile;
	argv[ac] = NULL;
	status = runPostScript(job, ps2faxCmd, argv, emsg);
	if (status == Job::done) {
	    /*
	     * Most RIPs exit with zero status even when there are
	     * PostScript problems so scan the the generated TIFF
	     * to verify the integrity of the data.
	     */
	    TIFF* tif = TIFFFdOpen(fd, outFile, "r");
	    if (tif) {
		while (!TIFFLastDirectory(tif))
		    if (!TIFFReadDirectory(tif)) {
			status = Job::format_failed;
			break;
		    }
		TIFFClose(tif);
	    } else
		status = Job::format_failed;
	    if (status == Job::done)	// discard any debugging output
		emsg = "";
	}
	(void) ::close(fd);		// NB: implicit unlock
    }
    return (status);
}

static void
closeAllBut(int fd)
{
    /*
     * NB: _POSIX_OPEN_MAX is the POSIX-approved max descriptor
     *     limit.  We should probably use getdtablesize and
     *     emulate it on those machines where it's not supported.
     */
    for (int f = 0; f < _POSIX_OPEN_MAX; f++)
	if (f != fd)
	    ::close(f);
}

/*
 * Startup the PostScript interpreter program in a subprocess
 * with the output returned through a pipe.  We could just use
 * popen or similar here, but we want to detect fork failure
 * separately from others so that jobs can be requeued instead
 * of rejected.
 */
JobStatus
faxQueueApp::runPostScript(Job& job, const fxStr& cmd, char* const* argv, fxStr& emsg)
{

    fxStr cmdline(argv[0]);
    for (u_int i = 1; argv[i] != NULL; i++)
	cmdline.append(fxStr::format(" %s", argv[i]));
    traceQueue(job, "CONVERT POSTSCRIPT: " | cmdline);
    JobStatus status;
    int pfd[2];
    if (::pipe(pfd) >= 0) {
	pid_t pid = ::fork();
	switch (pid) {
	case -1:			// error
	    jobError(job, "CONVERT POSTSCRIPT: fork: %m");
	    status = Job::requeued;	// job should be retried
	    ::close(pfd[1]);
	    break;
	case 0:				// child, exec command
	    if (pfd[1] != STDOUT_FILENO)
		::dup2(pfd[1], STDOUT_FILENO);
	    closeAllBut(STDOUT_FILENO);
	    ::dup2(STDOUT_FILENO, STDERR_FILENO);
	    Sys::execv(cmd, argv);
	    ::_exit(255);
	    /*NOTREACHED*/
	default:			// parent, read from pipe and wait
	    ::close(pfd[1]);
	    if (runPostScript1(job, pfd[0], emsg)) {
		int estat = -1;
		(void) Sys::waitpid(pid, estat);
		if (estat)
		    jobError(job, "CONVERT POSTSCRIPT: exit status %#d", estat);
		switch (estat) {
		case 0:	  status = Job::done; break;
		case 255: status = Job::no_formatter; break;
		default:  status = Job::format_failed; break;
		}
	    } else {
		::kill(pid, SIGTERM);
		(void) Sys::waitpid(pid);
		status = Job::format_failed;
	    }
	    break;
	}
	::close(pfd[0]);
    } else {
	jobError(job, "CONVERT POSTSCRIPT: pipe: %m");
	status = Job::format_failed;
    }
    return (status);
}

/*
 * Replace unprintable characters with ``?''s.
 */
static void
cleanse(char buf[], int n)
{
    while (--n >= 0)
	if (!isprint(buf[n]) && !isspace(buf[n]))
	    buf[n] = '?';
}

/*
 * Run the interpreter with the configured timeout and
 * collect the output from the interpreter in case there
 * is an error -- this is sent back to the user that
 * submitted the job.
 */
fxBool
faxQueueApp::runPostScript1(Job& job, int fd, fxStr& output)
{
    int n;
    Timeout timer;
    timer.startTimeout(postscriptTimeout*1000);
    char buf[1024];
    while ((n = Sys::read(fd, buf, sizeof (buf))) > 0 && !timer.wasTimeout()) {
	cleanse(buf, n);
	output.append(buf, n);
    }
    if (output.length() > 0 && output[output.length()-1] != '\n')
	output.append('\n');
    timer.stopTimeout();
    if (timer.wasTimeout()) {
	jobError(job, "CONVERT POSTSCRIPT: job time limit exceeded");
	output.append("\n[Job time limit exceeded]\n");
	return (FALSE);
    } else
	return (TRUE);
}

/*
 * Verify the format of a TIFF file against the capabilities
 * of the server's modem and the remote facsimile machine.
 */
JobStatus
faxQueueApp::checkFileFormat(Job& job, const fxStr& file, const FaxMachineInfo& info,
    fxStr& emsg)
{
    JobStatus status;
    TIFF* tif = TIFFOpen(file, "r");
    if (tif) {
	status = Job::done;
	do {
	    if (!checkPageFormat(job, tif, info, emsg))
		status = Job::rejected;
	} while (status == Job::done && TIFFReadDirectory(tif));
	TIFFClose(tif);
    } else {
	struct stat sb;
	if (Sys::stat(file, sb) < 0)
	    emsg = "Can not open document file " | file;
	else
	    emsg = "Document file is not valid TIFF "
		"(check for PostScript conversion problems)";
	status = Job::rejected;
    }
    if (status != Job::done)
	jobError(job, "SEND REJECT: " | emsg);
    return (status);
}

/*
 * Check the format of a page against the capabilities
 * of the modem (or the default capabilities if no modem
 * is currently setup).
 */
fxBool
faxQueueApp::checkPageFormat(Job& job, TIFF* tif, const FaxMachineInfo& info, fxStr& emsg)
{
    uint16 bps;
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bps);
    if (bps != 1) {
	emsg = fxStr::format("Not a bilevel image (bits/sample %u)", bps);
	return (FALSE);
    }
    uint16 spp;
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
    if (spp != 1) {
	emsg = fxStr::format("Multi-sample data (samples %u)", spp);
	return (FALSE);
    }
    uint16 compression = 0;
    (void) TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
    if (compression != COMPRESSION_CCITTFAX3) {
	emsg = "Not in Group 3 format";
	return (FALSE);
    }
    uint32 g3opts = 0;
    (void) TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS, &g3opts);
    if (g3opts & GROUP3OPT_2DENCODING) {
	if (info.getCalledBefore() && !info.getSupports2DEncoding()) {
	    emsg = "Client is incapable of receiving 2DMR-encoded documents";
	    return (FALSE);
	}
	if (!job.modem->supports2D()) {
	    emsg = "Modem is incapable of sending 2DMR-encoded documents";
	    return (FALSE);
	}
    }

    /*
     * Try to deduce the vertical resolution of the image
     * image.  This can be problematical for arbitrary TIFF
     * images because vendors sometimes do not give the units.
     * We, however, can depend on the info in images that
     * we generate because we are careful to include valid info.
     */
    float yres;
    if (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)) {
	short resunit = RESUNIT_NONE;
	(void) TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
	if (resunit == RESUNIT_CENTIMETER)
	    yres *= 25.4f;
    } else {
	/*
	 * No vertical resolution is specified, try
	 * to deduce one from the image length.
	 */
	u_long l;
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &l);
	yres = (l < 1450 ? 98 : 196);		// B4 at 98 lpi is ~1400 lines
    }
    if (yres >= 150 && !info.getSupportsHighRes()) {
	emsg = "Client is incapable of receiving high resolution documents";
	return (FALSE);
    }
    if (!job.modem->supportsVRes(yres)) {
	emsg = "Modem is incapable of sending high resolution documents";
	return (FALSE);
    }

    /*
     * Select page width according to the image width
     * and vertical resolution.
     */
    uint32 w;
    if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w)) {
	emsg = "Malformed document (no image width)";
	return (FALSE);
    }
    if (w > info.getMaxPageWidthInPixels()) {
	emsg = fxStr::format(
	    "Client is incapable of receiving document with page width %lu", w);
	return (FALSE);
    }
    if (!job.modem->supportsPageWidthInPixels((u_int) w)) {
	emsg = fxStr::format(
	    "Modem is incapable of sending document with page width %lu", w);
	return (FALSE);
    }

    /*
     * Select page length according to the image size and
     * vertical resolution.  Note that if the resolution
     * info is bogus, we may select the wrong page size.
     * Note also that we're a bit lenient in places here
     * to take into account sloppy coding practice (e.g.
     * using 200 lpi for high-res facsimile.)
     */
    u_long h = 0;
    if (!TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h)) {
	emsg = "Malformed document (no image length)";
	return (FALSE);
    }
    float len = h / (yres == 0 ? 1. : yres);		// page length in mm
    if (info.getMaxPageLengthInMM() != -1 && len > info.getMaxPageLengthInMM()+30) {
	emsg = fxStr::format("Client is incapable of receiving"
	     " documents with page length %lu", (u_int) len);
	return (FALSE);
    }
    if (!job.modem->supportsPageLengthInMM((u_int) len)) {
	emsg = fxStr::format("Modem is incapable of sending"
	    " documents with page length %lu", (u_int) len);
	return (FALSE);
    }
    return (TRUE);
}

/*
 * Generate a continuation cover page and insert it in
 * the array of files to be sent.  Note that we assume
 * the cover page command generates PostScript which we
 * immediately image, discarding the PostScript.  We
 * could have the cover page command script do this, but
 * then it would need to know how to invoke the PostScript
 * imager per the job characteristics.  Note that we could
 * optimize things here by updating the pagehandling and
 * page counts for the job instead of resetting pagehandling
 * so that everything just gets recalculated from scratch.
 */
void
faxQueueApp::makeCoverPage(Job& job, FaxRequest& req, const Class2Params& params)
{
    fxStr temp(req.cover | "#");
    fxStr cmd(coverCmd
	| " " | req.qfile
	| " " | contCoverPageTemplate
	| " " | temp);
    traceQueue(job, "COVER PAGE: " | cmd);
    if (runCmd(cmd, TRUE)) {
	fxStr emsg;
	if (convertPostScript(job, temp, req.cover, params, emsg)) {
	    req.insertFax(0, req.cover);
	    req.pagehandling = "";		// XXX force recalculation
	} else {
	    jobError(job,
		"SEND: No continuation cover page, PostScript imaging failed");
	}
	Sys::unlink(temp);
    } else {
	jobError(job,
	    "SEND: No continuation cover page, generation cmd failed");
    }
}

static void
doexec(const char* cmd, const char* devid, u_int tl, const char* file)
{
    const char* cp = ::strrchr(cmd, '/');
    if (tl != (u_int) -1) {
	fxStr level = fxStr::format("%u", tl);
	::execl(cmd, cp ? cp+1 : cmd, "-m", devid,
	     "-t", (const char*) level, file, NULL);
    } else {
	::execl(cmd, cp ? cp+1 : cmd, "-m", devid, file, NULL);
    }
}

const fxStr&
faxQueueApp::pickCmd(const FaxRequest& req)
{
    if (req.jobtype == "pager")
	return (sendPageCmd);
    if (req.jobtype == "uucp")
	return (sendUUCPCmd);
    return (sendFaxCmd);			// XXX gotta return something
}

void
faxQueueApp::sendJobStart(Job& job, FaxRequest* req, const DestControlInfo& dci)
{
    job.start = Sys::now();		// start of transmission
    // XXX start deadman timeout on active job
    u_int tl = dci.getTracingLevel();
    const fxStr& cmd = pickCmd(*req);
    pid_t pid = ::fork();
    switch (pid) {
    case 0:				// child, startup command
	closeAllBut(-1);		// NB: close 'em all
	doexec(cmd
	    , job.modem->getDeviceID()
	    , tl
	    , job.file);
	::sleep(3);			// XXX give parent time to catch signal
	::_exit(127);
	/*NOTREACHED*/
    case -1:				// fork failed, sleep and retry
	/*
	 * We were unable to start the command because the
	 * system is out of processes.  Take the job off the
	 * active list and requeue it for a future time. 
	 * If it appears that the we're doing this a lot,
	 * then lengthen the backoff.
	 */
	job.remove();			// off active list
	delayJob(job, *req, "Could not fork to start job transmission",
	    job.start + random() % requeueInterval);
	break;
    default:				// parent, setup handler to wait
	traceQueue(job, "CMD START "
	    | cmd
	    | " -m " | job.modem->getDeviceID()
	    | fxStr::format(tl == (u_int) -1 ? " " : " -t %u ", tl)
	    | job.file
	    | " (PID %lu)"
	    , pid
	);
	job.startSend(pid);
	break;
    }
    delete req;				// discard handle (NB: releases lock)
}

void
faxQueueApp::sendJobDone(Job& job, int status)
{
    time_t now = Sys::now();
    time_t duration = now - job.start;

    traceQueue(job, "CMD DONE: exit status %#x", status);
    releaseModem(job);				// done with modem
    FaxRequest* req = readRequest(job);		// reread the qfile
    if (!req) {
	logError("JOB %s: SEND FINISHED: %s; but job file vanished",
	    (char*) job.jobid, fmtTime(duration));
	setDead(job);
	return;
    }
    if (status&0xff) {
	req->notice = fxStr::format(
	    "Send program terminated abnormally with exit status %#x", status);
	req->status = send_failed;
	logError("JOB " | job.jobid | ": " | req->notice);
    } else if ((status >>= 8) == 127) {
	req->notice = "Send program terminated abnormally; unable to exec " |
	    pickCmd(*req);
	req->status = send_failed;
	logError("JOB " | job.jobid | ": " | req->notice);
    } else
	req->status = (FaxSendStatus) status;
    if (req->status == send_reformat) {
	/*
	 * Job requires reformatting to deal with the discovery
	 * of unexpected remote capabilities (either because
	 * the capabilities changed or because the remote side
	 * has never been called before and the default setup
	 * created a capabilities mismatch).  Purge the job of
	 * any formatted information and reset the state so that
	 * when the job is retried it will be reformatted according
	 * to the updated remote capabilities.
	 */
	u_int i = 0;
	while (i < req->requests.length()) {
	    faxRequest& freq = req->requests[i];
	    switch (freq.op) {
	    case FaxRequest::send_fax:
		req->removeItems(i);
		continue;
	    case FaxRequest::send_tiff_saved:
		freq.op = FaxRequest::send_tiff;
		break;
	    case FaxRequest::send_postscript_saved:
		freq.op = FaxRequest::send_postscript;
		break;
	    }
	    i++;
	}
	req->pagehandling = "";			// force recalculation
	req->status = send_retry;		// ... force retry
	req->tts = now;				// ... and do it now
    }
    if (job.killtime == 0 && req->status == send_retry) {
	/*
	 * The job timed out during the send attempt.  We
	 * couldn't do anything then, but now the job can
	 * be cleaned up.  Not sure if the user should be
	 * notified of the requeue as well as the timeout?
	 */
	deleteRequest(job, req, Job::timedout, TRUE);
	setDead(job);
    } else if (req->status == send_retry) {
	/*
	 * If a continuation cover page is required for
	 * the retransmission, fixup the job state so
	 * that it'll get one when it's next processed.
	 */
	if (req->cover != "") {
	    /*
	     * Job was previously setup to get a cover page.
	     * If the generated cover page was not sent,
	     * delete it so that it'll get recreated.
	     */
	    if (req->requests[0].item == req->cover)
		req->removeItems(0);
	} else if (req->npages > 0 && contCoverPageTemplate != "") {
	    /*
	     * At least one page was sent so any existing
	     * cover page is certain to be gone.  Setup
	     * to generate a cover page when the job is
	     * retried.
	     */
	    req->cover = docDir | "/cover" | req->jobid;
	}
	if (req->tts < now) {
	    /*
	     * Send failed and send app didn't specify a new
	     * tts, bump the ``time to send'' by the requeue
	     * interval, then rewrite the queue file.  This causes
	     * the job to be rescheduled for transmission
	     * at a future time.
	     */
	    req->tts = now + (requeueInterval>>1) + (random()%requeueInterval);
	}
	job.remove();				// remove from active list
	/*
	 * Bump the job priority so the retry will get processed
	 * before new jobs.  We bound the priority to keep it within
	 * a fixed ``range'' around it's starting priority.  This
	 * is intended to keep jobs in different ``classifications''
	 * from conflicting (e.g. raising a bulk-style fax up into
	 * the priority range of a non-bulk-style fax).
	 */
	if (JOBHASH(job.pri-1) == JOBHASH(req->usrpri))
	    job.pri--; 
	updateRequest(*req, job);		// update on-disk status
	if (req->tts > now) {
	    traceQueue(job, "SEND INCOMPLETE: requeue for "
		| strTime(req->tts - now)
		| "; " | req->notice);
	    setSleep(job, req->tts);
	    if (req->notify == FaxRequest::when_requeued)
		notifySender(req->mailaddr, job, Job::requeued);
	} else {
	    traceQueue(job, "SEND INCOMPLETE: retry immediately; " |
		req->notice); 
	    setReadyToRun(job);			// NB: job.tts will be <= now
	}
	delete req;				// implicit unlock of q file
    } else {
	traceQueue(job, "SEND DONE: " | strTime(duration));
	// NB: always notify client if job failed
	if (req->status == send_failed)
	    deleteRequest(job, req, Job::failed, TRUE, fmtTime(duration));
	else
	    deleteRequest(job, req, Job::done, FALSE, fmtTime(duration));
	setDead(job);
    }
}

/*
 * Job Queue Management Routines.
 */

/*
 * Insert a job in the queue of read-to-run jobs.
 */
void
faxQueueApp::setReadyToRun(Job& job)
{
    traceJob(job, "READY");
    JobIter iter(runqs[JOBHASH(job.pri)]);
    for (; iter.notDone() && iter.job().pri <= job.pri; iter++)
	;
    job.insert(iter.job());
}

/*
 * Place a job on the queue of jobs waiting to run
 * and start the associated timer.
 */
void
faxQueueApp::setSleep(Job& job, time_t tts)
{
    traceJob(job, "SLEEP FOR " | strTime(tts - Sys::now()));
    JobIter iter(sleepq);
    for (; iter.notDone() && iter.job().tts <= tts; iter++)
	;
    job.insert(iter.job());
    job.startTTSTimer(tts);
}

/*
 * Process a job that's finished.  The corpse gets placed
 * on the deadq and is reaped the next time the scheduler
 * runs.  If any jobs are blocked waiting for this job to
 * complete, one is made ready to run.
 */
void
faxQueueApp::setDead(Job& job)
{
    traceJob(job, "DEAD");
    DestInfo& di = destJobs[job.dest];
    di.done(job);			// remove from active destination list
    di.updateConfig();			// update file if something changed
    if (!di.isEmpty()) {
	/*
	 * Check if there are blocked jobs waiting to run
	 * and that there is now room to run one.  If so,
	 * take jobs off the blocked queue and make them
	 * ready for processing.
	 */
	Job* jb;
	const DestControlInfo& dci = destCtrls[job.dest];
	u_int n = 1;
	while (isOKToStartJobs(di, dci, n) && (jb = di.nextBlocked()))
	    setReadyToRun(*jb), n++;
    } else {
	/*
	 * This is the last job to the destination; purge
	 * the entry from the destination jobs database.
	 */
	destJobs.remove(job.dest);
    }
    if (activeJob(job.file))		// lazy removal from active list
	job.remove();
    job.insert(*deadq.next);		// setup job corpus for reaping
    if (job.modem)			// called from many places
	releaseModem(job);
    pokeScheduler();
}

/*
 * Place a job on the list of jobs actively being processed.
 */
void
faxQueueApp::setActive(Job& job)
{
    traceJob(job, "ACTIVE");
    job.insert(*activeq.next);
}

/*
 * Remove a job from whichever list its on and cancel
 * any pending timeout if the job was on the sleepq.
 */
Job*
faxQueueApp::removeJob(const fxStr& filename)
{
    // check list of jobs waiting for their time-to-send to go off
    for (JobIter iter(sleepq); iter.notDone(); iter++) {
	Job& job = iter;
	if (job.file == filename) {
	    job.remove();
	    job.stopTTSTimer();				// cancel timeout
	    return (&job);
	}
    }
    // check ready-to-run queue
    for (u_int i = 0; i < NQHASH; i++) {
	for (iter = runqs[i]; iter.notDone(); iter++) {
	    Job& job = iter;
	    if (job.file == filename) {
		job.remove();
		return (&job);
	    }
	}
    }
    /*
     * Check the list of jobs blocked by another job
     * to the same destination.  Note that we don't
     * remove the job from this list because that is
     * done when the job is placed on the dead queue.
     */
    for (DestInfoDictIter diter(destJobs); diter.notDone(); diter++) {
	Job* job = diter.value().unblock(filename);
	if (job)
	    return (job);
    }
    return (NULL);
}

/*
 * Create a new job entry and place them on the
 * appropriate queue.  A kill timer is also setup
 * for the job.
 */
void
faxQueueApp::submitJob(const fxStr& filename, FaxRequest& req)
{
    time_t now = Sys::now();
    Job* job = new Job(filename, req.jobid, req.modem,
	req.pri, req.tts == 0 ? now : req.tts);
    traceJob(*job, "CREATE");
    /*
     * Check various submission parameters.
     */
    if (req.killtime <= now) {
	traceQueue(*job, "KILL TIME EXPIRED");
	terminateJob(*job, Job::timedout);
	return;
    }
    if (req.modem != MODEM_ANY && !Modem::modemExists(req.modem)) {
	rejectSubmission(*job, req,
	    "REJECT: Requested modem " | req.modem | " is not registered");
	return;
    }
    if (req.requests.length() == 0) {
	rejectSubmission(*job, req, "REJECT: No work found in job file");
	return;
    }
    job->dest = canonicalizePhoneNumber(req.number);
    if (job->dest == "") {
	rejectSubmission(*job, req,
	    "REJECT: Unable to convert dial string to canonical format");
	return;
    }
    if (req.pagewidth > 303) {
	rejectSubmission(*job, req,
	    fxStr::format("REJECT: Page width (%u) appears invalid",
		req.pagewidth));
	return;
    }
    /*
     * Setup required modem capabilities for use in
     * selecting a modem to use in sending the job.
     */
    job->pagewidth = req.pagewidth;
    job->pagelength = req.pagelength;
    job->resolution = req.resolution;
    job->willpoll =
	(req.findRequest(FaxRequest::send_poll) != fx_invalidArrayIndex);
    /*
     * Start the kill timer and put the
     * job on the appropriate queue.
     */
    job->startKillTimer(req.killtime);
    if (job->tts <= now)
	setReadyToRun(*job);
    else
	setSleep(*job, job->tts);
}

/*
 * Reject a job submission.
 */
void
faxQueueApp::rejectSubmission(Job& job, FaxRequest& req, const fxStr& reason)
{
    req.status = send_failed;
    req.notice = reason;
    traceServer("JOB " | job.jobid | ": " | reason);
    deleteRequest(job, req, Job::rejected, TRUE);
    setDead(job);				// dispose of job
}

/*
 * Locate a job on the active queue.
 */
Job*
faxQueueApp::activeJob(const fxStr& filename)
{
    for (JobIter iter(activeq); iter.notDone(); iter++)
	if (iter.job().file == filename)
	    return (&iter.job());
    return (NULL);
}

/*
 * Terminate a job in response to a command message.
 * If the job is currently running, just notify the
 * modem process that has it--this will eventually
 * cause a return message to use that'll cause it to
 * be deleted.  Otherwise, immediately remove it from
 * the appropriate queue and purge any associated
 * resources.
 */
void
faxQueueApp::terminateJob(const fxStr& filename, JobStatus how)
{
    Job* job = activeJob(filename);
    if (job) {
	/*
	 * Job is being sent by a subprocess; signal the
	 * process and let it's termination trigger the
	 * normal job cleanup.  Note that we also mark the
	 * job in case the signal is missed (there are windows
	 * in the child where this can occur).
	 */
	job->abortPending = TRUE;		// mark job
	::kill(job->pid, SIGTERM);		// signal subprocess
    } else {
	/*
	 * Job is either sleeping or ready to run;
	 * termination can be handled immediately.
	 */
	job = removeJob(filename);
	if (job)
	    terminateJob(*job, how);
    }
}

void
faxQueueApp::terminateJob(Job& job, JobStatus why)
{
    FaxRequest* req = readRequest(job);
    if (req)
	deleteRequest(job, req, why, why != Job::removed);
    setDead(job);
}

/*
 * Reject a job at some time before it's handed off to the server thread.
 */
void
faxQueueApp::rejectJob(Job& job, FaxRequest& req, const fxStr& reason)
{
    req.status = send_failed;
    req.notice = reason;
    traceServer("JOB " | job.jobid | ": " | reason);
    setDead(job);				// dispose of job
}

/*
 * Deal with a job that's blocked by a concurrent job.
 */
void
faxQueueApp::blockJob(Job& job, FaxRequest& req, const char* mesg)
{
    req.notice = mesg;
    updateRequest(req, job);
    traceQueue(job, mesg);
    if (req.notify == FaxRequest::when_requeued)
	notifySender(req.mailaddr, job, Job::blocked); 
    releaseModem(job);
}

/*
 * Requeue a job that's delayed for some reason.
 */
void
faxQueueApp::delayJob(Job& job, FaxRequest& req, const char* mesg, time_t tts)
{
    fxStr reason(mesg);
    req.tts = tts;
    time_t delay = tts - Sys::now();
    // adjust kill time so job isn't removed before it runs
    job.stopKillTimer();
    req.killtime += delay;
    job.startKillTimer(req.killtime);
    req.notice = reason;
    updateRequest(req, job);
    traceQueue(job, reason | ": requeue for " | strTime(delay));
    if (req.notify == FaxRequest::when_requeued)
	notifySender(req.mailaddr, job, Job::requeued); 
    setSleep(job, tts);
    releaseModem(job);
}

/*
 * Process the job who's kill time expires.  The job is
 * terminated unless it is currently being tried, in which
 * case it's marked for termination after the attempt is
 * completed.
 */
void
faxQueueApp::timeoutJob(Job& job)
{
    traceQueue(job, "KILL TIME EXPIRED");
    if (activeJob(job.file)) {
	job.killtime = 0;			// mark job to be removed
    } else {
	job.remove();				// remove from sleep queue
	terminateJob(job, Job::timedout);
    }
}

/*
 * Create a job from a queue file and add it
 * to the scheduling queues.
 */
void
faxQueueApp::submitJob(const fxStr& filename)
{
    if (!Sys::isRegularFile(filename))
	return;
    int fd = Sys::open(filename, O_RDWR);
    if (fd >= 0) {
	if (::flock(fd, LOCK_SH) >= 0) {
	    FaxRequest req(filename);
	    fxBool reject;
	    /*
	     * There are three possibilities:
	     *
	     * 1. The queue file was read properly and the job
	     *    can be submitted.
	     * 2. There were problems reading the file, but
	     *    enough information was obtained to purge the
	     *    job from the queue.
	     * 3. Insufficient information was obtained to purge
	     *    the job; just skip it.
	     */
	    if (req.readQFile(fd, reject) && !reject)
		submitJob(filename, req);
	    else if (reject) {
		Job job(req.qfile, req.jobid, req.modem, req.pri, req.tts);
		req.status = send_failed;
		req.notice = "Invalid or corrupted job description file";
		traceServer("JOB " | job.jobid | ": " | req.notice);
		// NB: this may not work, but we try...
		deleteRequest(job, req, Job::rejected, TRUE);
	    } else
		traceServer(filename | ": Unable to purge job, ignoring it");
	}
	::close(fd);
    }
}

/*
 * Alter parameters of a job queued for execution.
 * Note that it is not possible to alter the parameters
 * of jobs that are actively being processed.
 */
void
faxQueueApp::alterJob(const char* s)
{
    const char cmd = *s++;
    const char* cp = ::strchr(s, ' ');
    if (!cp) {
	logError("Malformed JOB request \"%s\"", s);
	return;
    }
    fxStr filename(s, cp-s);
    Job* jp = removeJob(filename);
    if (!jp) {
	logError("JOB %s: Not found on queues.", (char*) filename);
	return;
    }
    while (isspace(*cp))
	cp++;
    switch (cmd) {
    case 'K':			// kill time
	if (!changeKillTime(*jp, ::atoi(cp)))
	    return;
	break;
    case 'M':			// modem device to use
	jp->device = cp;
	break;
    case 'P':			// change priority
	jp->pri = ::atoi(cp);
	break;
    case 'T':			// time-to-send
	jp->tts = ::atoi(cp);
	break;
    default:
	jobError(*jp, "Invalid alter command \"%c\" ignored.", cmd);
	break;
    }
    // place back on appropriate queue
    if (jp->tts <= Sys::now())
	setReadyToRun(*jp);
    else
	setSleep(*jp, jp->tts);
}

/*
 * Effect a change in the kill time in response
 * to a command message.
 */
fxBool
faxQueueApp::changeKillTime(Job& job, time_t killtime)
{
    job.stopKillTimer();
    if (killtime > Sys::now()) {
	job.startKillTimer(killtime);
	return (TRUE);
    } else {
	terminateJob(job, Job::timedout);
	return (FALSE);
    }
}

/*
 * Process the expiration of a job's time-to-send timer.
 * The job is moved to the ready-to-run queues and the
 * scheduler is poked.
 */
void
faxQueueApp::runJob(Job& job)
{
    job.remove();
    setReadyToRun(job);
    pokeScheduler();
}

/*
 * Scan the list of jobs and process those that are ready
 * to go.  Note that the scheduler should only ever be
 * invoked from the dispatcher via a timeout.  This way we
 * can be certain there are no active contexts holding
 * references to job corpses (or other data structures) that
 * we want to reap.  To invoke the scheduler the pokeScheduler
 * method should be called to setup an immediate timeout that
 * will cause the scheduler to be invoked from the dispatcher.
 */
void
faxQueueApp::runScheduler()
{
    /*
     * Terminate the server if there are no jobs currently
     * being processed.  We must be sure to wait for jobs
     * so that we can capture exit status from subprocesses
     * and so that any locks held on behalf of outbound jobs
     * do not appear to be stale (since they are held by this
     * process).
     */
    if (quit && activeq.next == &activeq) {
	close();
	return;
    }
    /*
     * We scan modems until we find one that's available and then
     * look for a compatible job.  This could be done the other way
     * around, but there tend to be fewer modems than jobs so this
     * should scale better when the server is loaded.
     */
    for (ModemIter miter(Modem::list); miter.notDone(); miter++) {
	Modem& modem = miter;
	if (modem.getState() != Modem::READY)
	    continue;
	for (u_int i = 0; i < NQHASH; i++) {
	    for (JobIter iter(runqs[i]); iter.notDone(); iter++) {
		Job& job = iter;
		fxAssert(job.tts <= Sys::now(), "Sleeping job on run queue");
		if (modem.assign(job)) {
		    job.remove();		// remove from run queue
		    traceJob(job, "PROCESS");
		    FaxRequest* req = readRequest(job);
		    if (req)
			processJob(job, req);	// initiate processing...
		    else
			setDead(job);
		}
	    }
	}
    }
    /*
     * Reap dead jobs.
     */
    for (JobIter iter(deadq); iter.notDone(); iter++) {
	Job* job = iter;
	job->remove();
	traceJob(*job, "DELETE");
	delete job;
    }
}

/*
 * Release a modem assigned to a job.  The scheduler
 * is prodded since doing this may permit something
 * else to be processed.
 */
void
faxQueueApp::releaseModem(Job& job)
{
    fxAssert(job.modem != NULL, "No assigned modem to release");
    job.modem->release();
    job.modem = NULL;			// remove reference to modem
    pokeScheduler();
}

/*
 * Set a timeout so that the job scheduler runs the
 * next time the dispatcher is invoked.
 */
void
faxQueueApp::pokeScheduler()
{
    schedTimeout.start();
}

/*
 * Create a request instance and read the
 * associated queue file into it.
 */
FaxRequest*
faxQueueApp::readRequest(Job& job)
{
    int fd = Sys::open(job.file, O_RDWR);
    if (fd >= 0) {
	if (::flock(fd, LOCK_EX) >= 0) {
	    FaxRequest* req = new FaxRequest(job.file);
	    fxBool reject;
	    if (req->readQFile(fd, reject) && !reject) {
		if (req->external == "")
		    req->external = job.dest;
		return (req);
	    }
	    jobError(job, "Could not read job file");
	    delete req;
	} else
	    jobError(job, "Could not lock job file: %m");
	::close(fd);
    } else {
	// file might have been removed by another server
	if (errno != ENOENT)
	    jobError(job, "Could not open job file: %m");
    }
    return (NULL);
}

/*
 * Update the request instance with information
 * from the job structure and then write the
 * associated queue file.
 */
void
faxQueueApp::updateRequest(FaxRequest& req, Job& job)
{
    req.pri = job.pri;
    req.writeQFile();
}

/*
 * Delete a request and associated state.
 */
void
faxQueueApp::deleteRequest(Job& job, FaxRequest* req, JobStatus why,
    fxBool force, const char* duration)
{
    deleteRequest(job, *req, why, force, duration);
    delete req;
}

void
faxQueueApp::deleteRequest(Job& job, FaxRequest& req, JobStatus why,
    fxBool force, const char* duration)
{
    if (req.notify != FaxRequest::no_notice || force) {
	req.writeQFile();			// update file for notifier
	notifySender(req.mailaddr, job, why, duration);
    }
    req.removeItems(0, req.requests.length());
    Sys::unlink(req.qfile);
}

/*
 * FIFO-related support.
 */

/*
 * Open the requisite FIFO special files.
 */
void
faxQueueApp::openFIFOs()
{
    fifo = openFIFO(fifoName, 0600, TRUE);
    Dispatcher::instance().link(fifo, Dispatcher::ReadMask, this);
}

void
faxQueueApp::closeFIFOs()
{
    ::close(fifo), fifo = -1;
}

int faxQueueApp::inputReady(int fd)		{ return FIFOInput(fd); }

/*
 * Process a message received through a FIFO.
 */
void
faxQueueApp::FIFOMessage(const char* cp)
{
    switch (cp[0]) {
    case 'C':				// configuration control
	traceServer("CONFIG \"%s\"", cp+1);
	readConfigItem(cp+1);
	break;
    case 'J':				// alter job parameter(s)
	traceServer("ALTER JOB PARAMS \"%s\"", cp+1);
	alterJob(cp+1);
	pokeScheduler();
	break;
    case 'Q':				// quit
	traceServer("QUIT");
	quit = TRUE;
	pokeScheduler();
	break;
    case 'R':				// remove job
	traceServer("REMOVE JOB \"%s\"", cp+1);
	terminateJob(cp+1, Job::removed);
	break;
    case 'K':				// kill job
	traceServer("KILL JOB \"%s\"", cp+1);
	terminateJob(cp+1, Job::killed);
	break;
    case 'S':				// submit a send job
	traceServer("SUBMIT JOB \"%s\"", cp+1);
	submitJob(cp+1);
	pokeScheduler();
	break;
    case '+':				// msg from ancillary modem proc
	const char* tp;
	tp = ::strchr(++cp, ':');
	if (tp) {
	    Modem::getModemByID(fxStr(cp,tp-cp)).FIFOMessage(tp+1);
	    pokeScheduler();
	    break;
	} else
	    faxApp::FIFOMessage(cp-1);
	break;
    default:
	faxApp::FIFOMessage(cp);
	break;
    }
}

/*
 * Configuration support.
 */

void
faxQueueApp::resetConfig()
{
    FaxConfig::resetConfig();
    dialRules = NULL;
    setupConfig();
}

#define	N(a)	(sizeof (a) / sizeof (a[0]))

const faxQueueApp::stringtag faxQueueApp::strings[] = {
{ "logfacility",	&faxQueueApp::logFacility,	LOG_FAX },
{ "areacode",		&faxQueueApp::areaCode	},
{ "countrycode",	&faxQueueApp::countryCode },
{ "longdistanceprefix",	&faxQueueApp::longDistancePrefix },
{ "internationalprefix",&faxQueueApp::internationalPrefix },
{ "uucplockdir",	&faxQueueApp::uucpLockDir,	UUCP_LOCKDIR },
{ "uucplocktype",	&faxQueueApp::uucpLockType,	UUCP_LOCKTYPE },
{ "contcoverpage",	&faxQueueApp::contCoverPageTemplate },
{ "contcovercmd",	&faxQueueApp::coverCmd,		FAX_COVERCMD },
{ "notifycmd",		&faxQueueApp::notifyCmd,	FAX_NOTIFYCMD },
{ "ps2faxcmd",		&faxQueueApp::ps2faxCmd,	FAX_PS2FAXCMD },
{ "sendfaxcmd",		&faxQueueApp::sendFaxCmd,
   FAX_LIBEXEC "/faxsend" },
{ "sendpagecmd",	&faxQueueApp::sendPageCmd,
   FAX_LIBEXEC "/pagesend" },
{ "senduucpcmd",	&faxQueueApp::sendUUCPCmd,
   FAX_LIBEXEC "/uucpsend" },
};
const faxQueueApp::numbertag faxQueueApp::numbers[] = {
{ "tracingmask",	&faxQueueApp::tracingMask,	// NB: must be first
   FAXTRACE_MODEMIO|FAXTRACE_TIMEOUTS },
{ "servertracing",	&faxQueueApp::tracingLevel,	FAXTRACE_SERVER },
{ "sessiontracing",	&faxQueueApp::logTracingLevel,	(u_int) -1 },
{ "uucplocktimeout",	&faxQueueApp::uucpLockTimeout,	0 },
{ "postscripttimeout",	&faxQueueApp::postscriptTimeout, 3*60 },
{ "maxconcurrentjobs",	&faxQueueApp::maxConcurrentJobs, 1 },
{ "maxsendpages",	&faxQueueApp::maxSendPages,	(u_int) -1 },
{ "maxtries",		&faxQueueApp::maxTries,		(u_int) FAX_RETRIES },
{ "maxdials",		&faxQueueApp::maxDials,		(u_int) FAX_REDIALS },
{ "jobreqother",	&faxQueueApp::requeueInterval,	FAX_REQUEUE },
};

void
faxQueueApp::setupConfig()
{
    int i;

    for (i = N(strings)-1; i >= 0; i--)
	(*this).*strings[i].p = (strings[i].def ? strings[i].def : "");
    for (i = N(numbers)-1; i >= 0; i--)
	(*this).*numbers[i].p = numbers[i].def;
    tod.reset();			// any day, any time
    use2D = TRUE;			// ok to use 2D data
    uucpLockMode = UUCP_LOCKMODE;
    delete dialRules, dialRules = NULL;
}

void
faxQueueApp::configError(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlogError(fmt, ap);
    va_end(ap);
}

void
faxQueueApp::configTrace(const char* fmt, ...)
{
    if (tracingLevel & FAXTRACE_CONFIG) {
	va_list ap;
	va_start(ap, fmt);
	vlogError(fmt, ap);
	va_end(ap);
    }
}

fxBool
faxQueueApp::setConfigItem(const char* tag, const char* value)
{
    u_int ix;
    if (findTag(tag, (const tags*) strings, N(strings), ix)) {
	(*this).*strings[ix].p = value;
	switch (ix) {
	case 0:	faxApp::setLogFacility(logFacility); break;
	}
    } else if (findTag(tag, (const tags*) numbers, N(numbers), ix)) {
	(*this).*numbers[ix].p = getNumber(value);
	switch (ix) {
	case 1: tracingLevel &= ~tracingMask; break;
	case 2: logTracingLevel &= ~tracingMask; break;
	case 3: UUCPLock::setLockTimeout(uucpLockTimeout); break;
	}
    } else if (streq(tag, "destcontrols"))
	destCtrls.setFilename(value);
    else if (streq(tag, "dialstringrules"))
	setDialRules(value);
    else if (streq(tag, "timeofday"))
	tod.parse(value);
    else if (streq(tag, "use2d"))
	use2D = getBoolean(value);
    else if (streq(tag, "uucplockmode"))
	uucpLockMode = ::strtol(value, 0, 8);
    else
	return (FALSE);
    return (TRUE);
}

void
faxQueueApp::setDialRules(const char* name)
{
    delete dialRules;
    dialRules = new DialStringRules(name);
    /*
     * Setup configuration environment.
     */
    dialRules->def("AreaCode", areaCode);
    dialRules->def("CountryCode", countryCode);
    dialRules->def("LongDistancePrefix", longDistancePrefix);
    dialRules->def("InternationalPrefix", internationalPrefix);
    if (!dialRules->parse()) {
	configError("Parse error in dial string rules \"%s\"", name);
	delete dialRules, dialRules = NULL;
    }
}

/*
 * Convert a dialing string to a canonical format.
 */
fxStr
faxQueueApp::canonicalizePhoneNumber(const fxStr& ds)
{
    if (dialRules)
	return dialRules->canonicalNumber(ds);
    else
	return ds;
}

/*
 * Create an appropriate UUCP lock instance.
 */
UUCPLock*
faxQueueApp::getUUCPLock(const fxStr& deviceName)
{
    return UUCPLock::newLock(uucpLockType,
	uucpLockDir, deviceName, uucpLockMode);
}

u_int faxQueueApp::getTracingLevel() const
    { return logTracingLevel; }
u_int faxQueueApp::getMaxConcurrentJobs() const
    { return maxConcurrentJobs; }
u_int faxQueueApp::getMaxSendPages() const
    { return maxSendPages; }
u_int faxQueueApp::getMaxDials() const
    { return maxDials; }
u_int faxQueueApp::getMaxTries() const
    { return maxTries; }
time_t faxQueueApp::nextTimeToSend(time_t t) const
    { return tod.nextTimeOfDay(t); }

/*
 * Miscellaneous stuff.
 */

/*
 * Notify the sender of a job that something has
 * happened -- the job has completed, it's been requeued
 * for later processing, etc.
 */
void
faxQueueApp::notifySender(const fxStr& mailaddr, Job& job, JobStatus why, const char* duration)
{
    static const fxStr quote(" \"");
    static const fxStr enquote("\"");

    traceServer("NOTIFY " | mailaddr);
    fxStr cmd(notifyCmd
	| quote |		      job.file | enquote
	| quote |      Job::jobStatusName(why) | enquote
	| quote |		      duration | enquote
	| quote | fxStr((long) job.pid, "%lu") | enquote
	| quote |		      job.dest | enquote
    );
    if (why == Job::requeued) {
	/*
	 * It's too hard to do localtime in an awk script,
	 * so if we may need it, we calculate it here
	 * and pass the result as an optional argument.
	 */
	char buf[30];
	::strftime(buf, sizeof (buf), " \"%H:%M\"", ::localtime(&job.tts));
	cmd.append(buf);
    }
    runCmd(cmd, TRUE);
}

void
faxQueueApp::traceServer(const char* fmt ...)
{
    if (tracingLevel & FAXTRACE_SERVER) {
	va_list ap;
	va_start(ap, fmt);
	vlogInfo(fmt, ap);
	va_end(ap);
    }
}

static void
vtraceJob(const Job& job, const char* fmt, va_list ap)
{
    time_t now = Sys::now();
    vlogInfo(
	  "JOB " | job.jobid
	| " (dest " | job.dest
	| fxStr::format(" pri %u", job.pri)
	| " tts " | strTime(job.tts - now)
	| " killtime " | strTime(job.killtime - now)
	| "): "
	| fmt, ap);
}

void
faxQueueApp::traceQueue(Job& job, const char* fmt ...)
{
    if (tracingLevel & FAXTRACE_QUEUEMGMT) {
	va_list ap;
	va_start(ap, fmt);
	vtraceJob(job, fmt, ap);
	va_end(ap);
    }
}

void
faxQueueApp::traceJob(Job& job, const char* fmt ...)
{
    if (tracingLevel & FAXTRACE_JOBMGMT) {
	va_list ap;
	va_start(ap, fmt);
	vtraceJob(job, fmt, ap);
	va_end(ap);
    }
}

void
faxQueueApp::traceQueue(const char* fmt ...)
{
    if (tracingLevel & FAXTRACE_QUEUEMGMT) {
	va_list ap;
	va_start(ap, fmt);
	vlogInfo(fmt, ap);
	va_end(ap);
    }
}

void
faxQueueApp::jobError(Job& job, const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlogError("JOB " | job.jobid | ": " | fmt, ap);
    va_end(ap);
}

static void
usage(const char* appName)
{
    fxFatal("usage: %s [-q queue-directory] [-d] [-m devid]", appName);
}

static void
sigCleanup(int)
{
    faxQueueApp::instance().close();
    ::_exit(-1);
}

int
main(int argc, char** argv)
{
    faxApp::setupLogging("FaxQueuer");

    fxStr appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    faxApp::setupPermissions();

    faxApp::setOpts("m:q:D");

    fxBool detach = TRUE;
    fxStr queueDir(FAX_SPOOLDIR);
    for (GetoptIter iter(argc, argv, faxApp::getOpts()); iter.notDone(); iter++)
	switch (iter.option()) {
	case 'q': queueDir = iter.optArg(); break;
	case 'D': detach = FALSE; break;
	case '?': usage(appName);
	}
    if (Sys::chdir(queueDir) < 0)
	fxFatal(queueDir | ": Can not change directory");
    if (detach)
	faxApp::detachFromTTY();

    faxQueueApp* app = new faxQueueApp;

    ::signal(SIGTERM, fxSIGHANDLER(sigCleanup));
    ::signal(SIGINT, fxSIGHANDLER(sigCleanup));

    app->initialize(argc, argv);
    app->open();
    while (app->isRunning())
	Dispatcher::instance().dispatch();
    app->close();
    delete app;

    return 0;
}
