/*	$Id$ */
/*
 * Copyright (c) 1994-1996 Sam Leffler
 * Copyright (c) 1994-1996 Silicon Graphics, Inc.
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
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#include <signal.h>

#include "Sys.h"

#include "Dispatcher.h"

#include "FaxMachineInfo.h"
#include "FaxRecvInfo.h"
#include "FaxSendInfo.h"
#include "FaxAcctInfo.h"
#include "UUCPLock.h"
#include "faxSendApp.h"
#include "config.h"

/*
 * HylaFAX Send Job Agent.
 */

faxSendApp* faxSendApp::_instance = NULL;

faxSendApp::faxSendApp(const fxStr& devName, const fxStr& devID)
    : FaxServer(devName, devID)
{
    ready = false;
    modemLock = NULL;
    setupConfig();

    fxAssert(_instance == NULL, "Cannot create multiple faxSendApp instances");
    _instance = this;
}

faxSendApp::~faxSendApp()
{
    delete modemLock;
}

faxSendApp& faxSendApp::instance() { return *_instance; }

void
faxSendApp::initialize(int argc, char** argv)
{
    FaxServer::initialize(argc, argv);
    faxApp::initialize(argc, argv);

    // NB: must do last to override config file information
    for (GetoptIter iter(argc, argv, getOpts()); iter.notDone(); iter++)
	switch (iter.option()) {
	case 'l':			// do uucp locking
	    modemLock = getUUCPLock(getModemDevice());
	    break;
	case 'c':			// set configuration parameter
	    readConfigItem(iter.optArg());
	    break;
	}
}

void
faxSendApp::open()
{
    FaxServer::open();
    faxApp::open();
}

void
faxSendApp::close()
{
    if (isRunning()) {
	if (state == FaxServer::SENDING) {
	    /*
	     * Terminate the active job and let the send
	     * operation complete so that the transfer is
	     * logged and the appropriate exit status is
	     * returned to the caller.
	     */
	    FaxServer::abortSession();
	} else {
	    FaxServer::close();
	    faxApp::close();
	}
    }
}

FaxSendStatus
faxSendApp::send(const char* filename)
{
    int fd = Sys::open(filename, O_RDWR);
    if (fd >= 0) {
	if (flock(fd, LOCK_EX) >= 0) {
	    FaxRequest* req = new FaxRequest(filename, fd);
	    /*
	     * Handle any session parameters that might be passed
	     * down from faxq (or possibly stuck in the modem config
	     * file).  Except for DesiredBR/EC/ST, these are applied 
	     * before reading the queue file so that any user-specified 
	     * values override.  We don't handle DesiredBR/EC/ST this
	     * way because it would break their usage in DestControls
	     * since there's currently no way for us to determine if
	     * the setting in the queue file is explicitly specified.
	     */
	    bool reject;
	    if (req->readQFile(reject) && !reject) {
		FaxMachineInfo info;
		info.updateConfig(canonicalizePhoneNumber(req->number));
		FaxAcctInfo ai;

		ai.start = Sys::now();

		/*
		 * Force any DesiredBR/EC/ST options in the configuration
		 * files (i.e. DestControls) to take precedence over
		 * any user-specified settings.  This shouldn't cause
		 * too many problems, hopefully, since their usage should
		 * be fairly rare either by configuration settings or by
		 * user-specification.
		 */

		if (desiredBR != (u_int) -1)
		    req->desiredbr = desiredBR;
		if (desiredEC != (u_int) -1)
		    req->desiredec = desiredEC;
		if (desiredST != (u_int) -1)
		    req->desiredst = desiredST;

		FaxServer::sendFax(*req, info, ai);

		ai.duration = Sys::now() - ai.start;
		ai.conntime = getConnectTime();
		ai.commid = req->commid;
		ai.device = getModemDeviceID();
		ai.dest = req->external;
		ai.jobid = req->jobid;
		ai.jobtag = req->jobtag;
		ai.user = req->mailaddr;
		ai.csi = info.getCSI();
		if (req->status == send_done)
		    ai.status = "";
		else
		    ai.status = req->notice;
		if (!ai.record("SEND"))
		    logError("Error writing SEND accounting record, dest=%s",
			(const char*) ai.dest);

		req->writeQFile();		// update on-disk copy
		FaxSendStatus status = req->status;
		delete req;
		return (status);		// return status for exit
	    } else
		delete req;
	    logError("Could not read request file");
	} else
	    logError("Could not lock request file: %m");
	Sys::close(fd);
    } else
	logError("Could not open request file: %m");
    return (send_failed);
}

