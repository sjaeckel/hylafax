/*	$Header: /usr/people/sam/fax/faxd/RCS/Class2Recv.c++,v 1.66 1994/07/04 18:36:46 sam Exp $ */
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

#include "t.30.h"
#include <stdlib.h>
#include <osfcn.h>

/*
 * Recv Protocol for Class-2-style modems.
 */

static const AnswerMsg answerMsgs[] = {
{ "+FCO",	4,
  FaxModem::AT_NOTHING, FaxModem::OK,	    FaxModem::CALLTYPE_FAX },
{ "+FDM",	4,
  FaxModem::AT_NOTHING, FaxModem::OK,	    FaxModem::CALLTYPE_DATA },
{ "+FHNG:",	6,
  FaxModem::AT_NOTHING, FaxModem::NOCARRIER,FaxModem::CALLTYPE_ERROR },
{ "VCON",	4,
  FaxModem::AT_NOTHING, FaxModem::OK,	    FaxModem::CALLTYPE_VOICE },
};
#define	NANSWERS	(sizeof (answerMsgs) / sizeof (answerMsgs[0]))

const AnswerMsg*
Class2Modem::findAnswer(const char* s)
{
    for (u_int i = 0; i < NANSWERS; i++)
	if (strneq(s, answerMsgs[i].msg, answerMsgs[i].len))
	    return (&answerMsgs[i]);
    return FaxModem::findAnswer(s);
}

/*
 * Begin a fax receive session.
 */
fxBool
Class2Modem::recvBegin(fxStr& emsg)
{
    fxBool status = FALSE;
    hangupCode[0] = '\0';
    ATResponse r;
    do {
	switch (r = atResponse(rbuf, 3*60*1000)) {
	case AT_NOANSWER:
	case AT_NOCARRIER:
	case AT_NODIALTONE:
	case AT_ERROR:
	case AT_TIMEOUT:
	case AT_EMPTYLINE:
	    processHangup("70");
	    status = FALSE;
	    break;
	case AT_FNSS:
	    // XXX parse and pass on to server
	    break;
	case AT_FTSI:
	    recvCheckTSI(stripQuotes(skipStatus(rbuf)));
	    break;
	case AT_FDCS:
	    if (recvDCS(rbuf))
		status = TRUE;
	    break;
	case AT_FHNG:
	    status = FALSE;
	    break;
	}
    } while (r != AT_TIMEOUT && r != AT_EMPTYLINE && r != AT_OK);
    if (!status)
	emsg = hangupCause(hangupCode);
    return (status);
}

/*
 * Process a received DCS.
 */
fxBool
Class2Modem::recvDCS(const char* cp)
{
    if (parseClass2Capabilities(skipStatus(cp), params)) {
	setDataTimeout(60, params.br);
	FaxModem::recvDCS(params);
	return (TRUE);
    } else {				// protocol botch
	processHangup("72");		// XXX "COMREC error"
	return (FALSE);
    }
}

/*
 * Signal that we're ready to receive a page
 * and then collect the data.  Return the
 * received post-page-message.
 */
fxBool
Class2Modem::recvPage(TIFF* tif, int& ppm, fxStr& emsg)
{
    int ppr;

    do {
	ppm = PPM_EOP;
	hangupCode[0] = 0;
	if (!vatFaxCmd(AT_NOTHING, "DR"))
	    goto bad;
	ATResponse r;
	while ((r = atResponse(rbuf, conf.pageStartTimeout)) != AT_CONNECT)
	    switch (r) {
	    case AT_FDCS:			// inter-page DCS
		(void) recvDCS(rbuf);
		break;
	    case AT_TIMEOUT:
	    case AT_EMPTYLINE:
	    case AT_ERROR:
	    case AT_NOCARRIER:
	    case AT_NODIALTONE:
	    case AT_NOANSWER:
	    case AT_FHNG:			// remote hangup
		goto bad;
	    }
	protoTrace("RECV: begin page");
	/*
	 * NB: always write data in LSB->MSB for folks that
	 *     don't understand the FillOrder tag!
	 */
	recvSetupPage(tif, group3opts, FILLORDER_LSB2MSB);
	if (!recvPageData(tif, emsg) || !recvPPM(tif, ppr))
	    goto bad;
	/*
	 * T.30 says to process operator intervention requests
	 * here rather than before the page data is received.
	 * This has the benefit of not recording the page as
	 * received when the post-page response might need to
	 * be retransmited.
	 */
	if (abortRequested()) {
	    // XXX no way to purge TIFF directory
	    emsg = "Receive aborted due to operator intervention";
	    return (FALSE);
	}
	// XXX deal with PRI interrupts
	/*
	 * If the host did the copy quality checking,
	 * then override the modem-specified post-page
	 * response according to the quality of the
	 * received page.
	 */
	if (hostDidCQ)
	    ppr = isQualityOK(params) ? PPR_MCF : PPR_RTN;
	if (ppr & 1) {
	    TIFFWriteDirectory(tif);	// complete page write
	    countPage();
	} else {
	    recvResetPage(tif);		// reset to overwrite data
	}
	if (!waitFor(AT_FET))		// post-page message status
	    goto bad;
	ppm = atoi(skipStatus(rbuf));
	if (!waitFor(AT_OK))		// synchronization from modem
	    goto bad;
	tracePPM("RECV recv", ppm);
	tracePPR("RECV send", ppr);
	if (ppr & 1)			// page good, work complete
	    return (TRUE);
    } while (!hostDidCQ || class2Cmd(ptsCmd, ppr));
bad:
    if (hangupCode[0] == 0)
	processHangup("90");			// "Unspecified Phase C error"
    emsg = hangupCause(hangupCode);
    return (FALSE);
}

