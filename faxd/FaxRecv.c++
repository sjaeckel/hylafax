#ident $Header: /usr/people/sam/flexkit/fax/faxd/RCS/FaxRecv.c++,v 1.17 91/05/28 19:55:06 sam Exp $

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
#include "FaxRecvInfo.h"
#include "RegExArray.h"
#include "t.30.h"
#include "config.h"

#include <osfcn.h>
#include <sys/stat.h>
#include <ctype.h>

/*
 * FAX Server Reception Protocol.
 */
extern "C" int fchmod(int, int);

void
FaxServer::recvFax(fxBool answerThePhone)
{
    if (opened) {
	unsigned t = alarm(0);
	sig_type f = signal(SIGALRM, (sig_type) SIG_IGN);
	fx_theExecutive->removeSelectHandler(rcvHandler);
	if (answerThePhone ||
	  (ringsBeforeAnswer && modem->waitForRings(ringsBeforeAnswer))) {
	    traceStatus(FAXTRACE_PROTOCOL, "RECV: begin");
	    time_t recvStart = time(0);
	    okToRecv = !qualifyTSI;	// anything ok if not qualifying
	    if (modem->recvBegin(modemDIS())) {
		TIFF* tif = modem->recvPhaseB(okToRecv);
		if (tif) {
		    (void) fchmod(TIFFFileno(tif), recvFileMode);
		    recvComplete(TIFFFileName(tif), time(0) - recvStart);
		    TIFFClose(tif);
		}
		modem->recvEnd();
	    }
	    modem->hangup();
	    traceStatus(FAXTRACE_PROTOCOL, "RECV: end");
	} else
	    modemFlushInput();
	fx_theExecutive->addSelectHandler(rcvHandler);
	signal(SIGALRM, f);
	if (t) alarm(t);		// XXX need to deduct time
    }
    sendVoid(recvCompleteChannel);
}

void
FaxServer::recvComplete(const char* qfile, time_t recvTime)
{
    FaxRecvInfo info;
    info.time = recvTime;
    info.qfile = qfile;
    info.npages = npages;
    info.pagewidth = clientInfo.maxPageWidth;
    info.pagelength = clientInfo.maxPageLength;
    info.resolution = clientInfo.supportsHighRes ? 196. : 98.;
    info.sender = recvTSI;
    sendData(jobRecvdChannel, new FaxRecvdData(info));
}

void
FaxServer::recvDCS(u_int dcs, u_int xinfo)
{
    clientInfo.supports2DEncoding = ((dcs & DCS_2DENCODE) != 0);
    clientInfo.supportsHighRes = ((dcs & DCS_7MMVRES) != 0);
    clientInfo.maxPageWidth = pageWidthCodes[(dcs & DCS_PAGEWIDTH)>>6];
    clientInfo.maxPageLength = pageLengthCodes[(dcs & DCS_PAGELENGTH)> 4];

    traceStatus(FAXTRACE_PROTOCOL, "REMOTE page width %d",
	clientInfo.maxPageWidth);
    traceStatus(FAXTRACE_PROTOCOL, "REMOTE page length %d",
	clientInfo.maxPageLength);
    traceStatus(FAXTRACE_PROTOCOL, "REMOTE vertical resolution %s line/mm",
	clientInfo.supportsHighRes ? "7.7" : "3.85");
    traceStatus(FAXTRACE_PROTOCOL, "REMOTE %d-d encoding",
	clientInfo.supports2DEncoding+1);
}

void
FaxServer::updateTSIPatterns()
{
    FILE* fd = fopen(FAX_TSIFILE, "r");
    if (fd != NULL) {
	struct stat sb;
	if (fstat(fileno(fd), &sb) >= 0 && sb.st_mtime >= lastPatModTime) {
	    RegExArray* pats = readTSIPatterns(fd);
	    if (tsiPats)
		delete tsiPats;
	    tsiPats = pats;
	    lastPatModTime = sb.st_mtime;
	}
	fclose(fd);
    } else if (tsiPats)
	// file's been removed, delete any existing info
	delete tsiPats, tsiPats = 0;
}

RegExArray*
FaxServer::readTSIPatterns(FILE* fd)
{
    RegExArray* pats = new RegExArray;
    char line[256];

    while (fgets(line, sizeof (line)-1, fd)) {
	char* cp = cp = strchr(line, '#');
	if (cp || (cp = strchr(line, '\n')))
	    *cp = '\0';
	/* trim off trailing white space */
	for (cp = strchr(line, '\0'); cp > line; cp--)
	    if (!isspace(cp[-1]))
		break;
	*cp = '\0';
	if (line[0] != '\0')
	    pats->append(new fxRegEx(line));
    }
    return (pats);
}

fxBool
FaxServer::recvCheckTSI(const fxStr& tsi)
{
    recvTSI = tsi;
    traceStatus(FAXTRACE_PROTOCOL, "REMOTE TSI \"%s\"", (char*) tsi);
    updateTSIPatterns();
    if (qualifyTSI) {		// check against database of acceptable tsi's
	okToRecv = FALSE;	// reject if no patterns!
	if (tsiPats) {
	    for (u_int i = 0; i < tsiPats->length(); i++) {
		fxRegEx* pat = (*tsiPats)[i];
		if (pat->find(tsi) != -1) {
		    okToRecv = TRUE;
		    break;
		}
	    }
	}
    } else
	okToRecv = TRUE;
    traceStatus(FAXTRACE_SERVER, "%s TSI \"%s\"",
	okToRecv ? "ACCEPT" : "REJECT", (char*) tsi);
    return (okToRecv);
}

void
FaxServer::recvSetupPage(TIFF* tif, long group3opts, int fillOrder)
{
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE,	FILETYPE_PAGE);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,	(u_long) clientInfo.maxPageWidth);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,	1);
    TIFFSetField(tif, TIFFTAG_COMPRESSION,	COMPRESSION_CCITTFAX3);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,	PHOTOMETRIC_MINISWHITE);
    TIFFSetField(tif, TIFFTAG_ORIENTATION,	ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL,	1);
    TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS,	group3opts);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,	-1L);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG,	PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_FILLORDER,	fillOrder);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION,	204.);
    float yres = (clientInfo.supportsHighRes ? 196. : 98.);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION,	yres);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT,	RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_SOFTWARE,		"FlexFAX Version 1.0");
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION,	(char*) recvTSI);
    // XXX stamp date&time
}
