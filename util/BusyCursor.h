#ident $Header: /d/sam/flexkit/fax/util/RCS/BusyCursor.h,v 1.2 91/05/23 13:00:02 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _BusyCursor_
#define	_BusyCursor_

#include "Cursor.h"

class fxWindow;

class BusyCursor {
private:
    const fxCursor* prevCursor;		// previous cursor when busy

    static fxCursor hourGlass;
public:
    void beginBusy(fxWindow*);
    void endBusy(fxWindow*);
};
#endif /* _BusyCursor_ */