/*
 * Modem locking support.
 */

bool
faxSendApp::lockModem()
{
    return (modemLock ? modemLock->lock() : true);
}

void
faxSendApp::unlockModem()
{
    if (modemLock)
	modemLock->unlock();
}

/*
 * Notification handlers.
 */

/*
 * Handle notification that the modem device has become
 * available again after a period of being unavailable.
 */
void
faxSendApp::notifyModemReady()
{
    ready = true;
}

/*
 * Handle notification that the modem device looks to
 * be in a state that requires operator intervention.
 */
void
faxSendApp::notifyModemWedged()
{
    if (!sendModemStatus(getModemDeviceID(), "W"))
	logError("MODEM %s appears to be wedged",
	    (const char*) getModemDevice());
    close();
}

void
faxSendApp::notifyCallPlaced(const FaxRequest& req)
{
    sendJobStatus(req.jobid, "c");
    FaxServer::notifyCallPlaced(req);
}

void
faxSendApp::notifyConnected(const FaxRequest& req)
{
    sendJobStatus(req.jobid, "C");
    FaxServer::notifyConnected(req);
}

void
faxSendApp::notifyPageSent(FaxRequest& req, const char* filename)
{
    FaxSendInfo si(filename, req.commid, req.npages+1,
	getPageTransferTime(), getClientParams());
    sendJobStatus(req.jobid, "d%s", (const char*) si.encode());

    FaxServer::notifyPageSent(req, filename);
}

/*
 * Handle notification that a document has been successfully
 * transmitted.  We remove the file from the request array so
 * that it's not resent if the job is requeued.
 */
void
faxSendApp::notifyDocumentSent(FaxRequest& req, u_int fi)
{
    FaxSendInfo si(req.requests[fi].item, req.commid, req.npages,
	getFileTransferTime(), getClientParams());

    FaxServer::notifyDocumentSent(req, fi);

    // NB: there is a racing with the scheduler and we should delay
    //     the FIFO message to scheduler until we renamed the document
    sendJobStatus(req.jobid, "D%s", (const char*) si.encode());
}

/*
 * Handle notification of a document received as a
 * result of a poll request.
 */
void
faxSendApp::notifyPollRecvd(FaxRequest& req, const FaxRecvInfo& ri)
{
    (void) sendJobStatus(req.jobid, "p%s", (const char*) ri.encode());

    FaxServer::notifyPollRecvd(req, ri);

    FaxAcctInfo ai;
    ai.user = req.mailaddr;
    ai.commid = getCommID();
    ai.duration = (time_t) ri.time;
    ai.start = Sys::now() - ai.duration;
    ai.conntime = ai.duration;
    ai.device = getModemDeviceID();
    ai.dest = req.external;
    ai.csi = ri.sender;
    ai.npages = ri.npages;
    ai.params = ri.params.encode();
    ai.status = ri.reason;
    ai.jobid = req.jobid;
    ai.jobtag = req.jobtag;
    if (!ai.record("POLL"))
	logError("Error writing POLL accounting record, dest=%s",
	    (const char*) ai.dest);

    // hand to delivery/notification command
    fxStr cmd(pollRcvdCmd
	 | quote |       req.mailaddr | enquote
	 | quote |           ri.qfile | enquote
	 | quote | getModemDeviceID() | enquote
	 | quote |          ai.commid | enquote
	 | quote |          ri.reason | enquote
     );
    traceServer("RECV POLL: %s", (const char*) cmd);
    setProcessPriority(BASE);			// lower priority
    runCmd(cmd, true);
    setProcessPriority(state);			// restore previous priority
}

