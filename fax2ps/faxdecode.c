/* $Header: /usr/people/sam/tiff/contrib/fax2ps/RCS/faxdecode.c,v 1.20.2.1 1995/03/16 18:56:30 sam Exp $ */

/*
 * Copyright (c) 1991, 1992, 1993, 1994 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "defs.h"
#include "t4.h"
#include "g3states.h"

Fax3DecodeState fax;

static	int decode1DRow(TIFF*, u_char*, int);
static	int decode2DRow(TIFF*, u_char*, int);
static	void fillspan(char*, int, int);
static	int findspan(u_char**, int, int, const u_char*);
static	int finddiff(u_char*, int, int, int);

static u_char bitMask[8] =
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
#define	isBitSet(sp)    ((sp)->b.data & bitMask[(sp)->b.bit])

#define	is2DEncoding(tif)	(fax.is2d)
#define	fetchByte(tif, sp)	(fax.cc--, *fax.bp++)

#define	BITCASE(b)			\
    case b:				\
    code <<= 1;				\
	if (data & (1<<(7-b))) code |= 1;\
    len++;				\
    if (code > 0) { bit = b+1; break; }

/*
 * Skip over input until an EOL code is found.  The
 * value of len is passed as 0 except during error
 * recovery when decoding 2D data.  Note also that
 * we don't use the optimized state tables to locate
 * an EOL because we can't assume much of anything
 * about our state (e.g. bit position).
 */
static void
skiptoeol(TIFF* tif, int len)
{
    Fax3DecodeState *sp = &fax;
    register int bit = sp->b.bit;
    register int data = sp->b.data;
    int code = 0;

    (void) tif;
    /*
     * Our handling of ``bit'' is painful because
     * the rest of the code does not maintain it as
     * exactly the bit offset in the current data
     * byte (bit == 0 means refill the data byte).
     * Thus we have to be careful on entry and
     * exit to insure that we maintain a value that's
     * understandable elsewhere in the decoding logic.
     */
    if (bit == 0)            /* force refill */
        bit = 8;
    for (;;) {
        switch (bit) {
again:  BITCASE(0);
        BITCASE(1);
        BITCASE(2);
        BITCASE(3);
        BITCASE(4);
        BITCASE(5);
        BITCASE(6);
        BITCASE(7);
        default:
            if (fax.cc <= 0)
                return;
            data = fetchByte(tif, sp);
            goto again;
        }
        if (len >= 12 && code == EOL)
            break;
        code = len = 0;
    }
    sp->b.bit = bit > 7 ? 0 : bit;    /* force refill */
    sp->b.data = data;
}

/*
 * Return the next bit in the input stream.  This is
 * used to extract 2D tag values and the color tag
 * at the end of a terminating uncompressed data code.
 */
static int
nextbit(TIFF* tif)
{
    Fax3DecodeState *sp = &fax;
    int bit;

    (void) tif;
    if (sp->b.bit == 0 && fax.cc > 0)
        sp->b.data = fetchByte(tif, sp);
    bit = isBitSet(sp);
    if (++(sp->b.bit) > 7)
        sp->b.bit = 0;
    return (bit);
}

/*
 * Setup state for decoding a strip.
 */
int
FaxPreDecode(TIFF* tif)
{
    Fax3DecodeState *sp = &fax;

    sp->b.bit = 0;            /* force initial read */
    sp->b.data = 0;
    sp->b.tag = G3_1D;
    if (sp->b.refline)
        memset(sp->b.refline, 0x00, sp->b.rowbytes);
    /*
     * If image has EOL codes, they precede each line
     * of data.  We skip over the first one here so that
     * when we decode rows, we can use an EOL to signal
     * that less than the expected number of pixels are
     * present for the scanline.
     */
    if ((fax.options & FAX3_NOEOL) == 0) {
        skiptoeol(tif, 0);
        if (is2DEncoding(tif))
            /* tag should always be 1D! */
            sp->b.tag = nextbit(tif) ? G3_1D : G3_2D;
    }
    return (1);
}

/*
 * Fill a span with ones.
 */
