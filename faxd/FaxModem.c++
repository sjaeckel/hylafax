/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxModem.c++,v 1.128 1995/04/08 21:30:13 sam Rel $ */
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
#include <ctype.h>
#include <stdlib.h>

#include "ClassModem.h"
#include "FaxServer.h"
#include "FaxTrace.h"
#include "FaxFont.h"
#include "t.30.h"
#include "Sys.h"

FaxModem::FaxModem(FaxServer& s, const ModemConfig& c)
    : ClassModem(s,c)
    , server(s)
{
    tagLineFont = NULL;
}

FaxModem::~FaxModem()
{
    delete tagLineFont;
}

/*
 * Default methods for modem driver interface.
 */

CallStatus
FaxModem::dialFax(const char* number, const Class2Params&, fxStr& emsg)
{
    return ClassModem::dial(number, emsg);
}

u_int FaxModem::getTagLineSlop() const		{ return tagLineSlop; }

void
FaxModem::sendBegin(const FaxRequest& req)
{
    if (conf.sendBeginCmd != "")
	atCmd(conf.sendBeginCmd);
    pageNumber = 1;
    setupTagLine(req);
}
void FaxModem::sendSetupPhaseB(){}
void FaxModem::sendEnd()	{}

/*
 * Decode the post-page-handling string to get the next
 * post-page message.  The string is assumed to have 3
 * characters per page of the form:
 *
 *   xxM
 *
 * where xx is a 2-digit hex encoding of the session
 * parameters required to send the page data and the M
 * the post-page message to use between this page and
 * the next page.  See FaxServer::sendPrepareFax for the
 * construction of this string.
 */ 
fxBool
FaxModem::decodePPM(const fxStr& pph, u_int& ppm, fxStr& emsg)
{
    if (pph.length() >= 3) {
	switch (pph[2]) {
	case 'P': ppm = PPM_EOP; return (TRUE);
	case 'M': ppm = PPM_EOM; return (TRUE);
	case 'S': ppm = PPM_MPS; return (TRUE);
	}
    }
    emsg = fxStr::format("Internal botch; %s post-page handling string \"%s\"",
	pph.length() > 3 ? "unknown" : "bad", (const char*) pph);
    return (FALSE);
}

/*
 * Modem capability (and related) query interfaces.
 */

fxStr
FaxModem::getCapabilities() const
{
    return modemParams.encodeCaps();
}

static u_int
bestBit(u_int bits, u_int top, u_int bot)
{
    while (top > bot && (bits & BIT(top)) == 0)
	top--;
    return (top);
}

/*
 * Return Class 2 code for best modem signalling rate.
 */
u_int
FaxModem::getBestSignallingRate() const
{
    return bestBit(modemParams.br, BR_14400, BR_2400);
}

/*
 * Compare the requested signalling rate against
 * those the modem can do and return the appropriate
 * Class 2 bit rate code.
 */
int
FaxModem::selectSignallingRate(int br) const
{
    for (; br >= 0 && (modemParams.br & BIT(br)) == 0; br--)
	;
    return (br);
}

/*
 * Compare the requested min scanline time
 * to what the modem can do and return the
 * lowest time the modem can do.
 */
int
FaxModem::selectScanlineTime(int st) const
{
    for (; st < ST_40MS && (modemParams.st & BIT(st)) == 0; st++)
	;
    return (st);
}

/*
 * Return the best min scanline time the modem
 * is capable of supporting.
 */
u_int
FaxModem::getBestScanlineTime() const
{
    u_int st;
    for (st = ST_0MS; st < ST_40MS; st++)
	if (modemParams.st & BIT(st))
	    break;
    return st;
}

/*
 * Return the best vres the modem supports.
 */
u_int
FaxModem::getBestVRes() const
{
    return bestBit(modemParams.vr, VR_FINE, VR_NORMAL);
}

/*
 * Return the best page width the modem supports.
 */
u_int
FaxModem::getBestPageWidth() const
{
    // XXX NB: we don't use anything > WD_2432
    return bestBit(modemParams.wd, WD_2432, WD_1728);
}

/*
 * Return the best page length the modem supports.
 */
u_int
FaxModem::getBestPageLength() const
{
    return bestBit(modemParams.ln, LN_INF, LN_A4);
}

/*
 * Return the best data format the modem supports.
 */
u_int
FaxModem::getBestDataFormat() const
{
    return bestBit(modemParams.df, DF_2DMMR, DF_1DMR);
}

