#ident	$Header: /d/sam/flexkit/fax/fax2ps/RCS/faxdecode.c,v 1.6 91/05/23 14:55:01 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "defs.h"
#include "t4.h"
#include "g3codes.h"
#include <assert.h>

#define	WHITE	0
#define	BLACK	1

Fax3DecodeState fax;

static	int decode1DRow(TIFF*, u_char*, int);
static	int decode2DRow(TIFF*, u_char*, int);
static	void fillspan(char*, int, int);
static	void emitcode(TIFF*, int, int, int);
static	int findspan(u_char**, int, int, u_char*);
static	int finddiff(u_char*, int, int);

#define	fetchByte(tif, sp)	(fax.cc--, *fax.bp++)

#define	BITCASE(b)			\
    case b:				\
	code <<= 1;			\
	if (data & b) code |= 1;	\
	len++;				\
	if (code > 0) { bit = (b>>1); break; }

static void
skiptoeol(TIFF* tif)
{
    Fax3DecodeState *sp = &fax;
    register int bit = sp->b.bit;
    register int data = sp->b.data;
    int code, len;

    do {
        code = len = 0;
	switch (bit) {
again:	BITCASE(0x80);
	BITCASE(0x40);
	BITCASE(0x20);
	BITCASE(0x10);
	BITCASE(0x08);
	BITCASE(0x04);
	BITCASE(0x02);
	BITCASE(0x01);
	default:
	    if (fax.cc <= 0) {
		TIFFError(TIFFFileName(tif),
		    "skiptoeol: Premature EOF at scanline %d",
		    TIFFCurrentRow(tif));
		    return;
	    }
	    data = fetchByte(tif, sp);
	    goto again;
	}
    } while (len < 12 || code != EOL);
    sp->b.bit = bit;
    sp->b.data = data;
}

