/*	$Header: /usr/people/sam/fax/faxd/RCS/Class2Send.c++,v 1.82 1994/06/15 23:40:00 sam Exp $ */
/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
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
#include <stdio.h>
#include "Class2.h"
#include "ModemConfig.h"

/*
 * Send Protocol for Class-2-style modems.
 */

CallStatus
Class2Modem::dial(const char* number, const Class2Params& dis, fxStr& emsg)
{
    if (conf.class2DDISCmd != "") {
	Class2Params ddis(dis);
	ddis.br = getBestSignallingRate();
	ddis.ec = EC_DISABLE;		// XXX
	ddis.bf = BF_DISABLE;
	ddis.st = getBestScanlineTime();
	if (class2Cmd(conf.class2DDISCmd, ddis))
	    params = ddis;
    }
    return (FaxModem::dial(number, dis, emsg));
}

/*
 * Status messages to ignore when dialing.
 */
static fxBool
isNoise(const char* s)
{
    static const char* noiseMsgs[] = {
	"CED",		// RC32ACL-based modems send this before +FCON
	"DIALING",
	"RRING",	// Telebit
	"RINGING",	// ZyXEL
	"+FHR:",	// Intel 144e
	NULL
    };

    for (u_int i = 0; noiseMsgs[i] != NULL; i++)
	if (strneq(s, noiseMsgs[i], strlen(noiseMsgs[i])))
	    return (TRUE);
    return (FALSE);
}

/*
 * Process the response to a dial command.
 */
CallStatus
Class2Modem::dialResponse(fxStr& emsg)
{
    ATResponse r;

    hangupCode[0] = '\0';
    do {
	/*
	 * Use a dead-man timeout since some
	 * modems seem to get hosed and lockup.
	 */
	r = atResponse(rbuf, conf.dialResponseTimeout);
	switch (r) {
	case AT_ERROR:	    return (ERROR);	// error in dial command
	case AT_BUSY:	    return (BUSY);	// busy signal
	case AT_NOCARRIER:  return (NOCARRIER);	// no carrier detected
	case AT_OK:	    return (NOCARRIER);	// (for AT&T DataPort)
	case AT_NODIALTONE: return (NODIALTONE);// local phone connection hosed
	case AT_NOANSWER:   return (NOANSWER);	// no answer or ring back
	case AT_FHNG:				// Class 2 hangup code
	    emsg = hangupCause(hangupCode);
	    switch (atoi(hangupCode)) {
	    case 1:	    return (NOANSWER);	// Ring detected w/o handshake
	    case 3:	    return (NOANSWER);	// No loop current (???)
	    case 4:	    return (NOANSWER);	// Ringback detected, no answer
	    case 5:	    return (NOANSWER);	// Ringback ", no answer w/o CED
	    case 10:	    return (NOFCON);	// Unspecified Phase A error
	    case 11:	    return (NOFCON);	// No answer (T.30 timeout)
	    }
	    break;
	case AT_FCON:	    return (OK);	// fax connection
	case AT_TIMEOUT:    return (FAILURE);	// timed out w/o response
	case AT_CONNECT:    return (DATACONN);	// modem thinks data connection
	}
    } while (r == AT_OTHER && isNoise(rbuf));
    return (FAILURE);
}

/*
 * Process the string of session-related information
 * sent to the caller on connecting to a fax machine.
 */
fxBool
Class2Modem::getPrologue(Class2Params& dis, u_int& nsf, fxStr& csi, fxBool& hasDoc)
{
    fxBool gotParams = FALSE;
    hasDoc = FALSE;
    nsf = 0;
    for (;;) {
	switch (atResponse(rbuf, conf.t1Timer)) {
	case AT_TIMEOUT:
	case AT_EMPTYLINE:
	case AT_NOCARRIER:
	case AT_NODIALTONE:
	case AT_NOANSWER:
	case AT_ERROR:
	    processHangup("10");		// Unspecified Phase A error
	    return (FALSE);
	case AT_FPOLL:
	    hasDoc = TRUE;
	    protoTrace("REMOTE has document to POLL");
	    break;
	case AT_FDIS:
	    gotParams = parseClass2Capabilities(skipStatus(rbuf), dis);
	    break;
	case AT_FNSF:
	    { const char* cp = skipStatus(rbuf);
	      nsf = fromHex(cp, strlen(cp));
	      protoTrace("REMOTE NSF \"%s\"", cp);
	    }
	    break;
	case AT_FCSI:
	    csi = stripQuotes(skipStatus(rbuf));
	    recvCSI(csi);
	    break;
	case AT_FHNG:
	    return (FALSE);
	case AT_OK:
	    return (gotParams);
	}
    }
}

/*
 * Initiate data transfer from the host to the modem when
 * doing a send.  Note that some modems require that we
 * wait for an XON from the modem in response to the +FDT,
 * before actually sending any data.
 */
