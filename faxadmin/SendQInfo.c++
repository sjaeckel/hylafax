#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/SendQInfo.c++,v 1.2 91/05/23 12:13:41 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Window.h"
#include "Label.h"
#include "Gap.h"
#include "Parts.h"
#include "Palette.h"

SendQInfo::SendQInfo(fxVisualApplication *app, SendQ* req) : fxDialogBox(app, TRUE)
{
    fxViewStack *top = new
	    fxViewStack(vertical, variableSize, variableSize, 9, 9, 6);
    top->setColor(fx_uiGray);
    fxViewStack* hs = new
	    fxViewStack(horizontal, variableSize, variableSize, 0,0, 4);
	hs->add(icon, alignTop);
	if (prompt)
	    interior->add(fxMakeTextBox((char*) prompt, variableSize, fixedSize,
		fx_theStyleGuide->dialogFontFamily,
		fx_theStyleGuide->dialogFontSize));
	hs->add(interior, alignLeft);
	top->add(hs);
    buttonStack = new
	fxViewStack(horizontal, fixedSize, fixedSize, 6, 0, 8);
    top->add(new fxGap(vertical, fixedSize, 4));
    top->add(buttonStack, alignRight);
    fxWindow::add(top);
}
SendQInfo::~SendQInfo() {}

const char* SendQInfo::className() const { return "SendQInfo"; }