/*
 * Return whether or not the modem supports 2DMR.
 */
fxBool
FaxModem::supports2D() const
{
    return (modemParams.df & BIT(DF_2DMR)) != 0;
}

/*
 * Return whether or not received EOLs are byte aligned.
 */
fxBool
FaxModem::supportsEOLPadding() const
{
    return FALSE;
}

/*
 * Return whether or not the modem is capable of polling.
 */
fxBool
FaxModem::supportsPolling() const
{
    return FALSE;
}

/*
 * Return whether or not the modem supports
 * the optional Error Correction Mode (ECM).
 */
fxBool
FaxModem::supportsECM() const
{
    return (modemParams.ec & BIT(EC_ENABLE)) != 0;
}

/*
 * Return whether or not the modem supports the
 * specified vertical resolution.  Note that we're
 * rather tolerant because of potential precision
 * problems and general sloppiness on the part of
 * applications writing TIFF files.
 */
fxBool
FaxModem::supportsVRes(float res) const
{
    if (75 <= res && res < 120)
	return modemParams.vr & BIT(VR_NORMAL);
    else if (150 <= res && res < 250)
	return modemParams.vr & BIT(VR_FINE);
    else
	return FALSE;
}

/*
 * Return whether or not the modem supports the
 * specified page width.
 */
fxBool
FaxModem::supportsPageWidth(u_int w) const
{
    switch (w) {
    case 1728:	return modemParams.wd & BIT(WD_1728);
    case 2048:	return modemParams.wd & BIT(WD_2048);
    case 2432:	return modemParams.wd & BIT(WD_2432);
    case 1216:	return modemParams.wd & BIT(WD_1216);
    case 864:	return modemParams.wd & BIT(WD_864);
    }
    return FALSE;
}

/*
 * Return whether or not the modem supports the
 * specified page length.  As above for vertical
 * resolution we're lenient in what we accept.
 */
fxBool
FaxModem::supportsPageLength(u_int l) const
{
    // XXX probably need to be more forgiving with values
    if (270 < l && l <= 330)
	return modemParams.ln & (BIT(LN_A4)|BIT(LN_INF));
    else if (330 < l && l <= 390)
	return modemParams.ln & (BIT(LN_B4)|BIT(LN_INF));
    else
	return modemParams.ln & BIT(LN_INF);
}

/*
 * Return modems best capabilities for setting up
 * the initial T.30 DIS when receiving data.
 */
u_int
FaxModem::modemDIS() const
{
    u_int DIS = DIS_T4RCVR
	      | Class2Params::vrDISTab[getBestVRes()]
	      | Class2Params::brDISTab[getBestSignallingRate()]
	      | Class2Params::wdDISTab[getBestPageWidth()]
	      | Class2Params::lnDISTab[getBestPageWidth()]
	      | Class2Params::dfDISTab[getBestDataFormat()]
	      | Class2Params::stDISTab[getBestScanlineTime()]
	      ;
    // tack on one extension byte
    DIS = (DIS | DIS_XTNDFIELD) << 8;
    if (modemParams.df >= DF_2DMRUNCOMP)
	DIS |= DIS_2DUNCOMP;
    if (modemParams.df >= DF_2DMMR)
	DIS |= DIS_G4COMP;
    return (DIS);
}

/*
 * Tracing support.
 */

/*
 * Trace a modem's capabilities.
 */
void
FaxModem::traceModemParams()
{
    traceBits(modemParams.vr, Class2Params::verticalResNames);
    traceBits(modemParams.br, Class2Params::bitRateNames);
    traceBits(modemParams.wd, Class2Params::pageWidthNames);
    traceBits(modemParams.ln, Class2Params::pageLengthNames);
    traceBits(modemParams.df, Class2Params::dataFormatNames);
    if (modemParams.ec & (BIT(EC_ENABLE)))
	modemSupports("error correction");
    if (modemParams.bf & BIT(BF_ENABLE))
	modemSupports("binary file transfer");
    traceBits(modemParams.st, Class2Params::scanlineTimeNames);
}

