/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxRecv.c++,v 1.87 1995/04/08 21:30:17 sam Rel $ */
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
#include <osfcn.h>
#include <sys/file.h>
#include <ctype.h>

#include "Sys.h"

#include "Dispatcher.h"
#include "tiffio.h"
#include "FaxServer.h"
#include "FaxRecvInfo.h"
#include "t.30.h"
#include "config.h"

/*
 * FAX Server Reception Protocol.
 */

fxBool
FaxServer::recvFax()
{
    traceProtocol("RECV FAX: begin");

    fxStr emsg;
    okToRecv = (qualifyTSI == "");	// anything ok if not qualifying
    recvTSI = "";			// sender's identity initially unknown
    FaxRecvInfoArray docs;
    FaxRecvInfo info;
    fxBool faxRecognized = FALSE;
    abortCall = FALSE;

    /*
     * Create the first file ahead of time to avoid timing
     * problems with Class 1 modems.  (Creating the file
     * after recvBegin can cause part of the first page to
     * be lost.)
     */
    TIFF* tif = setupForRecv("RECV FAX", info, docs);
    if (tif) {
	recvPages = 0;			// count of received pages
	recvStart = Sys::now();		// count initial negotiation
	if (faxRecognized = modem->recvBegin(emsg)) {
	    if (!recvDocuments("RECV FAX", tif, info, docs, emsg))
		modem->recvAbort();
	    if (!modem->recvEnd(emsg))
		traceProtocol("RECV FAX: %s (end)", (char*) emsg);
	} else {
	    traceProtocol("RECV FAX: %s (begin)", (char*) emsg);
	    TIFFClose(tif);
	}
    }
    /*
     * Now that the session is completed, do local processing
     * that might otherwise slow down the protocol (and potentially
     * cause timing problems).
     */
    for (u_int i = 0, n = docs.length(); i < n; i++) {
	const FaxRecvInfo& ri = docs[i];
	if (ri.npages > 0) {
	    Sys::chmod(ri.qfile, recvFileMode);
	    notifyRecvDone(ri);
	} else {
	    traceServer("RECV: No pages received");
	    Sys::unlink(ri.qfile);
	}
    }
    traceProtocol("RECV FAX: end");
    return (faxRecognized);
}

/*
 * Create and lock a temp file for receiving data.
 */
TIFF*
FaxServer::setupForRecv(const char* op, FaxRecvInfo& ri, FaxRecvInfoArray& docs)
{
    char* cp = Sys::tempnam(FAX_RECVDIR, "fax");
    if (cp) {
	ri.qfile = cp;
	::free(cp);
	ri.npages = 0;			// mark it to be deleted...
	docs.append(ri);		// ...add it in to the set
	TIFF* tif = TIFFOpen(ri.qfile, "w");
	if (tif != NULL) {
	    (void) flock(TIFFFileno(tif), LOCK_EX|LOCK_NB);
	    return (tif);
	}
	traceServer("%s: Unable to create file %s for received data", op,
	    (char*) ri.qfile);
    } else
	traceServer("%s: Unable to create temp file for received data", op);
    return (NULL);
}

/*
 * Receive one or more documents.
 */
fxBool
FaxServer::recvDocuments(const char* op, TIFF* tif, FaxRecvInfo& info, FaxRecvInfoArray& docs, fxStr& emsg)
{
    fxBool recvOK;
    int ppm;
    for (;;) {
	 if (!okToRecv) {
	    traceServer("%s: Permission denied (unacceptable client TSI)", op);
	    TIFFClose(tif);
	    return (FALSE);
	}
	npages = 0;
	time_t recvStart = Sys::now();
	recvOK = recvFaxPhaseD(tif, ppm, emsg);
	if (!recvOK)
	    traceProtocol("%s: %s (Phase D)", op, (char*)emsg);
	TIFFClose(tif);
	recvComplete(info, Sys::now() - recvStart, emsg);
	docs[docs.length()-1] = info;
	if (!recvOK || ppm == PPM_EOP)
	    return (recvOK);
	/*
	 * Setup state for another file.
	 */
	tif = setupForRecv(op, info, docs);
	if (tif == NULL)
	    return (FALSE);
	recvStart = time(0);
    }
    /*NOTREACHED*/
}

/*
 * Receive Phase B protocol processing.
 */
