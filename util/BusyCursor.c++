#ident $Header: /usr/people/sam/tmp/fax/util/RCS/BusyCursor.c++,v 1.2 91/05/23 13:00:09 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "BusyCursor.h"
#include "Window.h"

static u_short hourglass_bits[] = {
    0xffff, 0xffff, 0x4ff2, 0x23c4, 0x1188, 0x0890, 0x0520, 0x0240,
    0x03c0, 0x07e0, 0x0fb0, 0x1e78, 0x381c, 0x4002, 0x8001, 0xffff,
};
fxCursor BusyCursor::hourGlass(16, 1, 8, 8, hourglass_bits);

/*
 * Give feedback that a long-term operation is beginning.
 */
void
BusyCursor::beginBusy(fxWindow* w)
{
    if (w && w->isMapped()) {
	prevCursor = &w->getCursor();
	w->setCursor(hourGlass);
    }
}

/*
 * Reset state after a long-term operation.
 */
void
BusyCursor::endBusy(fxWindow* w)
{
    if (w && w->isMapped())
	w->setCursor(*prevCursor);
}
