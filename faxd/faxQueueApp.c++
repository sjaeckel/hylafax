/*	$Id: faxQueueApp.c++,v 1.134 1996/11/22 00:00:49 sam Rel $ */
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
#include "Sys.h"

#include <ctype.h>
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
#include "FaxRecvInfo.h"
#include "Timeout.h"
#include "UUCPLock.h"
#include "DialRules.h"
#include "Modem.h"
#include "Trigger.h"
#include "faxQueueApp.h"
#include "HylaClient.h"
#include "G3Decoder.h"
#include "FaxSendInfo.h"
#include "config.h"

/*
 * HylaFAX Spooling and Command Agent.
 */

const fxStr faxQueueApp::sendDir	= FAX_SENDDIR;
const fxStr faxQueueApp::docDir		= FAX_DOCDIR;
const fxStr faxQueueApp::clientDir	= FAX_CLIENTDIR;

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

    fxAssert(_instance == NULL, "Cannot create multiple faxQueueApp instances");
    _instance = this;
}

faxQueueApp::~faxQueueApp()
{
    HylaClient::purge();
    delete dialRules;
}

faxQueueApp& faxQueueApp::instance() { return *_instance; }

#include "version.h"

void
faxQueueApp::initialize(int argc, char** argv)
{
    updateConfig(configFile);		// read config file
    faxApp::initialize(argc, argv);

    logInfo("%s", VERSION);
    logInfo("%s", "Copyright (c) 1990-1996 Sam Leffler");
    logInfo("%s", "Copyright (c) 1991-1996 Silicon Graphics, Inc.");

    scanForModems();
}

void
faxQueueApp::open()
{
    faxApp::open();
    scanQueueDirectory();
    Modem::broadcast("HELLO");		// announce queuer presence
    scanClientDirectory();		// announce queuer presence
    pokeScheduler();
}

/*
 * Scan the spool area for modems.  We can't be certain the
 * modems are actively working without probing them; this
 * work is done simply to buildup the internal database for
 * broadcasting a ``HELLO'' message.  Later on, individual
 * modems are enabled for use based on messages received
 * through the FIFO.
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
    closedir(dir);
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
	logError("Could not scan " | sendDir | " directory for outbound jobs");
	return;
    }
    for (dirent* dp = readdir(dir); dp; dp = readdir(dir)) {
	if (dp->d_name[0] == 'q')
	    submitJob(&dp->d_name[1], TRUE);
    }
    closedir(dir);
}

/*
 * Scan the client area for active client processes
 * and send a ``HELLO message'' to notify them the
 * queuer process has restarted.  If no process is
 * listening on the FIFO, remove it; the associated
 * client state will be purged later.
 */
void
faxQueueApp::scanClientDirectory()
{
    DIR* dir = Sys::opendir(clientDir);
    if (dir == NULL) {
	logError("Could not scan " | clientDir | " directory for clients");
	return;
    }
    for (dirent* dp = readdir(dir); dp; dp = readdir(dir)) {
	if (!isdigit(dp->d_name[0]))
	    continue;
	fxStr fifo(clientDir | "/" | dp->d_name);
	if (Sys::isFIFOFile((const char*) fifo))
	    if (!HylaClient::getClient(fifo).send("HELLO", 6))
		Sys::unlink(fifo);
    }
    closedir(dir);
}

/*
 * Process a job.  Prepare it for transmission and
 * pass it on to the thread that does the actual
 * transmission work.  The job is marked ``active to
 * this destination'' prior to preparing it because
 * preparation may involve asynchronous activities.
 * The job is placed on the active list so that it
 * can be located by filename if necessary.
 */
