#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxSend.c++,v 1.18 91/09/23 13:45:39 sam Exp $

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
#include "FaxServer.h"
#include "FaxRequest.h"
#include "StrArray.h"
#include "t.30.h"

#include <osfcn.h>

/*
 * FAX Server Transmission Protocol.
 */
void
FaxServer::sendFax(FaxRequest* fax)
{
    npages = 0;
    if (fax && rcvHandler) {
	if (fax->files.length() == 0)
	    traceStatus(FAXTRACE_ANY, "protocol botch, no files to send");
	else if (fax->number == "")
	    traceStatus(FAXTRACE_ANY, "protocol botch, no phone number");
	else if (fax->sender == "")
	    traceStatus(FAXTRACE_ANY, "protocol botch, no sender");
	else if (fax->files.length() != fax->formats.length())
	    traceStatus(FAXTRACE_ANY, "protocol botch, #files != #formats (%d|%d)",
		fax->files.length(), fax->formats.length());
	else {
	    unsigned t = alarm(0);
	    sig_type f = signal(SIGALRM, (sig_type) SIG_IGN);
	    fx_theExecutive->removeSelectHandler(rcvHandler);
	    /*
	     * Construct the phone number to dial by forming the canonical
	     * form (+<country><areacode><number>, and then stripping
	     * off local prefixes to avoid confusing the phone company
	     * (i.e. dialing <long distance prefix><local area code><number>
	     * often confuses the phone company equipment).
	     */
	    fxStr canon(canonicalizePhoneNumber(fax->number));
	    fxStr dial(localizePhoneNumber(canon));
	    fax->notice = sendFax(dial, canon, fax->files);
	    fax->status = (fax->notice.length() == 0);
	    fx_theExecutive->addSelectHandler(rcvHandler);
	    signal(SIGALRM, f);
	    if (t) alarm(t);			// XXX need to deduct time
	}
    }
    if (fax)
	fax->npages = npages;
    sendData(jobCompleteChannel, new FaxRequestData(fax));
}

/*
 * Send the specified TIFF files to the FAX
 * agent at the given phone number.
 */
fxStr
FaxServer::sendFax(const fxStr& number, const fxStr& canon, const fxStrArray& files)
{
    fxStr completionMessage;
    fxBool updateClientInfo = FALSE;
    modem->hangup();
    traceStatus(FAXTRACE_SERVER, "DIAL \"%s\"", (char*) number);
    CallStatus callstat = modem->dial(number);
    if (callstat == FaxModem::OK) {
	modem->sendBegin();
	if (sendClientCapabilitiesOK()) {
	    updateClientInfo = TRUE;
	    modem->sendSetupPhaseB();
	    for (u_int i = 0, n = files.length(); i < n; i++) {
		traceStatus(FAXTRACE_SERVER,
		    "SEND file \"%s\"", (char*) files[i]);
		if (!sendFaxPhaseB(files[i], completionMessage, i == n-1)) {
		    break;
		}
		// document delivered, notify server
		sendCharPtr(sendCompleteChannel, files[i], fxObj::sync);
	    }
	}
	modem->sendEnd();
    } else {
	traceStatus(FAXTRACE_SERVER, "CALL FAILED; %s",
	    FaxModem::callStatus[callstat]);
	completionMessage = FaxModem::callStatus[callstat];
    }
    modem->hangup();
    if (updateClientInfo) {
	FaxMachineInfo info;
	info.restore(canon);
	if (info != clientInfo)
	    clientInfo.save(canon);
    }
    return (completionMessage);
}

/*
 * Check client's capabilities (DIS) for suitability.
 */
fxBool
FaxServer::sendClientCapabilitiesOK()
{
    u_int xinfo;
    u_int nsf;

    if (!modem->getPrologue(clientDIS, xinfo, nsf))
	return (FALSE);

    if ((clientDIS & DIS_T4RCVR) == 0) {	// not T.4 compatible
	traceStatus(FAXTRACE_PROTOCOL, "REJECT: remote not T.4 compatible");
	return (FALSE);
    }
    clientDCS = DCS_T4RCVR;

    int signallingRate =
	modem->selectSignallingRate((clientDIS & DIS_SIGRATE)>>12);
    if (signallingRate == -1) {
	traceStatus(FAXTRACE_PROTOCOL, "REJECT: can't match signalling rate");
	return (FALSE);
    }
    clientDCS |= signallingRate;

    clientInfo.supportsHighRes = ((clientDIS & DIS_7MMVRES) != 0);
    clientInfo.supports2DEncoding = ((clientDIS & DIS_2DENCODE) != 0);
    clientInfo.maxPageWidth = pageWidthCodes[(clientDIS & DIS_PAGEWIDTH)>>6];
    clientInfo.maxPageLength = pageLengthCodes[(clientDIS & DIS_PAGELENGTH)>>4];
    if (clientDIS & DIS_XTNDFIELD) {
	// XXX handle extended field info
	traceStatus(FAXTRACE_PROTOCOL, "REMOTE extended info %x", xinfo);
    }
    traceStatus(FAXTRACE_PROTOCOL,
	"REMOTE signalling rate: %s",
	    modem->getSignallingRateName((signallingRate&DCS_SIGRATE)>>12));
    traceStatus(FAXTRACE_PROTOCOL,
	"REMOTE max page width %d", clientInfo.maxPageWidth);
    traceStatus(FAXTRACE_PROTOCOL,
	"REMOTE max page length %d", clientInfo.maxPageLength);
    traceStatus(FAXTRACE_PROTOCOL,
	"REMOTE vertical resolution %s line/mm",
	clientInfo.supportsHighRes ? "7.7" : "3.85");
    traceStatus(FAXTRACE_PROTOCOL, "REMOTE %d-d encoding",
	clientInfo.supports2DEncoding+1);
    return (TRUE);
}