/*
 * Handle notification that a poll operation has been
 * successfully completed.  Note that any received
 * documents have already been passed to notifyPollRecvd.
 */
void
faxSendApp::notifyPollDone(FaxRequest& req, u_int pi)
{
    FaxSendInfo si(req.requests[pi].item, req.commid, req.npages,
	getFileTransferTime(), getClientParams());
    sendJobStatus(req.jobid, "P%s", (const char*) si.encode());
    FaxServer::notifyPollDone(req, pi);
}

/*
 * Configuration support.
 */

void
faxSendApp::resetConfig()
{
    FaxServer::resetConfig();
    setupConfig();
}

#define	N(a)	(sizeof (a) / sizeof (a[0]))

const faxSendApp::stringtag faxSendApp::strings[] = {
{ "pollrcvdcmd",	&faxSendApp::pollRcvdCmd,	FAX_POLLRCVDCMD },
};
const faxSendApp::numbertag faxSendApp::numbers[] = {
{ "desiredbr",		&faxSendApp::desiredBR,		(u_int) -1 },
{ "desiredst",		&faxSendApp::desiredST,		(u_int) -1 },
{ "desiredec",		&faxSendApp::desiredEC,		(u_int) -1 },
};

void
faxSendApp::setupConfig()
{
    int i;
    for (i = N(strings)-1; i >= 0; i--)
	(*this).*strings[i].p = (strings[i].def ? strings[i].def : "");
    for (i = N(numbers)-1; i >= 0; i--)
	(*this).*numbers[i].p = numbers[i].def;
}

bool
faxSendApp::setConfigItem(const char* tag, const char* value)
{
    u_int ix;
    if (findTag(tag, (const tags*) strings, N(strings), ix)) {
	(*this).*strings[ix].p = value;
    } else if (findTag(tag, (const tags*) numbers, N(numbers), ix)) {
	(*this).*numbers[ix].p = getNumber(value);
    } else
	return (FaxServer::setConfigItem(tag, value));
    return (true);
}
#undef	N

/*
 * Miscellaneous stuff.
 */

static void
usage(const char* appName)
{
    faxApp::fatal("usage: %s -m deviceID [-t tracelevel] [-l] qfile", appName);
}

static void
sigCleanup(int s)
{
    signal(s, fxSIGHANDLER(sigCleanup));
    logError("CAUGHT SIGNAL %d", s);
    faxSendApp::instance().close();
    if (!faxSendApp::instance().isRunning())
	_exit(send_failed);
}

int
main(int argc, char** argv)
{
    faxApp::setupLogging("FaxSend");

    fxStr appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    faxApp::setOpts("c:m:lpx");			// p+x are for FaxServer

    fxStr devID;
    for (GetoptIter iter(argc, argv, faxApp::getOpts()); iter.notDone(); iter++)
	switch (iter.option()) {
	case 'm': devID = iter.optArg(); break;
	case '?': usage(appName);
	}
    if (devID == "")
	usage(appName);

    faxSendApp* app = new faxSendApp(faxApp::idToDev(devID), devID);

    signal(SIGTERM, fxSIGHANDLER(sigCleanup));
    signal(SIGINT, fxSIGHANDLER(sigCleanup));

    app->initialize(argc, argv);
    app->open();
    while (app->isRunning() && !app->isReady())
	Dispatcher::instance().dispatch();
    FaxSendStatus status;
    if (app->isReady())
	status = app->send(argv[optind]);
    else
	status = send_retry;
    app->close();
    return (status);
}
