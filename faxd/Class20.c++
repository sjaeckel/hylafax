/*	$Header: /usr/people/sam/fax/faxd/RCS/Class20.c++,v 1.17 1994/09/23 00:59:30 sam Exp $ */
/*
 * Copyright (c) 1994 Sam Leffler
 * Copyright (c) 1994 Silicon Graphics, Inc.
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
#include "Class20.h"
#include "ModemConfig.h"

#include <stdlib.h>
#include <ctype.h>

Class20Modem::Class20Modem(FaxServer& s, const ModemConfig& c) : Class2Modem(s,c)
{
    serviceType = SERVICE_CLASS20;
    setupDefault(classCmd,	conf.class2Cmd,		"+FCLASS=2.0");
    setupDefault(mfrQueryCmd,	conf.mfrQueryCmd,	"+FMI?");
    setupDefault(modelQueryCmd,	conf.modelQueryCmd,	"+FMM?");
    setupDefault(revQueryCmd,	conf.revQueryCmd,	"+FMR?");
    setupDefault(dccQueryCmd,	conf.class2DCCQueryCmd, "+FCC=?");
    setupDefault(abortCmd,	conf.class2AbortCmd,	"KS");

    setupDefault(borCmd,	conf.class2BORCmd,	"BO=0");
    setupDefault(tbcCmd,	conf.class2TBCCmd,	"PP=0");
    setupDefault(crCmd,		conf.class2CRCmd,	"CR=1");
    setupDefault(phctoCmd,	conf.class2PHCTOCmd,	"CT=30");
    setupDefault(bugCmd,	conf.class2BUGCmd,	"BU=1");
    setupDefault(lidCmd,	conf.class2LIDCmd,	"LI");
    setupDefault(dccCmd,	conf.class2DCCCmd,	"CC");
    setupDefault(disCmd,	conf.class2DISCmd,	"IS");
    setupDefault(cigCmd,	conf.class2CIGCmd,	"PI");
    setupDefault(splCmd,	conf.class2SPLCmd,	"SP");
    setupDefault(ptsCmd,	conf.class2PTSCmd,	"PS");

    // ignore procedure interrupts
    setupDefault(pieCmd,	conf.class2PIECmd,	"IE=0");
    // enable reporting of everything
    setupDefault(nrCmd,		conf.class2NRCmd,	"NR=1,1,1,1");

    rtcRev = TIFFGetBitRevTable(conf.sendFillOrder == FILLORDER_LSB2MSB);
}

Class20Modem::~Class20Modem()
{
}

ATResponse
Class20Modem::atResponse(char* buf, long ms)
{
    if (FaxModem::atResponse(buf, ms) == AT_OTHER &&
      (buf[0] == '+' && buf[1] == 'F')) {
	if (strneq(buf, "+FHS:", 5)) {
	    processHangup(buf+5);
	    lastResponse = AT_FHNG;
	} else if (strneq(buf, "+FCO", 4))
	    lastResponse = AT_FCON;
	else if (strneq(buf, "+FPO", 4))
	    lastResponse = AT_FPOLL;
	else if (strneq(buf, "+FVO", 4))
	    lastResponse = AT_FVO;
	else if (strneq(buf, "+FIS:", 5))
	    lastResponse = AT_FDIS;
	else if (strneq(buf, "+FNF:", 5))
	    lastResponse = AT_FNSF;
	else if (strneq(buf, "+FCI:", 5))
	    lastResponse = AT_FCSI;
	else if (strneq(buf, "+FPS:", 5))
	    lastResponse = AT_FPTS;
	else if (strneq(buf, "+FCS:", 5))
	    lastResponse = AT_FDCS;
	else if (strneq(buf, "+FNS:", 5))
	    lastResponse = AT_FNSS;
	else if (strneq(buf, "+FTI:", 5))
	    lastResponse = AT_FTSI;
	else if (strneq(buf, "+FET:", 5))
	    lastResponse = AT_FET;
    }
    return (lastResponse);
}

/*
 * Abort a data transfer in progress.
 */
void
Class20Modem::abortDataTransfer()
{
    char c = CAN;
    putModemData(&c, 1);
}

/*
 * Send a page of data using the ``stream interface''.
 */