void
faxQueueApp::processJob(Job& job, FaxRequest* req,
    DestInfo& di, const DestControlInfo& dci)
{
    job.commid = "";				// set on return
    di.active(job);
    FaxMachineInfo& info = di.getInfo(job.dest);
    JobStatus status;
    setActive(job);				// place job on active list
    updateRequest(*req, job);
    if (!prepareJobNeeded(job, *req, status)) {
	if (status != Job::done) {
	    job.state = FaxRequest::state_done;
	    deleteRequest(job, req, status, TRUE);
	    setDead(job);
	} else
	    sendJobStart(job, req, dci);
    } else
	prepareJobStart(job, req, info, dci);
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
	case FaxRequest::send_pcl:		// convert PCL
	case FaxRequest::send_tiff:		// verify&possibly convert TIFF
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
    signal(s, fxSIGHANDLER(faxQueueApp::prepareCleanup));
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
    pid_t pid = fork();
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
	signal(SIGTERM, fxSIGHANDLER(faxQueueApp::prepareCleanup));
	signal(SIGINT, fxSIGHANDLER(faxQueueApp::prepareCleanup));
	_exit(prepareJob(job, *req, info, dci));
	/*NOTREACHED*/
    case -1:				// fork failed, sleep and retry
	delayJob(job, *req, "Could not fork to prepare job for transmission",
	    Sys::now() + random() % requeueInterval);
	delete req;
	break;
    default:				// parent, setup handler to wait
	job.startPrepare(pid);
	delete req;			// must reread after preparation
	Trigger::post(Trigger::JOB_PREP_BEGIN, job);
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
    Trigger::post(Trigger::JOB_PREP_END, job);
    if (status&0xff) {
	logError("JOB %s: bad exit status %#x from sub-fork",
	    (char*) job.jobid, status);
	status = Job::failed;
    } else
	status >>= 8;
    if (job.suspendPending) {		// co-thread waiting
	job.suspendPending = FALSE;
	releaseModem(job);
	return;
    }
    FaxRequest* req = readRequest(job);
    if (!req) {
	// NB: no way to notify the user (XXX)
	logError("JOB %s: qfile vanished during preparation",
	    (char*) job.jobid);
	setDead(job);
    } else {
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
 * Document Use Database.
 *
 * The server minimizes imaging operations by checking for the
 * existence of compatible, previously imaged, versions of documents.
 * This is done by using a file naming convention that includes the
 * source document name and the remote machine capabilities that are
 * used for imaging.  The work done here (and in other HylaFAX apps)
 * also assumes certain naming convention used by hfaxd when creating
 * document files.  Source documents are named:
 *
 *     doc<docnum>.<type>
 *
 * where <docnum> is a unique document number that is assigned by
 * hfaxd at the time the document is stored on the server.  Document
 * references by a job are then done using filenames (i.e. hard
 * links) of the form:
 *
 *	doc<docnum>.<type>.<jobid>
 *
 * where <jobid> is the unique identifier assigned to each outbound
 * job.  Then, each imaged document is named:
 *
 *	doc<docnum>.<type>;<encoded-capabilities>
 *
 * where <encoded-capabilities> is a string that encodes the remote
 * machine's capabilities.
 *
 * Before imaging a document the scheduler checks to see if there is
 * an existing file with the appropriate name.  If so then the file
 * is used and no preparation work is needed for sending the document.
 * Otherwise the document must be converted for transmission; this
 * result is written to a file with the appropriate name.  After an
 * imaged document has been transmitted it is not immediately removed,
 * but rather the scheduler is informed that the job no longer holds
 * (needs) a reference to the document and the scheduler records this
 * information so that when no jobs reference the original source
 * document, all imaged forms may be expunged.  As documents are
 * transmitted the job references to the original source documents are
 * converted to references to the ``base document name'' (the form
 * without the <jobid>) so that the link count on the inode for this
 * file reflects the number of references from jobs that are still
 * pending transmission.  This means that the scheduler can use the
 * link count to decide when to expunge imaged versions of a document.
 *
 * Note that the reference counts used here do not necessarily
 * *guarantee* that a pre-imaged version of a document will be available.
 * There are race conditions under which a document may be re-imaged
 * because a previously imaged version was removed.
 *
 * A separate document scavenger program should be run periodically
 * to expunge any document files that might be left in the docq for
 * unexpected reasons.  This program should read the set of jobs in
 * the sendq to build a onetime table of uses and then remove any
 * files found in the docq that are not referenced by a job.
 */

/*
 * Remove a reference to an imaged document and if no
 * references exist for the corresponding source document,
 * expunge all imaged versions of the document.
 */
void
faxQueueApp::unrefDoc(const fxStr& file)
{
    /*
     * Convert imaged document name to the base
     * (source) document name by removing the
     * encoded session parameters used for imaging.
     */
    u_int l = file.nextR(file.length(), ';');
    if (l == 0) {
	logError("Bogus document handed to unrefDoc: %s", (const char*) file);
	return;
    }
    fxStr doc = file.head(l-1);
    /*
     * Add file to the list of pending removals.  We
     * do this before checking below so that the list
     * of files will always have something on it.
     */
    fxStr& files = pendingDocs[doc];
    if (files.find(0, file) == files.length())		// suppress duplicates
	files.append(file | " ");
    if (tracingLevel & FAXTRACE_DOCREFS)
	logInfo("DOC UNREF: %s files %s",
	    (const char*) file, (const char*) files);
    /*
     * The following assumes that any source document has
     * been renamed to the base document name *before* this
     * routine is invoked (either directly or via a msg
     * received on a FIFO).  Specifically, if the stat
     * call fails we assume the file does not exist and
     * that it is safe to remove the imaged documents.
     * This is conservative and if wrong will not break
     * anything; just potentially cause extra imaging
     * work to be done.
     */
    struct stat sb;
    if (Sys::stat(doc, sb) < 0 || sb.st_nlink == 1) {
	if (tracingLevel & FAXTRACE_DOCREFS)
	    logInfo("DOC UNREF: expunge imaged files");
	/*
	 * There are no additional references to the
	 * original source document (all references
	 * should be from completed jobs that reference
	 * the original source document by its basename).
	 * Expunge imaged documents that were waiting for
	 * all potential uses to complete.
	 */
	l = 0;
	do {
	    (void) Sys::unlink(files.token(l, ' '));
	} while (l < files.length());
	pendingDocs.remove(doc);
    }
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
     * Note that by this time we believe the modem is capable
     * of certain requirements needed to transmit the document
     * (based on the capabilities passed to us by faxgetty).
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
     * o the modem is capable of sending 2D-encoded data,
     * o the remote side is known to be capable of it, and
     * o the user hasn't specified a desire to send 1D data.
     */
    if (req.desireddf > DF_1DMR) {
	params.df = (use2D && job.modem->supports2D() &&
	    info.getCalledBefore() && info.getSupports2DEncoding()) ?
		DF_2DMR : DF_1DMR;
    } else
	params.df = DF_1DMR;
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
	case FaxRequest::send_pcl:		// convert PCL
	case FaxRequest::send_tiff:		// verify&possibly convert TIFF
	    tmp = FaxRequest::mkbasedoc(freq.item) | ";" | params.encodePage();
	    status = convertDocument(job, freq, tmp, params, dci, req.notice);
	    if (status == Job::done) {
		/*
		 * Insert converted file into list and mark the
		 * original document so that it's saved, but
		 * not processed again.  The converted file
		 * is sent, while the saved file is kept around
		 * in case it needs to be returned to the sender.
		 */
		freq.op++;			// NB: assumes order of enum
		req.insertFax(i+1, tmp);
	    } else
		Sys::unlink(tmp);		// bail out
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
	    makeCoverPage(job, req, params, dci);
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
    /*
     * Figure out whether to try chopping off white space
     * from the bottom of pages.  This can only be done
     * if the remote device is thought to be capable of
     * accepting variable-length pages.
     */
    u_int pagechop;
    if (info.getMaxPageLengthInMM() == -1) {
	pagechop = req.pagechop;
	if (pagechop == FaxRequest::chop_default)
	    pagechop = pageChop;
    } else
	pagechop = FaxRequest::chop_none;
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
    req.totpages = req.npages;		// count pages previously transmitted
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
		emsg = "Can not open document file " | freq.item;
		goto bad;
	    }
	    if (freq.dirnum != 0 && !TIFFSetDirectory(tif, freq.dirnum)) {
		emsg = fxStr::format(
		    "Can not set directory %u in document file %s"
		    , freq.dirnum
		    , (const char*) freq.item
		);
		goto bad;
	    }
	    i++;			// advance for next find
	} else {
	    /*
	     * Read the next TIFF directory.
	     */
	    if (!TIFFReadDirectory(tif)) {
		emsg = fxStr::format(
		    "Error reading directory %u in document file %s"
		    , TIFFCurrentDirectory(tif)
		    , TIFFFileName(tif)
		);
		goto bad;
	    }
	}
	if (++req.totpages > maxPages) {
	    emsg = fxStr::format("Too many pages in submission; max %u",
		maxPages);
	    goto bad;
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
	req.pagehandling.append(next.encodePage());
	/*
	 * If page is to be chopped (i.e. sent with trailing white
	 * space removed so the received page uses minimal paper),
	 * scan the data and, if possible, record the amount of data
	 * that should not be sent.  The modem drivers will use this
	 * information during transmission if it's actually possible
	 * to do the chop (based on the negotiated session parameters).
	 */
	if (pagechop == FaxRequest::chop_all ||
	  (pagechop == FaxRequest::chop_last && TIFFLastDirectory(tif)))
	    preparePageChop(req, tif, next, req.pagehandling);
	params = next;
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
 * Page Chopping Support.
 */
class MemoryDecoder : public G3Decoder {
private:
    u_long	  cc;
    const u_char* bp;
    const u_char* endOfPage;
    u_int	  nblanks;

    int decodeNextByte();
public:
    MemoryDecoder(const u_char* data, u_long cc);
    ~MemoryDecoder();
    const u_char* current()				{ return bp; }

    void scanPageForBlanks(u_int fillorder, const Class2Params& params);

    const u_char* getEndOfPage()			{ return endOfPage; }
    u_int getLastBlanks()				{ return nblanks; }
};
MemoryDecoder::MemoryDecoder(const u_char* data, u_long n)
{
    bp = data;
    cc = n;
    endOfPage = NULL;
    nblanks = 0;
}
MemoryDecoder::~MemoryDecoder()				{}

int
MemoryDecoder::decodeNextByte()
{
    if (cc == 0)
	raiseRTC();			// XXX don't need to recognize EOF
    cc--;
    return (*bp++);
}

static fxBool
isBlank(uint16* runs, u_int rowpixels)
{
    u_int x = 0;
    for (;;) {
	if ((x += *runs++) >= rowpixels)
	    break;
	if (runs[0] != 0)
	    return (FALSE);
	if ((x += *runs++) >= rowpixels)
	    break;
    }
    return (TRUE);
}

void
MemoryDecoder::scanPageForBlanks(u_int fillorder, const Class2Params& params)
{
    setupDecoder(fillorder,  params.is2D());
    u_int rowpixels = params.pageWidth();	// NB: assume rowpixels <= 2432
    uint16 runs[2*2432];			// run arrays for cur+ref rows
    setRuns(runs, runs+2432, rowpixels);

    if (!RTCraised()) {
	/*
	 * Skip a 1" margin at the top of the page before
	 * scanning for trailing white space.  We do this
	 * to insure that there is always enough space on
	 * the page to image a tag line and to satisfy a
	 * fax machine that is incapable of imaging to the
	 * full extent of the page.
	 */
	u_int topMargin = 1*98;			// 1" at 98 lpi
	if (params.vr == VR_FINE)		// 196 lpi =>'s twice as many
	    topMargin *= 2;
	do {
	    (void) decodeRow(NULL, rowpixels);
	} while (--topMargin);
	/*
	 * Scan the remainder of the page data and calculate
	 * the number of blank lines at the bottom.
	 */
	for (;;) {
	    (void) decodeRow(NULL, rowpixels);
	    if (isBlank(lastRuns(), rowpixels)) {
		endOfPage = bp;			// include one blank row
		nblanks = 0;
		do {
		    nblanks++;
		    (void) decodeRow(NULL, rowpixels);
		} while (isBlank(lastRuns(), rowpixels));
	    }
	}
    }
}

void
faxQueueApp::preparePageChop(const FaxRequest& req,
    TIFF* tif, const Class2Params& params, fxStr& pagehandling)
{
    tstrip_t s = TIFFNumberOfStrips(tif)-1;
    uint32* stripbytecount;
    (void) TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripbytecount);
    u_int stripSize = (u_int) stripbytecount[s];
    if (stripSize == 0)
	return;
    u_char* data = new u_char[stripSize];
    if (TIFFReadRawStrip(tif, s, data, stripSize) >= 0) {
	uint16 fillorder;
	TIFFGetFieldDefaulted(tif, TIFFTAG_FILLORDER, &fillorder);

	MemoryDecoder dec(data, stripSize);
	dec.scanPageForBlanks(fillorder, params);

	float threshold = req.chopthreshold;
	if (threshold == -1)
	    threshold = pageChopThreshold;
	u_int minRows = (u_int)
	    ((params.vr == VR_NORMAL ? 98. : 196.) * threshold);
	if (dec.getLastBlanks() > minRows)
	    pagehandling.append(fxStr::format("Z%04x",
		stripSize - (dec.getEndOfPage() - data)));
    }
    delete data;
}

/*
 * Convert a document into a form suitable
 * for transmission to the remote fax machine.
 */
JobStatus
faxQueueApp::convertDocument(Job& job,
    const faxRequest& req,
    const fxStr& outFile,
    const Class2Params& params,
    const DestControlInfo& dci,
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
		if (flock(fd, LOCK_SH) == -1) {
		    status = Job::format_failed;
		    emsg = "Unable to lock shared document file";
		} else
		    status = Job::done;
		(void) Sys::close(fd);		// NB: implicit unlock
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
	    jobError(job, "CONVERT DOCUMENT: " | emsg | ": %m");
    } else {
	(void) flock(fd, LOCK_EX);		// XXX check for errors?
	/*
	 * Imaged document does not exist, run the document converter
	 * to generate it.  The converter is invoked according to:
	 *   -o file		output (temp) file
	 *   -r <res>		output resolution (dpi)
	 *   -w <pagewidth>	output page width (pixels)
	 *   -l <pagelength>	output page length (mm)
	 *   -m <maxpages>	max pages to generate
	 *   -1|-2		1d or 2d encoding
	 */
	char rbuf[20]; sprintf(rbuf, "%u", params.verticalRes());
	char wbuf[20]; sprintf(wbuf, "%u", params.pageWidth());
	char lbuf[20]; sprintf(lbuf, "%d", params.pageLength());
	char mbuf[20]; sprintf(mbuf, "%u", dci.getMaxSendPages());
	const char* argv[30];
	int ac = 0;
	switch (req.op) {
	case FaxRequest::send_postscript: argv[ac++] = ps2faxCmd; break;
	case FaxRequest::send_pcl:	  argv[ac++] = pcl2faxCmd; break;
	case FaxRequest::send_tiff:	  argv[ac++] = tiff2faxCmd; break;
	}
	argv[ac++] = "-o"; argv[ac++] = outFile;
	argv[ac++] = "-r"; argv[ac++] = rbuf;
	argv[ac++] = "-w"; argv[ac++] = wbuf;
	argv[ac++] = "-l"; argv[ac++] = lbuf;
	argv[ac++] = "-m"; argv[ac++] = mbuf;
	argv[ac++] = params.df == DF_1DMR ? "-1" : "-2";
	argv[ac++] = req.item;
	argv[ac] = NULL;
	// XXX the (char* const*) is a hack to force type compatibility
	status = runConverter(job, argv[0], (char* const*) argv, emsg);
	if (status == Job::done) {
	    /*
	     * Many converters exit with zero status even when
	     * there are problems so scan the the generated TIFF
	     * to verify the integrity of the converted data.
	     *
	     * NB: We must reopen the file instead of using the
	     *     previously opened file descriptor in case the
	     *     converter creates a new file with the given
	     *     output filename instead of just overwriting the
	     *     file created above.  This can easily happen if,
	     *     for example, the converter creates a link from
	     *     the input file to the target (e.g. tiff2fax
	     *     does this when no conversion is required).
	     */
	    TIFF* tif = TIFFOpen(outFile, "r");
	    if (tif) {
		while (!TIFFLastDirectory(tif))
		    if (!TIFFReadDirectory(tif)) {
			status = Job::format_failed;
			emsg = "Converted document is not valid TIFF";
			break;
		    }
		TIFFClose(tif);
	    } else {
		status = Job::format_failed;
		emsg = "Could not reopen converted document to verify format";
	    }
	    if (status == Job::done)	// discard any debugging output
		emsg = "";
	    else
		jobError(job, "CONVERT DOCUMENT: " | emsg);
	} else if (status == Job::rejected)
	    jobError(job, "SEND REJECT: " | emsg);
	(void) Sys::close(fd);		// NB: implicit unlock
    }
    return (status);
}

