/*
 * Bitmap line drawing support.
 */
#include "Bitmap.h"

#define	SWAP(type,a,b)	{ type t; t = a; a = b; b = t; }
/*
 * Rasterops for source
 *     all zeros: fxCAT(SF, (op & 3))
 * or  all ones:  fxCAT(SF, (op >> 2))
 */
#define SF0(d,s) ((d)&~(s))
#define SF1(d,s) ((d)^(s))
#define SF2(d,s) (d)
#define SF3(d,s) ((d)|(s))   

/* Macros to mask the ends of horizontal vectors */
#define M0(m)   (*dst &= ~(m))
#define M1(m)   (*dst ^= (m))
#define M2(m)   (*dst = (*dst&~m) | (color&m))
#define M3(m)   (*dst |= (m))

/* Macro to execute a macro for an opcode. */
#define CASE_BINARY_OP(op,macro,dsize)			\
    switch((int)op&3) {					\
	CASEOP(0,macro,dsize)				\
	CASEOP(1,macro,dsize)				\
	CASEOP(2,macro,dsize)				\
	CASEOP(3,macro,dsize)				\
    }
#define CASEOP(op,macro,dsize)				\
    case fxCAT(0x,op): macro(op,dsize);			\
	break;

/*
 * Execute the code s count times.
 */
#define rolledloop(count,s) {				\
    register int cnt;					\
    if ((cnt = (count)-1)>=0)				\
	do {s;} while (--cnt != -1);			\
}

/*
 * Macro to draw points in bitmaps.
 */
#define POINT(op, dsize)				\
    *dst = fxCAT(SF,op)(*dst,color)

/*
 * Macro to do horizontal vectors.
 * If source is all zeros, op = op&3,
 * else if source is all ones op = op>>2.
 */
#define HORIZ(op,dsize) {				\
    fxCAT(M,op)(mask1); dst++;				\
    rolledloop(count,*dst = fxCAT(SF,op)(*dst,colorlong); dst++;)	\
    if (count >= 0) {fxCAT(M,op)(mask2); dst++;}		\
}

/*
 * Macro to do vertical vectors. 
 * If source is all zeros, op = op&3,
 * else if source is all ones op = op>>2.
 */
#define VERT(op,dsize) {				\
    rolledloop(count,*dst = fxCAT(SF,op)(*dst,color);	\
	    dst = dsize ((int) dst + (int)vrtstep);)	\
}

#define MAJORX(op,dsize)				\
    do {						\
        do {						\
            POINT(op,void);				\
            if((color >>= 1) == 0) {color = 0x8000; dst++;}	\
        } while (((error += dy) <= 0) && (--count != -1));	\
        error -= dx;					\
        dst = dsize ((int) dst + (int)vrtstep);		\
    } while ((count >= 0) && (--count != -1));
#define MAJORY(op,dsize)				\
    do {						\
        do {						\
            POINT(op,void);				\
            dst = dsize ((int)dst + (int)vrtstep);	\
        } while (((error += dy) <= 0) && (--count != -1));	\
        error -= dx;					\
        if((color >>= 1) == 0) {color = 0x8000; dst++;};\
    } while ((count >= 0) && (--count != -1));

#define VISIBLE(x,y)					\
    ((unsigned int)(x) < xsize && (unsigned int)(y) < ysize)

