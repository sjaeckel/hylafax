/*	$Header: /usr/people/sam/fax/faxd/RCS/Class2Ersatz.c++,v 1.11 1994/05/30 18:31:00 sam Exp $ */
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
#include "Class2Ersatz.h"
#include "ModemConfig.h"

#include <stdlib.h>
#include <ctype.h>

Class2ErsatzModem::Class2ErsatzModem(FaxServer& s, const ModemConfig& c)
    : Class2Modem(s,c)
{
    serviceType = SERVICE_CLASS2;
    setupDefault(classCmd,	conf.class2Cmd,		"+FCLASS=2");
    setupDefault(mfrQueryCmd,	conf.mfrQueryCmd,	"+FMFR?");
    setupDefault(modelQueryCmd,	conf.modelQueryCmd,	"+FMDL?");
    setupDefault(revQueryCmd,	conf.revQueryCmd,	"+FREV?");
    setupDefault(dccQueryCmd,	conf.class2DCCQueryCmd, "+FDCC=?");
    setupDefault(abortCmd,	conf.class2AbortCmd,	"K");

    setupDefault(borCmd,	conf.class2BORCmd,	"BOR=0");
    setupDefault(tbcCmd,	conf.class2TBCCmd,	"TBC=0");
    setupDefault(crCmd,		conf.class2CRCmd,	"CR=1");
    setupDefault(phctoCmd,	conf.class2PHCTOCmd,	"PHCTO=30");
    setupDefault(bugCmd,	conf.class2BUGCmd,	"BUG=1");
    setupDefault(lidCmd,	conf.class2LIDCmd,	"LID");
    setupDefault(dccCmd,	conf.class2DCCCmd,	"DCC");
    setupDefault(disCmd,	conf.class2DISCmd,	"DIS");
    setupDefault(cigCmd,	conf.class2CIGCmd,	"CIG");
    setupDefault(splCmd,	conf.class2SPLCmd,	"SPL");
    setupDefault(ptsCmd,	conf.class2PTSCmd,	"PTS");
}

Class2ErsatzModem::~Class2ErsatzModem()
{
}

ATResponse
Class2ErsatzModem::atResponse(char* buf, long ms)
{
    if (FaxModem::atResponse(buf, ms) == AT_OTHER &&
      (buf[0] == '+' && buf[1] == 'F')) {
	if (strneq(buf, "+FHNG:", 6)) {
	    processHangup(buf+6);
	    lastResponse = AT_FHNG;
	} else if (strneq(buf, "+FCON", 5))
	    lastResponse = AT_FCON;
	else if (strneq(buf, "+FPOLL", 6))
	    lastResponse = AT_FPOLL;
	else if (strneq(buf, "+FDIS:", 6))
	    lastResponse = AT_FDIS;
	else if (strneq(buf, "+FNSF:", 6))
	    lastResponse = AT_FNSF;
	else if (strneq(buf, "+FCSI:", 6))
	    lastResponse = AT_FCSI;
	else if (strneq(buf, "+FPTS:", 6))
	    lastResponse = AT_FPTS;
	else if (strneq(buf, "+FDCS:", 6))
	    lastResponse = AT_FDCS;
	else if (strneq(buf, "+FNSS:", 6))
	    lastResponse = AT_FNSS;
	else if (strneq(buf, "+FTSI:", 6))
	    lastResponse = AT_FTSI;
	else if (strneq(buf, "+FET:", 5))
	    lastResponse = AT_FET;
    }
    return (lastResponse);
}

/*
 * Handle the page-end protocol.
 */
fxBool
Class2ErsatzModem::pageDone(u_int ppm, u_int& ppr)
{
    ppr = 0;			// something invalid
    if (vatFaxCmd(AT_NOTHING, "ET=%u", ppm)) {
	for (;;) {
	    switch (atResponse(rbuf, conf.pageDoneTimeout)) {
	    case AT_FPTS:
		if (sscanf(rbuf+6, "%u,", &ppr) != 1) {
		    protoTrace("MODEM protocol botch (\"%s\"), %s",
			rbuf, "can not parse PPR");
		    return (FALSE);		// force termination
		}
		break;
	    case AT_OK:				// normal result code
	    case AT_ERROR:			// possible if page retransmit
		return (TRUE);
	    case AT_FHNG:
		return (isNormalHangup());
	    case AT_EMPTYLINE:
		/*
		 * The ZyXEL modem appears to drop DCD when the
		 * remote side drops carrier, no matter whether
		 * DCD is configured to follow carrier or not.
		 * This results in a stream of empty lines,
		 * *sometimes* followed by the requisite trailing OK.
		 * As a hack workaround to deal with the situation
		 * we accept the post page response if this is the
		 * last page that we're sending and the page is
		 * good (i.e. we would hang up immediately anyway).
		 */
		if (ppm == PPM_EOP && ppr == PPR_MCF)
		    return (TRUE);
		/* fall thru... */
	    case AT_TIMEOUT:
	    case AT_NOCARRIER:
	    case AT_NODIALTONE:
	    case AT_NOANSWER:
		goto bad;
	    }
	}
    }
bad:
    processHangup("50");		// Unspecified Phase D error
    return (FALSE);
}

/*
 * Abort a data transfer in progress.
 */
void
Class2ErsatzModem::abortDataTransfer()
{
    char c = CAN;
    putModemData(&c, 1);
}

/*
 * Send an end-of-transmission signal to the modem.
 */
fxBool
Class2ErsatzModem::sendEOT()
{
    static char EOT[] = { DLE, ETX };
    return (putModemData(EOT, sizeof (EOT)));
}

/*
 * Send a page of data using the ``stream interface''.
 */
fxBool
Class2ErsatzModem::sendPage(TIFF* tif)
{
    protoTrace("SEND begin page");
    fxBool rc = TRUE;
    if (flowControl == FLOW_XONXOFF)
	setXONXOFF(FLOW_XONXOFF, FLOW_NONE, ACT_FLUSH);
    /*
     * Correct bit order of data if not what modem expects.
     */
    u_short fillorder;
    TIFFGetFieldDefaulted(tif, TIFFTAG_FILLORDER, &fillorder);
    const u_char* bitrev =
	TIFFGetBitRevTable(fillorder != conf.sendFillOrder);

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
    if (rc)
	rc = sendEOT();
    else
	abortDataTransfer();
    if (flowControl == FLOW_XONXOFF)
	setXONXOFF(getInputFlow(), FLOW_XONXOFF, ACT_DRAIN);
    protoTrace("SEND end page");
    return (rc ? (waitFor(AT_OK) && hangupCode[0] == '\0') : rc);
}
