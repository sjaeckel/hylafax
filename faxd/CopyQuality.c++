/*	$Header: /usr/people/sam/fax/./faxd/RCS/CopyQuality.c++,v 1.16 1995/04/08 21:29:56 sam Rel $ */
/*
 * Copyright (c) 1994-1995 Sam Leffler
 * Copyright (c) 1994-1995 Silicon Graphics, Inc.
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

/*
 * Page Data Receive and Copy Quality Support for Modem Drivers.
 */
#include "FaxModem.h"
#include "FaxTrace.h"
#include "ModemConfig.h"
#include "StackBuffer.h"
#include "FaxServer.h"

#define	RCVBUFSIZ	(16*1024)		// XXX

/*
 * Receive Phase C data with or without copy
 * quality checking and erroneous row fixup.
 */
fxBool
FaxModem::recvPageDLEData(TIFF* tif, fxBool checkQuality,
    const Class2Params& params, fxStr& emsg)
{
    setupDecoder(conf.recvFillOrder, params.is2D());

    fxStackBuffer curRow;			// current row's raw data
    setRecvBuf(curRow);				// reference for input decoder

    recvEOLCount = 0;				// count of EOL codes
    recvBadLineCount = 0;			// rows with a decoding error
    recvConsecutiveBadLineCount = 0;		// max consecutive bad rows
    /*
     * Data destined for the TIFF file is buffered in buf.
     * recvCC has the count of bytes pending in buf.  Beware
     * that this is not an automatic variable because our
     * use of setjmp/longjmp to deal with EOF and RTC detection
     * does not guarantee correct values being restored to
     * automatic variables when the stack is unwound.
     */
    u_char buf[RCVBUFSIZ];			// output buffer
    recvCC = 0;					// byte in output buffer
    if (EOFraised()) {
	abortPageRecv();
	emsg = "Missing EOL after 5 seconds";
	recvTrace("%s", (char*) emsg);
	return (FALSE);
    }
    if (checkQuality) {
	/*
	 * Receive a page of data w/ copy quality checking.
	 */
	u_int rowpixels = params.pageWidth();
	u_int rowbytes = howmany(rowpixels, 8);
	u_char scanlinebuf[1+howmany(2432,8)];	// current decoded scanline
	u_char reflinebuf[1+howmany(2432,8)];	// for refline
	fxStackBuffer curGood;			// raw data for last good row
	fxStackBuffer* recvGood = &curGood;

	setRefLine(reflinebuf+1);
	::memset(reflinebuf, 0, rowbytes+1);
	u_char* scanline = scanlinebuf+1;
	// XXX initialize recvGood to all white
	lastRowBad = FALSE;			// no previous row
	cblc = 0;				// current bad line run
	if (!RTCraised()) {
	    skipLeader();
	    for (;;) {
		/*
		 * Decode the next row of data into scanline.  If
		 * an error is encountered, replicate the last good
		 * row and continue.  We track statistics on bad
		 * lines and consecutive bad lines; these are used
		 * later for deciding whether or not the page quality
		 * is acceptable.
		 */
		::memset(scanline, 0, rowbytes);	// decoding only sets 1s
		if (decodeRow(scanline, rowpixels)) {
		    recvRow(tif, *getRecvBuf(), buf);	// record good row
		    { u_char* t = scanline;
		      scanline = getRefLine();
		      setRefLine(t); }
		    { fxStackBuffer* t = recvGood;
		      recvGood = getRecvBuf();
		      setRecvBuf(*t); }
		    if (lastRowBad) {			// reset state
			lastRowBad = FALSE;
			if (cblc > recvConsecutiveBadLineCount)
			    recvConsecutiveBadLineCount = cblc;
			cblc = 0;
		    }
		} else {
		    recvRow(tif, *recvGood, buf);	// replicate last good
		    recvBadLineCount++;
		    cblc++;
		    lastRowBad = TRUE;
		}
	    }
	}
	if (lastRowBad) {
	    /*
	     * Adjust the received line count to deduce the last
	     * consecutive bad line run since the RTC is often not
	     * readable and/or is followed by line noise or random
	     * junk from the sender.
	     */
	    copyQualityTrace("adjusting for trailing noise (%lu run)", cblc);
	    recvEOLCount -= cblc;
	    recvBadLineCount -= cblc;
	}
    } else {
	/*
	 * Receive a page of data w/o doing copy quality analysis.
	 */
	if (!RTCraised()) {
	    skipLeader();
	    for (;;) {
		skipRow();
		recvRow(tif, curRow, buf);
	    }
	}
    }
    if (recvCC != 0) {
	TIFFWriteRawStrip(tif, 0, buf, recvCC);
	recvTrace("%u bytes of data, %lu total lines", recvCC, recvEOLCount);
    }
    if (checkQuality)
	recvTrace("%lu bad lines %lu consecutive bad lines",
	    recvBadLineCount, recvConsecutiveBadLineCount);
    return (TRUE);
}

/*
 * Check if the configuration parameters indicate if
 * copy quality checking should be done on recvd pages.
 */
fxBool
FaxModem::checkQuality()
{
    return (conf.percentGoodLines != 0 || conf.maxConsecutiveBadLines != 0);
}

