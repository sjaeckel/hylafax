#ident $Header: /d/sam/flexkit/fax/faxd/RCS/EverexSend.c++,v 1.7 91/09/23 13:45:36 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <stdio.h>
#include "tiffio.h"
#include "Everex.h"
#include "FaxServer.h"

#include "t.30.h"
#include "everex.h"

/*
 * Send Protocol for ``old'' Class-1-style Everex modems.
 */

/*
 * Begin a send operation.
 */
void
EverexModem::sendBegin()
{
    atCmd("#S8=10#S9=30");	// s8 = retry count, s9 = retry interval
    if (type == EV958)		// send 1100Hz tone
	atCmd("#R#T1100=5");
}

/*
 * Terminate a send operation.
 */
void
EverexModem::sendEnd()
{
    sendFrame(FCF_DCN|FCF_SNDR);		// terminate session
}

/*
 * Setup file-independent parameters prior
 * to entering Phase B of the send protocol.
 */
void
EverexModem::sendSetupPhaseB()
{
    setupFrame(FCF_TSI|FCF_SNDR, (char*) lid);
}

int EverexModem::modemScanCodes[8] = { 20, 40, 10, 0, 5, 0, 0, 0 };

/*
 * Send the specified document using the
 * supplied DCS and min-scanline-time
 * parameters.  lastDoc indicates if this
 * is the last document to be sent.
 */
fxBool
EverexModem::sendPhaseB(TIFF* tif, u_int dcs, fxStr& emsg, fxBool lastDoc)
{
    setupFrame(FCF_DCS|FCF_SNDR, dcs<<8);
    atCmd("#S4=" | fxStr(modemRateCodes[(dcs & DCS_SIGRATE) >> 12], "%d"));
    atCmd("#S5=" | fxStr(modemScanCodes[(dcs & DCS_MINSCAN) >> 1], "%d"));
    is2D = (dcs & DCS_2DENCODE) != 0;
    return (sendTraining(emsg) &&
	sendDocument(tif, emsg, lastDoc ? FCF_EOP : FCF_EOM));
}

/*
 * Send capabilities and do training.
 */
fxBool
EverexModem::sendTraining(fxStr& emsg)
{
    server.traceStatus(FAXTRACE_PROTOCOL, "SEND training");
    for (int t = 0; t < 3; t++) {
	if (!sendFrame(FCF_TSI|FCF_SNDR, FCF_DCS|FCF_SNDR))
	    break;
	if (!atCmd("#P1"))
	    break;
	char buf[1024];
	if (getTimedModemLine(buf, TIMER_T4) && streq(buf, "R ", 2))
	    switch (fromHex(buf+2, 2)) {
	    case FCF_FTT:		// failure to train, retry
	    case FCF_CRP:		// repeat command
		break;
	    case FCF_CFR:		// confirmation
		server.traceStatus(FAXTRACE_PROTOCOL, "TRAINING succeeded");
		return (TRUE);
	    case FCF_DCN:		// disconnect
		emsg = "remote fax hungup during training";
		server.traceStatus(FAXTRACE_PROTOCOL, "TRAINING failed");
		return (FALSE);
	    }
    }
    emsg = "failure to train remote modem";
    return (FALSE);
}

/*
 * Send the specified document.
 */