static void
closeAllBut(int fd)
{
    for (int f = Sys::getOpenMax()-1; f >= 0; f--)
	if (f != fd)
	    Sys::close(f);
}

/*
 * Startup a document converter program in a subprocess
 * with the output returned through a pipe.  We could just use
 * popen or similar here, but we want to detect fork failure
 * separately from others so that jobs can be requeued instead
 * of rejected.
 */
JobStatus
faxQueueApp::runConverter(Job& job, const char* app, char* const* argv, fxStr& emsg)
{
    fxStr cmdline(argv[0]);
    for (u_int i = 1; argv[i] != NULL; i++)
	cmdline.append(fxStr::format(" %s", argv[i]));
    traceQueue(job, "CONVERT DOCUMENT: " | cmdline);
    JobStatus status;
    int pfd[2];
    if (pipe(pfd) >= 0) {
	pid_t pid = fork();
	switch (pid) {
	case -1:			// error
	    jobError(job, "CONVERT DOCUMENT: fork: %m");
	    status = Job::requeued;	// job should be retried
	    Sys::close(pfd[1]);
	    break;
	case 0:				// child, exec command
	    if (pfd[1] != STDOUT_FILENO)
		dup2(pfd[1], STDOUT_FILENO);
	    closeAllBut(STDOUT_FILENO);
	    dup2(STDOUT_FILENO, STDERR_FILENO);
	    Sys::execv(app, argv);
	    sleep(3);			// XXX give parent time to catch signal
	    _exit(255);
	    /*NOTREACHED*/
	default:			// parent, read from pipe and wait
	    Sys::close(pfd[1]);
	    if (runConverter1(job, pfd[0], emsg)) {
		int estat = -1;
		(void) Sys::waitpid(pid, estat);
		if (estat)
		    jobError(job, "CONVERT DOCUMENT: exit status %#x", estat);
		switch (estat) {
		case 0:			 status = Job::done; break;
	        case (254<<8):		 status = Job::rejected; break;
		case (255<<8): case 255: status = Job::no_formatter; break;
		default:		 status = Job::format_failed; break;
		}
	    } else {
		kill(pid, SIGTERM);
		(void) Sys::waitpid(pid);
		status = Job::format_failed;
	    }
	    break;
	}
	Sys::close(pfd[0]);
    } else {
	jobError(job, "CONVERT DOCUMENT: pipe: %m");
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
faxQueueApp::runConverter1(Job& job, int fd, fxStr& output)
{
    int n;
    Timeout timer;
    timer.startTimeout(postscriptTimeout*1000);
    char buf[1024];
    while ((n = Sys::read(fd, buf, sizeof (buf))) > 0 && !timer.wasTimeout()) {
	cleanse(buf, n);
	output.append(buf, n);
    }
    timer.stopTimeout();
    if (timer.wasTimeout()) {
	jobError(job, "CONVERT DOCUMENT: job time limit exceeded");
	output.append("\n[Job time limit exceeded]\n");
	return (FALSE);
    } else
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
faxQueueApp::makeCoverPage(Job& job, FaxRequest& req, const Class2Params& params, const DestControlInfo& dci)
{
    faxRequest freq(FaxRequest::send_postscript, 0, fxStr::null, req.cover);
    fxStr cmd(coverCmd
	| " " | req.qfile
	| " " | contCoverPageTemplate
	| " " | freq.item
    );
    traceQueue(job, "COVER PAGE: " | cmd);
    if (runCmd(cmd, TRUE)) {
	fxStr emsg;
	fxStr tmp = freq.item | ";" | params.encodePage();
	if (convertDocument(job, freq, tmp, params, dci, emsg)) {
	    req.insertFax(0, tmp);
	    req.cover = tmp;			// needed in sendJobDone
	    req.pagehandling = "";		// XXX force recalculation
	} else {
	    jobError(job, "SEND: No continuation cover page, "
		" document conversion failed: %s", (const char*) emsg);
	}
	Sys::unlink(freq.item);
    } else {
	jobError(job,
	    "SEND: No continuation cover page, generation cmd failed");
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

/*
 * Setup the argument vector and exec a subprocess.
 * This code assumes the command and dargs strings have
 * previously been processed to insert \0 characters
 * between each argument string (see crackArgv below).
 */
static void
doexec(const char* cmd, const fxStr& dargs, const char* devid, const char* file)
{
#define	MAXARGS	128
    const char* av[MAXARGS];
    int ac = 0;
    const char* cp = strrchr(cmd, '/');
    // NB: can't use ?: 'cuz of AIX compiler (XXX)
    if (cp)
	av[ac++] = cp+1;			// program name
    else
	av[ac++] = cmd;
    cp = strchr(cmd,'\0');
    const char* ep = strchr(cmd, '\0');
    while (cp < ep && ac < MAXARGS-4) {		// additional pre-split args
	av[ac++] = ++cp;
	cp = strchr(cp,'\0');
    }
    cp = dargs;
    ep = cp + dargs.length();
    while (cp < ep && ac < MAXARGS-4) {		// pre-split dargs
	av[ac++] = cp;
	cp = strchr(cp,'\0')+1;
    }
    av[ac++] = "-m"; av[ac++] = devid;
    av[ac++] = file;
    av[ac] = NULL;
    Sys::execv(cmd, (char* const*) av);
}
#undef MAXARGS

static void
join(fxStr& s, const fxStr& a)
{
    const char* cp = a;
    const char* ep = cp + a.length();
    while (cp < ep) {
	s.append(' ');
	s.append(cp);
	cp = strchr(cp,'\0')+1;
    }
}

static fxStr
joinargs(const fxStr& cmd, const fxStr& dargs)
{
    fxStr s;
    join(s, cmd);
    join(s, dargs);
    return s;
}

void
faxQueueApp::sendJobStart(Job& job, FaxRequest* req, const DestControlInfo& dci)
{
    job.start = Sys::now();		// start of transmission
    // XXX start deadman timeout on active job
    const fxStr& cmd = pickCmd(*req);
    fxStr dargs(dci.getArgs());
    pid_t pid = fork();
    switch (pid) {
    case 0:				// child, startup command
	closeAllBut(-1);		// NB: close 'em all
	doexec(cmd, dargs, job.modem->getDeviceID(), job.file);
	sleep(10);			// XXX give parent time to catch signal
	_exit(127);
	/*NOTREACHED*/
    case -1:				// fork failed, sleep and retry
	/*
	 * We were unable to start the command because the
	 * system is out of processes.  Take the job off the
	 * active list and requeue it for a future time. 
	 * If it appears that the we're doing this a lot,
	 * then lengthen the backoff.
	 */
	delayJob(job, *req, "Could not fork to start job transmission",
	    job.start + random() % requeueInterval);
	break;
    default:				// parent, setup handler to wait
	traceQueue(job, "CMD START"
	    | joinargs(cmd, dargs)
	    | " -m " | job.modem->getDeviceID()
	    | " "    | job.file
	    | " (PID %lu)"
	    , pid
	);
	job.startSend(pid);
	Trigger::post(Trigger::SEND_BEGIN, job);
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
    Trigger::post(Trigger::SEND_END, job);
    releaseModem(job);				// done with modem
    FaxRequest* req = readRequest(job);		// reread the qfile
    if (!req) {
	logError("JOB %s: SEND FINISHED: %s; but job file vanished",
	    (char*) job.jobid, fmtTime(duration));
	setDead(job);
	return;
    }
    job.commid = req->commid;			// passed from subprocess
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
	Trigger::post(Trigger::SEND_REFORMAT, job);
	u_int i = 0;
	while (i < req->requests.length()) {
	    faxRequest& freq = req->requests[i];
	    if (freq.op == FaxRequest::send_fax) {
		unrefDoc(freq.item);
		req->requests.remove(i);
		continue;
	    }
	    if (freq.isSavedOp())
		freq.op--;			// assumes order of enum
	    i++;
	}
	req->pagehandling = "";			// force recalculation
	req->status = send_retry;		// ... force retry
	req->tts = now;				// ... and do it now
    }
    /*
     * If the job did not finish and it is due to be
     * suspended (possibly followed by termination),
     * then treat it as if it is to be retried in case
     * it does get rescheduled.
     */
    if (req->status != send_done && job.suspendPending) {
	req->notice = "Job interrupted by user";
	req->status = send_retry;
    }
    if (job.killtime == 0 && req->status == send_retry) {
	/*
	 * The job timed out during the send attempt.  We
	 * couldn't do anything then, but now the job can
	 * be cleaned up.  Not sure if the user should be
	 * notified of the requeue as well as the timeout?
	 */
	fxAssert(!job.suspendPending, "Interrupted job timed out");
	job.state = FaxRequest::state_done;
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
	     * Job was previously setup to get a continuation
	     * cover page.  If the generated cover page was not
	     * sent, then delete it so that it'll get recreated.
	     */
	    if (req->requests[0].item == req->cover) {
		Sys::unlink(req->cover);
		req->requests.remove(0);
	    }
	} else if (req->useccover &&
	  req->npages > 0 && contCoverPageTemplate != "") {
	    /*
	     * At least one page was sent so any existing
	     * cover page is certain to be gone.  Setup
	     * to generate a cover page when the job is
	     * retried.  Note that we assume the continuation
	     * cover page will be PostScript (though the
	     * type is not used anywhere just now).
	     */
	    req->cover = docDir | "/cover" | req->jobid | ".ps";
	}
	if (req->tts < now) {
	    /*
	     * Send failed and send app didn't specify a new
	     * tts, bump the ``time to send'' by the requeue
	     * interval, then rewrite the queue file.  This causes
	     * the job to be rescheduled for transmission
	     * at a future time.
	     */
	    req->tts = now + (req->retrytime != 0
		? req->retrytime
		: (requeueInterval>>1) + (random()%requeueInterval));
	}
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
	job.state = (req->tts > now) ?
	    FaxRequest::state_sleeping : FaxRequest::state_ready;
	updateRequest(*req, job);		// update on-disk status
	if (!job.suspendPending) {
	    job.remove();			// remove from active list
	    if (req->tts > now) {
		traceQueue(job, "SEND INCOMPLETE: requeue for "
		    | strTime(req->tts - now)
		    | "; " | req->notice);
		setSleep(job, req->tts);
		Trigger::post(Trigger::SEND_REQUEUE, job);
		if (req->isNotify(FaxRequest::when_requeued))
		    notifySender(job, Job::requeued);
	    } else {
		traceQueue(job, "SEND INCOMPLETE: retry immediately; " |
		    req->notice); 
		setReadyToRun(job);		// NB: job.tts will be <= now
	    }
	} else					// signal waiting co-thread
	    job.suspendPending = FALSE;
	delete req;				// implicit unlock of q file
    } else {
	job.state = FaxRequest::state_done;
	traceQueue(job, "SEND DONE: " | strTime(duration));
	Trigger::post(Trigger::SEND_DONE, job);
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
    job.state = FaxRequest::state_ready;
    traceJob(job, "READY");
    Trigger::post(Trigger::JOB_READY, job);
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
    Trigger::post(Trigger::JOB_SLEEP, job);
    JobIter iter(sleepq);
    for (; iter.notDone() && iter.job().tts <= tts; iter++)
	;
    job.insert(iter.job());
    job.startTTSTimer(tts);
}

#define	isOKToStartJobs(di, dci, n) \
    (di.getActive()+n <= dci.getMaxConcurrentJobs())

/*
 * Process a job that's finished.  The corpse gets placed
 * on the deadq and is reaped the next time the scheduler
 * runs.  If any jobs are blocked waiting for this job to
 * complete, one is made ready to run.
 */
void
faxQueueApp::setDead(Job& job)
{
    job.state = FaxRequest::state_done;
    job.suspendPending = FALSE;
    traceJob(job, "DEAD");
    Trigger::post(Trigger::JOB_DEAD, job);
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
    if (job.isOnList())			// lazy remove from active list
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
    job.state = FaxRequest::state_active;
    traceJob(job, "ACTIVE");
    Trigger::post(Trigger::JOB_ACTIVE, job);
    job.insert(*activeq.next);
}

/*
 * Place a job on the list of jobs not being scheduled.
 */
void
faxQueueApp::setSuspend(Job& job)
{
    job.state = FaxRequest::state_suspended;
    traceJob(job, "SUSPEND");
    Trigger::post(Trigger::JOB_SUSPEND, job);
    job.insert(*suspendq.next);
}

/*
 * Create a new job entry and place them on the
 * appropriate queue.  A kill timer is also setup
 * for the job.
 */
fxBool
faxQueueApp::submitJob(FaxRequest& req, fxBool checkState)
{
    Job* job = new Job(req);
    traceJob(*job, "CREATE");
    Trigger::post(Trigger::JOB_CREATE, *job);
    return (submitJob(*job, req, checkState));
}

fxBool
faxQueueApp::submitJob(Job& job, FaxRequest& req, fxBool checkState)
{
    /*
     * Check various submission parameters.  We setup the
     * canonical version of the destination phone number
     * first so that any rejections that cause the notification
     * script to be run will return a proper value for the
     * destination phone number.
     */
    job.dest = canonicalizePhoneNumber(req.number);
    if (job.dest == "") {
	if (req.external == "")			// NB: for notification logic
	    req.external = req.number;
	rejectSubmission(job, req,
	    "REJECT: Unable to convert dial string to canonical format");
	return (FALSE);
    }
    time_t now = Sys::now();
    if (req.killtime <= now) {
	timeoutJob(job, req);
	return (FALSE);
    }
    if (!Modem::modemExists(req.modem) && !ModemClass::find(req.modem)) {
	rejectSubmission(job, req,
	    "REJECT: Requested modem " | req.modem | " is not registered");
	return (FALSE);
    }
    if (req.requests.length() == 0) {
	rejectSubmission(job, req, "REJECT: No work found in job file");
	return (FALSE);
    }
    if (req.pagewidth > 303) {
	rejectSubmission(job, req,
	    fxStr::format("REJECT: Page width (%u) appears invalid",
		req.pagewidth));
	return (FALSE);
    }
    /*
     * Verify the killtime is ``reasonable''; otherwise
     * select (through the Dispatcher) may be given a
     * crazy time value, potentially causing problems.
     */
    if (req.killtime-now > 365*24*60*60) {	// XXX should be based on tts
	rejectSubmission(job, req,
	    fxStr::format("REJECT: Job expiration time (%u) appears invalid",
		req.killtime));
	return (FALSE);
    }
    if (checkState) {
	/*
	 * Check the state from queue file and if
	 * it indicates the job was not being
	 * scheduled before then don't schedule it
	 * now.  This is used when the scheduler
	 * is restarted and reading the queue for
	 * the first time.
	 *
	 * NB: We reschedule blocked jobs in case
	 *     the job that was previously blocking
	 *     it was removed somehow.
	 */
	switch (req.state) {
	case FaxRequest::state_suspended:
	    setSuspend(job);
	    return (TRUE);
	case FaxRequest::state_done:
	    setDead(job);
	    return (TRUE);
	}
    }
    /*
     * Put the job on the appropriate queue
     * and start the job kill timer.
     */
    if (req.tts > now) {			// scheduled for future
	/*
	 * Check time-to-send as for killtime above.
	 */
	if (req.tts - now > 365*24*60*60) {
	    rejectSubmission(job, req,
		fxStr::format("REJECT: Time-to-send (%u) appears invalid",
		    req.tts));
	    return (FALSE);
	}
	job.startKillTimer(req.killtime);
	job.state = FaxRequest::state_pending;
	setSleep(job, job.tts);
    } else {					// ready to go now
	job.startKillTimer(req.killtime);
	setReadyToRun(job);
    }
    updateRequest(req, job);
    return (TRUE);
}

/*
 * Reject a job submission.
 */
void
faxQueueApp::rejectSubmission(Job& job, FaxRequest& req, const fxStr& reason)
{
    Trigger::post(Trigger::JOB_REJECT, job);
    req.status = send_failed;
    req.notice = reason;
    traceServer("JOB " | job.jobid | ": " | reason);
    deleteRequest(job, req, Job::rejected, TRUE);
    setDead(job);				// dispose of job
}

/*
 * Suspend a job by removing it from whatever
 * queue it's currently on and/or stopping any
 * timers.  If the job has an active subprocess
 * then the process is optionally sent a signal
 * and we wait for the process to stop before
 * returning to the caller.
 */
fxBool
faxQueueApp::suspendJob(Job& job, fxBool abortActive)
{
    if (job.suspendPending)			// already being suspended
	return (FALSE);
    switch (job.state) {
    case FaxRequest::state_active:
	/*
	 * Job is being handled by a subprocess; optionally
	 * signal the process and wait for it to terminate
	 * before returning.  We disable the kill timer so
	 * that if it goes off while we wait for the process
	 * to terminate the process completion work will not
	 * mistakenly terminate the job (see sendJobDone).
	 */
	job.suspendPending = TRUE;		// mark thread waiting
	if (abortActive)
	    (void) kill(job.pid, SIGTERM);	// signal subprocess
	job.stopKillTimer();
	while (job.suspendPending)		// wait for subprocess to exit
	    Dispatcher::instance().dispatch();
	/*
	 * Recheck the job state; it may have changed while
	 * we were waiting for the subprocess to terminate.
	 */
	if (job.state != FaxRequest::state_done)
	    break;
	/* fall thru... */
    case FaxRequest::state_done:
	return (FALSE);
    case FaxRequest::state_sleeping:
    case FaxRequest::state_pending:
	job.stopTTSTimer();			// cancel timeout
	/* fall thru... */
    case FaxRequest::state_suspended:
    case FaxRequest::state_ready:
	break;
    case FaxRequest::state_blocked:
	/*
	 * Decrement the count of job blocked to
	 * to the same destination.
	 */
	destJobs[job.dest].unblock(job);
	break;
    }
    job.remove();				// remove from old queue
    job.stopKillTimer();			// clear kill timer
    return (TRUE);
}

/*
 * Suspend a job and place it on the suspend queue.
 * If the job is currently active then we wait for
 * it to reach a state where it can be safely suspended.
 * This control is used by clients that want to modify
 * the state of a job (i.e. suspend, modify, submit).
 */
fxBool
faxQueueApp::suspendJob(const fxStr& jobid, fxBool abortActive)
{
    Job* job = Job::getJobByID(jobid);
    if (job && suspendJob(*job, abortActive)) {
	setSuspend(*job);
	FaxRequest* req = readRequest(*job);
	if (req) {
	    updateRequest(*req, *job);
	    delete req;
	}
	return (TRUE);
    } else
	return (FALSE);
}

/*
 * Terminate a job in response to a command message.
 * If the job is currently running the subprocess is
 * sent a signal to request that it abort whatever
 * it's doing and we wait for the process to terminate.
 * Otherwise, the job is immediately removed from
 * the appropriate queue and any associated resources
 * are purged.
 */
fxBool
faxQueueApp::terminateJob(const fxStr& jobid, JobStatus why)
{
    Job* job = Job::getJobByID(jobid);
    if (job && suspendJob(*job, TRUE)) {
	job->state = FaxRequest::state_done;
	Trigger::post(Trigger::JOB_KILL, *job);
	FaxRequest* req = readRequest(*job);
	if (req)
	    deleteRequest(*job, req, why, why != Job::removed);
	setDead(*job);
	return (TRUE);
    } else
	return (FALSE);
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
    job.state = FaxRequest::state_done;
    Trigger::post(Trigger::JOB_REJECT, job);
    setDead(job);				// dispose of job
}

/*
 * Deal with a job that's blocked by a concurrent job.
 */
void
faxQueueApp::blockJob(Job& job, FaxRequest& req, const char* mesg)
{
    job.state = FaxRequest::state_blocked;
    req.notice = mesg;
    updateRequest(req, job);
    traceQueue(job, mesg);
    if (req.isNotify(FaxRequest::when_requeued))
	notifySender(job, Job::blocked); 
    Trigger::post(Trigger::JOB_BLOCKED, job);
}

/*
 * Requeue a job that's delayed for some reason.
 */
void
faxQueueApp::delayJob(Job& job, FaxRequest& req, const char* mesg, time_t tts)
{
    job.state = FaxRequest::state_sleeping;
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
    if (req.isNotify(FaxRequest::when_requeued))
	notifySender(job, Job::requeued); 
    Trigger::post(Trigger::JOB_DELAYED, job);
    setSleep(job, tts);
    if (job.modem != NULL)
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
    Trigger::post(Trigger::JOB_TIMEDOUT, job);
    if (job.state != FaxRequest::state_active) {
	job.remove();				// remove from sleep queue
	job.state = FaxRequest::state_done;
	FaxRequest* req = readRequest(job);
	if (req)
	    deleteRequest(job, req, Job::timedout, TRUE);
	setDead(job);
    } else
	job.killtime = 0;			// mark job to be removed
}

/*
 * Like above, but called for a job that times
 * out at the point at which it is submitted (e.g.
 * after the server is restarted).  The work here
 * is subtley different; the q file must not be
 * re-read because it may result in recursive flock
 * calls which on some systems may cause deadlock
 * (systems that emulate flock with lockf do not
 * properly emulate flock).
 */
void
faxQueueApp::timeoutJob(Job& job, FaxRequest& req)
{
    job.state = FaxRequest::state_done;
    traceQueue(job, "KILL TIME EXPIRED");
    Trigger::post(Trigger::JOB_TIMEDOUT, job);
    deleteRequest(job, req, Job::timedout, TRUE);
    setDead(job);
}

/*
 * Resubmit an existing job or create a new job
 * using the specified job description file.
 */
fxBool
faxQueueApp::submitJob(const fxStr& jobid, fxBool checkState)
{
    Job* job = Job::getJobByID(jobid);
    if (job) {
	fxBool ok = FALSE;
	if (job->state == FaxRequest::state_suspended) {
	    job->remove();			// remove from suspend queue
	    FaxRequest* req = readRequest(*job);// XXX need better mechanism
	    if (req) {
		job->update(*req);		// update job state from file
		ok = submitJob(*job, *req);	// resubmit to scheduler
		delete req;			// NB: unlock qfile
	    } else
		setDead(*job);			// XXX???
	} else if (job->state == FaxRequest::state_done)
	    jobError(*job, "Cannot resubmit a completed job");
	else
	    ok = TRUE;				// other, nothing to do
	return (ok);
    }
    /*
     * Create a job from a queue file and add it
     * to the scheduling queues.
     */
    fxStr filename(FAX_SENDDIR "/" FAX_QFILEPREF | jobid);
    if (!Sys::isRegularFile(filename)) {
	logError("JOB %s: qfile %s is not a regular file.",
	    (const char*) jobid, (const char*) filename);
	return (FALSE);
    }
    fxBool status = FALSE;
    int fd = Sys::open(filename, O_RDWR);
    if (fd >= 0) {
	if (flock(fd, LOCK_SH) >= 0) {
	    FaxRequest req(filename, fd);
	    /*
	     * There are four possibilities:
	     *
	     * 1. The queue file was read properly and the job
	     *    can be submitted.
	     * 2. There were problems reading the file, but
	     *    enough information was obtained to purge the
	     *    job from the queue.
	     * 3. The job was previously submitted and completed
	     *    (either with success or failure).
	     * 4. Insufficient information was obtained to purge
	     *    the job; just skip it.
	     */
	    fxBool reject;
	    if (req.readQFile(reject) && !reject &&
	      req.state != FaxRequest::state_done) {
		status = submitJob(req, checkState);
	    } else if (reject) {
		Job job(req);
		job.state = FaxRequest::state_done;
		req.status = send_failed;
		req.notice = "Invalid or corrupted job description file";
		traceServer("JOB " | jobid | ": " | req.notice);
		// NB: this may not work, but we try...
		deleteRequest(job, req, Job::rejected, TRUE);
	    } else if (req.state == FaxRequest::state_done) {
		logError("JOB %s: Cannot resubmit a completed job",
		    (const char*) jobid);
	    } else
		traceServer(filename | ": Unable to purge job, ignoring it");
	} else
	    logError("JOB %s: Could not lock job file; %m.",
		(const char*) jobid);
	Sys::close(fd);
    } else
	logError("JOB %s: Could not open job file; %m.", (const char*) jobid);
    return (status);
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
     * Reread the configuration file if it has been
     * changed.  We do this before each scheduler run
     * since we are a long-running process and it should
     * not be necessary to restart the process to have
     * config file changes take effect.
     */
    (void) updateConfig(configFile);
    /*
     * Scan the job queue and locate a compatible modem to
     * use in processing the job.  Doing things in this order
     * insures the highest priority job is always processed
     * first.
     */
    for (u_int i = 0; i < NQHASH; i++) {
	for (JobIter iter(runqs[i]); iter.notDone(); iter++) {
	    Job& job = iter;
	    fxAssert(job.tts <= Sys::now(), "Sleeping job on run queue");
	    fxAssert(job.modem == NULL, "Job on run queue holding modem");

	    /*
	     * Read the on-disk job state and process the job.
	     * Doing all the processing below each time the job
	     * is considered for processing could be avoided by
	     * doing it only after assigning a modem but that
	     * would potentially cause the order of dispatch
	     * to be significantly different from the order
	     * of submission; something some folks care about.
	     */
	    traceJob(job, "PROCESS");
	    Trigger::post(Trigger::JOB_PROCESS, job);
	    FaxRequest* req = readRequest(job);
	    if (!req) {			// problem reading job state on-disk
		setDead(job);
		continue;
	    }
	    /*
	     * Do per-destination processing and checking.
	     */
	    DestInfo& di = destJobs[job.dest];
	    const DestControlInfo& dci = destCtrls[job.dest];
	    /*
	     * Constrain the maximum number of times the phone
	     * will be dialed and/or the number of attempts that
	     * will be made (and reject jobs accordingly).
	     */
	    u_short maxdials = fxmin((u_short) dci.getMaxDials(),req->maxdials);
	    if (req->totdials >= maxdials) {
		rejectJob(job, *req, fxStr::format(
		    "REJECT: Too many attempts to dial: %u, max %u",
		    req->totdials, maxdials));
		deleteRequest(job, req, Job::rejected, TRUE);
		continue;
	    }
	    u_short maxtries = fxmin((u_short) dci.getMaxTries(),req->maxtries);
	    if (req->tottries >= maxtries) {
		rejectJob(job, *req, fxStr::format(
		    "REJECT: Too many attempts to transmit: %u, max %u",
		    req->tottries, maxtries));
		deleteRequest(job, req, Job::rejected, TRUE);
		continue;
	    }
	    // NB: repeat this check so changes in max pages are applied
	    u_int maxpages = dci.getMaxSendPages();
	    if (req->totpages > maxpages) {
		rejectJob(job, *req, fxStr::format(
		    "REJECT: Too many pages in submission: %u, max %u",
		    req->totpages, maxpages));
		deleteRequest(job, req, Job::rejected, TRUE);
		continue;
	    }
	    if (dci.getRejectNotice() != "") {
		/*
		 * Calls to this destination are being rejected for
		 * a specified reason that we return to the sender.
		 */
		rejectJob(job, *req, "REJECT: " | dci.getRejectNotice());
		deleteRequest(job, req, Job::rejected, TRUE);
		continue;
	    }
	    time_t now = Sys::now();
	    time_t tts;
	    if (!di.isActive(job) && !isOKToStartJobs(di, dci, 1)) {
		/*
		 * This job would exceed the max number of concurrent
		 * jobs that may be sent to this destination.  Put it
		 * on a ``blocked queue'' for the destination; the job
		 * will be made ready to run when one of the existing
		 * jobs terminates.
		 */
		blockJob(job, *req, "Blocked by concurrent jobs");
		job.remove();			// remove from run queue
		di.block(job);			// place at tail of di queue
		delete req;
	    } else if ((tts = dci.nextTimeToSend(now)) != now) {
		/*
		 * This job may not be started now because of time-of-day
		 * restrictions.  Reschedule it for the next possible time.
		 */
		job.remove();			// remove from run queue
		delayJob(job, *req, "Delayed by time-of-day restrictions", tts);
		delete req;
	    } else if (assignModem(job)) {
		job.remove();			// remove from run queue
		/*
		 * We have a modem and have assigned it to the
		 * job.  The job is not on any list; processJob
		 * is responsible for requeing the job according
		 * to the outcome of the work it does (which may
		 * take place asynchronously in a sub-process).
		 * Likewise the release of the assigned modem is
		 * also assumed to take place asynchronously in
		 * the context of the job's processing.
		 */
		processJob(job, req, di, dci);
	    } else				// leave job on run queue
		delete req;
	}
    }
    /*
     * Reap dead jobs.
     */
    for (JobIter iter(deadq); iter.notDone(); iter++) {
	Job* job = iter;
	job->remove();
	traceJob(*job, "DELETE");
	Trigger::post(Trigger::JOB_REAP, *job);
	delete job;
    }
    /*
     * Reclaim resources associated with clients
     * that terminated without telling us.
     */
    HylaClient::purge();		// XXX maybe do this less often
}

/*
 * Attempt to assign a modem to a job.  If we are
 * unsuccessful and it was due to the modem being
 * locked for use by another program then we start
 * a thread to poll for the removal of the lock file;
 * this is necessary for send-only setups where we
 * do not get information about when modems are in
 * use from faxgetty processes.
 */
fxBool
faxQueueApp::assignModem(Job& job)
{
    fxAssert(job.modem == NULL, "Assigning modem to job that already has one");

    fxBool retryModemLookup;
    do {
	retryModemLookup = FALSE;
	Modem* modem = Modem::findModem(job);
	if (modem) {
	    if (modem->assign(job)) {
		Trigger::post(Trigger::MODEM_ASSIGN, *modem);
		return (TRUE);
	    }
	    /*
	     * Modem could not be assigned to job.  The
	     * modem is assumed to be ``removed'' from
	     * the list of potential modems scanned by
	     * findModem so we arrange to re-lookup a
	     * suitable modem for this job.  (a goto would
	     * be fine here but too many C++ compilers
	     * can't handle jumping past the above code...)
	     */
	    traceJob(job, "Unable to assign modem %s (cannot lock)",
		(const char*) modem->getDeviceID());
	    modem->startLockPolling(pollLockWait);
	    traceModem(*modem, "BUSY (begin polling)");
	    retryModemLookup = TRUE;
	} else
	    traceJob(job, "No assignable modem located");
    } while (retryModemLookup);
    return (FALSE);
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
    Trigger::post(Trigger::MODEM_RELEASE, *job.modem);
    job.modem->release();
    job.modem = NULL;			// remove reference to modem
    pokeScheduler();
}

/*
 * Poll to see if a modem's UUCP lock file is still
 * present.  If the lock has been removed then mark
 * the modem ready for use and poke the job scheduler
 * in case jobs were waiting for an available modem.
 * This work is only done when a modem is ``discovered''
 * to be in-use by an outbound process when operating
 * in a send-only environment (i.e. one w/o a faxgetty
 * process monitoring the state of each modem).
 */
void
faxQueueApp::pollForModemLock(Modem& modem)
{
    if (modem.lock->lock()) {
	modem.release();
	traceModem(modem, "READY (end polling)");
	pokeScheduler();
    } else
	modem.startLockPolling(pollLockWait);
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
	if (flock(fd, LOCK_EX) >= 0) {
	    FaxRequest* req = new FaxRequest(job.file, fd);
	    fxBool reject;
	    if (req->readQFile(reject) && !reject) {
		if (req->external == "")
		    req->external = job.dest;
		return (req);
	    }
	    jobError(job, "Could not read job file");
	    delete req;
	} else
	    jobError(job, "Could not lock job file: %m");
	Sys::close(fd);
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
    req.state = job.state;
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
    fxStr dest = FAX_DONEDIR |
	req.qfile.tail(req.qfile.length() - (sizeof (FAX_SENDDIR)-1));
    /*
     * Move completed jobs to the doneq area where
     * they can be retrieved for a period of time;
     * after which they are either removed or archived.
     */
    if (Sys::rename(req.qfile, dest) >= 0) {
	u_int i = 0;
	/*
	 * Remove entries for imaged documents and
	 * delete/rename references to source documents
	 * so the imaged versions can be expunged.
	 */
	while (i < req.requests.length()) {
	    faxRequest& freq = req.requests[i];
	    if (freq.op == FaxRequest::send_fax) {
		req.renameSaved(i);
		unrefDoc(freq.item);
		req.requests.remove(i);
	    } else
		i++;
	}
	req.qfile = dest;			// moved to doneq
	job.file = req.qfile;			// ...and track change
	req.state = FaxRequest::state_done;	// job is definitely done
	req.pri = job.pri;			// just in case someone cares
	req.tts = Sys::now();			// mark job termination time
	req.writeQFile();
	if (force || req.isNotify(FaxRequest::notify_any))
	    notifySender(job, why, duration);
    } else {
	/*
	 * Move failed, probably because there's no
	 * directory.  Treat the job the way we used
	 * to: purge everything.  This avoids filling
	 * the disk with stuff that'll not get removed;
	 * except for a scavenger program.
	 */
	jobError(job, "rename to %s failed: %s",
	    (const char*) dest, strerror(errno));
	if (force || req.isNotify(FaxRequest::notify_any)) {
	    req.writeQFile();
	    notifySender(job, why, duration);
	}
	u_int n = req.requests.length();
	for (u_int i = 0; i < n; i++) {
	    const faxRequest& freq = req.requests[i];
	    switch (freq.op) {
	    case FaxRequest::send_fax:
		unrefDoc(freq.item);
		break;
	    case FaxRequest::send_tiff:
	    case FaxRequest::send_tiff_saved:
	    case FaxRequest::send_postscript:
	    case FaxRequest::send_postscript_saved:
	    case FaxRequest::send_pcl:
	    case FaxRequest::send_pcl_saved:
		Sys::unlink(freq.item);
		break;
	    }
	}
	req.requests.remove(0, n);
	Sys::unlink(req.qfile);
    }
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
    Sys::close(fifo), fifo = -1;
}

int faxQueueApp::inputReady(int fd)		{ return FIFOInput(fd); }

/*
 * Process a message received through a FIFO.
 */
void
faxQueueApp::FIFOMessage(const char* cp)
{
    if (tracingLevel & FAXTRACE_FIFO)
	logInfo("FIFO RECV \"%s\"", cp);
    if (cp[0] == '\0') {
	logError("Bad fifo message \"%s\"", cp);
	return;
    }
    const char* tp = strchr(++cp, ':');
    if (tp)
	FIFOMessage(cp[-1], fxStr(cp,tp-cp), tp+1);
    else
	FIFOMessage(cp[-1], fxStr::null, cp);
}

void
faxQueueApp::FIFOMessage(char cmd, const fxStr& id, const char* args)
{
    fxBool status = FALSE;
    switch (cmd) {
    case '+':				// modem status msg
	FIFOModemMessage(id, args);
	return;
    case '*':				// job status msg from subproc's
	FIFOJobMessage(id, args);
	return;
    case '@':				// receive status msg
	FIFORecvMessage(id, args);
	return;
    case 'Q':				// quit
	traceServer("QUIT");
	quit = TRUE;
	pokeScheduler();
	return;				// NB: no return value expected
    case 'T':				// create new trigger 
	traceServer("TRIGGER %s", args);
	Trigger::create(id, args);
	return;				// NB: trigger id returned specially

    /*
     * The remaining commands generate a response if
     * the client has included a return address.
     */
    case 'C':				// configuration control
	traceServer("CONFIG %s", args);
	status = readConfigItem(args);
	break;
    case 'D':				// cancel an existing trigger
	traceServer("DELETE %s", args);
	status = Trigger::cancel(args);
	break;
    case 'R':				// remove job
	traceServer("REMOVE JOB %s", args);
	status = terminateJob(args, Job::removed);
	break;
    case 'K':				// kill job
	traceServer("KILL JOB %s", args);
	status = terminateJob(args, Job::killed);
	break;
    case 'S':				// submit an outbound job
	traceServer("SUBMIT JOB %s", args);
	if (status = submitJob(args))
	    pokeScheduler();
	break;
    case 'U':				// unreference file
	traceServer("UNREF DOC %s", args);
	unrefDoc(args);
	status = TRUE;
	break;
    case 'X':				// suspend job
	traceServer("SUSPEND JOB %s", args);
	if (status = suspendJob(args, FALSE))
	    pokeScheduler();
	break;
    case 'Y':				// interrupt job
	traceServer("INTERRUPT JOB %s", args);
	if (status = suspendJob(args, TRUE))
	    pokeScheduler();
	break;
    case 'N':				// noop
	status = TRUE;
	break;
    default:
	logError("Bad FIFO cmd '%c' from client %s", cmd, (const char*) id);
	break;
    }
    if (id != fxStr::null) {
	char msg[3];
	msg[0] = cmd;
	msg[1] = (status ? '*' : '!');
	msg[2] = '\0';
	if (tracingLevel & FAXTRACE_FIFO)
	    logInfo("FIFO SEND %s msg \"%s\"", (const char*) id, msg);
	HylaClient::getClient(id).send(msg, sizeof (msg));
    }
}

void
faxQueueApp::notifyModemWedged(Modem& modem)
{
    fxStr dev(idToDev(modem.getDeviceID()));
    logError("MODEM " | dev | " appears to be wedged");
    fxStr cmd(wedgedCmd
	| quote |  modem.getDeviceID() | enquote
	| quote |                  dev | enquote
    );
    traceServer("MODEM WEDGED: %s", (const char*) cmd);
    runCmd(cmd, TRUE);
}

void
faxQueueApp::FIFOModemMessage(const fxStr& devid, const char* msg)
{
    Modem& modem = Modem::getModemByID(devid);
    switch (msg[0]) {
    case 'R':			// modem ready, parse capabilities
	modem.stopLockPolling();
	if (msg[1] != '\0') {
	    modem.setCapabilities(msg+1);	// NB: also sets modem READY
	    traceModem(modem, "READY, capabilities %s", msg+1);
	} else {
	    modem.setState(Modem::READY);
	    traceModem(modem, "READY (no capabilities)");
	}
	Trigger::post(Trigger::MODEM_READY, modem);
	pokeScheduler();
	break;
    case 'B':			// modem busy doing something
	modem.stopLockPolling();
	traceModem(modem, "BUSY");
	modem.setState(Modem::BUSY);
	Trigger::post(Trigger::MODEM_BUSY, modem);
	break;
    case 'D':			// modem to be marked down
	modem.stopLockPolling();
	traceModem(modem, "DOWN");
	modem.setState(Modem::DOWN);
	Trigger::post(Trigger::MODEM_DOWN, modem);
	break;
    case 'N':			// modem phone number updated
	traceModem(modem, "NUMBER %s", msg+1);
	modem.setNumber(msg+1);
	break;
    case 'I':			// modem communication ID
	traceModem(modem, "COMID %s", msg+1);
	modem.setCommID(msg+1);
	break;
    case 'W':			// modem appears wedged
	// NB: modem should be marked down in a separate message
	notifyModemWedged(modem);
        Trigger::post(Trigger::MODEM_WEDGED, modem);
	break;
    case 'U':			// modem inuse by outbound job
	modem.stopLockPolling();
	traceModem(modem, "BUSY");
	modem.setState(Modem::BUSY);
	Trigger::post(Trigger::MODEM_INUSE, modem);
	break;
    case 'C':			// caller-ID information
	Trigger::post(Trigger::MODEM_CID, modem, msg+1);
	break;
    case 'd':			// data call begun
	Trigger::post(Trigger::MODEM_DATA_BEGIN, modem);
	break;
    case 'e':			// data call finished
	Trigger::post(Trigger::MODEM_DATA_END, modem);
	break;
    case 'v':			// voice call begun
	Trigger::post(Trigger::MODEM_VOICE_BEGIN, modem);
	break;
    case 'w':			// voice call finished
	Trigger::post(Trigger::MODEM_VOICE_END, modem);
	break;
    default:
	traceServer("FIFO: Bad modem message \"%s\" for modem " | devid, msg);
	break;
    }
}

void
faxQueueApp::FIFOJobMessage(const fxStr& jobid, const char* msg)
{
    Job* jp = Job::getJobByID(jobid);
    if (!jp) {
	traceServer("FIFO: JOB %s not found for msg \"%s\"",
	    (const char*) jobid, msg);
	return;
    }
    switch (msg[0]) {
    case 'c':			// call placed
	Trigger::post(Trigger::SEND_CALL, *jp);
	break;
    case 'C':			// call connected with fax
	Trigger::post(Trigger::SEND_CONNECTED, *jp);
	break;
    case 'd':			// page sent
	Trigger::post(Trigger::SEND_PAGE, *jp, msg+1);
	break;
    case 'D':			// document sent
	{ FaxSendInfo si; si.decode(msg+1); unrefDoc(si.qfile); }
	Trigger::post(Trigger::SEND_DOC, *jp, msg+1);
	break;
    case 'p':			// polled document received
	Trigger::post(Trigger::SEND_POLLRCVD, *jp, msg+1);
	break;
    case 'P':			// polling operation done
	Trigger::post(Trigger::SEND_POLLDONE, *jp, msg+1);
	break;
    default:
	traceServer("FIFO: Unknown job message \"%s\" for job " | jobid, msg);
	break;
    }
}

void
faxQueueApp::FIFORecvMessage(const fxStr& devid, const char* msg)
{
    Modem& modem = Modem::getModemByID(devid);
    switch (msg[0]) {
    case 'B':			// inbound call started
	Trigger::post(Trigger::RECV_BEGIN, modem);
	break;
    case 'E':			// inbound call finished
	Trigger::post(Trigger::RECV_END, modem);
	break;
    case 'S':			// session started (received initial parameters)
	Trigger::post(Trigger::RECV_START, modem, msg+1);
	break;
    case 'P':			// page done
	Trigger::post(Trigger::RECV_PAGE, modem, msg+1);
	break;
    case 'D':			// document done
	Trigger::post(Trigger::RECV_DOC, modem, msg+1);
	break;
    default:
	traceServer("FIFO: Unknown recv message \"%s\" for modem " | devid,msg);
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
{ "pcl2faxcmd",		&faxQueueApp::pcl2faxCmd,	FAX_PCL2FAXCMD },
{ "tiff2faxcmd",	&faxQueueApp::tiff2faxCmd,	FAX_TIFF2FAXCMD },
{ "sendfaxcmd",		&faxQueueApp::sendFaxCmd,
   FAX_LIBEXEC "/faxsend" },
{ "sendpagecmd",	&faxQueueApp::sendPageCmd,
   FAX_LIBEXEC "/pagesend" },
{ "senduucpcmd",	&faxQueueApp::sendUUCPCmd,
   FAX_LIBEXEC "/uucpsend" },
{ "wedgedcmd",		&faxQueueApp::wedgedCmd,	FAX_WEDGEDCMD },
};
const faxQueueApp::numbertag faxQueueApp::numbers[] = {
{ "tracingmask",	&faxQueueApp::tracingMask,	// NB: must be first
   FAXTRACE_MODEMIO|FAXTRACE_TIMEOUTS },
{ "servertracing",	&faxQueueApp::tracingLevel,	FAXTRACE_SERVER },
{ "uucplocktimeout",	&faxQueueApp::uucpLockTimeout,	0 },
{ "postscripttimeout",	&faxQueueApp::postscriptTimeout, 3*60 },
{ "maxconcurrentjobs",	&faxQueueApp::maxConcurrentJobs, 1 },
{ "maxsendpages",	&faxQueueApp::maxSendPages,	(u_int) -1 },
{ "maxtries",		&faxQueueApp::maxTries,		(u_int) FAX_RETRIES },
{ "maxdials",		&faxQueueApp::maxDials,		(u_int) FAX_REDIALS },
{ "jobreqother",	&faxQueueApp::requeueInterval,	FAX_REQUEUE },
{ "polllockwait",	&faxQueueApp::pollLockWait,	30 },
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
    ModemClass::reset();		// clear+add ``any modem'' class
    ModemClass::set(MODEM_ANY, new RegEx(".*"));
    pageChop = FaxRequest::chop_last;
    pageChopThreshold = 3.0;		// minimum of 3" of white space
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

static void
crackArgv(fxStr& s)
{
    char* cp = s;
    u_int l = s.length()+1;		// +1 for \0
    do {
	while (*cp && !isspace(*cp))
	    cp++;
	if (*cp == '\0')
	    break;
	*cp++ = '\0';
	char* tp = cp;
	while (isspace(*tp))
	    tp++;
	if (tp > cp)
	    memcpy(cp, tp, l-(tp - (char*) s));
    } while (*cp != '\0');
    s.resize(cp - (char*) s);
}

static void
tiffErrorHandler(const char* module, const char* fmt0, va_list ap)
{
    char fmt[128];
    if (module != NULL)
	sprintf(fmt, "%s: Warning, %s.", module, fmt0);
    else
	sprintf(fmt, "Warning, %s.", fmt0);
    vlogError(fmt, ap);
}

static void
tiffWarningHandler(const char* module, const char* fmt0, va_list ap)
{
    char fmt[128];
    if (module != NULL)
	sprintf(fmt, "%s: Warning, %s.", module, fmt0);
    else
	sprintf(fmt, "Warning, %s.", fmt0);
    vlogWarning(fmt, ap);
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
	if (ix >= 8)
	    crackArgv((*this).*strings[ix].p);
    } else if (findTag(tag, (const tags*) numbers, N(numbers), ix)) {
	(*this).*numbers[ix].p = getNumber(value);
	switch (ix) {
	case 1:
	    tracingLevel &= ~tracingMask;
	    if (dialRules)
		dialRules->setVerbose((tracingLevel&FAXTRACE_DIALRULES) != 0);
	    if (tracingLevel&FAXTRACE_TIFF) {
		TIFFSetErrorHandler(tiffErrorHandler);
		TIFFSetWarningHandler(tiffWarningHandler);
	    } else {
		TIFFSetErrorHandler(NULL);
		TIFFSetWarningHandler(NULL);
	    }
	    break;
	case 2: UUCPLock::setLockTimeout(uucpLockTimeout); break;
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
	uucpLockMode = (mode_t) strtol(value, 0, 8);
    else if (streq(tag, "modemclass")) {
	const char* cp;
	for (cp = value; *cp && *cp != ':'; cp++)
	    ;
	fxStr name(value, cp-value);
	for (cp++; *cp && isspace(*cp); cp++)
	    ;
	if (*cp != '\0') {
	    RegEx* re = new RegEx(cp);
	    if (re->getErrorCode() > REG_NOMATCH) {
		fxStr emsg;
		re->getError(emsg);
		configError("Bad pattern for modem class \"%s\": %s: " | emsg,
		    (const char*) name, re->pattern());
	    } else
		ModemClass::set(name, re);
	} else
	    configError("No regular expression for modem class");
    } else if (streq(tag, "pagechop")) {
	if (streq(value, "all"))
	    pageChop = FaxRequest::chop_all;
	else if (streq(value, "none"))
	    pageChop = FaxRequest::chop_none;
	else if (streq(value, "last"))
	    pageChop = FaxRequest::chop_last;
    } else if (streq(tag, "pagechopthreshold"))
	pageChopThreshold = atof(value);
    else
	return (FALSE);
    return (TRUE);
}

/*
 * Subclass DialStringRules so that we can redirect the
 * diagnostic and tracing interfaces through the server.
 */
class MyDialStringRules : public DialStringRules {
private:
    virtual void parseError(const char* fmt ...);
    virtual void traceParse(const char* fmt ...);
    virtual void traceRules(const char* fmt ...);
public:
    MyDialStringRules(const char* filename);
    ~MyDialStringRules();
};
MyDialStringRules::MyDialStringRules(const char* f) : DialStringRules(f) {}
MyDialStringRules::~MyDialStringRules() {}

void
MyDialStringRules::parseError(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlogError(fmt, ap);
    va_end(ap);
}
void
MyDialStringRules::traceParse(const char* fmt ...)
{
    if (faxQueueApp::instance().getTracingLevel() & FAXTRACE_DIALRULES) {
	va_list ap;
	va_start(ap, fmt);
	vlogInfo(fmt, ap);
	va_end(ap);
    }
}
void
MyDialStringRules::traceRules(const char* fmt ...)
{
    if (faxQueueApp::instance().getTracingLevel() & FAXTRACE_DIALRULES) {
	va_list ap;
	va_start(ap, fmt);
	vlogInfo(fmt, ap);
	va_end(ap);
    }
}

void
faxQueueApp::setDialRules(const char* name)
{
    delete dialRules;
    dialRules = new MyDialStringRules(name);
    dialRules->setVerbose((tracingLevel & FAXTRACE_DIALRULES) != 0);
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
    { return tracingLevel; }
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
faxQueueApp::notifySender(Job& job, JobStatus why, const char* duration)
{
    fxStr cmd(notifyCmd
	| quote |		 job.file | enquote
	| quote | Job::jobStatusName(why) | enquote
	| quote |		 duration | enquote
    );
    if (why == Job::requeued) {
	/*
	 * It's too hard to do localtime in an awk script,
	 * so if we may need it, we calculate it here
	 * and pass the result as an optional argument.
	 */
	char buf[30];
	strftime(buf, sizeof (buf), " \"%H:%M\"", localtime(&job.tts));
	cmd.append(buf);
    }
    traceServer("NOTIFY: %s", (const char*) cmd);
    runCmd(cmd, TRUE);
}

void
faxQueueApp::vtraceServer(const char* fmt, va_list ap)
{
    if (tracingLevel & FAXTRACE_SERVER)
	vlogInfo(fmt, ap);
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
    static const char* stateNames[] = {
        "state#0", "suspended", "pending", "sleeping", "blocked",
	"ready", "active", "done"
    };
    time_t now = Sys::now();
    vlogInfo(
	  "JOB " | job.jobid
	| " (" | stateNames[job.state&7]
	| " dest " | job.dest
	| fxStr::format(" pri %u", job.pri)
	| " tts " | strTime(job.tts - now)
	| " killtime " | strTime(job.killtime - now)
	| "): "
	| fmt, ap);
}

void
faxQueueApp::traceQueue(const Job& job, const char* fmt ...)
{
    if (tracingLevel & FAXTRACE_QUEUEMGMT) {
	va_list ap;
	va_start(ap, fmt);
	vtraceJob(job, fmt, ap);
	va_end(ap);
    }
}

void
faxQueueApp::traceJob(const Job& job, const char* fmt ...)
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
faxQueueApp::traceModem(const Modem& modem, const char* fmt ...)
{
    if (tracingLevel & FAXTRACE_MODEMSTATE) {
	va_list ap;
	va_start(ap, fmt);
	vlogInfo("MODEM " | modem.getDeviceID() | ": " | fmt, ap);
	va_end(ap);
    }
}

void
faxQueueApp::jobError(const Job& job, const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlogError("JOB " | job.jobid | ": " | fmt, ap);
    va_end(ap);
}

static void
usage(const char* appName)
{
    faxApp::fatal("usage: %s [-q queue-directory] [-D]", appName);
}

static void
sigCleanup(int)
{
    faxQueueApp::instance().close();
    _exit(-1);
}

int
main(int argc, char** argv)
{
    faxApp::setupLogging("FaxQueuer");

    fxStr appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    faxApp::setupPermissions();

    faxApp::setOpts("q:D");

    fxBool detach = TRUE;
    fxStr queueDir(FAX_SPOOLDIR);
    for (GetoptIter iter(argc, argv, faxApp::getOpts()); iter.notDone(); iter++)
	switch (iter.option()) {
	case 'q': queueDir = iter.optArg(); break;
	case 'D': detach = FALSE; break;
	case '?': usage(appName);
	}
    if (Sys::chdir(queueDir) < 0)
	faxApp::fatal(queueDir | ": Can not change directory");
    if (!Sys::isRegularFile(FAX_ETCDIR "/setup.cache"))
	faxApp::fatal("No " FAX_ETCDIR "/setup.cache file; run faxsetup(1M) first");
    if (detach)
	faxApp::detachFromTTY();

    faxQueueApp* app = new faxQueueApp;

    signal(SIGTERM, fxSIGHANDLER(sigCleanup));
    signal(SIGINT, fxSIGHANDLER(sigCleanup));

    app->initialize(argc, argv);
    app->open();
    while (app->isRunning())
	Dispatcher::instance().dispatch();
    app->close();
    delete app;

    return 0;
}
