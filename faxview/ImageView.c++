#ident $Header: /d/sam/flexkit/fax/faxview/RCS/ImageView.c++,v 1.9 91/09/03 15:19:03 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "ImageView.h"
#include "Bitmap.h"
#include "tiffio.h"
#include "X11/Xlib.h"
#include "GLContext.h"
#include <gl.h>
#include <libc.h>
#include <math.h>

extern	Display* fx_theDisplay;

static const float PAGEWIDTH = 8.5;
static const float PAGELENGTH = 11.;

int	ImageView::CMAPBASE0;
int 	ImageView::CMAPBASE1;

ImageView::ImageView(fxBool forceSmall)
{
    if (forceSmall || getgdesc(GD_XPMAX) < XMAXSCREEN)
	dispResolution = 66.;
    else
	dispResolution = 86;
    CMAPBASE1 = (1 << getgdesc(GD_BITS_NORM_SNG_CMODE))-1 - 4*4;
    CMAPBASE0 = CMAPBASE1 -  4*4;
    width = minWidth = (int)(PAGEWIDTH*dispResolution);
    height = minHeight = (int)(PAGELENGTH*dispResolution);
    raster = new u_short[width * height];
    bitstep0 = new u_char*[width];
    bitstep1 = new u_char*[width];
    rowoff = new u_short[width];
    haveImage = FALSE;
    needCmap = FALSE;
    filterHeight = 0;
    stepDstWidth = 0;
    stepSrcWidth = 0;
    contrast = EXP;
    photometric = (u_short) -1;
}

ImageView::~ImageView()
{
    delete raster;
    delete bitstep0;
    delete bitstep1;
    delete rowoff;
}

const char* ImageView::className() const { return ("ImageView"); }

short ImageView::getMinimumWidth() { return width; }
short ImageView::getMinimumHeight() { return height; }

void
ImageView::paint()
{
    const fxBoundingBox& b = getClipBoundsInContext();
    repairRegion(b.x, b.y, getClipBounds());
}

#include "DamageRegion.h"

void
ImageView::repair(const fxDamageRegion& d)
{
    repairRegion(d.bboxInContext.x, d.bboxInContext.y, d.bbox);
}

void
ImageView::repairRegion(short sx, short sy, const fxBoundingBox& b)
{
    if (haveImage) {
	if (needCmap)
	    setupCmap();
	short ix = b.x;
	short iy = b.y;
	short w = b.w;
	short h = b.h;
	u_short* lp = raster + iy*width + ix;
	if (w != width) {
	    for (; h-- >= 0; sy++, lp += width)
		::rectwrite(sx, sy, sx+w-1, sy+1, lp);
	} else
	    ::rectwrite(sx, sy, sx+w-1, sy+h-1, lp);
    }
}

/*
 * Install the specified image.  lpi specifies
 * the lines/inch of the source image.  The
 * image is resized to fit the display page using
 * a 3 x N box filter, where N depends on the
 * image height.  Typically this is either 3 x 3
 * or 3 x 2, for 196 lpi and 98 lpi images,
 * respectively.
 */
void
ImageView::setImage(const fxBitmap& b, float lpi, fxBool doUpdate)
{
    float pagelen = b.r.h / lpi;	// page length in inches
    u_int dheight = (u_int)(pagelen * dispResolution);
    if (dheight > height) {
	fprintf(stderr,
	    "Warning, page is longer than %g\" (%4.2f), shrinking to fit.\n",
	    PAGELENGTH, pagelen);
	dheight = height;
    }
    filterHeight = (u_short) fceil(float(b.r.h) / dheight);
    setupStepTables(b.r.w);

    int step = b.r.h;
    int limit = dheight;
    int err = 0;
    u_short rowcount;
    u_int sy = 0;
    u_short* row = raster;
    for (u_int dy = 0; dy < dheight; dy++) {
	startrow(row, (u_char*) b.addr(0, sy));
	rowcount = 1;
	err += step;
	while (err >= limit) {
	    err -= limit;
	    sy++;
	    if (err >= limit) {
		addrow(row, (u_char*) b.addr(0, sy));
		rowcount++;
	    }
	}
	/*
	 * The assumption here is that rowcount is
	 * either filterHeight or filterHeight-1.
	 * This is used to select one of the two
	 * colormap segments that have been previously
	 * setup according to the contrast, photometric,
	 * and filter characteristics.  The default
	 * is to presume filterHeight-1 rows will
	 * be accumulated.  If this is wrong, then
	 * we adjust the colormap index to reference
	 * the other colormap segment.
	 */
	if (rowcount == filterHeight)
	    adjustrow(row, CMAPBASE1 - CMAPBASE0);
	row += width;
    }
    for (; dy < height; dy++) {
	whiterow(row);
	row += width;
    }
    needCmap = TRUE;
    haveImage = TRUE;
    if (isOpened() && doUpdate)
	update();
}

#include "bitcount.h"

/*
 * Calculate the horizontal stepping tables according
 * to the widths of the source and destination images.
 */
void
ImageView::setupStepTables(u_short sw)
{
    if (stepSrcWidth != sw || stepDstWidth != width) {
	int step = sw;
	int limit = width;
	int err = 0;
	u_int sx = 1;
	for (u_int x = 0; x < width; x++) {
	    bitstep0[x] = bitcount0[sx & 7];
	    bitstep1[x] = bitcount1[sx & 7];
	    rowoff[x] = sx >> 3;
	    err += step;
	    while (err >= limit) {
		err -= limit;
		sx++;
	    }
	}
	stepSrcWidth = sw;
	stepDstWidth = width;
    }
}

/*
 * Fill a row with white.
 */