fxBool
Class2Modem::dataTransfer()
{
    fxBool status;
    if (xmitWaitForXON) {
	/*
	 * Wait for XON (DC1) from the modem after receiving
	 * CONNECT and before sending page data.  If XON/XOFF
	 * flow control is in use then disable it temporarily
	 * so that we can read the input stream for DC1.
	 */
	FlowControl oiFlow = getInputFlow();
	if (flowControl == FLOW_XONXOFF)
	    setXONXOFF(FLOW_NONE, getOutputFlow(), ACT_NOW);
	status = vatFaxCmd(AT_NOTHING, "DT") &&
	    waitFor(AT_CONNECT, conf.pageStartTimeout);
	if (status) {
	    protoTrace("SEND wait for XON");
	    int c;
	    startTimeout(conf.pageStartTimeout);
	    while ((c = getModemChar()) != EOF) {
		modemTrace("--> [1:%c]", c);
		if (c == DC1)
		    break;
	    }
	    stopTimeout("waiting for XON before sending page data");
	    status = (c == DC1);
	}
	if (flowControl == FLOW_XONXOFF)
	    setXONXOFF(oiFlow, getOutputFlow(), ACT_NOW);
    } else {
	status = vatFaxCmd(AT_NOTHING, "DT") &&
	    waitFor(AT_CONNECT, conf.pageStartTimeout);
    }
    return (status);
}

static fxBool
pageInfoChanged(const Class2Params& a, const Class2Params& b)
{
    return (a.vr != b.vr || a.wd != b.wd || a.ln != b.ln || a.df != b.df);
}

/*
 * Send the specified document using the supplied
 * parameters.  The pph is the post-page-handling
 * indicators calculated prior to intiating the call.
 */
FaxSendStatus
Class2Modem::sendPhaseB(TIFF* tif, Class2Params& next, FaxMachineInfo& info,
    fxStr& pph, fxStr& emsg)
{
    int ntrys = 0;			// # retraining/command repeats

    setDataTimeout(60, next.br);	// 60 seconds for 1024 byte writes
    hangupCode[0] = '\0';

    fxBool transferOK;
    fxBool morePages = FALSE;
    do {
	transferOK = FALSE;
	if (abortRequested())
	     goto failed;
	/*
	 * Check the next page to see if the transfer
	 * characteristics change.  If so, update the
	 * current T.30 session parameters.
	 */
	if (pageInfoChanged(params, next)) {
	    if (!class2Cmd(disCmd, next)) {
		emsg = "Unable to set session parameters";
		break;
	    }
	    params = next;
	}
	if (dataTransfer() && sendPage(tif)) {
	    /*
	     * Page transferred, process post page response from
	     * remote station (XXX need to deal with PRI requests).).
	     */
	    morePages = !TIFFLastDirectory(tif);
	    u_int ppm;
	    if (!decodePPM(pph, ppm, emsg))
		goto failed;
	    tracePPM("SEND send", ppm);
	    u_int ppr;
	    if (pageDone(ppm, ppr)) {
		tracePPR("SEND recv", ppr);
		switch (ppr) {
		case PPR_MCF:		// page good
		case PPR_PIP:		// page good, interrupt requested
		case PPR_RTP:		// page good, retrain requested
		    countPage();
		    pph.remove(0,3);	// discard page-handling info
		    ntrys = 0;
		    if (ppr == PPR_PIP) {
			emsg = "Procedure interrupt (operator intervention)";
			goto failed;
		    }
		    if (morePages) {
			if (!TIFFReadDirectory(tif)) {
			    emsg = "Problem reading document directory";
			    goto failed;
			}
			FaxSendStatus status =
			    sendSetupParams(tif, next, info, emsg);
			if (status != send_ok) {
			    sendAbort();
			    return (status);
			}
		    }
		    transferOK = TRUE;
		    break;
		case PPR_RTN:		// page bad, retrain requested
		    if (++ntrys >= 3) {
			emsg = "Unable to transmit page"
			       " (giving up after 3 attempts)";
			break;
		    }
		    morePages = TRUE;	// retransmit page
		    transferOK = TRUE;
		    break;
		case PPR_PIN:		// page bad, interrupt requested
		    emsg = "Unable to transmit page"
		       " (NAK with operator intervention)";
		    goto failed;
		default:
		    emsg = "Modem protocol error (unknown post-page response)";
		    break;
		}
	    }
	}
    } while (transferOK && morePages);
    if (!transferOK) {
	if (emsg == "") {
	    if (hangupCode[0])
		emsg = hangupCause(hangupCode);
	    else
		emsg = "Communication failure during Phase B/C";
	}
	sendAbort();			// terminate session
    }
    return (transferOK ? send_done : send_retry);
failed:
    sendAbort();
    return (send_failed);
}

/*
 * Abort an active Class 2 session.
 */
void
Class2Modem::sendAbort()
{
    (void) class2Cmd(abortCmd);
}
