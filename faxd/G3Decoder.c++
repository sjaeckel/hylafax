/*	$Header: /usr/people/sam/fax/faxd/RCS/G3Decoder.c++,v 1.7 1994/06/04 22:17:41 sam Exp $ */
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

/*
 * Group 3 Facsimile Reader Support.
 */
#include "G3Decoder.h"
#include "StackBuffer.h"
#include "tiffio.h"

#include "t4.h"
#include "g3states.h"

G3Decoder::G3Decoder() {}
G3Decoder::~G3Decoder() {}

void
G3Decoder::setupDecoder(u_int recvFillOrder, fxBool is2d)
{
    /*
     * The G3 decoding state tables are constructed for
     * data in MSB2LSB bit order.  Received data that
     * is not in this order is reversed using the
     * appropriate byte-wide bit-reversal table.
     */
    setup(TIFFGetBitRevTable(recvFillOrder != FILLORDER_MSB2LSB), is2d);
    bytePending = 0;				// clear state
    refline = NULL;
    recvBuf = NULL;
    prevByte = -1;
}

void G3Decoder::raiseEOF()	{ siglongjmp(jmpEOF, 1); }
void G3Decoder::raiseRTC()	{ siglongjmp(jmpRTC, 1); }

/*
 * Decode h rows that are w pixels wide and return
 * the decoded data in raster.
 */
void
G3Decoder::decode(void* raster, u_int w, u_int h)
{
    u_char reflinebuf[howmany(2432,8)];		// reference line for decoder
    u_int rowbytes = howmany(w, 8);

    setRefLine(reflinebuf);
    memset(reflinebuf, 0, rowbytes);
    memset(raster, 0, h*rowbytes);
    if (prevByte == -1)
	skipLeader();
    while (h-- > 0) {
	(void) decodeRow(raster, w);
	if (is2D)				// copy to refline for 2d rows
	    memcpy(reflinebuf, raster, rowbytes);
	raster = (u_char*)raster + rowbytes;
    }
}

/*
 * Skip h of data.  This is done without decoding
 * the pixels--we just scan for EOL codes.
 */
void
G3Decoder::skip(u_int h)
{
    if (prevByte == -1)
	skipLeader();
    while (h-- > 0)
	skipRow();
}

/*
 * Decode a single row of pixels and return
 * the decoded data in the scanline buffer.
 */
fxBool
G3Decoder::decodeRow(void* scanline, u_int w)
{
    if (is2D)
	tag = nextBit() ? G3_1D : G3_2D;
    return (tag == G3_1D) ?
	decode1DRow((u_char*)scanline, w) :
	decode2DRow((u_char*)scanline, w);
}

/*
 * Skip a single row of data by scanning for EOL.
 */
void
G3Decoder::skipRow()
{
    if (is2D)
	tag = nextBit() ? G3_1D : G3_2D;
    skipToEOL(0);
}

/* 
 * Return an indication of whether or not the next
 * row of data is 1D- or 2D-encoded.
 */
fxBool
G3Decoder::isNextRow1D()
{
    if (is2D) {
	fxBool is1D = (nextBit() != 0);
	ungetBit();
	return (is1D);
    } else
	return (TRUE);
}

/*
 * Scan page data for initial EOL, optionally followed
 * by a tag bit that indicates a 1d-encoded line.
 */ 
void
G3Decoder::skipLeader()
{
    do {
	skipToEOL(0);
    } while (is2D && nextBit() == 0);
    if (is2D)
	ungetBit();
}

/*
 * Decode a row of 1d data.
 */
fxBool
G3Decoder::decode1DRow(u_char* buf, u_int npels)
{
    int x = 0;
    int color = 0;

    for (;;) {
	int runlen;
        if (color == 0)
            runlen = decodeWhiteRun();
        else
            runlen = decodeBlackRun();
        if (runlen > 0) {
            if (color)
		fillspan(buf, x, x+runlen > npels ? npels-x : runlen);
            if ((x += runlen) >= npels)
		break;
        } else {
	    if (runlen == G3CODE_INVALID) {
		invalidCode("1D", x);
		break;
	    }
	    if (runlen == G3CODE_EOL) {
		prematureEOL("1D", x);
		return (FALSE);
	    }
	}
        color = ~color;
    }
    skipToEOL(0);
    if (x != npels)
	badPixelCount("1D", x);
    return (x == npels);
}

/*
 * Decode a run of white.
 */
int
G3Decoder::decodeWhiteRun()
{
    short state = bit;
    short action;
    int runlen = 0;

    for (;;) {
        if (bit == 0) {
    nextbyte:
            data = nextByte();
        }
        action = TIFFFax1DAction[state][data];
        state = TIFFFax1DNextState[state][data];
        if (action == ACT_INCOMP)
            goto nextbyte;
        if (action == ACT_INVALID)
            return (G3CODE_INVALID);
        if (action == ACT_EOL)
            return (G3CODE_EOL);
        bit = state;
        action = RUNLENGTH(action - ACT_WRUNT);
        runlen += action;
        if (action < 64)
            return (runlen);
    }
    /*NOTREACHED*/
}