fxBool
EverexModem::sendDocument(TIFF* tif, fxStr& emsg, int EOPcmd)
{
    for (;;) {
	setSpeakerVolume(FaxModem::OFF);
	fxBool transferOK = FALSE;
	if (sendPage(tif, emsg)) {
	    /*
	     * Wait for a signal from the modem that the high
	     * speed carrier was dropped (``M 4'').
	     */
	    int t = 0;
	    do {
		if (getTimedModemLine(rbuf, 30))	// XXX 30 is a guess
		    transferOK = streq(rbuf, "M 4");
	    } while (++t < 3 && !transferOK);
	    if (!transferOK)
		emsg = "unable to reestablish low speed carrier";
	}
	setSpeakerVolume(server.getModemSpeakerVolume());
	if (!transferOK)			// a problem, disconnect
	    return (FALSE);
	/*
	 * Everything went ok, look for the next page to send.
	 */
	int cmd = (TIFFReadDirectory(tif) ? FCF_MPS : EOPcmd);
again:
	int t = 0;
	do {
	    if (!sendFrame(cmd|FCF_SNDR)) {
		emsg = "modem communication failure";
		return (FALSE);
	    }
	    (void) getTimedModemLine(rbuf, TIMER_T4);
	} while (++t < 3 && !streq(rbuf, "R ", 2));
	if (!streq(rbuf, "R ", 2)) {
	    emsg = "no response to MPS or EOP repeated 3 tries";
	    return (FALSE);
	}
	switch (fromHex(rbuf+2, 2)) {
	case FCF_RTP:		// ack, continue after retraining
	    if (!sendTraining(emsg))
		return (FALSE);
	    // fall thru...
	case FCF_MCF:		// ack confirmation
	case FCF_PIP:		// ack, w/ operator intervention
	    if (cmd == FCF_MPS)
		continue;
	    return (TRUE);
	case FCF_DCN:		// disconnect, abort
	case FCF_PIN:		// XXX retransmit previous page
	    emsg = "remote fax disconnected prematurely";
	    return (FALSE);
	case FCF_RTN:		// nak, retry after retraining
	    if (!sendTraining(emsg))
		return (FALSE);
	    // fall thru...
	case FCF_CRP:
	    goto again;
	default:		// unexpected abort
	    emsg = "fax protocol error (unknown frame received)";
	    return (FALSE);
	}
    }
}

#define	FAX_SENDMODE \
    (S2_HOSTCTL|S2_DEFMSGSYS|S2_FLOWATBUF|S2_PADEOLS|S2_19200)

#include <sys/termio.h>

/*
 * Send a page of facsimile data (pre-encoded).
 */
fxBool
EverexModem::sendPage(TIFF* tif, fxStr& emsg)
{
    fxBool rc = FALSE;
    if (!modemFaxConfigure(FAX_SENDMODE)) {
	emsg = "unable to configure modem";
	return (rc);
    }
    if (!atCmd("#P5#P3#P6")) {		// switch modem & start transfer
	emsg = "unable to initiate page transfer";
	return (rc);
    }
    if (setBaudRate(B19200)) {
	server.traceStatus(FAXTRACE_PROTOCOL, "SEND begin page");
	u_long* stripbytecount;
	(void) TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripbytecount);
	int totbytes = (int) stripbytecount[0];
	if (totbytes > 3) {
	    u_char* buf = new u_char[totbytes];
	    if (TIFFReadRawStrip(tif, 0, buf, totbytes) >= 0) {
		int sentbytes = 0;
		// correct bit order of data if not what modem requires
		u_short fillorder = FILLORDER_MSB2LSB;
		(void) TIFFGetField(tif, TIFFTAG_FILLORDER, &fillorder);
		if (fillorder != FILLORDER_LSB2MSB)
		    TIFFReverseBits(buf, totbytes);
		// pass data to modem, being careful not to hang
		beginTimedTransfer();
		u_char* cp = buf;
		while (totbytes > 0 && !wasTimeout()) {
		    int n = fxmin(totbytes, 1024);
		    // 60 seconds should be enough
		    if (putModem(cp, n, 60)) {
			sentbytes += n;
			cp += n;
			totbytes -= n;
			// XXX check to make sure RTC isn't in data
		    }
		}
		endTimedTransfer();
		rc = !wasTimeout();		// return TRUE if no timeout
		if (!rc)
		    emsg = "timeout during transfer of page data";
		server.traceStatus(FAXTRACE_PROTOCOL,
		    "SENT %d bytes of data", sentbytes);
	    }
	    delete buf;
	}
	sendRTC();
	// TRUE =>'s be careful about flushing data
	// But if we timed out earlier, sendBreak could sleep indefinitely
	(void) sendBreak(rc);	
	(void) setBaudRate(B1200);
	server.traceStatus(FAXTRACE_PROTOCOL, "SEND end page");
	countPage();
    }
    return (rc);			// XXX
}

fxBool
EverexModem::sendRTC()
{
    server.traceStatus(FAXTRACE_PROTOCOL, "SEND RTC");
    // byte-aligned and zero-padded EOLs
    char RTC[12];
    char EOL = is2D ? 0xc0 : 0x80;
    for (int i = 0; i < 12; i += 2) {
	RTC[i] = 0x00; RTC[i+1] = EOL;
    }
    return (putModem(RTC, sizeof (RTC), 10));
}