void
Class2Modem::abortPageRecv()
{
    char c = CAN;
    putModem(&c, 1, 1);
}

/*
 * Receive Phase C data using the Class 2 ``stream interface''.
 */
fxBool
Class2Modem::recvPageData(TIFF* tif, fxStr& emsg)
{
    if (flowControl == FLOW_XONXOFF)
	(void) setXONXOFF(FLOW_NONE, FLOW_XONXOFF, ACT_FLUSH);
    (void) putModem(&recvDataTrigger, 1);	// initiate data transfer

    /*
     * Have host do copy quality checking if the modem does not
     * support checking for this data format and if the configuration
     * parameters indicate CQ checking is to be done.
     */
    hostDidCQ = (modemCQ && BIT(params.df)) == 0 && checkQuality();
    fxBool pageRecvd = recvPageDLEData(tif, hostDidCQ, params, emsg);

    // be careful about flushing here -- otherwise we lose +FPTS codes
    if (flowControl == FLOW_XONXOFF)
	(void) setXONXOFF(FLOW_XONXOFF, getInputFlow(), ACT_DRAIN);

    if (!pageRecvd)
	processHangup("91");			// "Missing EOL after 5 seconds"
    return (pageRecvd);
}

fxBool
Class2Modem::recvPPM(TIFF* tif, int& ppr)
{
    for (;;) {
	switch (atResponse(rbuf, conf.pageDoneTimeout)) {
	case AT_OK:
	    protoTrace("MODEM protocol botch: OK without +FPTS:");
	    /* fall thru... */
	case AT_TIMEOUT:
	case AT_EMPTYLINE:
	case AT_NOCARRIER:
	case AT_NODIALTONE:
	case AT_NOANSWER:
	case AT_ERROR:
	    processHangup("50");
	    return (FALSE);
	case AT_FPTS:
	    return parseFPTS(tif, skipStatus(rbuf), ppr);
	case AT_FET:
	    protoTrace("MODEM protocol botch: +FET: without +FPTS:");
	    processHangup("100");		// "Unspecified Phase D error"
	    return (FALSE);
	case AT_FHNG:
	    waitFor(AT_OK);			// resynchronize modem
	    return (FALSE);
	}
    }
}

fxBool
Class2Modem::parseFPTS(TIFF* tif, const char* cp, int& ppr)
{
    int lc = 0;
    int blc = 0;
    int cblc = 0;
    ppr = 0;
    if (sscanf(cp, "%d,%d,%d,%d", &ppr, &lc, &blc, &cblc) > 0) {
	// NB: ignore modem line count, always use our own
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, getRecvEOLCount());
	TIFFSetField(tif, TIFFTAG_CLEANFAXDATA, blc ?
	    CLEANFAXDATA_REGENERATED : CLEANFAXDATA_CLEAN);
	if (blc) {
	    TIFFSetField(tif, TIFFTAG_BADFAXLINES, (u_long) blc);
	    TIFFSetField(tif, TIFFTAG_CONSECUTIVEBADFAXLINES, cblc);
	}
	return (TRUE);
    } else {
	protoTrace("MODEM protocol botch: \"%s\"; "
	    "can not parse line count", cp);
	processHangup("100");		// "Unspecified Phase D error"
	return (FALSE);
    }
}

/*
 * Complete a receive session.
 */
fxBool
Class2Modem::recvEnd(fxStr&)
{
    if (isNormalHangup())
	(void) vatFaxCmd(AT_NOCARRIER, "DR");	// wait for DCN
    else
	(void) class2Cmd(abortCmd);		// abort session
    return (TRUE);
}

/*
 * Abort an active receive session.
 */
void
Class2Modem::recvAbort()
{
    strcpy(hangupCode, "50");			// force abort in recvEnd
}