static void
fillspan(char* cp, int x, int count)
{
    static unsigned char masks[] =
	{ 0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

    cp += x>>3;
    if (x &= 7) {			/* align to byte boundary */
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

static void
emitcode(TIFF* tif, int dx, int x, int count)
{
    CodeEntry* thisCode;

    switch (fax.pass) {
    case 1:	/* count potential code & pair use */
	thisCode = enterCode(x-dx, count);
	thisCode->c.count++;
	if (dopairs) {
	    if (fax.lastCode)
		enterPair(fax.lastCode, thisCode)->c.count++;
	    fax.lastCode = thisCode;
	}
	break;
    case 2:	/* rescan w/ potential codes */
	thisCode = enterCode(x-dx, count);
	if (fax.lastCode) {
	    CodePairEntry* pair =
		findPair(fax.lastCode, thisCode);
	    if (pair) {
		pair->c.count++;
		fax.lastCode = 0;
	    } else {
		fax.lastCode->c.count++;
		fax.lastCode = thisCode;
	    }
	} else
	    fax.lastCode = thisCode;
	break;
    case 3:	/* generate encoded output */
	thisCode = enterCode(x-dx, count);
	if (dopairs) {
	    if (fax.lastCode) {
		if (!printPair(tif, fax.lastCode, thisCode)) {
		    printCode(tif, fax.lastCode);
		    fax.lastCode = thisCode;
		} else
		    fax.lastCode = 0;
	    } else
		fax.lastCode = thisCode;
	} else
	    printCode(tif, thisCode);
	break;
    }
}

/*
 * Decode the requested amount of data.
 */
int
Fax3DecodeRow(TIFF* tif, int npels)
{
    Fax3DecodeState *sp = &fax;
    int scanlinesize = TIFFScanlineSize(tif);

    fax.lastCode = 0;
    /* decoding only sets non-zero black bits */
    if (fax.is2d)
	bzero(sp->scanline, scanlinesize);
    if (sp->b.tag == G3_1D) {
	if (!decode1DRow(tif, sp->scanline, npels))
	    return (0);
    } else {
	if (!decode2DRow(tif, sp->scanline, npels))
	    return (0);
    }
    if (fax.is2d) {
	/*
	 * Fetch the tag bit that indicates
	 * whether the next row is 1d or 2d
	 * encoded.  If 2d-encoded, then setup
	 * the reference line from the decoded
	 * scanline just completed.
	 */
	if (sp->b.bit == 0) {
	    if (fax.cc <= 0) {
		TIFFError("Fax3Decode", "%s: Premature EOF at scanline %d",
		    TIFFFileName(tif), TIFFCurrentRow(tif));
		return (0);
	    }
	    sp->b.data = fetchByte(tif, sp);
	    sp->b.bit = 0x80;
	}
	sp->b.tag = (sp->b.data & sp->b.bit) ? G3_1D : G3_2D;
	if (sp->b.tag == G3_2D)
	    bcopy(sp->scanline, sp->b.refline, scanlinesize);
	sp->b.bit >>= 1;
    }
    switch (sp->pass) {
    case 2:
	if (fax.lastCode)
	    fax.lastCode->c.count++;
	break;
    case 3:
	if (fax.lastCode)
	    printCode(tif, fax.lastCode);
	break;
    }
    return (1);
}

/*
 * Process one row of 1d Huffman-encoded data.
 */
static int
decode1DRow(TIFF* tif, u_char* buf, int npels)
{
    Fax3DecodeState *sp = &fax;
    short bit = sp->b.bit;
    short data = sp->b.data;
    int x = 0;
    int dx = 0;
    int count = 0;
    short len = 0;
    short code = 0;
    short color = WHITE;
    tableentry *te;
    static char module[] = "Fax3Decode1D";

    if (debug)
	printf("row %d: 1D\n", TIFFCurrentRow(tif));
    for (;;) {
	switch (bit) {
again:	BITCASE(0x80);
	BITCASE(0x40);
	BITCASE(0x20);
	BITCASE(0x10);
	BITCASE(0x08);
	BITCASE(0x04);
	BITCASE(0x02);
	BITCASE(0x01);
	default:
	    if (fax.cc <= 0) {
		TIFFError(module, "%s: Premature EOF at scanline %d (x %d)",
		    TIFFFileName(tif), TIFFCurrentRow(tif), x);
		return (0);
	    }
	    data = fetchByte(tif, sp);
	    goto again;
	}
	if (len >= 12) {
	    if (code == EOL) {
		if (x == 0) {
		    code = len = 0;
		    continue;
		}
		sp->b.bit = bit;
		sp->b.data = data;
		TIFFWarning(TIFFFileName(tif),
		    "%s: Premature EOL at scanline %d (x %d)",
		    module, TIFFCurrentRow(tif), x);
		return (1);	/* try to resynchronize... */
	    }
	    if (len > 13) {
		TIFFError(TIFFFileName(tif),
       "%s: Bad code word (len %d code 0x%x) at scanline %d (x %d)",
		    module, len, code, TIFFCurrentRow(tif), x);
		break;
	    }
	}
	if (color == WHITE) {
	    u_char ix = TIFFFax3wtab[code << (13-len)];
	    if (ix == 0xff)
		continue;
	    te = &TIFFFaxWhiteCodes[ix];
	} else {
	    u_char ix = TIFFFax3btab[code << (13-len)];
	    if (ix == 0xff)
		continue;
	    te = &TIFFFaxBlackCodes[ix];
	}
	if (te->length != len)
	    continue;
	count += te->runlen;
	if (te->runlen < 64) {		/* terminating code */
	    if (x+count > npels)
		count = npels-x;
	    if (count > 0) {
		if (color == BLACK) {
		    if (fax.is2d)
			fillspan((char*) buf, x, count);
		    emitcode(tif, dx, x, count);
		    dx = x+count;
		}
		x += count;
		if (x >= npels)
		    break;
	    }
	    count = 0;
	    color = !color;
	}
	code = len = 0;
    }
    sp->b.data = data;
    sp->b.bit = bit;
    skiptoeol(tif);
    return (x == npels);
}

/*
 * Group 3 2d Decoding support.
 */
/* NB: the order of the decoding modes is used below */
#define	MODE_HORIZ	0	/* horizontal mode, handling 1st runlen */
#define	MODE_HORIZ1	1	/* horizontal mode, handling 2d runlen */
#define	MODE_2D		2	/* look for 2D codes */
#define	MODE_UNCOMP	3	/* uncompressed mode */

#include "g32dtab.h"

#define	PIXEL(buf,ix)	((((buf)[(ix)>>3]) >> (7-((ix)&7))) & 1)

static int
findb1(Fax3DecodeState* sp, int a0, short color, int npels)
{
    int b1;

    if (a0 || PIXEL(sp->b.refline, 0) == WHITE)
	b1 = finddiff(sp->b.refline, a0, npels);
    else
	b1 = 0;
    if (PIXEL(sp->b.refline, b1) == color)
	b1 = finddiff(sp->b.refline, b1, npels);
    return (b1);
}

/*
 * Process one row of 2d encoded data.
 */
static int
decode2DRow(TIFF* tif, u_char* buf, int npels)
{
    Fax3DecodeState *sp = &fax;
    short bit = sp->b.bit;
    short data = sp->b.data;
    short len = 0;
    short code = 0;
    int a0 = 0;
    int a1 = 0;
    int b1 = 0;
    int b2 = 0;
    int dx = 0;
    int count = 0;
    short mode = MODE_2D;
    short color = WHITE;
    static char module[] = "Fax3Decode2D";

    if (debug)
	printf("row %d: 2D\n", TIFFCurrentRow(tif));
    do {
	switch (bit) {
again:	BITCASE(0x80);
	BITCASE(0x40);
	BITCASE(0x20);
	BITCASE(0x10);
	BITCASE(0x08);
	BITCASE(0x04);
	BITCASE(0x02);
	BITCASE(0x01);
	default:
	    if (fax.cc <= 0) {
		TIFFError(module, "%s: Premature EOF at scanline %d",
		    TIFFFileName(tif), TIFFCurrentRow(tif));
		return (0);
	    }
	    data = fetchByte(tif, sp);
	    goto again;
	}
	if (len >= 12) {
	    if (code == EOL) {
		if (a0 == 0) {
		    code = len = 0;
		    continue;
		}
		sp->b.bit = bit;
		sp->b.data = data;
		TIFFWarning(TIFFFileName(tif),
		    "%s: Premature EOL at scanline %d (x %d)",
		    module, TIFFCurrentRow(tif), a0);
		return (1);	/* try to resynchronize... */
	    }
	    if (len > 13) {
		TIFFError(TIFFFileName(tif),
	"%s: Bad code word (len %d code 0x%x) at scanline %d",
		    module, len, code, TIFFCurrentRow(tif));
		break;
	    }
	}
	if (mode == MODE_2D) {			/* 2d codes */
	    int v;
	    assert(PACK(code,len) < NG32D);
	    v = g32dtab[PACK(code,len)];
	    if (v == 0)
		continue;
	    switch (UNPACKMODE(v)) {
	    case PASS:
		b1 = findb1(sp, a0, color, npels);
		b2 = finddiff(sp->b.refline, b1, npels);
		if (debug)
		    printf("PASS %s a0 %d b1 %d b2 %d\n",
			color ? "black" : "white", a0, b1, b2);
		assert(a0 <= b2);
		if (color == BLACK) {
		    fillspan((char*) buf, a0, b2 - a0);
		    emitcode(tif, dx, a0, b2 - a0);
		    dx = b2;
		}
		a0 = b2;
		break;
	    case HORIZONTAL:
		if (debug)
		    printf("HORIZ");
		mode = MODE_HORIZ;
		count = 0;
		break;
	    case VERTICAL:
		b1 = findb1(sp, a0, color, npels);
		a1 = b1 + UNPACKINFO(v);
		if (debug)
		    printf("VERT %s a0 %d b1 %d a1 %d\n",
			color ? "black" : "white", a0, b1, a1);
		assert(a0 <= a1);
		if (color == BLACK) {
		    fillspan((char*) buf, a0, a1 - a0);
		    emitcode(tif, dx, a0, a1 - a0);
		    dx = a1;
		}
		a0 = a1;
		color = !color;
		break;
	    case EXTENSION:
		if (debug)
		    printf("EXT %d\n", UNPACKINFO(v));
		if (UNPACKINFO(v) != UNCOMPRESSED) {
		    sp->b.bit = bit;
		    sp->b.data = data;
		    TIFFError(TIFFFileName(tif),
			"%s: scanline %d, Extension %d is not implemented",
			module, TIFFCurrentRow(tif), UNPACKINFO(v));
		    return (1);
		}
		mode = MODE_UNCOMP;
		break;
	    }
	} else if (mode == MODE_UNCOMP) {	/* uncompressed mode */
	    /*
	     * Select from the special set of code words.
	     */
	    assert(code <= 3);
	    switch (PACK(code,len)) {
	    case PACK(1,1):
	    case PACK(1,2):
	    case PACK(1,3):
	    case PACK(1,4):
	    case PACK(1,5):
		fillspan((char*) buf, a0+len-1, 1);
		emitcode(tif, dx, a0-1, 1);
		a0 += len;
		dx = a0;
		break;
	    case PACK(1,6):
		a0 += 5;
		break;
	    case PACK(1,7):
		continue;
	    case PACK(2, 8): case PACK(3, 8):
	    case PACK(2, 9): case PACK(3, 9):
	    case PACK(2,10): case PACK(3,10):
	    case PACK(2,11): case PACK(3,11):
	    case PACK(2,12): case PACK(3,12):
		a0 += len - 8;
		mode = MODE_2D;
		color = (code == 2 ? WHITE : BLACK);
		break;
	    }
	} else {				/* horizontal mode */
	    tableentry *te;
	    /*
	     * In horizontal mode, collect 1d code
	     * words that represent a0.a1 and a1.a2.
	     */
	    if (color == WHITE) {
		u_char ix = TIFFFax3wtab[code << (13-len)];
		if (ix == 0xff)
		    continue;
		te = &TIFFFaxWhiteCodes[ix];
	    } else {
		u_char ix = TIFFFax3btab[code << (13-len)];
		if (ix == 0xff)
		    continue;
		te = &TIFFFaxBlackCodes[ix];
	    }
	    if (te->length != len)
		continue;
	    count += te->runlen;
	    if (te->runlen < 64) {	/* terminating code */
		if (debug)
		    printf(" %s/%d", color ? "black" : "white", count);
		if (a0 + count > npels)
		    count = npels - a0;
		if (count > 0) {
		    if (color == BLACK) {
			fillspan((char*) buf, a0, count);
			emitcode(tif, dx, a0, count);
			dx = a0 + count;
		    }
		    a0 += count;
		}
		mode++;			/* NB: assumes state ordering */
		count = 0;
		color = !color;
		if (debug && mode == MODE_2D)
		    putchar('\n');
	    }
	}
	code = len = 0;
    } while (a0 < npels || mode == MODE_HORIZ1);
    sp->b.data = data;
    sp->b.bit = bit;
    skiptoeol(tif);
    return (a0 >= npels);
}

static u_char zeroruns[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,	/* 0x00 - 0x0f */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/* 0x10 - 0x1f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0x20 - 0x2f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0x30 - 0x3f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x40 - 0x4f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x50 - 0x5f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x60 - 0x6f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x70 - 0x7f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 - 0x8f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 - 0x9f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 - 0xaf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 - 0xbf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 - 0xcf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 - 0xdf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 - 0xef */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xf0 - 0xff */
};
static u_char oneruns[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 - 0x0f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 - 0x1f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x20 - 0x2f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x30 - 0x3f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x40 - 0x4f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x50 - 0x5f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x60 - 0x6f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x70 - 0x7f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x80 - 0x8f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x90 - 0x9f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0xa0 - 0xaf */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0xb0 - 0xbf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0xc0 - 0xcf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0xd0 - 0xdf */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/* 0xe0 - 0xef */
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8,	/* 0xf0 - 0xff */
};

/*
 * Bit handling utilities.
 */

/*
 * Find a span of ones or zeros using the supplied
 * table.  The byte-aligned start of the bit string
 * is supplied along with the start+end bit indices.
 * The table gives the number of consecutive ones or
 * zeros starting from the msb and is indexed by byte
 * value.
 */
static int
findspan(u_char** bpp, int bs, int be, u_char* tab)
{
    u_char *bp = *bpp;
    int bits = be - bs;
    int n, span;

    /*
     * Check partial byte on lhs.
     */
    if (bits > 0 && (n = (bs & 7))) {
	span = tab[(*bp << n) & 0xff];
	if (span > 8-n)		/* table value too generous */
	    span = 8-n;
	if (n+span < 8)		/* doesn't extend to edge of byte */
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
	if (n < 8)		/* end of run */
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

/*
 * Return the offset of the next bit in the range
 * [bs..be] that is different from bs.  The end,
 * be, is returned if no such bit exists.
 */
static int
finddiff(u_char* cp, int bs, int be)
{
    cp += bs >> 3;			/* adjust byte offset */
    return (bs + findspan(&cp, bs, be,
	(*cp & (0x80 >> (bs&7))) ? oneruns : zeroruns));
}
