/*	$Header: /usr/people/sam/fax/./faxd/RCS/pageSendApp.c++,v 1.22 1995/04/08 21:32:27 sam Rel $ */
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
#include <osfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#include <signal.h>
#include <ctype.h>

#include "FaxMachineInfo.h"
#include "UUCPLock.h"
#include "pageSendApp.h"
#include "FaxRequest.h"
#include "Dispatcher.h"
#include "StackBuffer.h"
#include "Sys.h"
#include "ixo.h"

#include "config.h"

/*
 * Send messages with IXO/TAP protocol.
 */

extern	const char* fmtTime(time_t);

pageSendApp* pageSendApp::_instance = NULL;

pageSendApp::pageSendApp(const fxStr& devName, const fxStr& devID)
    : ModemServer(devName, devID)
{
    ready = FALSE;
    modemLock = NULL;
    setupConfig();

    fxAssert(_instance == NULL, "Cannot create multiple pageSendApp instances");
    _instance = this;
}

pageSendApp::~pageSendApp()
{
    delete modemLock;
}

pageSendApp& pageSendApp::instance() { return *_instance; }

void
pageSendApp::initialize(int argc, char** argv)
{
    ModemServer::initialize(argc, argv);
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
pageSendApp::open()
{
    ModemServer::open();
    faxApp::open();
}

void
pageSendApp::close()
{
    if (isRunning()) {
	if (state == ModemServer::SENDING) {
	    /*
	     * Terminate the active job and let the send
	     * operation complete so that the transfer is
	     * logged and the appropriate exit status is
	     * returned to the caller.
	     */
	    ModemServer::abortSession();
	} else {
	    ModemServer::close();
	    faxApp::close();
	}
    }
}

FaxSendStatus
pageSendApp::send(const char* filename)
{
    int fd = Sys::open(filename, O_RDWR);
    if (fd >= 0) {
	if (::flock(fd, LOCK_EX) >= 0) {
	    FaxRequest* req = new FaxRequest(filename);
	    fxBool reject;
	    if (req->readQFile(fd, reject) && !reject) {
		if (req->findRequest(FaxRequest::send_page) != fx_invalidArrayIndex) {
		    FaxMachineInfo info;
		    info.updateConfig(canonicalizePhoneNumber(req->number));
		    sendPage(*req, info);
		} else
		    sendFailed(*req, send_failed, "Job has no PIN to send to");
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

void
pageSendApp::sendPage(FaxRequest& req, FaxMachineInfo& info)
{
    if (lockModem()) {
	beginSession(req.number);
	// NB: may need to set baud rate here XXX
	if (setupModem()) {
	    changeState(SENDING);
	    fxStr emsg;
	    setServerStatus("Sending page " | req.jobid);
	    /*
	     * Construct the phone number to dial by applying the
	     * dialing rules to the user-specified dialing string.
	     */
	    fxStr msg;
	    if (prepareMsg(req, info, msg))
		sendPage(req, info, prepareDialString(req.number), msg);
	    changeState(MODEMWAIT);		// ...sort of...
	} else
	    sendFailed(req, send_retry, "Can not setup modem", 4*pollModemWait);
	discardModem(TRUE);
	endSession();
	unlockModem();
    } else {
	sendFailed(req, send_retry, "Can not lock modem device",2*pollLockWait);
    }
}

fxBool
pageSendApp::prepareMsg(FaxRequest& req, FaxMachineInfo& info, fxStr& msg)
{
    u_int i = req.findRequest(FaxRequest::send_data);
    if (i == fx_invalidArrayIndex)		// page w/o text
	return (TRUE);
    int fd = Sys::open(req.requests[i].item, O_RDONLY);
    if (fd < 0) {
	sendFailed(req, send_failed,
	    "Internal error: unable to open text message file");
	return (FALSE);
    }
    struct stat sb;
    (void) Sys::fstat(fd, sb);
    msg.resize(sb.st_size);
    if (Sys::read(fd, &msg[0], sb.st_size) != sb.st_size) {
	sendFailed(req, send_failed,
	    "Internal error: unable to read text message file");
	return (FALSE);
    }
    ::close(fd);

    u_int maxMsgLen = info.getPagerMaxMsgLength();
    if (maxMsgLen == (u_int) -1)		// not set, use default
	maxMsgLen = pagerMaxMsgLength;
    if (msg.length() > maxMsgLen) {
	traceServer("Pager message length %u too large; truncated to %u",
	    msg.length(), maxMsgLen);
	msg.resize(maxMsgLen);
    }
    return (TRUE);
}

void
pageSendApp::sendFailed(FaxRequest& req, FaxSendStatus stat, const char* notice, u_int tts)
{
    req.status = stat;
    req.notice = notice;
    /*
     * When requeued for the default interval (requeueOther),
     * don't adjust the time-to-send field so that the spooler
     * will set it according to the default algorithm that 
     * uses the command-line parameter and a random jitter.
     */
    if (tts != requeueOther)
	req.tts = Sys::now() + tts;
    traceServer("PAGE FAILED: %s", notice);
}

void
pageSendApp::sendPage(FaxRequest& req, FaxMachineInfo& info, const fxStr& number, const fxStr& msg)
{
    if (!getModem()->dataService()) {
	sendFailed(req, send_failed, "Unable to configure modem for data use");
	return;
    }
    req.notice = "";
    abortCall = FALSE;
    fxStr notice;
    time_t pageStart = Sys::now();
    if (pagerSetupCmds != "")			// configure line speed, etc.
	(void) getModem()->atCmd(pagerSetupCmds);
    CallStatus callstat = getModem()->dial(number, notice);
    (void) abortRequested();			// check for user abort
    if (callstat == ClassModem::OK && !abortCall) {
	req.ndials = 0;				// consec. failed dial attempts
	req.tottries++;				// total answered calls
	req.totdials++;				// total attempted calls
	info.setCalledBefore(TRUE);
	info.setDialFailures(0);

	req.status = send_ok;			// be optimistic
	if (pagePrologue(req, info, notice)) {
	    while (req.requests.length() > 0) {	// messages
		u_int i = req.findRequest(FaxRequest::send_page);
		if (i == fx_invalidArrayIndex)
		    break;
		if (req.requests[i].item.length() == 0) {
		    sendFailed(req, send_failed, "No PIN specified");
		    break;
		}
		if (!sendPagerMsg(req, req.requests[i], msg, req.notice)) {
		    /*
		     * On protocol errors retry more quickly
		     * (there's no reason to wait is there?).
		     */
		    if (req.status == send_retry) {
			req.tts = time(0) + requeueProto;
			break;
		    }
		}
		req.removeItems(i);
	    }
	    if (req.status == send_ok)
		(void) pageEpilogue(req, info, notice);
	} else
	    sendFailed(req, req.status, notice, requeueProto);
	if (req.status == send_ok) {
	    time_t now = Sys::now();
	    traceServer("SEND PAGE: FROM " | req.mailaddr
		| " TO " | req.external | " (sent in %s)",
		fmtTime(now - pageStart));
	    info.setSendFailures(0);
	} else {
	    info.setSendFailures(info.getSendFailures()+1);
	    info.setLastSendFailure(req.notice);
	}
    } else if (!abortCall) {
	/*
	 * Analyze the call status codes and selectively decide if the
	 * job should be retried.  We try to avoid the situations where
	 * we might be calling the wrong number so that we don't end up
	 * harrassing someone w/ repeated calls.
	 */
	req.ndials++;
	req.totdials++;			// total attempted calls
	switch (callstat) {
	case ClassModem::NOCARRIER:	// no carrier detected on remote side
	    /*
	     * Since some modems can not distinguish between ``No Carrier''
	     * and ``No Answer'' we offer this configurable hack whereby
	     * we'll retry the job <n> times in the face of ``No Carrier''
	     * dialing errors; if we've never previously reached a modem
	     * at that number.  This should not be used except if
	     * the modem is incapable of distinguishing between
	     * ``No Carrier'' and ``No Answer''.
	     */
	    if (!info.getCalledBefore() && req.ndials > noCarrierRetrys) {
		sendFailed(req, send_failed, notice);
		break;
	    }
	    /* fall thru... */
	case ClassModem::NODIALTONE:	// no local dialtone, possibly unplugged
	case ClassModem::ERROR:		// modem might just need to be reset
	case ClassModem::FAILURE:	// modem returned something unexpected
	case ClassModem::BUSY:		// busy signal
	case ClassModem::NOANSWER:	// no answer or ring back
	    sendFailed(req, send_retry, notice, requeueTTS[callstat]);
	    /* fall thru... */
	case ClassModem::OK:		// call was aborted by user
	    break;
	}
	if (callstat != ClassModem::OK) {
	    info.setDialFailures(info.getDialFailures()+1);
	    info.setLastDialFailure(req.notice);
	}
    }
    if (abortCall)
	sendFailed(req, send_failed, "Job aborted by user");
    else if (req.status == send_retry) {
	if (req.totdials == req.maxdials) {
	    notice = req.notice | "; too many attempts to dial";
	    sendFailed(req, send_failed, notice);
	} else if (req.tottries == req.maxtries) {
	    notice = req.notice | "; too many attempts to send";
	    sendFailed(req, send_failed, notice);
	}
    }
    /*
     * Cleanup after the call.  If we have new information on
     * the client's remote capabilities, the machine info
     * database will be updated when the instance is destroyed.
     */
    getModem()->hangup();
}

u_int
pageSendApp::getResponse(fxStackBuffer& buf, long secs)
{
    buf.reset();
    if (secs) startTimeout(secs*1000);
    for (;;) {
	int c = getModemChar(0);
	if (c == EOF)
	    break;
	if (c == '\r') {
	    if (buf.getLength() > 0)		// discard leading \r's
		break;
	} else if (c != '\n')			// discard all \n's
	    buf.put(c);
    }
    if (secs) stopTimeout("reading line from modem");
    if (buf.getLength() > 0)
	traceIXOCom("-->", (u_char*) (char*) buf, buf.getLength());
    return (buf.getLength());
}

/*
 * Scan through a buffer looking for a potential
 * code byte return in a protocol response.
 * This is needed because some pager services such
 * as PageNet intersperse protocol messages and
 * verbose text messages.
 */
static fxBool
scanForCode(const u_char*& cp, u_int& len)
{
    if (len > 0) {
	do {
	    cp++, len--;
	} while (len > 0 &&
	    *cp != ACK && *cp != NAK && *cp != ESC && *cp != RS);
    }
    return (len > 0);
}

fxBool
pageSendApp::pagePrologue(FaxRequest& req, const FaxMachineInfo& info, fxStr& emsg)
{
    fxStackBuffer buf;
    time_t start;

    /*
     * Send \r and wait for ``ID='' response.
     * Repeat at 2 second intervals until a
     * response is received or ntries have
     * been done.
     */
    traceIXO("EXPECT ID (paging central identification)");
    start = Sys::now();
    fxBool gotID = FALSE;
    do {
	putModem("\r", 1);
	if (getResponse(buf, ixoIDProbe) >= 3) {
	    // skip leading white space
	    for (const char* cp = buf; *cp && isspace(*cp); cp++)
		;
	    gotID = strneq(cp, "ID=", 3);
	}
	if (gotID) {
	    traceIXO("RECV ID (\"%.*s\")",
		buf.getLength(), (const char*) buf);
	} else
	    traceResponse(buf);
    } while (!gotID && Sys::now() - start < ixoIDTimeout);
    if (!gotID) {
	emsg = "No initial ID response from paging central";
	req.status = send_retry;
	return (FALSE);
    }
    flushModemInput();			// paging central may send multiple ID=
    /*
     * Identify use of automatic protocol (as opposed
     * to manual) and proceed with login procedure:
     *
     *    ESC SST<pwd>.
     *
     * ESC means ``automatic dump mode'' protocol.
     * SS  identifies service:
     *    P = Pager ID
     *    G = Message (?)
     * T identifies type of terminal or device sending:
     *    1 = ``category of entry devices using the same protocol''
     *    	  (PETs and IXO)
     *    7,8,9 = ``wild card terminals or devices which may 
     *         relate to a specific users' system''.
     * <pwd> is a 6-character alpha-numeric password
     *    string (optional)
     */
    const fxStr& pass = info.getPagerPassword();
    fxStr prolog("\033" | ixoService | ixoDeviceID);
    if (pass != "")
	prolog.append(pass);
    prolog.append('\r');

    int ntries = ixoLoginRetries;	// retry login up to 3 times
    int unknown = ixoMaxUnknown;	// accept up to 3 unknown messages
    start = Sys::now();
    do {
	traceIXO("SEND device identification/login request");
	putModem((const char*) prolog, prolog.length());
	u_int len = getResponse(buf, ixoLoginTimeout);
	const u_char* cp = buf;
	while (len > 0) {
	    switch (cp[0]) {
	    case ACK:			// login successful, wait for go-ahead
		traceIXO("RECV ACK (login successful)");
		return (pageGoAhead(req, info, emsg));
	    case NAK:			// login failed, retry
		traceIXO("RECV NAK (login unsuccessful)");
		if (--ntries == 0) {
		    emsg = "Login failed multiple times";
		    req.status = send_retry;
		    return (FALSE);
		}
		start = Sys::now();	// restart timer
		/*
		 * NB: we should just goto the top of the loop,
		 *     but old cfront-based compilers aren't
		 *     smart enough to handle goto's that might
		 *     bypass destructors.
		 */
		unknown++;		// counteract loop iteration
		len = 0;		// don't scan forward in buffer
		break;
	    case ESC:
		if (len > 1) {
		    if (cp[1] == EOT) {
			traceIXO("RECV EOT (forced disconnect)");
			emsg =
			    "Paging central responded with forced disconnect";
			req.status = send_failed;
			return (FALSE);
		    }
		    // check for go-ahead message
		    if (len > 2 && cp[1] == '[' && cp[2] == 'p') {
			traceIXO("RECV ACK (login successful & got go-ahead)");
			return (TRUE);
		    }
		}
		break;
	    }
	    if (!scanForCode(cp, len))
		traceResponse(buf);
	}
    } while (Sys::now()-start < ixoLoginTimeout && --unknown);
    emsg = fxStr::format("Protocol failure: %s from paging central",
	(unknown ?
	    "timeout waiting for response" : "too many unknown responses"));
    req.status = send_retry;
    return (FALSE);
}

fxBool
pageSendApp::pageGoAhead(FaxRequest& req, const FaxMachineInfo&, fxStr& emsg)
{
    fxStackBuffer buf;
    time_t start = Sys::now();
    u_int unknown = ixoMaxUnknown;
    do {
	u_int len = getResponse(buf, ixoGATimeout);
	const u_char* cp = buf;
	while (len > 0) {
	    if (len > 2 && cp[0] == ESC && cp[1] == '[' && cp[2] == 'p') {
		traceIXO("RECV go-ahead (prologue done)");
		return (TRUE);
	    }
	    (void) scanForCode(cp, len);
	}
	traceResponse(buf);
    } while (Sys::now()-start < ixoGATimeout && --unknown);
    emsg = fxStr::format("Protocol failure: %s waiting for go-ahead message",
	unknown ? "timeout" : "too many unknown responses");
    req.status = send_retry;
    return (FALSE);
}

/*
 * Calculate packet checksum and append to buffer.
 */
static void
addChecksum(fxStackBuffer& buf)
{
    int sum = 0;
    for (u_int i = 0; i < buf.getLength(); i++)
	sum += buf[i];

    char check[3];
    check[2] = '0' + (sum & 15); sum = sum >> 4;
    check[1] = '0' + (sum & 15); sum = sum >> 4;
    check[0] = '0' + (sum & 15);
    buf.put(check, 3);
}

fxBool
pageSendApp::sendPagerMsg(FaxRequest& req, faxRequest& preq, const fxStr& msg, fxStr& emsg)
{
    /*
     * Build page packet:
     *    STX pin CR line1 CR ... linen CR EEE checksum CR
     * where pin is the destination Pager ID and line<n>
     * are the lines of the message to send.  The trailing
     * EEE depends on whether or not the message is continued
     * on into the next block and/or whether this is the last
     * block in the transaction.
     */
    fxStackBuffer buf;
    buf.put(STX);
    buf.put(preq.item);				// copy PIN to packet
    buf.put('\r');
    buf.put(msg);				// copy text message
    buf.put('\r');
    buf.put(ETX);				// XXX
    addChecksum(buf);				// append packet checksum
    buf.put('\r');

    fxStackBuffer resp;				// paging central response
    u_int ntries = ixoXmitRetries;		// up to 3 xmits of message
    u_int unknown = ixoMaxUnknown;		// up to 3 unknown responses
    time_t start = Sys::now();
    do {
	traceIXO("SEND message block");
	putModem((const char*) buf, buf.getLength());
	u_int len = getResponse(resp, ixoXmitTimeout);
	const u_char* cp = resp;
	while (len > 0) {
	    switch (cp[0]) {
	    case ACK:
		traceIXO("RECV ACK (message block accepted)");
		return (TRUE);
	    case NAK:
		traceIXO("RECV NAK (message block rejected)");
		if (--ntries == 0) {
		    req.status = send_retry;
		    emsg = "Message block not acknowledged by paging central "
			"after multiple tries";
		    return (FALSE);
		}
		start = Sys::now();		// restart timer
		/*
		 * NB: we should just goto the top of the loop,
		 *     but old cfront-based compilers aren't
		 *     smart enough to handle goto's that might
		 *     bypass destructors.
		 */
		unknown++;			// counteract loop iteration
		len = 0;			// don't scan forward
		break;
	    case RS:
		traceIXO("RECV RS (message block rejected; skip to next)");
		/*
		 * This actually means to abandon the current transaction
		 * and proceed to the next.  However we treat it as a
		 * total failure since it's not clear within the present
		 * design whether proceeding to the next transaction is
		 * the right thing to do.
		 */
		req.status = send_failed;
		emsg = "Message block transmit failed; "
		    "paging central rejected it";
		return (FALSE);
	    case ESC:
		if (len > 1 && cp[1] == EOT) {
		    traceIXO("RECV EOT (forced disconnect)");
		    req.status = send_failed;
		    emsg = "Protocol failure: paging central responded to "
			"message block transmit with forced disconnect";
		    return (FALSE);
		}
		break;
	    }
	    if (!scanForCode(cp, len))
		traceResponse(resp);
	}
    } while (Sys::now()-start < ixoXmitTimeout && --unknown);
    emsg = fxStr::format("Protocol failure: %s to message block transmit",
	(unknown ?
	    "timeout waiting for response" : "too many unknown responses"));
    req.status = send_retry;
    return (FALSE);
}

fxBool
pageSendApp::pageEpilogue(FaxRequest& req, const FaxMachineInfo&, fxStr& emsg)
{
    putModem("\4\r", 2);		// EOT then <CR>

    fxStackBuffer buf;
    time_t start = Sys::now();
    do {
	u_int len = getResponse(buf, ixoAckTimeout);
	const u_char* cp = buf;
	while (len > 0) {
	    switch (cp[0]) {
	    case ESC:
		if (len > 1 && cp[1] == EOT) {
		    traceIXO("RECV EOT (disconnect)");
		    return (TRUE);
		}
		break;
	    case RS:
		traceIXO("RECV RS (message content rejected)");
		emsg = "Paging central rejected content; check PIN";
		req.status = send_failed;
		return (FALSE);
	    }
	    (void) scanForCode(cp, len);
	}
	traceResponse(buf);
	// NB: ignore unknown responses
    } while (Sys::now() - start < ixoAckTimeout);
    req.status = send_retry;
    emsg = "Protocol failure: timeout waiting for transaction ACK/NAK "
	"from paging central";
    return (FALSE);
}

void
pageSendApp::traceResponse(const fxStackBuffer& buf)
{
    u_int len = buf.getLength();
    if (len > 0) {
	const char* cp = buf;
	do {
	    if (!isprint(*cp)) {
		traceIXO("RECV unknown paging central response: \"%.*s\"",
		    buf.getLength(), (const char*) buf);
		return;
	    }
	} while (--len);
	/*
	 * No unprintable characters, just log the string w/o
	 * the alarming "Unknown paging central response".
	 */
	traceIXO("%.*s", buf.getLength(), (const char*) buf);
    }
}

void
pageSendApp::traceIXOCom(const char* dir, const u_char* data, u_int cc)
{
    if (log) {
	if ((logTracingLevel& FAXTRACE_IXO) == 0)
	    return;
    } else if ((tracingLevel & FAXTRACE_IXO) == 0)
	return;

    fxStackBuffer buf;
    for (u_int i = 0; i < cc; i++) {
	u_char b = data[i];
	if (!isprint(b)) {
	    const char* octdigits = "01234567";
	    char s[4];
	    s[0] = '\\';
	    s[1] = octdigits[b>>6];
	    s[2] = octdigits[(b>>3)&07];
	    s[3] = octdigits[b&07];
	    buf.put(s, 4);
	} else
	    buf.put(b);
    }
    traceStatus(FAXTRACE_IXO, "%s <%u:%.*s>",
	dir, cc, buf.getLength(), (const char*) buf);
}

void
pageSendApp::traceIXO(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    vtraceStatus(FAXTRACE_PROTOCOL, fmt, ap);
    va_end(ap);
}

fxBool
pageSendApp::putModem(const void* data, int n, long ms)
{
    traceIXOCom("<--",  (const u_char*) data, n);
    return (putModem1(data, n, ms));
}

/*
 * Configuration support.
 */

void
pageSendApp::resetConfig()
{
    ModemServer::resetConfig();
    setupConfig();
}

#define	N(a)	(sizeof (a) / sizeof (a[0]))

const pageSendApp::stringtag pageSendApp::strings[] = {
{ "ixoservice",		&pageSendApp::ixoService,	IXO_SERVICE },
{ "ixodeviceid",	&pageSendApp::ixoDeviceID,	IXO_DEVICEID },
};
const pageSendApp::stringtag pageSendApp::atcmds[] = {
{ "pagersetupcmds",	&pageSendApp::pagerSetupCmds },
};
const pageSendApp::numbertag pageSendApp::numbers[] = {
{ "pagermaxmsglength",	&pageSendApp::pagerMaxMsgLength,128 },
{ "ixomaxunknown",	&pageSendApp::ixoMaxUnknown,	IXO_MAXUNKNOWN },
{ "ixoidprobe",		&pageSendApp::ixoIDProbe,	IXO_IDPROBE },
{ "ixoidtimeout",	&pageSendApp::ixoIDTimeout,	IXO_IDTIMEOUT },
{ "ixologinretries",	&pageSendApp::ixoLoginRetries,	IXO_LOGINRETRIES },
{ "ixologintimeout",	&pageSendApp::ixoLoginTimeout,	IXO_LOGINTIMEOUT },
{ "ixogatimeout",	&pageSendApp::ixoGATimeout,	IXO_GATIMEOUT },
{ "ixoxmitretries",	&pageSendApp::ixoXmitRetries,	IXO_XMITRETRIES },
{ "ixoxmittimeout",	&pageSendApp::ixoXmitTimeout,	IXO_XMITTIMEOUT },
{ "ixoacktimeout",	&pageSendApp::ixoAckTimeout,	IXO_ACKTIMEOUT },
};

void
pageSendApp::setupConfig()
{
    int i;
    for (i = N(strings)-1; i >= 0; i--)
	(*this).*strings[i].p = (strings[i].def ? strings[i].def : "");
    for (i = N(atcmds)-1; i >= 0; i--)
	(*this).*atcmds[i].p = (atcmds[i].def ? atcmds[i].def : "");
    for (i = N(numbers)-1; i >= 0; i--)
	(*this).*numbers[i].p = numbers[i].def;
}

fxBool
pageSendApp::setConfigItem(const char* tag, const char* value)
{
    u_int ix;
    if (findTag(tag, (const tags*)atcmds, N(atcmds), ix)) {
	(*this).*atcmds[ix].p = parseATCmd(value);
    } else if (findTag(tag, (const tags*) strings, N(strings), ix)) {
	(*this).*strings[ix].p = value;
    } else if (findTag(tag, (const tags*)numbers, N(numbers), ix)) {
	(*this).*numbers[ix].p = getNumber(value);
    } else
	return (ModemServer::setConfigItem(tag, value));
    return (TRUE);
}
#undef	N

/*
 * Modem and TTY setup
 */
fxBool 
pageSendApp::setupModem()
{
    return (ModemServer::setupModem() && setParity(EVEN));
}

/*
 * Modem locking support.
 */

fxBool
pageSendApp::lockModem()
{
    return (modemLock ? modemLock->lock() : TRUE);
}

void
pageSendApp::unlockModem()
{
    if (modemLock)
	modemLock->unlock();
}

/*
 * Notification handlers.
 */

void
pageSendApp::notifyModemReady()
{
    ready = TRUE;
}

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
    pageSendApp::instance().close();
    if (!pageSendApp::instance().isRunning())
	::_exit(send_failed);
}

int
main(int argc, char** argv)
{
    faxApp::setupLogging("PageSend");

    fxStr appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    faxApp::setOpts("c:m:t:l");

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

    pageSendApp* app = new pageSendApp(device, devID);

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
