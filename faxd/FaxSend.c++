/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxSend.c++,v 1.142 1995/04/08 21:30:25 sam Rel $ */
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
#include <osfcn.h>

#include "Sys.h"

#include "Dispatcher.h"
#include "tiffio.h"
#include "FaxServer.h"
#include "FaxMachineInfo.h"
#include "FaxRecvInfo.h"
#include "FaxAcctInfo.h"
#include "UUCPLock.h"
#include "t.30.h"
#include "config.h"

/*
 * FAX Server Transmission Protocol.
 */
void
FaxServer::sendFax(FaxRequest& fax, FaxMachineInfo& clientInfo, FaxAcctInfo& ai)
{
    npages = fax.npages;
    if (lockModem()) {
	beginSession(fax.number);
	if (setupModem()) {
	    changeState(SENDING);
	    IOHandler* handler =
		Dispatcher::instance().handler(
		    getModemFd(), Dispatcher::ReadMask);
	    if (handler)
		Dispatcher::instance().unlink(getModemFd());
	    fxStr emsg;
	    setServerStatus("Sending job " | fax.jobid);
	    /*
	     * Construct the phone number to dial by applying the
	     * dialing rules to the user-specified dialing string.
	     */
	    sendFax(fax, clientInfo, prepareDialString(fax.number));
	    /*
	     * Because some modems are impossible to safely hangup in the
	     * event of a problem, we force a close on the device so that
	     * the modem will see DTR go down and (hopefully) clean up any
	     * bad state its in.  We then wait a couple of seconds before
	     * trying to setup the modem again so that it can have some
	     * time to settle.  We want a handle on the modem so that we
	     * can be prepared to answer incoming phone calls.
	     */
	    discardModem(TRUE);
	    changeState(MODEMWAIT, 5);
	} else {
	    sendFailed(fax, send_retry, "Can not setup modem", 4*pollModemWait);
	    discardModem(TRUE);
	    changeState(MODEMWAIT, pollModemWait);
	}
	endSession();
	unlockModem();
    } else {
	if (state != LOCKWAIT)
	    sendFailed(fax, send_retry,
		"Can not lock modem device", 2*pollLockWait);
	if (state != SENDING && state != ANSWERING && state != RECEIVING)
	    changeState(LOCKWAIT, pollLockWait);
    }
    /*
     * Record transmit accounting information for caller.
     */
    ai.npages = npages - fax.npages;
    fax.npages = npages;
    fax.sigrate = clientParams.bitRateName();
    ai.sigrate = atoi(fax.sigrate);
    fax.df = clientParams.dataFormatName();
    ai.df = fax.df;
}

void
FaxServer::sendFailed(FaxRequest& fax, FaxSendStatus stat, const char* notice, u_int tts)
{
    fax.status = stat;
    fax.notice = notice;
    /*
     * When requeued for the default interval (requeueOther),
     * don't adjust the time-to-send field so that the spooler
     * will set it according to the default algorithm that 
     * uses the command-line parameter and a random jitter.
     */
    if (tts != requeueOther)
	fax.tts = Sys::now() + tts;
    traceServer("SEND FAILED: %s", notice);
}

/*
 * Send the specified TIFF files to the FAX
 * agent at the given phone number.
 */
