#ident $Header: /d/sam/flexkit/fax/faxd/RCS/EverexRecv.c++,v 1.7 91/05/23 12:25:23 sam Exp $

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
#include <sys/termio.h>
#include <libc.h>

#include "flock.h"		// XXX

/*
 * Recv Protocol for Class-1-style Everex modems.
 */

#define	FAX_RECVMODE	(S2_HOSTCTL|S2_PADEOLS|S2_19200)

fxBool
EverexModem::recvBegin(u_int dis)
{
    if (!atCmd("#A#S8=8#S9=45V1&E1S0=0X4"))
	return (FALSE);
    if (!modemFaxConfigure(FAX_RECVMODE|S2_RESET))
	return (FALSE);
    if (!setupFrame(FCF_DIS|FCF_RCVR, dis))
	return (FALSE);
    if (!setupFrame(FCF_CSI|FCF_RCVR, (char*) lid))
	return (FALSE);
    if (!atCmd("#T2100=30"))
	return (FALSE);
    return (recvIdentification());
}

fxBool
EverexModem::recvIdentification()
{
    /*
     * Protocol says to try 3 times over a T1 interval.
     */
    int t = 3;
    while (t-- > 0 && sendFrame(FCF_CSI|FCF_RCVR, FCF_DIS|FCF_RCVR))
	if (getTimedModemLine(rbuf, TIMER_T1/3) && streq(rbuf, "R ", 2))
	    return (TRUE);
    return (FALSE);
}

TIFF*
EverexModem::recvPhaseB(fxBool okToRecv)
{
    char buf[1024];

    tif = 0;
    /*
     * Note that we enter the loop after sending DIS
     * and verifying that a response was received within
     * the allowed time period.  Thus, the first trip
     * just processes the response pending in rbuf.
     */
    resetPages();
    do {
	strcpy(buf, rbuf);
	switch (buf[0]) {
	case 'M':		// modem carrier change
	    if (streq(buf, "M 1", 3)) {
		/*
		 * Page carrier was received, enter
		 * phase C (receive facsimile message).
		 */
		if (!tif) {
		    // NB: this assumes we're in the "right" directory
		    tif = TIFFOpen(tempnam("recvq", "fax"), "w");
		    if (!tif)	// XXX??? do this earlier perhaps?
			goto done;
		    (void) flock(TIFFFileno(tif), LOCK_EX|LOCK_NB);
		    server.traceStatus(FAXTRACE_SERVER,
			"RECV data in \"%s\"", TIFFFileName(tif));
		}
		recvPage();
	    }
	    break;
	case 'R':		// HDLC frame(s) received
	    for (char* cp = buf+2; *cp; cp += 3) {
		int code = fromHex(cp, 2) &~ FCF_SNDR;
		switch (code) {
		case FCF_DCS:
		    if (!recvDCS())
			goto done;
		    (void) recvTraining(okToRecv);
		    if (!okToRecv)
			goto done;
		    goto top;
		case FCF_TSI:
		    /*
		     * Receive client's TSI and check if it's ok
		     * for us to accept facsimile.  If not, then
		     * we'll the session after following through
		     * with training (T.30 is not clear whether
		     * it's ok to abort prior to doing training).
		     */
		    if (!recvTSI(tsi))
			goto done;
		    okToRecv = server.recvCheckTSI(tsi);
		    continue;
		case FCF_EOM:
		case FCF_PRI_EOM:
		case FCF_EOP:
		case FCF_PRI_EOP:
		case FCF_MPS:
		case FCF_PRI_MPS:
		    if (!sendFrame(FCF_MCF|FCF_RCVR))
			goto done;
		    goto top;
		case FCF_DTC:
		case FCF_DIS:		// XXX don't handle DTC or DIS
		case FCF_DCN:
		    goto done;
		case FCF_CRP:		// resend last frame
		    if (!sendFrame(lastFrame))
			goto done;
		    goto top;
		default:		// unexpected, abort
		    goto done;
		}
	    }
	    break;
	default:			// something unknown
	    goto done;
	}
    top:
	;
    } while (getTimedModemLine(rbuf, TIMER_T2) || recvIdentification());
done:
    return (tif);
}