fxBool
FaxServer::recvFaxPhaseD(TIFF* tif, int& ppm, fxStr& emsg)
{
    ppm = PPM_EOP;
    do {
	if (++recvPages > maxRecvPages) {
	    emsg = "Maximum receive page count exceeded, job terminated";
	    return (FALSE);
	}
	if (!modem->recvPage(tif, ppm, emsg))
	    return (FALSE);
	if (PPM_PRI_MPS <= ppm && ppm <= PPM_PRI_EOP) {
	    emsg = "Procedure interrupt received, job terminated";
	    return (FALSE);
	}
    } while (ppm == PPM_MPS || ppm == PPM_PRI_MPS);
    return (TRUE);
}

/*
 * Fill in a receive information block
 * from the server's current receive state.
 */
void
FaxServer::recvComplete(FaxRecvInfo& info, time_t recvTime, const fxStr& emsg)
{
    info.time = recvTime;
    info.npages = npages;
    info.pagewidth = clientParams.pageWidth();
    info.pagelength = clientParams.pageLength();
    info.sigrate = ::atoi(clientParams.bitRateName());
    info.protocol = clientParams.dataFormatName();
    info.resolution = (clientParams.vr == VR_FINE ? 196 : 98);
    info.sender = recvTSI;
    info.reason = emsg;
}

/*
 * Process a received DCS.
 */
void
FaxServer::recvDCS(const Class2Params& params)
{
    clientParams = params;

    traceProtocol("REMOTE wants %s", params.bitRateName());
    traceProtocol("REMOTE wants %s", params.pageWidthName());
    traceProtocol("REMOTE wants %s", params.pageLengthName());
    traceProtocol("REMOTE wants %s", params.verticalResName());
    traceProtocol("REMOTE wants %s", params.dataFormatName());
}

/*
 * Process a received Non-Standard-Facilities message.
 */
void
FaxServer::recvNSF(u_int)
{
}

/*
 * Check a received TSI against any list of acceptable
 * TSI patterns defined for the server.  This form of
 * access control depends on the sender passing a valid
 * TSI.  With caller-ID, this access control can be made
 * more reliable.
 */
fxBool
FaxServer::recvCheckTSI(const fxStr& tsi)
{
    recvTSI = tsi;
    okToRecv = isTSIOk(tsi);
    traceProtocol("REMOTE TSI \"%s\"", (char*) tsi);
    traceServer("%s TSI \"%s\"", okToRecv ? "ACCEPT" : "REJECT", (char*) tsi);
    if (okToRecv)
	setServerStatus("Receiving from \"%s\"", (char*) tsi);
    return (okToRecv);
}

#include "version.h"

/*
 * Prepare for the reception of page data by setting the
 * TIFF tags to reflect the data characteristics.
 */
void
FaxServer::recvSetupPage(TIFF* tif, long group3opts, int fillOrder)
{
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE,	FILETYPE_PAGE);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,
	(uint32) clientParams.pageWidth());
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,	1);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,	PHOTOMETRIC_MINISWHITE);
    TIFFSetField(tif, TIFFTAG_ORIENTATION,	ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL,	1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,	(uint32) -1);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG,	PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_FILLORDER,	(uint16) fillOrder);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION,	204.);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION,
	(float) clientParams.verticalRes());
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT,	RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_SOFTWARE,		VERSION);
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION,	(char*) recvTSI);
    switch (clientParams.df) {
    case DF_2DMMR:
	TIFFSetField(tif, TIFFTAG_COMPRESSION,	COMPRESSION_CCITTFAX4);
	break;
    case DF_2DMRUNCOMP:
	TIFFSetField(tif, TIFFTAG_COMPRESSION,	COMPRESSION_CCITTFAX3);
	group3opts |= GROUP3OPT_2DENCODING|GROUP3OPT_UNCOMPRESSED;
	TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS,(uint32) group3opts);
	break;
    case DF_2DMR:
	TIFFSetField(tif, TIFFTAG_COMPRESSION,	COMPRESSION_CCITTFAX3);
	group3opts |= GROUP3OPT_2DENCODING;
	TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS,(uint32) group3opts);
	break;
    case DF_1DMR:
	TIFFSetField(tif, TIFFTAG_COMPRESSION,	COMPRESSION_CCITTFAX3);
	group3opts &= ~GROUP3OPT_2DENCODING;
	TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS,(uint32) group3opts);
	break;
    }
    char dateTime[24];
    time_t now = time(0);
    ::strftime(dateTime, sizeof (dateTime), "%Y:%m:%d %H:%M:%S",
	::localtime(&now));
    TIFFSetField(tif, TIFFTAG_DATETIME,	    dateTime);
    TIFFSetField(tif, TIFFTAG_MAKE,	    (char*) modem->getManufacturer());
    TIFFSetField(tif, TIFFTAG_MODEL,	    (char*) modem->getModel());
    TIFFSetField(tif, TIFFTAG_HOSTCOMPUTER, (char*) hostname);
}