void
FaxServer::sendFax(FaxRequest& fax, FaxMachineInfo& clientInfo, const fxStr& number)
{
    /*
     * Force the modem into the appropriate class
     * used to send facsimile.  We do this before
     * doing any fax-specific operations such as
     * requesting polling.
     */
    if (!modem->faxService()) {
	sendFailed(fax, send_failed, "Unable to configure modem for fax use");
	return;
    }
    /*
     * Check if this job includes a poll request, and
     * if it does, inform the modem in case it needs to
     * do something to get back status about whether or
     * not documents are available for retrieval.
     */
    if (fax.findRequest(FaxRequest::send_poll) != fx_invalidArrayIndex)
	modem->requestToPoll();
    fax.notice = "";
    abortCall = FALSE;
    /*
     * Calculate initial page-related session parameters so
     * that braindead Class 2 modems can constrain the modem
     * before dialing the telephone.
     */
    fxStr notice;
    Class2Params dis;
    dis.decode(fax.pagehandling);
    CallStatus callstat = modem->dialFax(number, dis, notice);
    (void) abortRequested();			// check for user abort
    if (callstat == ClassModem::OK && !abortCall) {
	/*
	 * Call reached a fax machine.  Check remote's
	 * capabilities against those required by the
	 * job and setup for transmission.
	 */
	fax.ndials = 0;				// consec. failed dial attempts
	fax.tottries++;				// total answered calls
	fax.totdials++;				// total attempted calls
	clientInfo.setCalledBefore(TRUE);
	clientInfo.setDialFailures(0);
	modem->sendBegin(fax);
	fxBool remoteHasDoc = FALSE;
	u_int nsf;
	fxStr csi;
	FaxSendStatus status = modem->getPrologue(
	    clientCapabilities, nsf, csi, remoteHasDoc, notice);
	if (status != send_ok) {
	    sendFailed(fax, status, notice, requeueProto);
	} else {
	    clientInfo.setCSI(csi);			// record remote CSI
	    if (!sendClientCapabilitiesOK(clientInfo, nsf, notice)) {
		// NB: mark job completed 'cuz there's no way recover
		sendFailed(fax, send_failed, notice);
	    } else {
		modem->sendSetupPhaseB();
		/*
		 * Group 3 protocol forces any sends to precede any polling.
		 */
		fax.status = send_done;			// be optimistic
		u_int opages = npages;
		while (fax.requests.length() > 0) {	// send operations
		    u_int i = fax.findRequest(FaxRequest::send_fax);
		    if (i == fx_invalidArrayIndex)
			break;
		    faxRequest& freq = fax.requests[i];
		    traceProtocol("SEND file \"%s\"", (char*) freq.item);
		    if (!sendFaxPhaseB(fax, freq, clientInfo)) {
			/*
			 * On protocol errors retry more quickly
			 * (there's no reason to wait is there?).
			 */
			if (fax.status == send_retry ||
			  fax.status == send_reformat)
			    fax.tts = time(0) + requeueProto;
			break;
		    }
		    /*
		     * The file was delivered, notify the server.
		     * Note that a side effect of the notification
		     * is that this file is deleted from the set of
		     * files to send (so that it's not sent again
		     * if the job is requeued).  This is why we call
		     * find again at the top of the loop
		     */
		    notifyDocumentSent(fax, i);
		}
		if (fax.status == send_done && fax.requests.length() > 0)
		    sendPoll(fax, remoteHasDoc);
		fax.totpages -= npages - opages;	// adjust total pages
	    }
	}
	modem->sendEnd();
	if (fax.status != send_done) {
	    clientInfo.setSendFailures(clientInfo.getSendFailures()+1);
	    clientInfo.setLastSendFailure(fax.notice);
	} else
	    clientInfo.setSendFailures(0);
    } else if (!abortCall) {
	/*
	 * Analyze the call status codes and selectively decide if the
	 * job should be retried.  We try to avoid the situations where
	 * we might be calling the wrong number so that we don't end up
	 * harrassing someone w/ repeated calls.
	 */
	fax.ndials++;			// number of consecutive failed calls
	fax.totdials++;			// total attempted calls
	switch (callstat) {
	case ClassModem::NOCARRIER:	// no carrier detected on remote side
	    /*
	     * Since some modems can not distinguish between ``No Carrier''
	     * and ``No Answer'' we offer this configurable hack whereby
	     * we'll retry the job <n> times in the face of ``No Carrier''
	     * dialing errors; if we've never previously reached a facsimile
	     * machine at that number.  This should not be used except if
	     * the modem is incapable of distinguishing betwee ``No Carrier''
	     * and ``No Answer''.
	     */
	    if (!clientInfo.getCalledBefore() && fax.ndials > noCarrierRetrys)
		sendFailed(fax, send_failed, notice);
	    else
		sendFailed(fax, send_retry, notice, requeueTTS[callstat]);
	    break;
	case ClassModem::NODIALTONE:	// no local dialtone, possibly unplugged
	case ClassModem::ERROR:		// modem might just need to be reset
	case ClassModem::FAILURE:	// modem returned something unexpected
	    sendFailed(fax, send_retry, notice, requeueTTS[callstat]);
	    break;
	case ClassModem::NOFCON:	// carrier seen, but handshake failed
	case ClassModem::DATACONN:	// data connection established
	    clientInfo.setCalledBefore(TRUE);
	    /* fall thru... */
	case ClassModem::BUSY:		// busy signal
	case ClassModem::NOANSWER:	// no answer or ring back
	    sendFailed(fax, send_retry, notice, requeueTTS[callstat]);
	    /* fall thru... */
	case ClassModem::OK:		// call was aborted by user
	    break;
	}
	if (callstat != ClassModem::OK) {
	    clientInfo.setDialFailures(clientInfo.getDialFailures()+1);
	    clientInfo.setLastDialFailure(fax.notice);
	}
    }
    if (abortCall)
	sendFailed(fax, send_failed, "Job aborted by user");
    else if (fax.status == send_retry) {
	if (fax.totdials == fax.maxdials) {
	    notice = fax.notice | "; too many attempts to dial";
	    sendFailed(fax, send_failed, notice);
	} else if (fax.tottries == fax.maxtries) {
	    notice = fax.notice | "; too many attempts to send";
	    sendFailed(fax, send_failed, notice);
	}
    }
    /*
     * Cleanup after the call.  If we have new information on
     * the client's remote capabilities, the machine info
     * database will be updated when the instance is destroyed.
     */
    modem->hangup();
}

