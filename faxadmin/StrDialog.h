#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/StrDialog.h,v 1.2 91/05/23 12:14:05 sam Exp $
#ifndef _StrDialog_
#define	_StrDialog_

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "ConfirmDialog.h"

class fxFieldEditor;
class fxStr;

class StrDialog : public ConfirmDialog {
protected:
    fxFieldEditor*   editor;
public:
    StrDialog(fxVisualApplication* app, fxView* icon, const char* prompt,
	fxBool modal = FALSE);
    virtual ~StrDialog();
    virtual const char* className() const;

    virtual void setValue(const fxStr& s);
    virtual const fxStr& getValue();
};

extern StrDialog* MakeQuestionDialog(fxVisualApplication *app,
			const char* prompt,
			fxBool modal = FALSE);
#endif /* _StrDialog_ */
