#include "Bitmap.h"

fxBitmap::fxBitmap(const fxBoundingBox& r, u_short* bits)
{
    newBitmap(r, bits);
}

fxBitmap::fxBitmap(u_int w, u_int h, u_short* bits)
{
    newBitmap(fxBoundingBox(0,0,w,h), bits);
}

fxBitmap::~fxBitmap()
{
    if (bits && mybits) delete bits;
}

void
fxBitmap::newBitmap(const fxBoundingBox &ur, u_short *ubits)
{
    r = ur;
    rowbytes = ((r.w+15)>>3) &~ 1;
    mybits = (ubits == 0);
    bits = (mybits ? new u_short[rowbytes * r.h] : ubits);
    if (mybits)
	fxBitmap::clear();
}

u_int
fxBitmap::get(int x, int y) const
{
     return (r.inside(x,y) ? *addr(x, y) & (0x8000>>skew(x)) : 0);
}

void
fxBitmap::set(int x, int y, u_int v)
{
    if (!r.inside(x,y))
	return;
    u_short *a = addr(x,y);
    if (v)
	*a |= 0x8000 >> skew(x);
    else
	*a &= ~(0x8000 >> skew(x));
}

void
fxBitmap::point(int x, int y, fxBitmapCode c)
{
    u_int bit;
    switch (c) {
    case F_CLR:			bit = 0; break;
    case F_SET: case F_STORE:	bit = 1; break;
    case F_XOR: case F_NOT:	bit = ~get(x,y); break;
    default:			return;
    }
    set(x,y, bit);
}

void
fxBitmap::rect(const fxBoundingBox& dr, fxBitmapCode c)
{
    bitblt(dr.x, dr.y, (fxBitmap*)0, dr, c);
}

/*
 * Form a circle of specified radius centered at p.
 * The boundary is a sequence of vertically, horizontally,
 * or diagonally adjacent points that minimize
 *	abs(x^2+y^2-radius^2).
 *
 * The circle is guaranteed to be symmetric about the
 * horizontal, vertical, and diagonal axes
 */
void
fxBitmap::circle(int x, int y, u_int radius, fxBitmapCode c)
{
    int x1 = x;
    int y1 = y;
    int eps = 0;		// x^2 + y^2 - radius^2
    int dxsq = 1;		// (x+dx)^2-x^2
    int dysq = 1 - 2*radius;
    int exy;
    int x0 = x;
    int y0 = y;

    y0 -= radius;
    y1 += radius;
    if (c == F_XOR) {		// endpoints coincide
	point(x0,y0, c);
	point(x0,y1, c);
    }
    while (y1 > y0) {
	point(x0,y0, c);
	point(x0,y1, c);
	point(x1,y0, c);
	point(x1,y1, c);
	exy = eps + dxsq + dysq;
	if (-exy <= eps+dxsq) {
	    y1--;
	    y0++;
	    eps += dysq;
	    dysq += 2;
	}
	if (exy <= -eps) {
	    x1++;
	    x0--;
	    eps += dxsq;
	    dxsq += 2;
	}
    }
    point(x0, y0, c);
    point(x1, y0, c);
}

#define labs(x,y) if((x=y)<0) x= -x

/*
 * Calculate b*b*x*x + a*a*y*y - a*a*b*b avoiding overflow.
 */
static long
resid(register int a, register int b, int x, int y)
{
    long e = 0;
    long u = b*((long)a*a - (long)x*x);
    long v = (long)a*y*y;
#define	BIG	077777
#define	HUGE	07777777777L
    register long q = u>BIG? HUGE/u: BIG;
    register long r = v>BIG? HUGE/v: BIG;

    assert(q != 0 && r != 0);
    while (a || b) {
	if (e >= 0 && b) {
	    if (q > b) q = b;
	    e -= q*u;
	    b -= q;
	} else {
	    if (r > a) r = a;
	    e += r*v;
	    a -= r;
	}
    }
    return (e);
#undef	HUGE
#undef	BIG
}

inline int abs(int x) { return (x < 0 ? -x : x); }
/*
 * Service routine used for both elliptic arcs and ellipses 
 * traces clockwise an ellipse centered at x0,y0 with half-axes
 * a,b starting from the point x1,y1 and ending at x2,y2
 * performing an action at each point
 * x1,y1,x2,y2 are measured relative to center
 * when x1,y1 = x2,y2 the whole ellipse is traced
 * e is the error b^2 x^2 + a^2 y^2 - a^2 b^2
 */
