/*
 * Bitmap bitblt support.
 *
 * This code supports only the following operations:
 *    0x0	F_CLR		dst <- 0
 *    0x5	F_NOT		dst <- ~src
 *    0x6	F_XOR		dst <- dst ^ src
 *    0x8	F_AND		dst <- dst & src
 *    0xc	F_STORE		dst <- src
 *    0xe	F_OR		dst <- dst | src
 *    0xf	F_SET		dst <- 1
 */
#include "Bitmap.h"

/*
 * The possible rasterops.
 * Given opcode i, the correct
 * function to execute it is Fi.
 */
#define F0(d,s) (0)
#define F5(d,s) (~(d))
#define F6(d,s) ((d)^(s))
#define F8(d,s) ((d)&(s))
#define FC(d,s) (s)
#define FE(d,s) ((d)|(s))
#define FF(d,s) (~0)

/* short versions of the above */
#define S0(d,s) (0)
#define S5(d,s) st(~st(d))
#define S6(d,s) st(st(d)^st(s))
#define S8(d,s) st(st(d)&st(s))
#define SC(d,s) st(s)
#define SE(d,s) st(st(d)|st(s))
#define SF(d,s) st(~0)

/* macros to exeucte some of the unary rasterops under a mask */
#define M0(m)	(*dst &= ~(m))
#define M5(m)	(*dst ^= (m))
#define MF(m)	(*dst |= (m))

/* macros used for top->bottom, left->right inversions */
#define ADD1(x) ((x)++) 
#define SUB1(x) ((x)--)
#define ADD1inverse SUB1
#define SUB1inverse ADD1

#define fast(a,b) {a;}
#define fastT(t,a,b) if (t) {a;} else {b;}
#define slow(a,b) {b;}
#define slowT(t,a,b) {b;}

/*
 * CASE_BINARY_OP executes a macro for an opcode.
 * It effectively translates a variable "op" argument into
 * a constant.  That constant will eventually be used
 * to construct rasterop function names.  The "speed"
 * argument to the called macro is either "fast" or
 * "slow", depending on whether or not the fast version
 * of the rasterop should be compiled.  Speed(a,b) acts
 * as a boolean that executes "a" if speed is fast, and
 * "b" if speed is slow.  This trick of using macros as
 * compile time booleans is used frequently.
 */
#define CASE_BINARY_OP(op,macro,incr,ssize,dsize,incr2,incsize)	\
    switch(op) {						\
	CASEOP(6,macro,incr,fast,ssize,dsize,incr2,incsize)	\
	CASEOP(8,macro,incr,slow,ssize,dsize,incr2,incsize)	\
	CASEOP(C,macro,incr,fast,ssize,dsize,incr2,incsize)	\
	CASEOP(E,macro,incr,fast,ssize,dsize,incr2,incsize)	\
    }

/*
 * Execute the code s count times.
 */
#define rolledloop(count,s) {					\
    register int cnt;						\
    if ((cnt = (count)-1)>=0)					\
	do {s;} while (--cnt != -1);				\
}

#define atlsrc	(((*src)<<16) | *(src+1))
#define lsrc	((long *)src)
#define ldst	((long *)dst)
#define st(x)	((short)(x))

#define always(x,y) {x;}
#define never(x,y) {y;}

/*
 * The `widecols' test asks whether or not a row
 * in this rop touches more than one word.
 */
#define whenwide(x,y) {if(widecols>=0) {x;} else {y;}}

/*
 * Execute a rasterop, making the widecols/speed tradeoff.
 * incr indicates whether we're starting from the upper left or
 * the lower right (it's either ADD1 or SUB1).
 */
#define CASEOP(op,macro,incr,speed,ssize,dsize,incr2,incsize)		\
    case fxCAT(0x,op):							\
	speed (								\
	    whenwide (							\
		macro(op,incr,always,speed,ssize,dsize,incr2,incsize),	\
	        macro(op,incr,never,speed,ssize,dsize,incr2,incsize)	\
	    ),								\
	    macro(op,incr,whenwide,speed,ssize,dsize,incr2,incsize)	\
	)								\
	break;

/*
 * Use ROPCAST to reset src and dst pointers since they
 * can be pointer to long or pointer to short.
 */