void
FaxModem::tracePPM(const char* dir, u_int ppm)
{
    static const char* ppmNames[16] = {
	"unknown PPM 0x00",
	"EOM (more documents)",				// FCF_EOM
	"MPS (more pages, same document)",		// FCF_MPS
	"unknown PPM 0x03",
	"EOP (no more pages or documents)",		// FCF_EOP
	"unknown PPM 0x05",
	"unknown PPM 0x06",
	"unknown PPM 0x07",
	"unknown PPM 0x08",
	"PRI-EOM (more documents after interrupt)",	// FCF_PRI_EOM
	"PRI-MPS (more pages after interrupt)",		// FCF_PRI_MPS
	"unknown PPM 0x0B",
	"PRI-EOP (no more pages after interrupt)",	// FCF_PRI_EOP
	"unknown PPM 0x0D",
	"unknown PPM 0x0E",
    };
    protoTrace("%s %s", dir, ppmNames[ppm&0xf]);
}

void
FaxModem::tracePPR(const char* dir, u_int ppr)
{
    static const char* pprNames[16] = {
	"unknown PPR 0x00",
	"MCF (message confirmation)",			// FCF_MCF/PPR_MCF
	"RTN (retrain negative)",			// FCF_RTN/PPR_RTN
	"RTP (retrain positive)",			// FCF_RTP/PPR_RTP
	"PIN (procedural interrupt negative)",		// FCF_PIN/PPR_PIN
	"PIP (procedural interrupt positive)",		// FCF_PIP/PPR_PIP
	"unknown PPR 0x06",
	"unknown PPR 0x07",
	"CRP (command repeat)",				// FCF_CRP
	"unknown PPR 0x09",
	"unknown PPR 0x0A",
	"unknown PPR 0x0B",
	"unknown PPR 0x0C",
	"unknown PPR 0x0D",
	"unknown PPR 0x0E",
	"DCN (disconnect)",				// FCF_DCN
    };
    protoTrace("%s %s", dir, pprNames[ppr&0xf]);
}

/*
 * Modem i/o support.
 */

/*
 * Miscellaneous server interfaces hooks.
 */
fxBool FaxModem::isFaxModem() const		{ return TRUE; }

fxBool FaxModem::getHDLCTracing()
    { return (server.getSessionTracing() & FAXTRACE_HDLC) != 0; }

FaxSendStatus
FaxModem::sendSetupParams(TIFF* tif, Class2Params& params,
    FaxMachineInfo& info, fxStr& emsg)
{
    return server.sendSetupParams(tif, params, info, emsg);
}

fxBool
FaxModem::recvCheckTSI(const fxStr& tsi)
{
    fxStr s(tsi);
    s.remove(0, s.skip(0,' '));		// strip leading white space
    u_int pos = s.skipR(s.length(),' ');
    s.remove(pos, s.length() - pos);	// and trailing white space
    return server.recvCheckTSI(s);
}
void
FaxModem::recvCSI(fxStr& csi)
{
    csi.remove(0, csi.skip(0,' '));	// strip leading white space
    u_int pos = csi.skipR(csi.length(),' ');
    csi.remove(pos, csi.length() - pos);// and trailing white space
    protoTrace("REMOTE CSI \"%s\"", (char*) csi);
}
void FaxModem::recvDCS(Class2Params& params)
    { server.recvDCS(params); }
void FaxModem::recvNSF(u_int nsf)
    { server.recvNSF(nsf); }

void
FaxModem::recvSetupPage(TIFF* tif, long group3opts, int fillOrder)
{
    server.recvSetupPage(tif, group3opts, fillOrder);
    /*
     * Record the file offset to the start of the data
     * in the file.  We write zero bytes to force the
     * strip offset to be setup in case this is the first
     * time the strip is being written.
     */
    u_char null[1];
    (void) TIFFWriteRawStrip(tif, 0, null, 0);
    u_long* lp;
    (void) TIFFGetField(tif, TIFFTAG_STRIPOFFSETS, &lp);
    savedWriteOff = lp[0];
}

/*
 * Reset the TIFF state for the current page so that
 * subsequent data overwrites anything previously
 * written.  This is done by reseting the file offset
 * and setting the strip's bytecount and offset to
 * values they had at the start of the page.  This
 * scheme assumes that only page data is written to
 * the file between the time recvSetupPage is called
 * and recvResetPage is called.
 */
void
FaxModem::recvResetPage(TIFF* tif)
{
    u_long* lp;
    TIFFSetWriteOffset(tif, 0);		// force library to reset state
    TIFFGetField(tif, TIFFTAG_STRIPOFFSETS, &lp);	lp[0] = savedWriteOff;
    TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &lp);	lp[0] = 0;
}

void
FaxModem::countPage()
{
    pageNumber++;
    server.npages++;
}
