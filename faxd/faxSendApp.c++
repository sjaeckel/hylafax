/*	$Header: /usr/people/sam/fax/./faxd/RCS/faxSendApp.c++,v 1.21 1995/04/08 21:31:23 sam Rel $ */
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
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <osfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#include <signal.h>

#include "Sys.h"

#include "Dispatcher.h"

#include "FaxMachineInfo.h"
#include "FaxRecvInfo.h"
#include "FaxAcctInfo.h"
#include "UUCPLock.h"
#include "faxSendApp.h"
#include "config.h"

/*
 * HylaFAX Send Job Agent.
 */

extern	const char* fmtTime(time_t);

faxSendApp* faxSendApp::_instance = NULL;

faxSendApp::faxSendApp(const fxStr& devName, const fxStr& devID)
    : FaxServer(devName, devID)
{
    ready = FALSE;
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
	case 't':			// session tracing level
	    setConfigItem("sessiontracing", iter.optArg());
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
	if (::flock(fd, LOCK_EX) >= 0) {
	    FaxRequest* req = new FaxRequest(filename);
	    fxBool reject;
	    if (req->readQFile(fd, reject) && !reject) {
		FaxMachineInfo info;
		info.updateConfig(canonicalizePhoneNumber(req->number));
		FaxAcctInfo ai;

		ai.start = fileStart = Sys::now();

		FaxServer::sendFax(*req, info, ai);

		ai.duration = Sys::now() - ai.start;
		ai.device = getModemDeviceID();
		ai.dest = req->external;
		ai.jobid = req->jobid;
		ai.user = req->mailaddr;
		ai.csi = info.getCSI();
		if (req->status == send_done)
		    ai.status = "";
		else
		    ai.status = req->notice;
		account("SEND", ai);

		req->writeQFile();		// update on-disk copy
		return (req->status);		// return status for exit
	    } else
		delete req;
	    logError("Could not read request file");
	} else
	    logError("Could not lock request file: %m");
	::close(fd);
    } else
	logError("Could not open request file: %m");
    return (send_failed);
}

/*
 * Modem locking support.
 */

