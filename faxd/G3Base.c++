/*	$Header: /usr/people/sam/fax/./faxd/RCS/G3Base.c++,v 1.6 1995/04/08 21:30:32 sam Rel $ */
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
 * Group 3 Facsimile Reader/Writer Support.
 */
#include "G3Base.h"

void
G3Base::setup(const u_char* b, fxBool is2d)
{
    is2D = is2d;
    tag = G3_1D;
    bitmap = b;
}

/*
 * Bit manipulation utilities.
 */

/*
 * Fill a span with ones.
 */
void
G3Base::fillspan(u_char* cp, int x, int count)
{
    static const u_char masks[] =
        { 0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

    if (count <= 0)
        return;
    cp += x>>3;
    if (x &= 7) {			// align to byte boundary
        if (count < 8 - x) {
            *cp++ |= masks[count] >> x;
            return;
        }
        *cp++ |= 0xff >> x;
        count -= 8 - x;
    }
    while (count >= 8) {
        *cp++ = 0xff;
        count -= 8;
    }
    *cp |= masks[count];
}

/*
 * Find a span of ones or zeros using the supplied
 * table.  The byte-aligned start of the bit string
 * is supplied along with the start+end bit indices.
 * The table gives the number of consecutive ones or
 * zeros starting from the msb and is indexed by byte
 * value.
 */
int
G3Base::findspan(const u_char** bpp, int bs, int be, const u_char* tab)
{
    const u_char *bp = *bpp;
    int bits = be - bs;
    int n, span;

    /*
     * Check partial byte on lhs.
     */
    if (bits > 0 && (n = (bs & 7))) {
	span = tab[(*bp << n) & 0xff];
	if (span > 8-n)        /* table value too generous */
	    span = 8-n;
	if (span > bits)	/* constrain span to bit range */
	    span = bits;
	if (n+span < 8)        /* doesn't extend to edge of byte */
	    goto done;
	bits -= span;
	bp++;
    } else
	span = 0;
    /*
     * Scan full bytes for all 1's or all 0's.
     */
    while (bits >= 8) {
	n = tab[*bp];
	span += n;
	bits -= n;
	if (n < 8)        /* end of run */
	    goto done;
	bp++;
    }
    /*
     * Check partial byte on rhs.
     */
    if (bits > 0) {
	n = tab[*bp];
	span += (n > bits ? bits : n);
    }
done:
    *bpp = bp;
    return (span);
}

const u_char G3Base::zeroruns[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,    /* 0x00 - 0x0f */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,    /* 0x10 - 0x1f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* 0x20 - 0x2f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* 0x30 - 0x3f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0x40 - 0x4f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0x50 - 0x5f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0x60 - 0x6f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0x70 - 0x7f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x80 - 0x8f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x90 - 0x9f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0xa0 - 0xaf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0xb0 - 0xbf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0xc0 - 0xcf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0xd0 - 0xdf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0xe0 - 0xef */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0xf0 - 0xff */
};
const u_char G3Base::oneruns[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x00 - 0x0f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x10 - 0x1f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x20 - 0x2f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x30 - 0x3f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x40 - 0x4f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x50 - 0x5f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x60 - 0x6f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x70 - 0x7f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0x80 - 0x8f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0x90 - 0x9f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0xa0 - 0xaf */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 0xb0 - 0xbf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* 0xc0 - 0xcf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* 0xd0 - 0xdf */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,    /* 0xe0 - 0xef */
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8,    /* 0xf0 - 0xff */
};

/*
 * Return the offset of the next bit in the range
 * [bs..be] that is different from bs.  The end,
 * be, is returned if no such bit exists.
 */
int
G3Base::finddiff(const u_char* cp, int bs, int be)
{
    cp += bs >> 3;            /* adjust byte offset */
    return (bs + findspan(&cp, bs, be,
	(*cp & (0x80 >> (bs&7))) ? oneruns : zeroruns));
}