/*
 * Decode a run of black.
 */
int
G3Decoder::decodeBlackRun()
{
    short state = bit + 8;
    short action;
    int runlen = 0;

    for (;;) {
        if (bit == 0) {
    nextbyte:
            data = nextByte();
        }
        action = TIFFFax1DAction[state][data];
        state = TIFFFax1DNextState[state][data];
        if (action == ACT_INCOMP)
            goto nextbyte;
        if (action == ACT_INVALID)
            return (G3CODE_INVALID);
        if (action == ACT_EOL)
            return (G3CODE_EOL);
        bit = state;
        action = RUNLENGTH(action - ACT_BRUNT);
        runlen += action;
        if (action < 64)
            return (runlen);
        state += 8;
    }
    /*NOTREACHED*/
}

/*
 * Group 3 2d Decoding support.
 */

/*
 * Decode one row of 2d data.
 */
fxBool
G3Decoder::decode2DRow(u_char* buf, u_int npels)
{
#define	PIXEL(buf,ix)    ((((buf)[(ix)>>3]) >> (7-((ix)&7))) & 1)
    int a0 = 0;
    int b1 = 0;
    int b2 = 0;
    int run1, run2;        /* for horizontal mode */
    short mode;
    short color = 0;

    do {
        if (bit == 0 || bit > 7)
            data = nextByte();
        mode = TIFFFax2DMode[bit][data];
        bit = TIFFFax2DNextState[bit][data];
        switch (mode) {
        case MODE_NULL:
            break;
        case MODE_PASS:
            if (a0 || PIXEL(refline, 0) == color) {
                b1 = finddiff(refline, a0, npels);
                if (color == PIXEL(refline, b1))
                    b1 = finddiff(refline, b1, npels);
            } else
                b1 = 0;
            b2 = finddiff(refline, b1, npels);
            if (color)
                fillspan(buf, a0, b2 - a0);
            a0 += b2 - a0;
            break;
        case MODE_HORIZ:
            if (color == 0) {
                run1 = decodeWhiteRun();
                run2 = decodeBlackRun();
            } else {
                run1 = decodeBlackRun();
                run2 = decodeWhiteRun();
            }
	    /*
	     * Do the appropriate fill.  Note that we exit
	     * this logic with the same color that we enter
	     * with since we do 2 fills.  This explains the
	     * somewhat obscure logic below.
	     */
	    if (run1 < 0) {
		if (run1 == G3CODE_EOL) {
		   prematureEOL("2D", a0);
		   return (FALSE);
		}
		invalidCode("1D", a0);
		goto bad2;
	    }
	    if (a0 + run1 > npels)
		run1 = npels - a0;
	    if (color)
		fillspan(buf, a0, run1);
	    a0 += run1;
	    if (run2 < 0) {
		if (run2 == G3CODE_EOL) {
		   prematureEOL("2D", a0);
		   return (FALSE);
		}
		invalidCode("1D", a0);
		goto bad2;
	    }
	    if (a0 + run2 > npels)
		run2 = npels - a0;
	    if (!color)
		fillspan(buf, a0, run2);
	    a0 += run2;
            break;
        case MODE_VERT_V0:
        case MODE_VERT_VR1:
        case MODE_VERT_VR2:
        case MODE_VERT_VR3:
        case MODE_VERT_VL1:
        case MODE_VERT_VL2:
        case MODE_VERT_VL3:
            /*
             * Calculate b1 as the "first changing element
             * on the reference line to right of a0 and of
             * opposite color to a0".  In addition, "the
             * first starting picture element a0 of each
             * coding line is imaginarily set at a position
             * just before the first picture element, and
             * is regarded as a white element".  For us,
             * the condition (a0 == 0 && color == sp->b.white)
             * describes this initial condition. 
             */
            if (!(a0 == 0 && color == 0 && PIXEL(refline, 0) != 0)) {
                b1 = finddiff(refline, a0, npels);
                if (color == PIXEL(refline, b1))
                    b1 = finddiff(refline, b1, npels);
            } else
                b1 = 0;
            b1 += mode - MODE_VERT_V0;
            if (color)
                fillspan(buf, a0, b1 - a0);
            color = !color;
            a0 += b1 - a0;
            break;
	case MODE_UNCOMP:
            /*
             * Uncompressed mode: select from the
             * special set of code words.
             */
            do {
                mode = decodeUncompCode();
                switch (mode) {
                case UNCOMP_RUN1:
                case UNCOMP_RUN2:
                case UNCOMP_RUN3:
                case UNCOMP_RUN4:
                case UNCOMP_RUN5:
                    run1 = mode - UNCOMP_RUN0;
                    fillspan(buf, a0+run1-1, 1);
                    a0 += run1;
                    break;
                case UNCOMP_RUN6:
                    a0 += 5;
                    break;
                case UNCOMP_TRUN0:
                case UNCOMP_TRUN1:
                case UNCOMP_TRUN2:
                case UNCOMP_TRUN3:
                case UNCOMP_TRUN4:
                    run1 = mode - UNCOMP_TRUN0;
                    a0 += run1;
                    color = (nextBit() != 0);
                    break;
                case UNCOMP_INVALID:
		    invalidCode("uncompressed", a0);
                    goto bad2;
                }
            } while (mode < UNCOMP_EXIT);
            break;
	case MODE_ERROR_1:
	    prematureEOL("2D", a0);
	    skipToEOL(7);	// seen 7 0's already
	    return (0);
	case MODE_ERROR:
	    invalidCode("2D", a0);
            goto bad2;
	default:
	    badDecodingState("2D", a0);
	    goto bad2;
        }
    } while (a0 < npels);
bad2:
    skipToEOL(0);
    return (a0 >= npels);			// XXX a0 == npels
#undef	PIXEL
}