/*
 * Process a polling request.
 */
void
FaxServer::sendPoll(FaxRequest& fax, fxBool remoteHasDoc)
{
    u_int i = fax.findRequest(FaxRequest::send_poll);
    if (i == fx_invalidArrayIndex) {
	fax.notice = "polling operation not done because of internal failure";
	traceServer("internal muckup, lost polling request");
	// NB: job is marked done
    } else if (!remoteHasDoc) {
	fax.notice = "remote has no document to poll";
	traceServer("REJECT: " | fax.notice);
	// override to force status about polling failure
	if (fax.notify == FaxRequest::no_notice)
	    fax.notify = FaxRequest::when_done;
    } else {
	fxStr cig = canonicalizePhoneNumber(fax.requests[i].item);
	if (cig == "")
	    cig = canonicalizePhoneNumber(FAXNumber);
	traceProtocol("POLL with CIG \"%s\"", (char*) cig);
	FaxRecvInfoArray docs;
	fax.status =
	    (pollFaxPhaseB(cig, docs, fax.notice) ? send_done : send_retry);
	for (u_int j = 0; j < docs.length(); j++) {
	    const FaxRecvInfo& ri = docs[j];
	    if (ri.npages > 0) {
		Sys::chmod(ri.qfile, recvFileMode);
		notifyPollRecvd(fax, ri);
	    } else {
		traceServer("POLL: empty file \"%s\" deleted", (char*)ri.qfile);
		Sys::unlink(ri.qfile);
	    }
	}
	if (fax.status == send_done)
	    notifyPollDone(fax, i);
    }
}

/*
 * Phase B of Group 3 protocol.
 */
fxBool
FaxServer::sendFaxPhaseB(FaxRequest& fax, faxRequest& freq, FaxMachineInfo& clientInfo)
{
    fax.status = send_failed;			// assume failure

    TIFF* tif = TIFFOpen(freq.item, "r");
    if (tif && (freq.dirnum == 0 || TIFFSetDirectory(tif, freq.dirnum))) {
	// set up DCS according to file characteristics
	fax.status = sendSetupParams(tif, clientParams, clientInfo, fax.notice);
	if (fax.status == send_ok) {
	    /*
	     * Count pages sent and advance dirnum so that if we
	     * terminate prematurely we'll only transmit what's left
	     * in the current document/file.  Also, if nothing is
	     * sent, bump the counter on the number of times we've
	     * attempted to send the current page.  We don't try
	     * more than 3 times--to avoid looping.
	     */
	    u_int prevPages = npages;
	    fax.status = modem->sendPhaseB(tif, clientParams, clientInfo,
		fax.pagehandling, fax.notice);
	    if (npages == prevPages) {
		fax.ntries++;
		if (fax.ntries > 2) {
		    if (fax.notice != "")
			fax.notice.append("; ");
		    fax.notice.append(
			"Giving up after 3 attempts to send same page");
		    traceServer("SEND: %s \"%s\", dirnum %d",
			(char*) fax.notice, (char*) freq.item, freq.dirnum);
		    fax.status = send_failed;
		}
	    } else {
		freq.dirnum += npages - prevPages;
		fax.ntries = 0;
	    }
	}
    } else {
	fax.notice = tif ? "Can not set directory in document file" :
			   "Can not open document file";
	traceServer("SEND: %s \"%s\", dirnum %d",
	    (char*) fax.notice, (char*) freq.item, freq.dirnum);
    }
    if (tif)
	TIFFClose(tif);
    return (fax.status == send_ok);
}

