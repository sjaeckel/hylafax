#ident $Header: /d/sam/flexkit/fax/faxd/RCS/Class2Send.c++,v 1.10 91/09/23 13:45:34 sam Exp $

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
#include "Class2.h"
#include "FaxServer.h"

#include "t.30.h"
#include "class2.h"

/*
 * Send Protocol for Class-2-style modems.
 */
CallStatus
Class2Modem::dial(const fxStr& number)
{
    char* dialCmd = server.getToneDialing() ? "DT" : "DP";
    if (!atCmd(dialCmd | number, FALSE))
	return (FAILURE);
    for (;;) {
	if (getModemLine(rbuf) <= 0)
	    return (FAILURE);
	if (streq(rbuf, "+FCON", 5))
	    return (OK);
	if (streq(rbuf, "BUSY", 4))
	    return (BUSY);
	if (streq(rbuf, "ERROR", 5))
	    return (FAILURE);
	if (streq(rbuf, "NO CARRIER", 10))
	    return (NOCARRIER);
	if (streq(rbuf, "NO DIALTONE", 11))
	    return (NODIALTONE);
	if (streq(rbuf, "+FHNG:", 6)) {
	    hangupCode = (u_int) atoi(rbuf+6);
	    server.traceStatus(FAXTRACE_PROTOCOL,
		"REMOTE HANGUP: %s (code %s)",
		hangupCause(hangupCode), rbuf+6);
	    return (FAILURE);
	}
	return (ERROR);		// XXX
    }
}

fxBool
Class2Modem::getPrologue(u_int& dis, u_int& xinfo, u_int& nsf)
{
    fxBool status = FALSE;
    int n;

    nsf = 0;
    do {
	n = getModemLine(rbuf);
	if (n >= 6) {
	    if (streq(rbuf, "+FNSF:", 6)) {
	    } else if (streq(rbuf, "+FCSI:", 6)) {
		fxStr csi = rbuf+7;
		if (csi.length() > 1) {		// strip quote marks
		    if (csi[0] == '"')
			csi.remove(0);
		    if (csi[csi.length()-1] == '"')
			csi.resize(csi.length()-1);
		}
		// strip leading white space
		csi.remove(0, csi.skip(0, " \t"));
		server.traceStatus(FAXTRACE_PROTOCOL,
		    "REMOTE CSI \"%s\"", (char*) csi);
	    } else if (streq(rbuf, "+FDIS:", 6)) {
		int vr, br, wd, ln, df, ec, bf, st;
		n = sscanf(rbuf+6, "%d,%d,%d,%d,%d,%d,%d,%d",
		    &vr, &br, &wd, &ln, &df, &ec, &bf, &st);
		if (n != 8) {		// protocol botch
		    server.traceStatus(FAXTRACE_PROTOCOL,
			"MODEM protocol botch, can't parse \"%s\"", rbuf);
		    continue;
		}
		// fabricate DIS from component pieces
		// XXX bounds check array indexes
		dis = DIS_T4RCVR
		    | stDISTab[st]
		    | lnDISTab[ln]
		    | wdDISTab[wd]
		    | dfDISTab[df]
		    | vrDISTab[vr]
		    | brDISTab[br]
		    ;
		xinfo = 0;
		status = TRUE;
	    }
	}
    } while (n >= 0 && !streq(rbuf, "OK", 2));
    return (status);
}

/*
 * Send the specified document using the
 * supplied DCS.  The min-scanline-time
 * parameter is not used as the modem handles
 * the negotiation.  lastDoc indicates if this
 * is the last document to be sent.
 */