// XXX TSI-based access control
fxBool
EverexModem::recvTSI(fxStr& tsi)
{
    if (!atCmd("#FRC2?", FALSE))
	return (FALSE);
    char buf[1024];
    int n = getModemLine(buf);
    if (n > 2) {
	const u_char* DigitMap = (const u_char*)
	    "\x004 " "\x00C0" "\x08C1" "\x04C2" "\x0CC3" "\x02C4"
	    "\x0AC5" "\x06C6" "\x0EC7" "\x01C8" "\x09C9";
	if (n > 40) n = 40;		// spec says no more than 20 digits
	tsi.resize((n+1)/2);
	u_int d = 0;
	fxBool seenDigit = FALSE;
	for (char* cp = buf+n-2; n > 1; cp -= 2, n -= 2) {
	   int digit = fromHex(cp, 2);
	   for (const u_char* dp = DigitMap; *dp; dp += 2)
		if (*dp == digit) {
		    if (dp[1] != ' ')
			seenDigit = TRUE;
		    if (seenDigit)
			tsi[d++] = dp[1];
		    break;
		}
	}
	tsi.resize(d);
    }
    return (sync());
}

fxBool
EverexModem::recvDCS()
{
    if (!atCmd("#FRC1?", FALSE))
	return (FALSE);
    char buf[1024];
    int n = getModemLine(buf);
    if (n >= 6) {
	u_int xinfo = 0;
	u_int dcs = fromHex(buf, fxmin(n,6));
	if (dcs & DCS_XTNDFIELD) {
	    if (n < 8)
		goto bad;
	    xinfo = fromHex(buf+6, 2);
	}
	is2D = (dcs & DCS_2DENCODE) != 0;
	server.recvDCS(dcs, xinfo);
	return (sync());
    }
bad:
    (void) sync();
    return (FALSE);
}

fxBool
EverexModem::recvTraining(fxBool okToRecv)
{
    startTimeout(TIMER_T1);
    char buf[1024];
    while (getModemLine(buf) > 2 && !streq(buf, "M ", 2))
	;
    stopTimeout("reading from");
    if (!wasTimeout() && streq(buf, "M 3", 3)) {
	if (okToRecv)
	    sendFrame(FCF_CFR|FCF_RCVR);	// confirm configuration
	return (TRUE);
    } else {
	sendFrame(FCF_FTT|FCF_RCVR);		// failure to train
	return (FALSE);
    }
}

void
EverexModem::recvPage()
{
    server.traceStatus(FAXTRACE_PROTOCOL, "RECV: begin page");
    setSpeakerVolume(FaxModem::OFF);
    atCmd("#P7");			// switch to high speed and recv fax
    if (setBaudRate(FAX_RECVMODE & S2_9600 ? B9600 : B19200, FALSE)) {
	long group3opts = GROUP3OPT_FILLBITS;
	if (is2D)
	    group3opts |= GROUP3OPT_2DENCODING;
	server.recvSetupPage(tif, group3opts, FILLORDER_LSB2MSB);
	recvPageData();
	(void) sendBreak(FALSE);
	(void) setBaudRate(B1200);
	TIFFWriteDirectory(tif);
	countPage();
    }
#ifdef notdef
    int n;
    while ((n = getModemLine(rbuf)) && n < 3)
	;
    // M 2 is positive ack
#endif
    sync();
    setSpeakerVolume(server.getModemSpeakerVolume());
    server.traceStatus(FAXTRACE_PROTOCOL, "RECV: end page");
}

static const int EOL = 0x001;		// end-of-line code (11 0's + 1)

#define	BITCASE(b)			\
    case b:				\
	c <<= 1;			\
	if (shdata & b) c |= 1;		\
	l++;				\
	if (c > 0) { shbit = (b<<1); break; }

void
EverexModem::recvCode(int& len, int& code)
{
    short c = 0;
    short l = 0;
    switch (shbit & 0xff) {
again:
    BITCASE(0x01);
    BITCASE(0x02);
    BITCASE(0x04);
    BITCASE(0x08);
    BITCASE(0x10);
    BITCASE(0x20);
    BITCASE(0x40);
    BITCASE(0x80);
    default:
	shdata = getModemChar(30);		// XXX just a guess
	if (shdata == EOF)
	    longjmp(recvEOF, 1);
	goto again;
    }
    code = c;
    len = l;
}