fxBool
Class20Modem::sendPage(TIFF* tif)
{
    fxBool rc = TRUE;
    protoTrace("SEND begin page");
    if (flowControl == FLOW_XONXOFF)
	setXONXOFF(FLOW_XONXOFF, FLOW_NONE, ACT_FLUSH);
    /*
     * Correct bit order of data if not what modem expects.
     */
    u_short fillorder;
    TIFFGetFieldDefaulted(tif, TIFFTAG_FILLORDER, &fillorder);
    const u_char* bitrev = TIFFGetBitRevTable(fillorder != conf.sendFillOrder);

    fxBool firstStrip = setupTagLineSlop(params);
    u_int ts = getTagLineSlop();
    u_long* stripbytecount;
    (void) TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripbytecount);
    for (u_int strip = 0; strip < TIFFNumberOfStrips(tif) && rc; strip++) {
	u_int totbytes = (u_int) stripbytecount[strip];
	if (totbytes > 0) {
	    u_char* data = new u_char[totbytes+ts];
	    if (TIFFReadRawStrip(tif, strip, data+ts, totbytes) >= 0) {
		u_char* dp;
		if (firstStrip) {
		    /*
		     * Generate tag line at the top of the page.
		     */
		    dp = imageTagLine(data, fillorder, params);
		    totbytes = totbytes+ts - (dp-data);
		    firstStrip = FALSE;
		} else
		    dp = data;
		/*
		 * Pass data to modem, filtering DLE's and
		 * being careful not to get hung up.
		 */
		beginTimedTransfer();
		rc = putModemDLEData(dp, totbytes, bitrev, getDataTimeout());
		endTimedTransfer();
		protoTrace("SENT %u bytes of data", totbytes);
	    }
	    delete data;
	}
    }
    if (!rc) {
	abortDataTransfer();
	(void) sendRTC(params.is2D());
    } else
	rc = sendRTC(params.is2D());
    if (flowControl == FLOW_XONXOFF)
	setXONXOFF(getInputFlow(), FLOW_XONXOFF, ACT_DRAIN);
    protoTrace("SEND end page");
    return (rc);
}

/*
 * Send RTC to terminate a page.
 */
fxBool
Class20Modem::sendRTC(fxBool is2D)
{
    static const u_char RTC1D[9] =
	{ 0x00,0x10,0x01,0x00,0x10,0x01,0x00,0x10,0x01 };
    static const u_char RTC2D[10] =
	{ 0x00,0x18,0x00,0xC0,0x06,0x00,0x30,0x01,0x80,0x0C };
    protoTrace("SEND %s RTC", is2D ? "2D" : "1D");
    if (is2D)
	return putModemDLEData(RTC2D, sizeof (RTC2D), rtcRev, getDataTimeout());
    else
	return putModemDLEData(RTC1D, sizeof (RTC1D), rtcRev, getDataTimeout());
}

/*
 * Handle the page-end protocol.  Since Class 2.0 returns
 * OK/ERROR according to the post-page response, we don't
 * need to query the modem to get the actual code and
 * instead synthesize codes, ignoring whether or not the
 * modem does retraining on the next page transfer.
 */
fxBool
Class20Modem::pageDone(u_int ppm, u_int& ppr)
{
    static char ppmCodes[3] = { 0x2C, 0x3B, 0x2E };
    char eop[2];

    eop[0] = DLE;
    eop[1] = ppmCodes[ppm];

    ppr = 0;					// something invalid
    if (putModemData(eop, sizeof (eop))) {
	for (;;) {
	    switch (atResponse(rbuf, conf.pageDoneTimeout)) {
	    case AT_FHNG:
		if (!isNormalHangup())
		    return (FALSE);
		/* fall thru... */
	    case AT_OK:				// page data good
		ppr = PPR_MCF;			// could be PPR_RTP/PPR_PIP
		return (TRUE);
	    case AT_ERROR:			// page data bad
		ppr = PPR_RTN;			// could be PPR_PIN
		return (TRUE);
	    case AT_EMPTYLINE:
	    case AT_TIMEOUT:
	    case AT_NOCARRIER:
	    case AT_NODIALTONE:
	    case AT_NOANSWER:
		goto bad;
	    }
	}
    }
bad:
    processHangup("50");			// Unspecified Phase D error
    return (FALSE);
}

/*
 * Class 2.0 must override the default behaviour used
 * Class 1+2 modems in order to do special handling of
 * <DLE><SUB> escape (translate to <DLE><DLE>).
 */
int
Class20Modem::decodeNextByte()
{
    int b = getModemDataChar();
    if (b == EOF)
	raiseEOF();
    if (b == DLE) {
	switch (b = getModemDataChar()) {
	case EOF: raiseEOF();
	case ETX: raiseRTC();		// RTC
	case DLE: break;		// <DLE><DLE> -> <DLE>
	case SUB: b = DLE;		// <DLE><SUB> -> <DLE><DLE>
	    /* fall thru... */
	default:
	    setPendingByte(b);
	    b = DLE;
	    break;
	}
    }
    return (b);
}