fxBool
faxSendApp::lockModem()
{
    return (modemLock ? modemLock->lock() : TRUE);
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

void
faxSendApp::notifyModemReady()
{
    ready = TRUE;
}

/*
 * Handle notification of a document received as a
 * result of a poll request.
 */
void
faxSendApp::notifyPollRecvd(FaxRequest& req, const FaxRecvInfo& ri)
{
    recordRecv(ri);
    // hand to delivery/notification command
    runCmd(pollRcvdCmd
	 | " " | req.mailaddr
	 | " " | ri.qfile
	 | " " | fxStr(ri.time / 60.,"%.2f")
	 | " " | fxStr((int) ri.sigrate, "%u")
	 | " \"" | ri.protocol | "\""
	 | " \"" | ri.reason | "\""
	 | " " | getModemDeviceID()
	 , TRUE);
}

/*
 * Handle notification that a poll operation has been
 * successfully completed.  Note that any received
 * documents have already been passed to notifyPollRecvd.
 */
void
faxSendApp::notifyPollDone(FaxRequest& req, u_int pi)
{
    time_t now = Sys::now();
    traceServer("POLL FAX: BY %s TO %s completed in %s",
	(char*) req.mailaddr, (char*) req.external, fmtTime(now - fileStart));
    fileStart = now;
    if (req.requests[pi].op == FaxRequest::send_poll) {
	req.removeItems(pi);
	req.writeQFile();
    } else
	logError("notifyPollDone called for non-poll request");
}

/*
 * Handle notification that a document has been successfully
 * transmitted.  We remove the file from the request array so
 * that it's not resent if the job is requeued.
 */
void
faxSendApp::notifyDocumentSent(FaxRequest& req, u_int fi)
{
    time_t now = Sys::now();
    traceServer("SEND FAX: FROM " | req.mailaddr
	| " TO " | req.external | " (%s sent in %s)",
	(char*) req.requests[fi].item, fmtTime(now - fileStart));
    fileStart = now;			// for next file
    if (req.requests[fi].op == FaxRequest::send_fax) {
	u_int n = 1;
	if (fi > 0 && req.requests[fi-1].isSavedOp()) {
	    /*
	     * Document sent was converted from another; delete
	     * the original as well.  (Or perhaps we should hold
	     * onto it to return to sender in case of a problem?)
	     */
	    fi--, n++;
	}
	req.removeItems(fi, n);
	req.writeQFile();
    } else
	logError("notifyDocumentSent called for non-TIFF file");
}

void faxSendApp::notifyRecvDone(const FaxRecvInfo&)	{}

void
faxSendApp::recordRecv(const FaxRecvInfo& ri)
{
    char type[80];
    if (ri.pagelength == 297 || ri.pagelength == (u_int) -1)
	::strcpy(type, "A4");
    else if (ri.pagelength == 364)
	::strcpy(type, "B4");
    else
	::sprintf(type, "(%u x %.2f)", ri.pagewidth, ri.pagelength);
    traceServer("RECV: %s from %s, %d %s pages, %u dpi, %s, %s at %u bps",
	(char*) ri.qfile, (char*) ri.sender,
	ri.npages, type, ri.resolution,
	(char*) ri.protocol, fmtTime((time_t) ri.time), ri.sigrate);

    FaxAcctInfo ai;
    ai.user = "fax";
    ai.duration = (time_t) ri.time;
    ai.start = time(0) - ai.duration;
    ai.device = getModemDeviceID();
    ai.dest = getModemNumber();
    ai.csi = ri.sender;
    ai.npages = ri.npages;
    ai.sigrate = ri.sigrate;
    ai.df = ri.protocol;
    ai.status = ri.reason;
    ai.jobid = "";
    account("RECV", ai);
}

/*
 * Record a transfer in the transfer log file.
 */
void
faxSendApp::account(const char* cmd, const FaxAcctInfo& ai)
{
    if (!ai.record(cmd))
	logError("Problem writing %s accounting record, dest=%s",
	    cmd, (const char*) ai.dest);
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

void
faxSendApp::setupConfig()
{
    for (int i = N(strings)-1; i >= 0; i--)
	(*this).*strings[i].p = (strings[i].def ? strings[i].def : "");
}

fxBool
faxSendApp::setConfigItem(const char* tag, const char* value)
{
    u_int ix;
    if (findTag(tag, (const tags*) strings, N(strings), ix)) {
	(*this).*strings[ix].p = value;
    } else
	return (FaxServer::setConfigItem(tag, value));
    return (TRUE);
}
#undef	N

/*
 * Miscellaneous stuff.
 */

static void
usage(const char* appName)
{
    fxFatal("usage: %s -m deviceID [-t tracelevel] [-l] qfile ...", appName);
}

static void
sigCleanup(int s)
{
    logError("CAUGHT SIGNAL %d", s);
    faxSendApp::instance().close();
    if (!faxSendApp::instance().isRunning())
	::_exit(send_failed);
}

int
main(int argc, char** argv)
{
    faxApp::setupLogging("FaxSend");

    fxStr appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    faxApp::setOpts("c:m:t:lpx");		// p+x are for FaxServer

    fxStr devID;
    for (GetoptIter iter(argc, argv, faxApp::getOpts()); iter.notDone(); iter++)
	switch (iter.option()) {
	case 'm': devID = iter.optArg(); break;
	case '?': usage(appName);
	}
    if (devID == "")
	usage(appName);

    /*
     * Construct the device special file from the device
     * id by converting all '_'s to '/'s and inserting a
     * leading DEV_PREFIX.  The _ to / conversion is done
     * for SVR4 systems which have their devices in
     * subdirectories!
     */
    fxStr device = devID;
    while ((l = device.next(0, '_')) < device.length())
	device[l] = '/';
    device.insert(DEV_PREFIX);

    faxSendApp* app = new faxSendApp(device, devID);

    ::signal(SIGTERM, fxSIGHANDLER(sigCleanup));
    ::signal(SIGINT, fxSIGHANDLER(sigCleanup));

    app->initialize(argc, argv);
    app->open();
    while (app->isRunning() && !app->isReady())
	Dispatcher::instance().dispatch();
    FaxSendStatus status;
    if (app->isReady())
	status = app->send(argv[optind]);
    else
	status = send_failed;
    app->close();
    return (status);
}