#define ROPCAST(ssize,dsize)						\
	src = ssize ((int)src + deltasrc);				\
	dst = dsize ((int)dst + deltadst);

/* 
 * These are for doing a special increment/decrement in
 * BINARY_ROP_SHIFT0.  We want to increment src/dst by
 * twice its size, so take what it's cast to and divide
 * by what it is declared as.  NOTE that this is a kludge
 * and is right on the edge of making the macro expansion
 * of CASE_BINARY_OP too long (generating an error in the pre-
 * processor).  ALSO - if left-hand-casts aren't accepted
 * at all by the compiler, further changes will need to
 * be made here because there still are some.
 */
#define ADD2(x,sizex)	((x) += ((sizeof(long))/(sizeof sizex)))
#define SUB2(x,sizex)	((x) -= ((sizeof(long))/(sizeof sizex)))

/*
 * Do a rasterop where the shift is 0: that is, the source
 * and destination are aligned.
 *
 * Add 2 args: incr2 for incrementing/decrementing a cast pointer; and 
 * incsize, the size used to do the special increment/decrement.
 * UGLY because it causes 2 extra args to be passed through all the other
 * macros, but for the moment it compiles.  NOTE that if src and dst are
 * ever not the same type (as is the case in a couple of spots), this will
 * be trouble because this change assumes they are both of the same type.
 */
#define BINARY_ROP_SHIFT0(op,incr,multiword,ssize,dsize,incr2,incsize)	\
    for (shift = dh; --shift != -1;) {					\
	*dst = st(fxCAT(S,op)(*dst, *incr(src))&mask1)|st(*dst&notmask1);\
	incr(dst);							\
        multiword (							\
	    rolledloop(widecols, (					\
		(*dst = fxCAT(S,op)(*dst,*incr(src))),			\
		incr(dst)						\
	    ));								\
	    *dst = st(fxCAT(S,op)(*dst, *incr(src))&mask2)|st(*dst&notmask2);\
	    incr(dst);							\
	,)								\
        ROPCAST(ssize,dsize)						\
    }

#define neverodd(a,b) a;
#define alwaysodd(a,b) if(widecols&1) {a;} else {b;}
#define whenwideodd(a,b) if(widecols&1) {a;} else {b;}

/*
 * Do a rasterop: this contains the code for shift == 0
 * Otherwise it invokes GENERAL_BINARY_ROP appropriately.
 */
#define BINARY_ROP(op,incr,multiword,speed,ssize,dsize,incr2,incsize) {	\
  multiword (, mask1 &= mask2; notmask1 = ~mask1; )			\
  fxCAT(speed,T) (shift == 0, 						\
    BINARY_ROP_SHIFT0(op,incr,multiword,ssize,dsize,incr2,incsize),	\
    GENERAL_BINARY_ROP(op,incr,multiword,ssize,dsize)			\
  )									\
}

/*
 * The meat of rasterop; it does the rasterop 16 bits at a time:
 * it fetches a long source word, shifts it to align it with the
 * destination, then does a short operation to memory.
 */
#define GENERAL_BINARY_ROP(op,incr,multiword,ssize,dsize) {	\
    int rows;								\
    for (rows = dh; --rows >= 0; ) {					\
	*dst = st(st(fxCAT(S,op)(*dst,st(atlsrc>>shift))) & mask1) |	\
	    st(*dst & notmask1);					\
	incr(dst); incr(src);						\
        multiword (							\
	    rolledloop(widecols, (					\
		*dst = fxCAT(S,op)(*dst,st(atlsrc>>shift)), 		\
	        incr(src), incr(dst)					\
	     ));			 				\
	    *dst = st(st(fxCAT(S,op)(*dst,st(atlsrc>>shift))) & mask2) |\
		st(*dst & notmask2);					\
	    incr(dst); incr(src);					\
	,)								\
        ROPCAST(ssize,dsize)						\
    }									\
}

/*
 * Unary (destination only) rasterop are relatively trivial.
 * They only bother to exploit the widecols test.
 */
