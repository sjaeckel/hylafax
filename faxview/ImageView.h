#ident $Header: /usr/people/sam/flexkit/fax/faxview/RCS/ImageView.h,v 1.7 91/07/23 12:36:20 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef	_ImageView_
#define	_ImageView_

#include "View.h"

class fxBitmap;
class fxDamageRegion;

class ImageView : public fxView {
public:
    enum Contrast {
	EXP50,
	EXP60,
	EXP70,
	EXP80,
	EXP90,
	EXP,
	LINEAR,
    };
private:
    float	dispResolution;		// display resolution used
    fxBool	haveImage;		// true if an image is setup
    fxBool	needCmap;		// true if a new colormap is needed
    Contrast	contrast;		// current contrast
    u_short	photometric;		// current photometric of raster
    u_short	filterHeight;		// filter height in rows
    u_short	stepSrcWidth;		// src image stepping width
    u_short	stepDstWidth;		// dest stepping width
    u_char**	bitstep0;		// horizontal bit stepping (1st byte)
    u_char**	bitstep1;		// horizontal bit stepping (2nd byte)
    u_short*	rowoff;			// row offset for stepping
    u_short*	raster;			// displayed raster

    static int CMAPBASE0;
    static int CMAPBASE1;

    void startrow(u_short* drow, u_char* srow);
    void addrow(u_short* drow, u_char* srow);
    void adjustrow(u_short* row, u_short delta);
    void whiterow(u_short* row);
    void setupStepTables(u_short sw);
    void setupCmap();
    void setupCmap(u_int n, u_short base);

    void repairRegion(short sx, short sy, const fxBoundingBox& b);
public:
    ImageView(fxBool forceSmall = FALSE);
    virtual ~ImageView();

    virtual const char* className() const;

    virtual short getMinimumWidth();
    virtual short getMinimumHeight();

    virtual void paint();			// complete repaint
    virtual void repair(const fxDamageRegion &);// partial repaint

    void setImage(const fxBitmap&, float vres, fxBool doUpdate = TRUE);
    void setPhotometric(u_short, fxBool doUpdate = TRUE);
    void setContrast(Contrast, fxBool doUpdate = TRUE);

    void flip(fxBool doUpdate = TRUE);
    void reverse(fxBool doUpdate = TRUE);
};
fxMakeObjPtr(ImageView);
#endif