/*
 * Check client's capabilities (DIS) against those of the
 * modem and select the parameters that are best for us.
 */
fxBool
FaxServer::sendClientCapabilitiesOK(FaxMachineInfo& clientInfo, u_int nsf, fxStr& emsg)
{
    /*
     * Select signalling rate and minimum scanline time
     * for the duration of the session.  These are not
     * changed once they are set here.
     */
    clientInfo.setMaxSignallingRate(clientCapabilities.br);
    int signallingRate =
	modem->selectSignallingRate(clientInfo.getMaxSignallingRate());
    if (signallingRate == -1) {
	emsg = "Modem does not support negotiated signalling rate";
	return (FALSE);
    }
    clientParams.br = signallingRate;

    clientInfo.setMinScanlineTime(clientCapabilities.st);
    int minScanlineTime =
	modem->selectScanlineTime(clientInfo.getMinScanlineTime());
    if (minScanlineTime == -1) {
	emsg = "Modem does not support negotiated min scanline time";
	return (FALSE);
    }
    clientParams.st = minScanlineTime;

    /*
     * Use optional Error Correction Mode (ECM) if the
     * peer implements and our modem is also capable.
     */
    if (clientCapabilities.ec == EC_ENABLE && modem->supportsECM())
	clientParams.ec = EC_ENABLE;
    else
	clientParams.ec = EC_DISABLE;
    clientParams.bf = BF_DISABLE;
    /*
     * Record the remote machine's capabilities for use below in
     * selecting tranfer parameters for each page sent.  The info
     * constructed here is also recorded in a private database for
     * use in pre-formatting documents sent in future conversations.
     */
    clientInfo.setSupportsHighRes(clientCapabilities.vr == VR_FINE);
    clientInfo.setSupports2DEncoding(clientCapabilities.df >= DF_2DMR);
    clientInfo.setMaxPageWidthInPixels(clientCapabilities.pageWidth());
    clientInfo.setMaxPageLengthInMM(clientCapabilities.pageLength());
    if (nsf) {
	// XXX add Adobe's PostScript protocol
	traceProtocol("REMOTE NSF %#x", nsf);
    }
    traceProtocol("REMOTE best rate %s", clientCapabilities.bitRateName());
    traceProtocol("REMOTE max %s", clientCapabilities.pageWidthName());
    traceProtocol("REMOTE max %s", clientCapabilities.pageLengthName());
    traceProtocol("REMOTE best vres %s", clientCapabilities.verticalResName());
    traceProtocol("REMOTE best format %s", clientCapabilities.dataFormatName());
    if (clientCapabilities.ec == EC_ENABLE)
	traceProtocol("REMOTE supports error correction mode");
    traceProtocol("REMOTE best %s", clientCapabilities.scanlineTimeName());
    traceProtocol("REMOTE %s PostScript transfer",
	clientInfo.getSupportsPostScript() ? "supports" : "does not support");

    traceProtocol("USE %s", clientParams.bitRateName());
    traceProtocol("USE %s", clientParams.scanlineTimeName());
    if (clientParams.ec == EC_ENABLE)
	traceProtocol("USE error correction mode");
    return (TRUE);
}

/*
 * Select session parameters according to the info
 * in the TIFF file.  We setup the encoding scheme,
 * page width & length, and vertical-resolution
 * parameters.  If the remote machine is incapable
 * of handling the image, we bail out.
 *
 * Note that we shouldn't be rejecting too many files
 * because we cache the capabilities of the remote machine
 * and use this to image the facsimile.  This work is
 * mainly done to optimize transmission and to reject
 * anything that might sneak by.
 */
