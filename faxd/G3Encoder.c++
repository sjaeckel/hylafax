/*	$Header: /usr/people/sam/fax/faxd/RCS/G3Encoder.c++,v 1.4 1994/07/02 19:38:57 sam Exp $ */
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
 * Group 3 Facsimile Writer Support.
 */
#include "G3Encoder.h"
#include "StackBuffer.h"
#include "tiffio.h"
#include "t4.h"

G3Encoder::G3Encoder(fxStackBuffer& b) : buf(b) {}
G3Encoder::~G3Encoder() {}

/*
 * Reset encoding state.
 */
void
G3Encoder::setupEncoder(u_int fillOrder, fxBool is2d)
{
    /*
     * G3-encoded data is generated in MSB2LSB bit order, so we
     * need to bit reverse only if the desired order is different.
     */
    setup(TIFFGetBitRevTable(fillOrder != FILLORDER_MSB2LSB), is2d);
    bit = 8;
}

/*
 * Flush 8-bits of encoded data to the output buffer.
 */
inline void
G3Encoder::flushBits()
{
    buf.put(bitmap[data]);
    data = 0;
    bit = 8;
}

/*
 * Encode a multi-line raster.  We do everything with
 * 1D-data, inserting the appropriate tag bits when
 * 2D-encoding is required.
 */
void
G3Encoder::encode(const void* vp, u_int w, u_int h)
{
    u_int rowbytes = howmany(w, 8);

    while (h-- > 0) {
	if (bit != 4)					// byte-align EOL
	    putBits(0, (bit < 4) ? bit+4 : bit-4);
	if (is2D)
	    putBits((EOL<<1)|1, 12+1);
	else
	    putBits(EOL, 12);
	int bs = 0, span;
	const u_char* bp = (const u_char*) vp;
	for (;;) {
	    span = findspan(&bp, bs, w, zeroruns);	// white span
	    putspan(span, TIFFFaxWhiteCodes);
	    bs += span;
	    if (bs >= w)
		break;
	    span = findspan(&bp, bs, w, oneruns);	// black span
	    putspan(span, TIFFFaxBlackCodes);
	    bs += span;
	    if (bs >= w)
		break;
	}
	vp = (const u_char*)vp + rowbytes;
    }
    if (bit != 8)					// flush partial byte
	flushBits();
}

/*
 * Write a code to the output stream.
 */
inline void
G3Encoder::putcode(const tableentry& te)
{
    putBits(te.code, te.length);
}

/*
 * Write the sequence of codes that describes
 * the specified span of zero's or one's.  The
 * appropriate table that holds the make-up and
 * terminating codes is supplied.
 */
void
G3Encoder::putspan(int span, const tableentry* tab)
{
    while (span >= 2624) {
	const tableentry& te = tab[63 + (2560>>6)];
	putcode(te);
	span -= te.runlen;
    }
    if (span >= 64) {
	const tableentry& te = tab[63 + (span>>6)];
	putcode(te);
	span -= te.runlen;
    }
    putcode(tab[span]);
}

/*
 * Write a variable-length bit-value to the output
 * stream. Values are assumed to be at most 16 bits.
 */
void
G3Encoder::putBits(u_int bits, u_int length)
{
    static const u_int mask[9] =
	{ 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };

    while (length > bit) {
	data |= bits >> (length - bit);
	length -= bit;
	flushBits();
    }
    data |= (bits & mask[length]) << (bit - length);
    bit -= length;
    if (bit == 0)
	flushBits();
}