/*
 * Return the next uncompressed mode code word.
 */
int
G3Decoder::decodeUncompCode()
{
    short code;
    do {
        if (bit == 0 || bit > 7)
            data = nextByte();
        code = TIFFFaxUncompAction[bit][data];
        bit = TIFFFaxUncompNextState[bit][data];
    } while (code == ACT_INCOMP);
    return (code);
}

/*
 * Miscellaneous stuff.
 */
#define	BITCASE(B)			\
    case B:				\
    code <<= 1;				\
    if (d & (1<<(7-B))) code |= 1;	\
    len++;				\
    if (code > 0) { b = B+1; break; }

/*
 * Skip over input until an EOL code is found.  The
 * value of len is passed as 0 except during error
 * recovery when decoding 2D data.  Note also that
 * we don't use the optimized state tables to locate
 * an EOL because we can't assume much of anything
 * about our state (e.g. bit position).
 */
void
G3Decoder::skipToEOL(int len)
{
    register int b = bit;
    register int d = data;
    int code = 0;

    /*
     * Our handling of ``bit'' is painful because
     * the rest of the code does not maintain it as
     * exactly the bit offset in the current data
     * byte (bit == 0 means refill the data byte).
     * Thus we have to be careful on entry and
     * exit to insure that we maintain a value that's
     * understandable elsewhere in the decoding logic.
     */
    if (b == 0)			// force refill
        b = 8;
    for (;;) {
        switch (b) {
again:  BITCASE(0);
        BITCASE(1);
        BITCASE(2);
        BITCASE(3);
        BITCASE(4);
        BITCASE(5);
        BITCASE(6);
        BITCASE(7);
        default:
            d = nextByte();
            goto again;
        }
        if (len >= 12 && code == EOL)
            break;
        code = len = 0;
    }
    bit = b > 7 ? 0 : b;	// force refill
    data = d;
}
#undef BITCASE

/*
 * Return the next bit in the input stream.  This is
 * used to extract 2D tag values and the color tag
 * at the end of a terminating uncompressed data code.
 */
int
G3Decoder::nextBit()
{
    if (bit != 0) {
	static const short bitMask[8] =
	    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
	int b = data & bitMask[bit];
	bit = (bit+1) & 7;
	return (b);
    } else {
        data = nextByte();
	bit = 1;
	return (data & 0x80);
    }
}

/*
 * Push back a single bit from the raw data stream.
 * Note that this routine assumes it is always called
 * to push back a bit returned by nextBit().
 */
void
G3Decoder::ungetBit()
{
    if (bit == 1) {
	setPendingByte(data);
	bit = 0;
    } else
	bit = (bit-1) & 7;
}

fxBool G3Decoder::isByteAligned()	{ return (bit == 0 || bit > 7); }

void
G3Decoder::flushRecvBuf()
{
    if (bit != 0 && recvBuf)
	recvBuf->put(EOL);
}

/*
 * Push back a byte from the input stream so
 * that it will be used in the next call to
 * nextByte.
 */
void
G3Decoder::setPendingByte(u_char b)
{
    bytePending = b | 0x100;
}

/*
 * Return the next decoded byte of page data from
 * the input stream.  The byte is returned in the
 * bit order required by the G3 decoder and it is
 * also stashed in the receive buffer for writing
 * to the receive file.
 */
int
G3Decoder::nextByte()
{
    int b;
    if (bytePending & 0x100) {		// return any pushback
	b = bytePending & 0xff;
	bytePending = 0;
    } else {				// decode from input stream
	if (recvBuf && prevByte != -1)
	    recvBuf->put(prevByte);	// record in raw input buffer
	b = prevByte = decodeNextByte();
    }
    return (bitmap[b]);			// return with proper bit order
}

void G3Decoder::invalidCode(const char*, int) {}
void G3Decoder::prematureEOL(const char*, int) {}
void G3Decoder::badPixelCount(const char*, int) {}
void G3Decoder::badDecodingState(const char*, int) {}