/*
 * Check the statistics accumulated during a page recived
 * against the configuration parameters and return an
 * indication of whether or not the page quality is acceptable.
 */
fxBool
FaxModem::isQualityOK(const Class2Params& params)
{
    if (conf.percentGoodLines != 0 && recvEOLCount != 0) {
	u_long percent = 100 * (recvEOLCount - recvBadLineCount) / recvEOLCount;
	if (percent < conf.percentGoodLines) {
	    serverTrace("RECV: REJECT page quality, %u%% good lines (%u%% required)",
		percent, conf.percentGoodLines);
	    return (FALSE);
	}
    }
    u_int cblc = conf.maxConsecutiveBadLines;
    if (cblc != 0) {
	if (params.vr == VR_FINE)
	    cblc *= 2;
	if (recvConsecutiveBadLineCount > cblc) {
	    serverTrace("RECV: REJECT page quality, %u-line run (max %u)",
		recvConsecutiveBadLineCount, cblc);
	    return (FALSE);
	}
    }
    return (TRUE);
}

/*
 * Return the next decoded byte of page data.
 */
int
FaxModem::decodeNextByte()
{
    int b = getModemDataChar();
    if (b == EOF)
	raiseEOF();
    if (b == DLE) {
	switch (b = getModemDataChar()) {
	case EOF: raiseEOF();
	case ETX: raiseRTC();
	case DLE: break;		// <DLE><DLE> -> <DLE>
	default:
	    setPendingByte(b);
	    b = DLE;
	    break;
	}
    }
    return (b);
}

/*
 * Move the received row of raw input data from the
 * stack buffer to buf, flushing buf if space is not
 * available.  If data is received from the modem in
 * MSB2LSB order, then reverse it before writing to
 * the file.  EOL codes are also counted here.
 */
void
FaxModem::recvRow(TIFF* tif, fxStackBuffer& row, u_char* buf)
{
    u_int n = row.getLength();
    if (recvCC + n >= RCVBUFSIZ) {
	TIFFWriteRawStrip(tif, 0, buf, recvCC);
	recvTrace("%u bytes of data, %lu total lines", recvCC, recvEOLCount);
	recvCC = 0;
    }
    /*
     * Always put data out to the file in LSB2MSB bit order.
     * We do this because some TIFF readers (mostly on the PC)
     * don't understand MSB2LSB and/or the FillOrder tag.
     */
    if (conf.recvFillOrder != FILLORDER_LSB2MSB) {
	const u_char* cp = (u_char*) row;
	const u_char* bitrev = TIFFGetBitRevTable(1);
	for (; n > 8; n -= 8) {
	    buf[recvCC+0] = bitrev[cp[0]];
	    buf[recvCC+1] = bitrev[cp[1]];
	    buf[recvCC+2] = bitrev[cp[2]];
	    buf[recvCC+3] = bitrev[cp[3]];
	    buf[recvCC+4] = bitrev[cp[4]];
	    buf[recvCC+5] = bitrev[cp[5]];
	    buf[recvCC+6] = bitrev[cp[6]];
	    buf[recvCC+7] = bitrev[cp[7]];
	    recvCC += 8, cp += 8;
	}
	while (n-- > 0)
	    buf[recvCC++] = bitrev[*cp++];
    } else {
	::memcpy(&buf[recvCC], (char*) row, row.getLength());
	recvCC += row.getLength();
    }
    row.reset();
    recvEOLCount++;
}

u_long FaxModem::getRecvEOLCount() const
    { return recvEOLCount; }
u_long FaxModem::getRecvBadLineCount() const
    { return recvBadLineCount; }
u_long FaxModem::getRecvConsecutiveBadLineCount() const
    { return recvConsecutiveBadLineCount; }

/*
 * Trace a protocol-receive related activity.
 */
void
FaxModem::recvTrace(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    static const fxStr recv("RECV: ");
    server.vtraceStatus(FAXTRACE_PROTOCOL, recv | fmt, ap);
    va_end(ap);
}

/*
 * Note an invalid G3 code word.
 */
void
FaxModem::invalidCode(const char* type, int x)
{
    copyQualityTrace("Invalid %s code word, row %lu, x %d",
	type, recvEOLCount, x);
}

/*
 * Note an EOL code recognized before the expected row end.
 */
void
FaxModem::prematureEOL(const char* type, int x)
{
    copyQualityTrace("Premature EOL (%s), row %lu, x %d",
	type, recvEOLCount, x);
}

/*
 * Note a row decode that gives the wrong pixel count.
 */
void
FaxModem::badPixelCount(const char* type, int x)
{
    copyQualityTrace("Bad %s pixel count, row %lu, x %d",
	type, recvEOLCount, x);
}

void
FaxModem::badDecodingState(const char* type, int x)
{
    copyQualityTrace("Panic, bad %s decoding state, row %lu, x %d",
	type, recvEOLCount, x);
}

/*
 * Trace a copy quality-reated activity.
 */
void
FaxModem::copyQualityTrace(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    static const fxStr cq("RECV/CQ: ");
    server.vtraceStatus(FAXTRACE_COPYQUALITY, cq | fmt, ap);
    va_end(ap);
}