void
fxBitmap::segment(int x0, int y0, int x1, int y1, fxBitmapCode c)
{
    register int dx, dy, count, error;
    register u_int color;
    register u_char *vrtstep;
    int xstart;
    int ystart;
    u_int xsize;
    u_int ysize;
    int reflect;

    xsize = r.w;
    ysize = r.h;
    dx = x1 - x0;
    dy = y1 - y0;

    /* 
     * Handle length 1 or 2 vectors by just drawing endpoints.
     */
    if ((unsigned)(dx+1) <= 2 && (unsigned)(dy+1) <= 2) {
	register u_short *dst;
	if (VISIBLE(x0,y0)) {
	    dst = addr(x0, y0);
	    color = 0x8000 >> skew(x0);
	    CASE_BINARY_OP(c,POINT,void)
	}
	if ((dx != 0 || dy != 0) && VISIBLE(x1,y1)) {
	    dst = addr(x1, y1);
	    color = 0x8000 >> skew(x1);
	    CASE_BINARY_OP(c,POINT,void)
	}
	return;
    }

    /* 
     * Need to normalize the vector so that the following
     * algorithm can limit the number of cases to be considered.
     * We can always interchange the points in x, so that
     * p0 is to the left of p1.
     */
    if (dx < 0) {		// force vector to scan to right
	dx = -dx;
	dy = -dy;
	SWAP(int, x0, x1);
	SWAP(int, y0, y1);
    }

    /* 
     * The clipping routine needs to work with a vector that
     * increases in y from y0 to y1.  We want to change
     * y0 and y1 so that there is an increase in y from
     * y0 to y1 without affecting the clipping and bounds
     * checking that we will do.  We accomplish this by reflecting
     * the vector around the horizontal centerline of dst,
     * and remember that we did this by incrementing reflect
     * by 2.  The reflection will be undone before the vector
     * is drawn.
     */
    reflect = 0;
    if (dy < 0) {
	dy = -dy;
	y0 = (ysize - 1) - y0;
	y1 = (ysize - 1) - y1; 
	reflect += 2;
    }

    /* 
     * Can now do bounds check, since the vector increasing in x
     * and y can check easily if it has no chance of intersecting
     * the destination rectangle: if the vector ends before the
     * beginning of the target or begins after the end!
     */
    if (y1 < 0 || y0 >= ysize || x1 < 0 || x0 >= xsize)
	return;

    /* 
     * Special case for horizontal lines
     */
    if (dy == 0) {
	if (x0 < 0)
	    x0 = 0;
	if (x1 >= xsize)
	    x1 = xsize - 1;
	dx = x1 - x0;
	int x = r.x + x0;
	u_short mask1 = ((u_short)0xffff) >> (x&0xf);
	u_short mask2 = ((u_short)0xffff) << (15 -((dx +(x&0xf))&0xf));
	count = (((x&0xf) + dx) >> 4) - 1;
	if (count < 0)
	    mask1 &= mask2;
	u_short* dst = addr(x, r.y + y0);
#define colorlong	-1
	CASE_BINARY_OP(c,HORIZ,void)
	return;
    }

    /* 
     * Special case for vertical lines.
     */
    if (dx == 0) {
	if (y0 < 0)
	    y0 = 0;
	if (y1 >= ysize)
	    y1 = ysize - 1;
	dy = y1 - y0;
	vrtstep = (u_char *)rowbytes;
	if (reflect & 2)	
	    y0 = ysize - y1-1; 
	count =  dy + 1;
	register u_short* dst = addr(x0, y0);
	color = 0x8000 >> skew(x0);
	CASE_BINARY_OP(c,VERT,(u_short*))
	return;
    }

    /* 
     * One more reflection: we want to assume that dx >= dy.
     * So if this is not true, we reflect the vector around
     * the diagonal line x = y and remember that we did this
     * by adding 1 to reflect.
     */
    if (dx < dy) {
	SWAP(int, x0, y0);
	SWAP(int, x1, y1);
	SWAP(int, dx, dy);
	SWAP(u_int, xsize, ysize);
	reflect += 1;
    }

    error = -(dx >> 1);			// error at (x0,y0)
    xstart = x0;
    ystart = y0;

    /* 
     * Begin hard part of clipping.
     *
     * We have insured that we are clipping on a vector which
     * has dx > 0, dy > 0 and dx >= dy.  The error is the
     * vertical distance from the true line to the approximation
     * in units where each pixel is dx by dx.  Moving one to
     * the right (increasing x by 1) subtracts dy from the error.
     * Moving one pixel down (increasing y by 1) adds dx to the
     * error.  Bresenham functions by restoring the error to
     * the range (-dx,0] whenever the error leaves it.  The
     * algorithm increases x and increases y when it needs to
     * constrain the error.
     */

    /* 
     * Clip left end to yield start of visible vector.  If the
     * starting x coordinate is negative, we must advance to
     * the first x coordinate which will be drawn (x=0).  As we
     * advance (-start.x) units in the x direction, the error
     * increases by (-start.x)*dy.  This means that the error
     * when x=0 will be
     *	-(dx/2)+(-start.x)*dy
     * For each y advance in this range, the error is reduced
     * by dx, and should be in the range (-dx,0] at the y value
     * at x=0.  Thus to compute the increment in y we should take
     *	(-(dx/2)+(-start.x)*dy+(dx-1))/dx
     * where the numerator represents the length of the interval
     *	[-dx+1,-(dx/2)+(-start.x)*dy]
     * The number of dx steps by which the error can be reduced
     * and stay in this interval is the y increment which would
     * result if the vector were drawn over this interval.
     */
    if (xstart < 0) {
	error += -xstart * dy;
	xstart = 0;
	count = (error + (dx - 1)) / dx;
	ystart += count;
	error -= count * dx;
    }

    /* 
     * After having advanced x to be at least 0, advance
     * y to be in range.  If y is already too large (and can
     * only get larger!), just give up.  Otherwise, if start.y < 0,
     * we need to compute the value of x at which y is first 0.
     * In advancing y to be zero, the error decreases by
     * (-start.y)*dx, in the y steps.
     *
     * Immediately after an advance in y the error is in the
     * range (-dx,-dx+dy].  This can be seen by noting that
     * what last happened was that the error was in the range
     * (-dy,0], the error became positive by adding dy, to be
     * in the range (0,dy], and we subtracted dx to get into
     * the range (-dx,-dx+dy].
     *
     * Thus we need to advance x to cause the error to change
     * to be at most (-dx+dy), or, in steps of dy, at most:
     *	((-dx+dy)-error)/dy
     * which is the number of dy steps in the interval
     * [error,-dx+dy].
     */
    if (ystart >= ysize)
	return;
    if (ystart < 0) {			// skip to dst top edge
	error += ystart * dx;
	ystart = 0;
	count = ((-dx + dy) - error) / dy;
	xstart += count;
	error += count * dy;
	if (xstart >= xsize)
	    return;
    }

    /* 
     * Now clip right end.
     *
     * If the last x position is outside the rectangle, then
     * clip the vector back to within the rectangle at x=xsize-1.
     * The corresponding y value has error
     *	-(dx/2)+((xsize-1)-x0)*dy
     * We need an error in the range (-dx,0], so compute the
     * corresponding number of steps in y from the number of
     * dy's in the interval:
     *	(-dx,-(dx/2)+((xsize-1)-x0)*dy]
     * which is:
     *	[-dx+1,-(dx/2)+((xsize-1)-x0)*dy]
     * or:
     *	(-(dx/2)+((xsize-1)-x0)*dy+dx-1)/dx
     * Note that:
     *	dx - (dx/2) != (dx/2)
     * (consider dx odd), so we can't simplify much further.
     */
    if (x1 >= xsize) {
	x1 = xsize - 1;
	y1 = y0 + (-(dx >> 1) + (((xsize - 1) - x0) * dy) + dx - 1) / dx;
    }
    /* 
     * If the last y position is outside the rectangle, then
     * clip it back at y=ysize-1.  The corresponding x value
     * has error
     *	-(dx/2)-((ysize-1)-y0)*dx
     * We want the last x value with this y value, which
     * will have an error in the range (-dy,0] (so that
     * increasing x one more time will make the error > 0.)
     * Thus the amount of error allocatable to dy steps is
     * from the length of the interval:
     *	[-(dx/2)-((ysize-1)-y0)*dx,0]
     * that is,
     *	(0-(-(dx/2)-((ysize-1)-y0)*dx))/dy
     * or:
     *	((dx/2)+((ysize-1)-y0)*dx)/dy
     */
    if (y1 >= ysize) {
	y1 = ysize - 1;
	x1 = x0 + ((dx >> 1) + (((ysize - 1) - y0) * dx)) / dy;
    }

    /* 
     * Now set up for the Bresenham routines.
     */
    count = x1 - xstart;
    vrtstep = (u_char*)rowbytes;
    if (reflect & 1) {			// major axis is y
	if (reflect & 2) {		// unreflect major axis (misnamed x)
	    xstart = ((xsize - 1) - xstart); 
	    vrtstep = (u_char*)-rowbytes;
	}
	SWAP(int, xstart, ystart);
	register u_short* dst = addr(xstart, ystart);
	color = 0x8000 >> skew(xstart);
	CASE_BINARY_OP(c,MAJORY,(u_short*))
    } else {				// major axis is x
	if (reflect & 2) {		// unreflect minor axis
	    ystart = ((ysize - 1) - ystart); 
	    vrtstep = (u_char*)-rowbytes;
	}
	register u_short* dst = addr(xstart, ystart);
	color = 0x8000 >> skew(xstart);
	CASE_BINARY_OP(c,MAJORX,(u_short*))
    }
}