int
EverexModem::recvBit()
{
    if ((shbit & 0xff) == 0) {
	shdata = getModemChar(30);
	if (shdata == EOF)
	    longjmp(recvEOF, 1);
	shbit = 0x01;
    }
    int b = (shdata & shbit) != 0;
    shbit <<= 1;
    return (b);
}

void
EverexModem::recvPageData()
{
    u_long badfaxrows = 0;		// # of rows w/ errors
    int badfaxrun = 0;			// current run of bad rows
    int	maxbadfaxrun = 0;		// longest bad run
    fxBool seenRTC = FALSE;		// if true, saw RTC
    u_char thisrow[howmany(2432,8)];	// current accumulated row
    u_char lastrow[howmany(2432,8)];	// previous row for regeneration
    int eols = 0;			// count of consecutive EOL codes
    int lastlen = 0;			// length of last row received (bytes)
    u_long row = 0;
    int tag = 0;			// 2D decoding tag that follows EOL

    if (setjmp(recvEOF) == 0) {
    top:
	u_char* cp = &thisrow[0];
	int bit = 0x80;
	int data = 0;
	int len;
	int code;
	fxBool emptyLine = TRUE;
	while (!seenRTC) {
	    recvCode(len, code);
	    if (len >= 12) {
		if (code == EOL) {
		    /*
		     * Found an EOL, flush the current row and
		     * check for RTC (6 consecutive EOL codes).
		     */
		    if (is2D)
			tag = recvBit();
		    if (!emptyLine) {
			if (bit != 0x80)	// zero pad to byte boundary
			    *cp++ = data;
			// insert EOL (and tag bit if 2D)
			*cp++ = 0x00;
			*cp++ = is2D ? (0x02|tag) : 0x01;
			lastlen = cp - thisrow;
			TIFFReverseBits(thisrow, lastlen);
			TIFFWriteRawStrip(tif, 0, thisrow, lastlen);
			bcopy(thisrow, lastrow, lastlen);
			row++;
			eols = 0;
		    } else
			seenRTC = (++eols == 3);// XXX RTC is 6
		    if (eols > 0)
			server.traceStatus(FAXTRACE_PROTOCOL,
			    "RECV: row %lu, got EOL, eols %d", row, eols);
		    if (badfaxrun > maxbadfaxrun)
			    maxbadfaxrun = badfaxrun;
		    badfaxrun = 0;
		    goto top;
		}
		if (len > 13) {
		    /*
		     * Encountered a bogus code word; skip to the
		     * EOL and regenerate the previous line. 
		     */
		    server.traceStatus(FAXTRACE_PROTOCOL,
			"RECV: bad code word 0x%x, len %d, row %lu",
			code, len, row);
		    badfaxrows++;
		    badfaxrun++;
		    // skip to EOL
		    do
			recvCode(len, code);
		    while (len < 12 || code != EOL);
		    if (is2D)
			tag = recvBit();
		    // regenerate previous row
		    // XXX not right for 2D encoding
		    TIFFWriteRawStrip(tif, 0, lastrow, lastlen);
		    goto top;
		}
	    }
	    // shift code into scanline buffer
	    // XXX maybe unroll expand loop?
	    for (u_int mask = 1<<(len-1); mask; mask >>= 1) {
		if (code & mask)
		    data |= bit;
		if ((bit >>= 1) == 0) {
		    *cp++ = data;
		    data = 0;
		    bit = 0x80;
		}
	    }
	    emptyLine = FALSE;
	}
    } else
	server.traceStatus(FAXTRACE_PROTOCOL,
	    "RECV: premature EOF, row %lu", row);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, row);
    if (badfaxrows) {
	TIFFSetField(tif, TIFFTAG_BADFAXLINES,	badfaxrows);
	TIFFSetField(tif, TIFFTAG_CLEANFAXDATA,	CLEANFAXDATA_REGENERATED);
	TIFFSetField(tif, TIFFTAG_CONSECUTIVEBADFAXLINES, maxbadfaxrun);
    } else
	TIFFSetField(tif, TIFFTAG_CLEANFAXDATA,	CLEANFAXDATA_CLEAN);
}
