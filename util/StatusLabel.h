#ident $Header: /d/sam/flexkit/fax/util/RCS/StatusLabel.h,v 1.2 91/05/23 12:49:56 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _StatusLabel_
#define	_StatusLabel_

#include "View.h"
#include "Color.h"
#include "Font.h"
#include <stdarg.h>

class StatusLabel : public fxView {
protected:
    fxLayoutConstraint xConstraint;
    fxStr	text;
    fxFontPtr	font;
    fxGLColor	textColor;
    fxGLColor	fillColor;
public:
    StatusLabel(fxLayoutConstraint x = variableSize,
	fxFont* f = 0, fxRGBColor* tc = 0, fxRGBColor* fc = 0);
    virtual ~StatusLabel();

    fxLayoutConstraint getLayoutConstraint(fxOrientation axis);
    void paint();

    fxFont* getFont()	{ return (font); }

    void setTextColor(const fxRGBColor& c);
    void setFillColor(const fxRGBColor& c);
    void setValue(const char* fmt ...);
    void vsetValue(const char* fmt, va_list ap);
};
#endif /* _StatusLabel_ */
