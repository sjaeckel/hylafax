#ident $Header: /d/sam/flexkit/fax/faxd/RCS/Class2Recv.c++,v 1.7 91/05/23 12:25:08 sam Exp $

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
#include <libc.h>
#include <osfcn.h>

#include "flock.h"		// XXX

/*
 * Recv Protocol for Class-2-style modems.
 */
fxBool
Class2Modem::recvBegin(u_int dis)
{
    fxBool status = FALSE;

    dis >>= 8;			// convert to 24-bit format
    int vr = (dis & DIS_7MMVRES) != 0;
    int br = DCSbrTab[(dis & DIS_SIGRATE) >> 12];
    int wd = DCSwdTab[(dis & DIS_PAGEWIDTH) >> 6];
    int ln = DCSlnTab[(dis & DIS_PAGELENGTH) >> 4];
    int df = (dis & DIS_2DENCODE) != 0;
    int st = DCSstTab[(dis & DIS_MINSCAN) >> 1];
    if (class2Cmd("DCC", vr, br, wd, ln, df, 0, 0, st) && answer() == OK) {
	int n;
	do {
	    n = getModemLine(rbuf);
	    if (n >= 6) {
		if (streq(rbuf, "+FNSS:", 6)) {
		} else if (streq(rbuf, "+FTSI:", 6)) {
		    recvTSI(rbuf+7);
		} else if (streq(rbuf, "+FDCS:", 6)) {
		    if (recvDCS(rbuf+6))
			status = TRUE;
		} else if (streq(rbuf, "+FHNG:", 6)) {
		    u_int code = (u_int) atoi(rbuf+6);
		    server.traceStatus(FAXTRACE_PROTOCOL,
			"REMOTE HANGUP: %s (code %s)",
			hangupCause(code), rbuf+6);
		    status = FALSE;
		}
	    }
	} while (n >= 0 && !streq(rbuf, "OK", 2));
    }
    return (status);
}

CallStatus
Class2Modem::answer()
{
    if (!atCmd("A", FALSE))
	return (FAILURE);
    for (;;) {
	if (getModemLine(rbuf) <= 0)
	    return (FAILURE);
	if (streq(rbuf, "+FCON", 5))
	    return (OK);
	if (streq(rbuf, "+FHNG:", 6)) {
	    u_int code = (u_int) atoi(rbuf+6);
	    server.traceStatus(FAXTRACE_PROTOCOL,
		"REMOTE HANGUP: %s (code %s)",
		hangupCause(code), rbuf+6);
	    return (NOCARRIER);		// XXX
	}
	if (streq(rbuf, "NO CARRIER", 10))
	    return (NOCARRIER);
	if (streq(rbuf, "NO DIALTONE", 11))
	    return (NODIALTONE);
	if (streq(rbuf, "ERROR", 5))
	    return (ERROR);
    }
}

fxBool
Class2Modem::recvTSI(const char* cp)
{
    fxStr tsi = cp;
    if (tsi.length() > 1) {		// strip quote marks
	if (tsi[0] == '"')
	    tsi.remove(0);
	if (tsi[tsi.length()-1] == '"')
	    tsi.resize(tsi.length()-1);
    }
    tsi.remove(0, tsi.skip(0, " \t"));	// strip leading white space
    return server.recvCheckTSI(tsi);
}

fxBool
Class2Modem::recvDCS(const char* cp)
{
    int vr, br, wd, ln, df, ec, bf, st;
    int n = sscanf(cp, "%d,%d,%d,%d,%d,%d,%d,%d",
	&vr, &br, &wd, &ln, &df, &ec, &bf, &st);
    if (n == 8) {		// protocol botch
	// fabricate DCS from component pieces
	// XXX bounds check array indexes
	u_int xinfo = 0;
	u_int dcs = DCS_T4RCVR
	    | stDISTab[st]
	    | lnDISTab[ln]
	    | wdDISTab[wd]
	    | dfDISTab[df]
	    | vrDISTab[vr]
	    | brDISTab[br]
	    ;
	is2D = (dcs & DCS_2DENCODE) != 0;
	server.recvDCS(dcs, xinfo);
	return (TRUE);
    } else {
	server.traceStatus(FAXTRACE_PROTOCOL,
	    "MODEM protocol botch, can't parse \"%s\"", cp);
	return (FALSE);
    }
}

