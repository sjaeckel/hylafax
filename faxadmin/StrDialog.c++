#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/StrDialog.c++,v 1.2 91/05/23 12:14:02 sam Exp $

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
#include "StrDialog.h"
#include "Label.h"
#include "FieldEditor.h"
#include "Gap.h"
#include "Parts.h"
#include "Str.h"

StrDialog::StrDialog(fxVisualApplication *app, fxView *icon,
	const char *prompt, fxBool modal) :
    ConfirmDialog(app, icon, 0, modal)
{
    fxViewStack* hs = new
	    fxViewStack(horizontal, variableSize, variableSize, 0, 0, 4);
	hs->add(new fxLabel(prompt));
	fxViewStack* frame = fxMakeFrame(horizontal, variableSize, fixedSize);
	    editor = new fxFieldEditor(variableSize, 2, 2);
		// XXX dialogFontFamily&FontSize
		editor->setMinimumSize(250, editor->getMinimumHeight());
		editor->connect("acceptValue", this, "close");
	    frame->add(editor);
	hs->add(frame);
    interior->add(hs);
    setStartWithMouseInView(
	addGoAwayButton("Accept", fxMakeAcceptButton("Accept")));
    addCancelButton();
}
StrDialog::~StrDialog() {}
const char* StrDialog::className() const { return "StrDialog"; }

void StrDialog::setValue(const fxStr& s) { editor->setValue(s); }
const fxStr& StrDialog::getValue() { return (editor->getValue()); }

StrDialog*
MakeQuestionDialog(fxVisualApplication *app, const char* prompt, fxBool modal)
{
    return new StrDialog(app, fxMakeQuestionDialogIcon(), prompt, modal);
}