#define UNARY_ROP(op,incr,multiword,speed,ssize,dsize,incr2,incsize) {	\
    register int rows;							\
    for (rows = dh; --rows >= 0; ) {					\
        fxCAT(M,op)(mask1); incr(dst);					\
        multiword (							\
	    rolledloop(widecols, (					\
		(*dst = fxCAT(F,op)(*dst,color)),			\
		incr(dst)						\
	    ));								\
	    fxCAT(M,op)(mask2); incr(dst);				\
	,)								\
        dst = (dsize) ((int)dst + deltadst);				\
    }									\
}

void
fxBitmap::bitblt(int dx, int dy, const fxBitmap* bsrc, const fxBoundingBox &sr, fxBitmapCode c)
{
    register int deltadst;
    register int deltasrc;
    register u_short* dst;
    register u_short* src;
    register shift;
    register u_short mask1;
    register u_short mask2;
    register u_short notmask1;
    register u_short notmask2;
    int sx = sr.x;
    int sy = sr.y;

    dx += r.x;
    dy += r.y;
    int dw = sr.w;
    int dh = sr.h;
    // clip destination to r
    if (dx < r.x) {
	dw -= r.x-dx;
	sx += r.x-dx;
	dx = r.x;
    } else if (dx+dw > r.x+r.w)
	dw = r.x+r.w-dx;
    if (dy < r.y) {
	dh -= r.y-dy;
	sy += r.y-dy;
	dy = r.y;
    } else if (dy+dh > r.y+r.h)
	dh = r.y+r.h-dy;
    dw = fxmin(fxmax(dw, 0), r.w);
    dh = fxmin(fxmax(dh, 0), r.h);
    int widecols = ((dx+dw-1)>>4) - (dx>>4) - 1;
    int bytes_per_row = (widecols+2)*2;
    if (bsrc == 0 || c == F_CLR || c == F_SET || c == F_NOT) {
	dst = addr(dx, dy);
	mask1 = 0xffff >> (dx&0xf);
	mask2 = 0xffff << (15 - ((dx+dw-1)&0xf));
	if (widecols < 0)
	    mask1 &= mask2;
	deltadst = rowbytes - bytes_per_row;
	switch (c) {
	CASEOP(0, UNARY_ROP, ADD1, fast, void, u_short*, 0, 0)
	CASEOP(5, UNARY_ROP, ADD1, fast, void, u_short*, 0, 0)
	CASEOP(F, UNARY_ROP, ADD1, fast, void, u_short*, 0, 0)
	}
    } else {
	deltadst = rowbytes;
	dst = addr(dx, dy);
	deltasrc = bsrc->rowbytes;
	sx += bsrc->r.x;
	sy += bsrc->r.y;
	src = bsrc->addr(sx, sy);
	if ((shift = (dx&0xf) - (sx&0xf)) < 0)
	    shift += 16;
	else if (shift > 0)
	    src--;
	if (dy < sy && bits == bsrc->bits) {		// bottom to top
	    src = (u_short*)((int)src + (dh-1)*deltasrc);
	    dst = (u_short*)((int)dst + (dh-1)*deltadst);
	    deltasrc = -deltasrc;
	    deltadst = -deltadst;
	}
	if (dx < sx || dy != sy || bits != bsrc->bits) {// left to right
	    deltasrc -= bytes_per_row;
	    deltadst -= bytes_per_row;
	    mask1 = 0xffff >> (dx&0xf);
	    notmask1 = ~mask1;
	    mask2 = 0xffff << (15 - ((dx+dw-1)&0xf));
	    notmask2 = ~mask2;
	    CASE_BINARY_OP(c, BINARY_ROP, ADD1,
		(u_short*), (u_short*), ADD2, (u_short));
	} else {					// right to left
	    deltasrc += bytes_per_row;
	    deltadst += bytes_per_row;
	    src = (u_short *)((int)src + bytes_per_row - sizeof (u_short));
	    dst = (u_short *)((int)dst + bytes_per_row - sizeof (u_short));
	    mask1 = 0xffff << (15 - ((dx+dw-1) & 0xf));
	    notmask1 = ~mask1;
	    mask2 = 0xffff >> (dx&0xf);
	    notmask2 = ~mask2;
	    CASE_BINARY_OP(c, BINARY_ROP, SUB1,
		(u_short *), (u_short *), SUB2, (u_short));
	}
    }
}