void
ImageView::whiterow(u_short* row)
{
    u_short c = CMAPBASE0;
    if (photometric == PHOTOMETRIC_MINISBLACK)
	c += 3*filterHeight;
    u_short x = width-1;
    do
	row[x] = c;
    while (x-- != 0);
}

/*
 * Copy the first row from the source
 * image to the destination raster. 
 */
void
ImageView::startrow(u_short* drow, u_char* srow)
{
    u_short x = width-1;
    do {
	u_char* bits0 = bitstep0[x];
	u_char* bits1 = bitstep1[x];
	u_char* src = srow+rowoff[x];
	drow[x] = CMAPBASE1 + bits0[src[0]] + bits1[src[1]];
    } while (x-- != 0);
}

/*
 * Add a row from the source image
 * to the destination image.
 */
void
ImageView::addrow(u_short* drow, u_char* srow)
{
    u_short x = width-1;
    do {
	u_char* bits0 = bitstep0[x];
	u_char* bits1 = bitstep1[x];
	u_char* src = srow+rowoff[x];
	drow[x] += bits0[src[0]] + bits1[src[1]];
    } while (x-- != 0);
}

/*
 * Adjust the calculated cmap indices of a row.
 * This is done when the accumulated values are
 * not full filterHeight high.
 */
void
ImageView::adjustrow(u_short* row, u_short delta)
{
    u_short x = width-1;
    do
	row[x] -= delta;
    while (x-- != 0);
}

static int clamp(float v, int low, int high)
    { return (v < low ? low : v > high ? high : (int)v); }

static void
expFill(float pct[], u_int p, u_int n)
{
    u_int c = (p * n) / 100;
    for (u_int i = 1; i < c; i++)
	pct[i] = 1-fexp(i/float(n-1))/ M_E;
    for (; i < n; i++)
	pct[i] = 0.;
}

/*
 * Setup the two colormaps -- one for rows crafted
 * from filterHeight source rows and one for those
 * crafted from filterHeight-1 source rows.
 */
void
ImageView::setupCmap()
{
    if (filterHeight > 0) {
	// 3 is for the fixed width horizontal filtering
	setupCmap(3*filterHeight+1, CMAPBASE0);
	setupCmap(3*(filterHeight-1)+1, CMAPBASE1);
	needCmap = FALSE;
    } else
	needCmap = TRUE;
}

void
ImageView::setupCmap(u_int n, u_short base)
{
    float pct[40];			// known to be large enough
    pct[0] = 1;				// force white
    u_int i;
    switch (contrast) {
    case EXP50: expFill(pct, 50, n); break;
    case EXP60:	expFill(pct, 60, n); break;
    case EXP70:	expFill(pct, 70, n); break;
    case EXP80:	expFill(pct, 80, n); break;
    case EXP90:	expFill(pct, 90, n); break;
    case EXP:	expFill(pct, 100, n); break;
    case LINEAR:
	for (i = 1; i < n; i++)
	    pct[i] = 1-float(i)/(n-1);
	break;
    }
    XColor xc[64];
    switch (photometric) {
    case PHOTOMETRIC_MINISBLACK:
	for (i = 0; i < n; i++) {
	    xc[i].pixel = base+i;
	    u_short c = clamp(255*pct[(n-1)-i], 0, 255);
	    xc[i].red = xc[i].green = xc[i].blue = c<<8;
	    xc[i].flags = DoRed | DoGreen | DoBlue;
	}
	break;
    case PHOTOMETRIC_MINISWHITE:
	for (i = 0; i < n; i++) {
	    xc[i].pixel = base+i;
	    u_short c = clamp(255*pct[i], 0, 255);
	    xc[i].red = xc[i].green = xc[i].blue = c<<8;
	    xc[i].flags = DoRed | DoGreen | DoBlue;
	}
	break;
    }
    fxGLContext::push();
    getGLContext()->set(FALSE);
    XStoreColors(fx_theDisplay, fxGLContext::currentCmap, xc, n);
    fxGLContext::pop();
}

/*
 * Set the current contrast.
 */
void
ImageView::setContrast(Contrast c, fxBool doUpdate)
{
    contrast = c;
    if (doUpdate)
	setupCmap();
    else
	needCmap = TRUE;
}

/*
 * Set the current photometric
 * interpretation for the image.
 */
void
ImageView::setPhotometric(u_short photo, fxBool doUpdate)
{
    photometric = photo;
    if (doUpdate)
	setupCmap();
    else
	needCmap = TRUE;
}

/*
 * Flip an image in Y.
 */
void
ImageView::flip(fxBool doUpdate)
{
    int h = height;
    int mid = h / 2;
    if (h & 1) mid++;
    u_short* tmp = new u_short[width];
    int ytop = height-1;
    int ybot = 0;
    int rowbytes = width * sizeof (u_short);
    for (; h > mid; h--) {
	bcopy(&raster[ytop*width], tmp, rowbytes);
	bcopy(&raster[ybot*width], &raster[ytop*width], rowbytes);
	bcopy(tmp, &raster[ybot*width], rowbytes);
	ytop--, ybot++;
    }
    delete tmp;
    if (isOpened() && doUpdate)
	update();
}

/*
 * Flip an image in X.
 */
void
ImageView::reverse(fxBool doUpdate)
{
    int mid = width / 2;
    for (int y = height-1; y >= 0; y--) {
	u_short* data = &raster[y*width];
	int top = width-1;
	int bot = 0;
	for (int x = width; x > mid; x--) {
	    u_short b = data[top];
	    data[top] = data[bot];
	    data[bot] = b;
	    top--, bot++;
	}
    }
    if (isOpened() && doUpdate)
	update();
}