/*
 * Phase B of Group 3 protocol.
 */
fxBool
FaxServer::sendFaxPhaseB(const fxStr& file, fxStr& emsg, fxBool lastDoc)
{
    TIFF* tif = TIFFOpen(file, "r");
    if (!tif) {
	traceStatus(FAXTRACE_SERVER, "SEND: Can not open TIFF file %s",
	    (char*) file);
	return (TRUE);			// NB: force file to be skipped
    }
    fxBool status = FALSE;
    // set up DCS according to file characteristics
    if (sendSetupDCS(tif, clientDCS, emsg))
	status = modem->sendPhaseB(tif, clientDCS, emsg, lastDoc);
    TIFFClose(tif);
    return (status);
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
fxBool
FaxServer::sendSetupDCS(TIFF* tif, u_int& dcs, fxStr& emsg)
{
    short compression;
    (void) TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
    if (compression != COMPRESSION_CCITTFAX3) {
	emsg = "document not in Group 3 format";
	traceStatus(FAXTRACE_SERVER, "SEND: %s (file %s, compression %d)",
	    (char*) emsg, TIFFFileName(tif), compression);
	return (FALSE);
    }

    // XXX perhaps should verify samples and bits/sample???
    long g3opts;
    if (!TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS, &g3opts))
	g3opts = 0;
    if (g3opts & GROUP3OPT_2DENCODING) {
	if (!clientInfo.supports2DEncoding) {
	    emsg = "client does not support 2D encoding";
	    traceStatus(FAXTRACE_SERVER, "REJECT: %s", (char*) emsg);
	    return (FALSE);
	}
	dcs |= DCS_2DENCODE;
    } else
	dcs &= ~DCS_2DENCODE;

    u_long w;
    (void) TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    if (w > clientInfo.maxPageWidth) {
	emsg = "page width mismatch";
	traceStatus(FAXTRACE_SERVER,
	    "REJECT: %s, max remote page width %u, image width %lu",
	    (char*) emsg, clientInfo.maxPageWidth, w);
	return (FALSE);
    }
    dcs = (dcs &~ DCS_PAGEWIDTH) |
	(w <= 1728 ? DCSWIDTH_1728 : w <= 2048 ? DCSWIDTH_2048 : DCSWIDTH_2432);

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
    } else
	yres = 7.7;				// assume medium res

    u_short minScanlineTime;
    if (yres < 7.) {
	dcs &= ~DCS_7MMVRES;
	minScanlineTime = minScanlineTimeCodes[(dcs & DIS_MINSCAN)>>1][0];
    } else {
	if (!clientInfo.supportsHighRes) {
	    emsg = "vertical resolution mismatch";
	    traceStatus(FAXTRACE_SERVER,
		"REJECT: %s, image resolution %g line/mm",
		(char*) emsg, yres);
	    return (FALSE);
	}
	dcs |= DCS_7MMVRES;
	minScanlineTime = minScanlineTimeCodes[(dcs & DIS_MINSCAN)>>1][1];
    }

    /*
     * Select page length according to the image size and
     * vertical resolution.  Note that if the resolution
     * info is bogus, we may select the wrong page size.
     * Note also that we're a bit lenient in places here
     * to take into account sloppy coding practice (e.g.
     * using 200 dpi for high-res facsimile.
     */
    dcs &= ~DCS_PAGELENGTH;
    if (clientInfo.maxPageLength != -1) {
	u_long h = 0;
	(void) TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
	float len = h / yres;			// page length in mm
	if (len > clientInfo.maxPageLength) {
	    emsg = "page length mismatch";
	    traceStatus(FAXTRACE_SERVER,
		"REJECT: %s, max remote page length %d, image length %lu",
		(char*) emsg, clientInfo.maxPageLength, h);
	    return (FALSE);
	}
	// 330 is chosen 'cuz it's half way between A4 & B4 lengths
	dcs |= (len < 330 ? DCSLENGTH_A4 : DCSLENGTH_B4);
    } else
	dcs |= DCSLENGTH_UNLIMITED;

    dcs &= ~DCS_MINSCAN;
    switch (minScanlineTime) {
    case 40:	dcs |= DCSMINSCAN_40MS; break;
    case 20:	dcs |= DCSMINSCAN_20MS; break;
    case 10:	dcs |= DCSMINSCAN_10MS; break;
    case  5:	dcs |= DCSMINSCAN_5MS; break;
    case  0:	dcs |= DCSMINSCAN_0MS; break;
    }
    traceStatus(FAXTRACE_PROTOCOL, "USE page width %d pixels",
	pageWidthCodes[(dcs & DCS_PAGEWIDTH)>>6]);
    traceStatus(FAXTRACE_PROTOCOL, "USE page length %d mm",
	pageLengthCodes[(dcs & DCS_PAGELENGTH)>>4]);
    traceStatus(FAXTRACE_PROTOCOL, "USE %s l/mm resolution",
	(dcs & DCS_7MMVRES) ? "7.7" : "3.85");
    traceStatus(FAXTRACE_PROTOCOL, "USE min scanline time %d", minScanlineTime);
    traceStatus(FAXTRACE_PROTOCOL, "USE %s-d encoding",
	(dcs & DCS_2DENCODE) ? "2" : "1");
    return (TRUE);
}