TIFF*
Class2Modem::recvPhaseB(fxBool okToRecv)
{
    // NB: this assumes we're in the "right" directory
    if (okToRecv) {
	tif = TIFFOpen(tempnam("recvq", "fax"), "w");
	if (tif) {
	    (void) flock(TIFFFileno(tif), LOCK_EX|LOCK_NB);
	    server.traceStatus(FAXTRACE_SERVER,
		"RECV data in \"%s\"", TIFFFileName(tif));
	    resetPages();
	    while (recvPage() && okToRecv)
		;
	    return (tif);		// caller does close
	}
    } else
	tif = 0;
    abort();				// terminate session
    if (tif)
	(void) unlink(TIFFFileName(tif));
    return (tif);
}

fxBool
Class2Modem::recvPage()
{
    if (dataReception()) {
	do {
	    int n = getModemLine(rbuf);
	    if (n <= 0)
		break;
	    if (streq(rbuf, "CONNECT", 7)) {
		server.traceStatus(FAXTRACE_PROTOCOL, "RECV: begin page");
		long group3opts = GROUP3OPT_FILLBITS;
		if (is2D)
		    group3opts |= GROUP3OPT_2DENCODING;
		server.recvSetupPage(tif, group3opts,
		    (bor & BOR_C) == BOR_C_REV ?
			FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
		(void) recvPageData();	// XXX
		TIFFWriteDirectory(tif);
		countPage();
		server.traceStatus(FAXTRACE_PROTOCOL, "RECV: end page");
		return (TRUE);
	    }
	    if (streq(rbuf, "+FHNG:", 6)) {
		u_int code = (u_int) atoi(rbuf+6);
		server.traceStatus(FAXTRACE_PROTOCOL,
		    "REMOTE HANGUP: %s (code %s)", hangupCause(code), rbuf+6);
	    }
	} while (!streq(rbuf, "OK", 2) && !streq(rbuf, "ERROR", 5));
    }
    return (FALSE);
}

void
Class2Modem::recvData(u_char* buf, int n)
{
    TIFFWriteRawStrip(tif, 0, buf, n);
    server.traceStatus(FAXTRACE_PROTOCOL, "RECV: %d bytes of data", n);
}

fxBool
Class2Modem::recvPageData()
{
    (void) setInputFlowControl(FALSE);	// disable xon/xoff interpretation
    char dc1 = DC1;
    (void) putModem(&dc1, 1);		// initiate data transfer

    u_char buf[16*1024];
    int n = 0;
    for (;;) {
	int b = getModemChar(30);
	if (b == EOF) {
	    server.traceStatus(FAXTRACE_PROTOCOL, "RECV: premature EOF");
	    break;
	}
	if (b == DLE) {
	    b = getModemChar(30);
	    if (b == EOF || b == ETX) {
		if (b == EOF)
		    server.traceStatus(FAXTRACE_PROTOCOL, "RECV: premature EOF");
		break;
	    }
	    if (b != DLE) {
		if (n == sizeof (buf))
		    recvData(buf, sizeof (buf)), n = 0;
		buf[n++] = DLE;
	    }
	}
	if (n == sizeof (buf))
	    recvData(buf, sizeof (buf)), n = 0;
	buf[n++] = b;
    }
    if (n > 0)
	recvData(buf, n);
    // be careful about flushing here -- otherwise we lose +FPTS codes
    (void) setInputFlowControl(TRUE, FALSE);

    /*
     * Handle end-of-page protocol.
     */
    fxBool pageGood = FALSE;
    do {
	n = getModemLine(rbuf);
	if (n <= 0)
	    break;
	if (streq(rbuf, "+FET:", 4)) {
	} else if (streq(rbuf, "+FHNG:", 6)) {
	    u_int code = (u_int) atoi(rbuf+6);
	    server.traceStatus(FAXTRACE_PROTOCOL,
		"REMOTE HANGUP: %s (code %s)", hangupCause(code), rbuf+6);
	} else if (streq(rbuf, "+FPTS:", 6)) {
	    int ppr = 0;
	    int lc = 0;
	    int blc = 0;
	    int cblc = 0;
	    if (sscanf(rbuf+6, "%d,%d,%d,%d", &ppr, &lc, &blc, &cblc) < 2) {
		server.traceStatus(FAXTRACE_PROTOCOL,
		    "MODEM protocol botch (\"%s\"), can't parse line count",
		    rbuf); 
		continue;
	    }
	    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (u_long) lc);
	    TIFFSetField(tif, TIFFTAG_CLEANFAXDATA,
		blc ? CLEANFAXDATA_REGENERATED : CLEANFAXDATA_CLEAN);
	    if (blc) {
		TIFFSetField(tif, TIFFTAG_BADFAXLINES,  (u_long) blc);
		TIFFSetField(tif, TIFFTAG_CONSECUTIVEBADFAXLINES, cblc);
	    }
	    pageGood = ppr & 1;		// XXX handle interrupts
	}
    } while (!streq(rbuf, "OK", 2));
    return (pageGood); 
}