FaxSendStatus
FaxServer::sendSetupParams1(TIFF* tif,
    Class2Params& params, const FaxMachineInfo& clientInfo, fxStr& emsg)
{
    uint16 compression;
    (void) TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
    if (compression != COMPRESSION_CCITTFAX3) {
	emsg = fxStr::format("Document is not in a Group 3-compatible"
	    " format (compression %u)", compression);
	return (send_failed);
    }

    // XXX perhaps should verify samples and bits/sample???
    uint32 g3opts;
    if (!TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS, &g3opts))
	g3opts = 0;
    if (g3opts & GROUP3OPT_2DENCODING) {
	if (!clientInfo.getSupports2DEncoding()) {
	    emsg = "Document was encoded with 2DMR,"
		   " but client does not support this data format";
	    return (send_reformat);
	}
	params.df = DF_2DMR;
    } else
	params.df = DF_1DMR;

    uint32 w;
    (void) TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    if (w > clientInfo.getMaxPageWidthInPixels()) {
	emsg = fxStr::format("Client does not support document page width"
		", max remote page width %u pixels, image width %lu pixels",
		clientInfo.getMaxPageWidthInPixels(), w);
	return (send_reformat);
    }
    // NB: only common values
    params.wd = (w <= 1728 ? WD_1728 : w <= 2048 ? WD_2048 : WD_2432);

    /*
     * Try to deduce the vertical resolution of the image
     * image.  This can be problematical for arbitrary TIFF
     * images 'cuz vendors sometimes don't give the units.
     * We, however, can depend on the info in images that
     * we generate 'cuz we're careful to include valid info.
     */
    float yres;
    if (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)) {
	short resunit = RESUNIT_NONE;
	(void) TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
	if (resunit == RESUNIT_INCH)
	    yres /= 25.4;
    } else {
	/*
	 * No vertical resolution is specified, try
	 * to deduce one from the image length.
	 */
	u_long l;
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &l);
	yres = (l < 1450 ? 3.85 : 7.7);		// B4 at 98 lpi is ~1400 lines
    }
    if (yres >= 7.) {
	if (!clientInfo.getSupportsHighRes()) {
	    emsg = fxStr::format("High resolution document is not supported"
		          " by client, image resolution %g lines/mm", yres);
	    return (send_reformat);
	}
	params.vr = VR_FINE;
    } else
	params.vr = VR_NORMAL;

    /*
     * Select page length according to the image size and
     * vertical resolution.  Note that if the resolution
     * info is bogus, we may select the wrong page size.
     * Note also that we're a bit lenient in places here
     * to take into account sloppy coding practice (e.g.
     * using 200 dpi for high-res facsimile.
     */
    if (clientInfo.getMaxPageLengthInMM() != -1) {
	u_long h = 0;
	(void) TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
	float len = h / yres;			// page length in mm
	if ((int) len > clientInfo.getMaxPageLengthInMM()) {
	    emsg = fxStr::format("Client does not support document page length"
			  ", max remote page length %d mm"
			  ", image length %lu rows (%.2f mm)",
		clientInfo.getMaxPageLengthInMM(), h, len);
	    return (send_reformat);
	}
	// 330 is chosen 'cuz it's half way between A4 & B4 lengths
	params.ln = (len < 330 ? LN_A4 : LN_B4);
    } else
	params.ln = LN_INF;
    return (send_ok);
}

FaxSendStatus
FaxServer::sendSetupParams(TIFF* tif, Class2Params& params, const FaxMachineInfo& clientInfo, fxStr& emsg)
{
    FaxSendStatus status = sendSetupParams1(tif, params, clientInfo, emsg);
    if (status == send_ok) {
	traceProtocol("USE %s", params.pageWidthName());
	traceProtocol("USE %s", params.pageLengthName());
	traceProtocol("USE %s", params.verticalResName());
	traceProtocol("USE %s", params.dataFormatName());
    } else if (status == send_reformat) {
	traceServer(emsg);
    } else if (status == send_failed) {
	traceServer("REJECT: " | emsg);
    }
    return (status);
}
