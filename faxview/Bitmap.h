#ifndef fx_Bitmap_
#define	fx_Bitmap_

#include "Ptr.h"
#include "BoundingBox.h"

// XXX this stuff was designed to use Point & Rectangle data types,
// fxBoundingBox usage is a cheap substitute that probably should be
// removed

// 1-bit image and drawing support.
class fxBitmap : public fxRCObj {
public:
    enum fxBitmapCode {			// drawing codes
	F_CLR = 0x0,
	F_NOT = 0x5,
	F_XOR = 0x6,
	F_AND = 0x8,
	F_STORE = 0xc,
	F_OR =  0xe,
	F_SET = 0xf,
	F_NOP = 0x10,			// XXX
    };

    static void scale(fxBitmap& b, const fxBoundingBox& dst,
	const fxBitmap& src, const fxBoundingBox& src);
    static void rotate(fxBitmap& dst, const fxBitmap& src, float angle);
protected:
    u_short	mybits	: 1;		// true if we allocated bits
    u_short	rowbytes;		// width of scanline */
    u_short*	bits;			// ul corner of bitmap representation

    void newBitmap(const fxBoundingBox& r, u_short* bits);

    void arc0(int x0, int y0, int, int,
	    int x1, int y1, int x2, int y2, fxBitmapCode);
public:
    fxBoundingBox r;			// bitmap dimensions XXX */

    fxBitmap(const fxBoundingBox& r, u_short* bits = 0);
    fxBitmap(u_int w, u_int h, u_short* bits = 0);
    virtual ~fxBitmap();

// XXX addr is a protection leak for const
    u_short* addr(int x, int y) const
	{ return ((u_short*)((int)bits+(y*rowbytes)+((x>>3)&~1))); }
    u_short skew(int x) const	{ return (x & 15); }

    u_int get(int x, int y) const;
    virtual void set(int x, int y, u_int value);
// drawing routines
    virtual void point(int x, int y, fxBitmapCode);
    virtual void segment(int x1, int y1, int x2, int y2, fxBitmapCode);
    virtual void rect(const fxBoundingBox& r, fxBitmapCode);
    virtual void circle(int x, int y, u_int radius, fxBitmapCode);
    virtual void ellipse(int x, int y, int maj, int min, fxBitmapCode);
    virtual void arc(int x0, int y0, u_int r, int x1, int y1, int x2, int y2, fxBitmapCode);
    virtual void border(const fxBoundingBox& r, u_int width, fxBitmapCode);
    virtual void clear();
    virtual void bitblt(int dx, int dy,
		    const fxBitmap* src, const fxBoundingBox& rsrc,
		    fxBitmapCode);
    virtual void texture(const fxBoundingBox& r, const fxBitmap& src,
			fxBitmapCode);
};
fxDECLARE_Ptr(fxBitmap);
#endif	/* fx_Bitmap_ */