fxBool
Class2Modem::sendPhaseB(TIFF* tif, u_int dcs, fxStr& emsg, fxBool lastDoc)
{
    fxBool transferOK = FALSE;
    int EOPcmd = lastDoc ? PPM_EOP : PPM_EOM;
    int cmd;

    // XXX this should be done on a page-by-page basis
    is2D = (dcs & DCS_2DENCODE) != 0;
    int vr = (dcs & DCS_7MMVRES) != 0;
    int br = DCSbrTab[(dcs & DCS_SIGRATE) >> 12];
    int wd = DCSwdTab[(dcs & DCS_PAGEWIDTH) >> 6];
    int ln = DCSlnTab[(dcs & DCS_PAGELENGTH) >> 4];
    int st = DCSstTab[(dcs & DCS_MINSCAN) >> 1];
    hangupCode = 0;
    if (class2Cmd("DCC", vr, br, wd, ln, is2D, 0, 0, st)) {
	setSpeakerVolume(FaxModem::OFF);
	do {
	    if (dataTransfer() && waitFor("CONNECT")) {
		transferOK = sendPage(tif);
		cmd = (TIFFReadDirectory(tif) ? PPM_MPS : EOPcmd);
		if (transferOK)
		    transferOK = class2Cmd("ET", cmd);	// XXX check status
	    } else {
		transferOK = FALSE;
		abort();
	    }
	} while (transferOK && cmd != EOPcmd);
	setSpeakerVolume(server.getModemSpeakerVolume());
    }
    if (!transferOK) {
	if (hangupCode)
	    emsg = hangupCause(hangupCode);
	else
	    emsg = "communication failure during Phase B";
    }
    return (transferOK);
}

fxBool
Class2Modem::sendPage(TIFF* tif)
{
    fxBool rc = TRUE;
    server.traceStatus(FAXTRACE_PROTOCOL, "SEND begin page");
    u_long* stripbytecount;
    (void) TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripbytecount);
    int totbytes = (int) stripbytecount[0];
    if (totbytes > 3) {
	u_char* buf = new u_char[totbytes];
	if (TIFFReadRawStrip(tif, 0, buf, totbytes) >= 0) {
	    int sentbytes = 0;
	    // correct bit order of data if not what modem expects
	    u_short fillorder = FILLORDER_MSB2LSB;
	    (void) TIFFGetField(tif, TIFFTAG_FILLORDER, &fillorder);
	    int modemBitOrder = (bor & BOR_C) == BOR_C_REV ?
		FILLORDER_MSB2LSB : FILLORDER_LSB2MSB;
	    u_char* bitRevTable = (fillorder != modemBitOrder) ?
		TIFFBitRevTable : TIFFNoBitRevTable;
	    // pass data to modem, filtering DLE's and
	    // being careful not to get hung up
	    beginTimedTransfer();
	    u_char* cp = buf;
	    u_char dlebuf[2*1024];
	    while (totbytes > 0 && !wasTimeout()) {
		// copy to temp buffer, doubling DLE's & doing bit reversal
		u_int i, j;
		u_int n = fxmin((u_int) totbytes, sizeof (dlebuf)/2);
		for (i = 0, j = 0; i < n; i++, j++) {
		    dlebuf[j] = bitRevTable[cp[i]];
		    if (dlebuf[j] == DLE)
			dlebuf[++j] = DLE;
		}
		// 60 seconds should be enough
		if (putModem(dlebuf, j, 30)) {
		    sentbytes += n;
		    cp += n;
		    totbytes -= n;
		}
	    }
	    endTimedTransfer();
	    rc = !wasTimeout();		// return TRUE if no timeout
	    server.traceStatus(FAXTRACE_PROTOCOL,
		"SENT %d bytes of data", sentbytes);
	}
	delete buf;
    }
    sendEOT();
    server.traceStatus(FAXTRACE_PROTOCOL, "SEND end page");
    countPage();
    return (!waitFor("OK") ? FALSE : rc);
}

fxBool
Class2Modem::sendEOT()
{
    char EOT[2];
    EOT[0] = DLE; EOT[1] = ETX;
    return (putModem(EOT, sizeof (EOT), 30));
}