static void
fillspan(register char* cp, register int x, register int count)
{
	static const unsigned char masks[] =
        { 0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

    if (count <= 0)
        return;
    cp += x>>3;
    if (x &= 7) {            /* align to byte boundary */
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
 * Decode the requested amount of data.
 */
int
Fax3DecodeRow(TIFF* tif, int npels)
{
    Fax3DecodeState *sp = &fax;
    int status;

    while (npels > 0) {
	/* decoding only sets non-zero bits */
	memset(sp->scanline, 0, sp->b.rowbytes);
        if (sp->b.tag == G3_1D)
            status = decode1DRow(tif, sp->scanline, sp->b.rowpixels);
        else
            status = decode2DRow(tif, sp->scanline, sp->b.rowpixels);
	if (status == G3CODE_EOF)
	    break;
        if (is2DEncoding(tif)) {
            /*
             * Fetch the tag bit that indicates
             * whether the next row is 1d or 2d
             * encoded.  If 2d-encoded, then setup
             * the reference line from the decoded
             * scanline just completed.
             */
            sp->b.tag = nextbit(tif) ? G3_1D : G3_2D;
            if (sp->b.tag == G3_2D)
                memcpy(sp->b.refline, sp->scanline, sp->b.rowbytes);
        }
        npels -= sp->b.rowpixels;
	fax.row++;
    }
    return (npels == 0);
}

int
Fax4DecodeRow(TIFF* tif, int npels)
{
    Fax3DecodeState *sp = &fax;

    while (npels > 0) {
	/* decoding only sets non-zero bits */
	memset(sp->scanline, 0, sp->b.rowbytes);
	if (!decode2DRow(tif, sp->scanline, sp->b.rowpixels))
	    return (0);
	memcpy(sp->b.refline, sp->scanline, sp->b.rowbytes);
	fax.row++;
        npels -= sp->b.rowpixels;
    }
    return (1);
}

/*
 * Decode a code and return the associated run length.
 */
static int
decode_run(TIFF* tif, const u_short fsm[][256])
{
    Fax3DecodeState *sp = &fax;
    int state = sp->b.bit;
    int action;
    int runlen = 0;

    (void) tif;
    for (;;) {
	if (state == 0) {
nextbyte:
	    if (fax.cc <= 0)
		return (G3CODE_EOF);
	    sp->b.data = fetchByte(tif, sp);
	}
	state = fsm[state][sp->b.data];
	action = state >> 8; state &= 0xff;
	if (action == ACT_INCOMP)
		goto nextbyte;
	sp->b.bit = state;
	action -= ACT_RUNT;
	if (action < 0)			/* ACT_INVALID or ACT_EOL */
		return (action);
	if (action < 64)
		return (runlen + action);
	runlen += 64*(action-64);
    }
    /*NOTREACHED*/
}

#define	decode_white_run(tif)	decode_run(tif, TIFFFax1DFSM+0)
#define	decode_black_run(tif)	decode_run(tif, TIFFFax1DFSM+8)

do_row()
{
}


/*
 * Process one row of 1d Huffman-encoded data.
 */
static int
decode1DRow(TIFF* tif, u_char* buf, int npels)
{
    Fax3DecodeState *sp = &fax;
    int x = 0;
    int dx = 0;
    int runlen;
    static const char module[] = "Fax3Decode1D";

    for (;;) {
	runlen = decode_white_run(tif);
	if (runlen < 0)
	    goto exception;
	if (x+runlen > npels)
	    runlen = npels-x;
	if (runlen > 0) {
	    x += runlen;
	    if (x >= npels)
		goto done;
	}
	runlen = decode_black_run(tif);
	if (runlen < 0)
	    goto exception;
	if (x+runlen > npels)
	    runlen = npels-x;
	if (runlen > 0) {
	    fillspan((char *)buf, x, runlen);
	    dx = (x += runlen);
	    if (x >= npels)
		goto done;
	}
    }
exception:
    switch (runlen) {
    case G3CODE_EOF:
	TIFFWarning(module, "%s: Premature EOF at scanline %d (x %d)",
	    TIFFFileName(tif), fax.row, x);
	return (G3CODE_EOF);
    case G3CODE_INVALID:	/* invalid code */
	/*
	 * An invalid code was encountered.
	 * Flush the remainder of the line
	 * and allow the caller to decide whether
	 * or not to continue.  Note that this
	 * only works if we have a G3 image
	 * with EOL markers.
	 */
	TIFFError(module, "%s: Bad code word at scanline %d (x %d)",
	   TIFFFileName(tif), fax.row, x);
	break;
    case G3CODE_EOL:	/* premature end-of-line code */
	TIFFWarning(module, "%s: Premature EOL at scanline %d (x %d)",
	    TIFFFileName(tif), fax.row, x);
	return (1);	/* try to resynchronize... */
    }
done:
    /*
     * Cleanup at the end of the row.  This convoluted
     * logic is merely so that we can reuse the code with
     * two other related compression algorithms (2 & 32771).
     *
     * Note also that our handling of word alignment assumes
     * that the buffer is at least word aligned.  This is
     * the case for most all versions of malloc (typically
     * the buffer is returned longword aligned).
     */
    if ((tif->tif_options & FAX3_NOEOL) == 0)
	skiptoeol(tif, 0);
    if (tif->tif_options & FAX3_BYTEALIGN)
	sp->b.bit = 0;
    if ((tif->tif_options & FAX3_WORDALIGN) && ((long)fax.bp & 1))
	(void) fetchByte(tif, sp);
    return (x == npels ? 1 : G3CODE_EOL);
}

/*
 * Group 3 2d Decoding support.
 */

/*
 * Return the next uncompressed mode code word.
 */
static int
decode_uncomp_code(TIFF* tif)
{
    Fax3DecodeState *sp = &fax;
    int code;

    (void) tif;
    do {
        if (sp->b.bit == 0 || sp->b.bit > 7) {
            if (fax.cc <= 0)
                return (UNCOMP_EOF);
            sp->b.data = fetchByte(tif, sp);
        }
        code = TIFFFaxUncompFSM[sp->b.bit][sp->b.data];
        sp->b.bit = code & 0xff; code >>= 8;
    } while (code == ACT_INCOMP);
    return (code);
}

/*
 * Process one row of 2d encoded data.
 */
static int
decode2DRow(TIFF* tif, u_char* buf, int npels)
{
#define	PIXEL(buf,ix)    ((((buf)[(ix)>>3]) >> (7-((ix)&7))) & 1)
    Fax3DecodeState *sp = &fax;
    int a0 = -1;
    int b1, b2;
    int dx = 0;
    int run1, run2;        /* for horizontal mode */
    int mode;
    int color = 0;
    static const char module[] = "Fax3Decode2D";

    do {
	do {
	    if (sp->b.bit == 0 || sp->b.bit > 7) {
		if (fax.cc <= 0) {
		    TIFFError(module,
		    "%s: Premature EOF at scanline %d",
			TIFFFileName(tif), fax.row);
		    return (0);
		}
		sp->b.data = fetchByte(tif, sp);
	    }
	    mode = TIFFFax2DFSM[sp->b.bit][sp->b.data];
	    sp->b.bit = mode & 0xff; mode >>= 8;
	} while (mode == MODE_NULL);
        switch (mode) {
        case MODE_PASS:
	    b2 = finddiff(sp->b.refline, a0, npels, !color);
	    b1 = finddiff(sp->b.refline, b2, npels, color);
	    b2 = finddiff(sp->b.refline, b1, npels, !color);
	    if (color) {
		if (a0 < 0)
		    a0 = 0;
		fillspan((char *)buf, a0, b2 - a0);
	    }
	    if (color) {
		dx = b2;
	    }
	    a0 = b2;
	    break;
        case MODE_HORIZ:
            if (color == 0) {
                run1 = decode_white_run(tif);
                run2 = decode_black_run(tif);
            } else {
                run1 = decode_black_run(tif);
                run2 = decode_white_run(tif);
            }
	    if (run1 >= 0 && run2 >= 0) {
		/*
		 * Do the appropriate fill.  Note that we exit
		 * this logic with the same color that we enter
		 * with since we do 2 fills.  This explains the
		 * somewhat obscure logic below.
		 */
		if (a0 < 0)
		    a0 = 0;
		if (a0 + run1 > npels)
		    run1 = npels - a0;
		if (color)
		    fillspan((char *)buf, a0, run1);
		if (color != 0) {
		    dx = a0 + run1;
		}
		a0 += run1;
		if (a0 + run2 > npels)
		    run2 = npels - a0;
		if (!color)
		    fillspan((char *)buf, a0, run2);
		if (!color != 0) {
		    dx = a0 + run2;
		}
		a0 += run2;
	    }
            break;
        case MODE_VERT_V0:
        case MODE_VERT_VR1:
        case MODE_VERT_VR2:
        case MODE_VERT_VR3:
        case MODE_VERT_VL1:
        case MODE_VERT_VL2:
        case MODE_VERT_VL3:
	    b2 = finddiff(sp->b.refline, a0, npels, !color);
	    b1 = finddiff(sp->b.refline, b2, npels, color);
	    b1 += mode - MODE_VERT_V0;
	    if (a0 < 0)
		a0 = 0;
	    if (color)
		fillspan((char *)buf, a0, b1 - a0);
	    if (color != 0) {
		dx = b1;
	    }
	    color = !color;
	    a0 = b1;
	    break;
	case MODE_UNCOMP:
            /*
             * Uncompressed mode: select from the
             * special set of code words.
             */
	    if (a0 < 0)
		a0 = 0;
            do {
                mode = decode_uncomp_code(tif);
                switch (mode) {
                case UNCOMP_RUN1:
                case UNCOMP_RUN2:
                case UNCOMP_RUN3:
                case UNCOMP_RUN4:
                case UNCOMP_RUN5:
                    run1 = mode - UNCOMP_RUN0;
                    fillspan((char *)buf, a0+run1-1, 1);
                    a0 += run1;
		    if (color != 0) {
			dx = a0;
		    }
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
                    color = nextbit(tif);
                    break;
                case UNCOMP_INVALID:
                    TIFFError(module,
                "%s: Bad uncompressed code word at scanline %d",
                        TIFFFileName(tif), fax.row);
                    goto bad;
                case UNCOMP_EOF:
                    TIFFError(module,
                        "%s: Premature EOF at scanline %d",
                        TIFFFileName(tif), fax.row);
                    return (0);
                }
            } while (mode < UNCOMP_EXIT);
            break;
	case MODE_ERROR_1:
            if ((fax.options & FAX3_NOEOL) == 0) {
                TIFFWarning(TIFFFileName(tif),
                    "%s: Premature EOL at scanline %d (x %d)",
                    module, fax.row, a0);
                skiptoeol(tif, 7);    /* seen 7 0's already */
                return (1);        /* try to synchronize */
            }
            /* fall thru... */
	case MODE_ERROR:
            TIFFError(TIFFFileName(tif),
                "%s: Bad 2D code word at scanline %d",
                module, fax.row);
            goto bad;
	default:
            TIFFError(TIFFFileName(tif),
                "%s: Panic, bad decoding state at scanline %d",
                module, fax.row);
            return (0);
        }
    } while (a0 < npels);
bad:
    /*
     * Cleanup at the end of row.  We check for
     * EOL separately so that this code can be
     * reused by the Group 4 decoding routine.
     */
    if ((fax.options & FAX3_NOEOL) == 0)
        skiptoeol(tif, 0);
    return (a0 >= npels ? 1 : G3CODE_EOL);
#undef	PIXEL
}

static const u_char zeroruns[256] = {
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
static const u_char oneruns[256] = {
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
findspan(u_char** bpp, int bs, int be, register const u_char* tab)
{
	register u_char *bp = *bpp;
	register int bits = be - bs;
	register int n, span;

    /*
     * Check partial byte on lhs.
     */
    if (bits > 0 && (n = (bs & 7))) {
	span = tab[(*bp << n) & 0xff];
	if (span > 8-n)		/* table value too generous */
	    span = 8-n;
	if (span > bits)	/* constrain span to bit range */
	    span = bits;
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
 * [bs..be] that is different from the specified
 * color.  The end, be, is returned if no such bit
 * exists.
 */
static int
finddiff(u_char* cp, int bs, int be, int color)
{
    cp += bs >> 3;            /* adjust byte offset */
    return (bs + findspan(&cp, bs, be, color ? oneruns : zeroruns));
}

/*
 * Turn a bit-mapped scanline into the appropriate sequence
 * of PostScript characters to be rendered.
 *  
 * Written by Bret D. Whissel,
 * Florida State University Meteorology Department
 * March 13-15, 1995.
 */
void 
print_scan_row(int w, u_char *scanline)
{
  int colormode = 0;
  int runlength, seqlength;
  int runs[2432];		/* An overestimate in most cases. */
  int i, s, e, t, l, bitsleft;
  char outstr[512];		/* Overestimate.  The number of required 
				   characters will be at worst (w+5)/6 + 3 */
  struct sarr {
    char *wstr, *bstr;
    int width;
  };
  /*
   *  These no longer need to be strings, merely chars, but I'm
   *  leaving this as is in case someone needs to generate multiple
   *  character encodings or escape sequences.
   */
  static struct sarr WBarr[] = {
    {"d", "n", 512},{"e", "o", 256},{"f", "p", 128},{"g", "q", 64},
    {"h", "r", 32}, {"i", "s", 16},  {"j", "t", 8}, {"k", "u", 4},
    {"l", "v", 2}, {"m", "w", 1}};
  static char *svalue[] = {
    " ", "!", "\"", "#", "$", "&", "'", "*",
    "+", ",", "-",  ".", "/", "0", "1", "2",
    "3", "4", "5",  "6", "7", "8", "9", ":",
    ";", "<", "=",  ">", "?", "@", "A", "B",
    "C", "D", "E",  "F", "G", "H", "I", "J",
    "K", "L", "M",  "N", "O", "P", "Q", "R",
    "S", "T", "U",  "V", "W", "X", "Y", "Z",
    "[", "]", "^",  "_", "`", "a", "b", "c",
  };

  /* First, re-convert the scanline into a sequence of runlengths. It
     would be cleaner to use the encoded information directly, but
     it seemed too complicated to understand the 2-D decoding process
     from the C code. */

  s = t = seqlength = 0;
  e = w;
  colormode = 0;
  while (t != e) {
    t = finddiff(scanline, s, e, colormode);
    runs[seqlength++] = t - s;
    s = t;
    colormode ^= 1;
  }

  if (seqlength <= 1)		/* If there were no sequences, (or
				   only whitespace) skip this row. */
    return;

  printf("%d m(", fax.row);	/* Startup */

  outstr[0] = '\0';

  /* colormode = 0 for white, 1 for black.  Toggle the colormode when a
     new runlength is read.

     If a runlength is greater than 6 pixels, then spit out black or
     white characters until the runlength drops to 6 or less.  Once
     a runlength is <= 6, then combine black and white runlengths
     until a 6-pixel pattern is obtained.  Then write out the special
     character.  Six-pixel patterns were selected since 64 patterns
     is the largest power of two less than the 92 "easily printable"
     PostScript characters (i.e., no escape codes or octal chars).

  */

  colormode = 1;		/* First thing we do is change color in
				   the main loop. */
  runlength = 0;
  i = 0;
  while(i < seqlength) {
    if (!runlength) {
      colormode ^= 1;
      runlength = runs[i++];
      if (!colormode && i == seqlength)	
	break;			/* Don't bother printing the final run
				   of white. */
    }
    l = 0;
    while (runlength > 6) {	/* Run is greater than six... */
      if (runlength >= WBarr[l].width) {
	strcat(outstr, colormode ? WBarr[l].bstr : WBarr[l].wstr);
	runlength -= WBarr[l].width;
      } else l++;
    }
    while (runlength > 0 && runlength <= 6) {
      bitsleft = 6; t = 0;
      while (bitsleft) {
	if (runlength <= bitsleft) {
	  if (colormode)
	    t |= ((1 << runlength)-1) << (bitsleft-runlength);
	  bitsleft -= runlength;
	  runlength = 0;
	  if (bitsleft) {
	    if (i < seqlength) {
	      colormode ^= 1;
	      runlength = runs[i++];
	    } else 
	      break;
	  }
	} else {		/* runlength exceeds bits left */
	  if (colormode)
	    t |= ((1 << bitsleft)-1);
	  runlength -= bitsleft;
	  bitsleft = 0;
	}
      } /* while (bitsleft) */
      strcat(outstr, svalue[t]);
    } /* while (runlength > 0 && runlength <= 6) */
  } /* while (i < seqlength) */
  
  strcat(outstr, ")s");

#ifdef SHORT_POSTSCRIPT_LINES

  /* For older PostScript interpreters which may limit the number of 
     characters on one line, break up the output line into bite-size 
     pieces.  The newline character prints nuthin' in the encoding
     vector, so it should do no harm to spit out newlines at random 
     places in the output string. */

  e = strlen(outstr);
  i = 0;
  while (e--) {
    putc(outstr[i++], stdout);
    if (!(i%72) && e > 5)
      putc('\n', stdout);
  }
  putc('\n', stdout);

#else

  puts(outstr);

#endif
}