void
fxBitmap::arc0(int x0, int y0, int a, int b,
	    int x1, int y1, int x2, int y2, fxBitmapCode c)
{
    int dx = y1>0? 1: y1<0? -1: x1>0? -1: 1;
    int dy = x1>0? -1: x1<0? 1: y1>0? -1: 1;
    long a2 = (long) a*a;
    long b2 = (long) b*b;
    register long e = resid(a, b, x1, y1);
    register long dex = b2*(2*dx*x1+1);
    register long dey = a2*(2*dy*y1+1);
    register long ex, ey, exy;

    a2 *= 2;
    b2 *= 2;
    do {
	labs(ex, e+dex);
	labs(ey, e+dey);
	labs(exy, e+dex+dey);
	if (exy <= ex || ey < ex) {
	    y1 += dy;
	    e += dey;
	    dey += a2;
	}
	if (exy <= ey || ex < ey) {
	    x1 += dx;
	    e += dex;
	    dex += b2;
	}
	point(x0+x1, y0+y1, c);
	if (x1 == 0) {
	    dy = -dy;
	    dey = -dey + a2;
	} else if (y1 == 0) {
	    for (int z = x1; abs(z += dx) <= a;)
		point(x0+z, y0+y1, c);
	    dx = -dx;
	    dex = -dex + b2;
	}
    } while (x1 != x2 || y1 != y2);
}

void
fxBitmap::ellipse(int x, int y, int maj, int min, fxBitmapCode c)
{
    if (maj == 0 || min == 0)
	segment(x-maj, y-min, x+maj, y+min, c);
    else
	arc0(x, y, maj, min, 0,min, 0,min, c);
}

/*
 * Trace clockwise circular arc centered at x0,y0 with
 * radius r, starting from the point x1,y1 and ending
 * at x2,y2 performing an action at each point
 * x1,y1,x2,y2 are measured relative to center
 * when x1,y1 = x2,y2 the whole circle is traced
 * e is the error x^2 + y^2 - r^2.
 */
void
fxBitmap::arc(int x0, int y0, u_int r,
	    int x1, int y1, int x2, int y2, fxBitmapCode c)
{
    int dx = x1>x2 ? -1 : 1;
    int dy = y1>y2 ? -1 : 1;
    register long e = x1*x1 + y1*y1 - r*r;	// XXX overflow
    register long dex = 2*dx*x1+1;
    register long dey = 2*dy*y1+1;
    register long ex, ey, exy;

    if (x1 != x2 || y1 != y2)
	point(x0+x1, y0+y1, c);
    do {
	labs(ex, e+dex);
	labs(ey, e+dey);
	labs(exy, e+dex+dey);
	if (exy <= ex || ey < ex) {
	    y1 += dy;
	    e += dey;
	    dey += 2;
	}
	if (exy <= ey || ex < ey) {
	    x1 += dx;
	    e += dex;
	    dex += 2;
	}
	point(x0+x1, y0+y1, c);
    } while (y1 != y2);
}

void
fxBitmap::border(const fxBoundingBox &dr, u_int w, fxBitmapCode c)
{
    int cx = dr.x+dr.w-1;
    int cy = dr.y+dr.h-1;

    if (w < 2) {
	segment(dr.x, dr.y, cx, dr.y, c);
	segment(cx, dr.y, cx, cy, c);
	segment(dr.x, cy, cx, cy, c);
	segment(dr.x, dr.y, dr.x, cy, c);
    } else {
	rect(fxBoundingBox(dr.x, dr.y, w, dr.h), c);
	rect(fxBoundingBox(dr.x, cy-w, dr.w, w), c);
	rect(fxBoundingBox(dr.x, dr.y+w, w, dr.h-w), c);
	rect(fxBoundingBox(cx-w, dr.y+w, dr.w, dr.h-w), c);
    }
}

void
fxBitmap::clear()
{
    bitblt(0,0, (fxBitmap*)0, r, F_CLR);
}

void
fxBitmap::texture(const fxBoundingBox& dr, const fxBitmap& text, fxBitmapCode c)
{
    fxBoundingBox tr(text.r);

    for (int y = dr.y; y < dr.y+dr.h; y += text.r.h) {
	if (dr.y+dr.h - y < tr.y+tr.h)
	    tr.h = dr.y+dr.h - y;
	tr.w = text.r.w;
	for (int x = dr.x; x < dr.x; x += text.r.w) {
	    if (dr.x+dr.w - x < tr.x+tr.w)
		tr.w = dr.x+dr.w - x;
	    bitblt(x, y, &text, tr, c);
	}
    }
}
