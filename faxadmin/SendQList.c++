#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/SendQList.c++,v 1.5 91/05/23 12:13:47 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Font.h"
#include "Color.h"
#include "Palette.h"
#include "StyleGuide.h"
#include "SendQList.h"
#include "SendQ.h"
#include <libc.h>

SendQList::SendQList(fxLayoutConstraint xc, fxLayoutConstraint yc) :
    fxScrollList(xc, yc, TRUE),
    textColor(fx_black),
    highlightColor(fx_uiGray)
{
    otherFnt = fxGetFont("Helvetica", 11);
    senderFnt = fxGetFont("Helvetica-Oblique", 11);
    pageColor = fx_lightGray;

    senderMaxWidth = 15 * senderFnt->getWidth('a');
    destMaxWidth = 16 * otherFnt->getWidth('a');
    // XXX 2* should be enough
    senderBuf = (char*) malloc(2*35);
    senderOffset = 1 + otherFnt->getWidth("9999") + 8;
    destOffset = senderOffset + senderMaxWidth;
    dateOffset = destOffset + destMaxWidth;

    lineHeight = fxmax((u_int) senderFnt->getMaxHeight(),
			(u_int) otherFnt->getMaxHeight());
    setupSize(dateOffset + otherFnt->getWidth("31 Sep 23:59") + 8, 0);
}

SendQList::~SendQList()
{
    delete senderBuf;
}

const char* SendQList::className() const { return ("SendQList"); }

u_int
SendQList::slotHeight() const
{
    return (lineHeight);
}

void
SendQList::setSelection(SendQ* a, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
	fxScrollList::selectItem(ix, TRUE, allowUpdate);
}

SendQ*
SendQList::getSelection()
{
    if (pickSlot == fxInvalidNameIndex)
	return (0);
    else
	return (items[pickSlot].entry);
}

void
SendQList::selectItem(SendQ* a, fxBool clearSelectionFirst, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
        fxScrollList::selectItem(ix, clearSelectionFirst, allowUpdate);
}

void
SendQList::deselectItem(SendQ* a, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
        fxScrollList::deselectItem(ix, allowUpdate);
}

//----------------------------------------------------------------------

void
SendQList::add(SendQ* e, fxBool allowUpdate, fxBool picked)
{
    items.append(SendQItem(e,
	getStrLen(e->sender, senderFnt, senderMaxWidth),
	getStrLen(e->number, otherFnt, destMaxWidth)));
    fxScrollList::add(allowUpdate, picked);
}

void
SendQList::insert(SendQ* e, u_int ix, fxBool allowUpdate, fxBool picked)
{
    items.insert(SendQItem(e,
	getStrLen(e->sender, senderFnt, senderMaxWidth),
	getStrLen(e->number, otherFnt, destMaxWidth)), ix);
    fxScrollList::insert(ix, allowUpdate, picked);
}

void
SendQList::insertUnique(SendQ* a, u_int ix, fxBool allowUpdate, fxBool picked)
{
    if (!contains(a))
	insert(a, ix, allowUpdate, picked);
}

void
SendQList::addUnique(SendQ* a, fxBool allowUpdate, fxBool picked)
{
    if (!contains(a))
	add(a, allowUpdate, picked);
}

void
SendQList::remove(SendQ* a, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex) {
	items.remove(ix);
	fxScrollList::remove(ix, allowUpdate);
    }
}

void
SendQList::removeAll(fxBool allowUpdate)
{
    items.remove(0, items.length());
    fxScrollList::removeAll(allowUpdate);
}

fxBool
SendQList::contains(SendQ* a) const
{
    return (itemToSlot(a) != fxInvalidNameIndex);
}

void
SendQList::sort(fxBool allowUpdate)
{
    items.qsort();
    if (opened && allowUpdate)
	update();
}

//----------------------------------------------------------------------

short
SendQList::getStrLen(const fxStr& s, fxFont* fnt, u_short maxWidth)
{
    maxWidth -= fnt->getWidth("...");
    u_short w = 0;
    for (short i = 0; i < s.length() && w < maxWidth; i++)
	w += fnt->getWidth(s[i]);
    return (i);
}

void
SendQList::paint()
{
    if (newSize)
	sizeChanged();
    // now paint up the names
    int y = height + viewingPixelOffset;
    u_int n = items.length();
    for (u_int i = firstVisibleName; i < n; i++) {
	y -= lineHeight;
	paintLine(i, y, ispicked[i] && enabled);
	if (y <= 0) break;
    }
    if (y > 0)
	pageColor.fill(0, 0, width, y);
}

void
SendQList::paintLine(u_int ix)
{
    int y = height + viewingPixelOffset - (ix - firstVisibleName) * lineHeight;
    if (y > 0)
	paintLine(ix, y - lineHeight, ispicked[ix] && enabled);
}

void
SendQList::paintLine(u_int ix, int y, fxBool picked)
{
    fxGLColor* bg = picked ? &highlightColor : &pageColor;
    bg->fill(0, y, width, lineHeight);
    const SendQItem& qi = items[ix];
    const SendQ& q = *qi.entry;
    textColor.color();
    int descender = otherFnt->getMaxDescender();
    sprintf(senderBuf, "%4d", q.jobid);
    otherFnt->imageSpan(senderBuf, 4, 1, y+descender);
    u_short len = qi.senderLen;
    const char* cp = q.sender;
    if (len != q.sender.length()) {
	bcopy(cp, senderBuf, len);
	bcopy("...", senderBuf+len, 3);
	cp = senderBuf;
	len += 3;
    }
    senderFnt->imageSpan(cp, len, senderOffset, y+senderFnt->getMaxDescender());
    otherFnt->imageSpan(q.number, qi.destLen, destOffset, y+descender);
    cftime(senderBuf, "%d %h %R", &q.tts);
    otherFnt->imageSpan(senderBuf, strlen(senderBuf), dateOffset, y+descender);
}

fxBool
SendQList::scroll(SendQ* a)
{
    u_int slot = itemToSlot(a);
    if (slot != fxInvalidNameIndex) {
	if (slot < firstVisibleName ||
	    (slot - firstVisibleName) * lineHeight >= height) {
	    int nSlots = height / lineHeight;
	    int top = slot - nSlots/2;
	    if (top < 0)
		top = 0;
	    if (opened)
		fxScrollList::scroll(top * lineHeight);
	    return (TRUE);
	}
    }
    return (FALSE);
}

//----------------------------------------------------------------------

void
SendQList::sendSlotValue(u_int slot)
{
}

void
SendQList::getSelections(SendQPtrArray& result)
{
    result.resize(0);
    if (selectCount > 0) {
	for (int i = beginPick; i <= endPick; i++)
	    if (ispicked[i])
		result.append(items[i].entry);
    }
}

SendQ*
SendQList::getFirstSelection()
{
    if (pickSlot == fxInvalidNameIndex) {
	if (selectCount > 0) {
	    for (u_int i = beginPick; i <= endPick; i++)
		if (ispicked[i])
		    return (items[i].entry);
	}
	return (0);
    } else
	return (items[pickSlot].entry);
}

//----------------------------------------------------------------------

u_int
SendQList::itemToSlot(SendQ* a) const
{
    if (pickSlot != fxInvalidNameIndex && items[pickSlot].entry == a)
	return (pickSlot);
    for (u_int i = 0, n = items.length(); i < n; i++)
	if (items[i].entry == a)
	    return (i);
    return (fxInvalidNameIndex);
}

SendQ*
SendQList::slotToItem(u_int slot) const
{
    if (slot < items.length())
	return (items[slot].entry);
    else
	return (0);
}

void
SendQList::updateLine(u_int ix)
{
    fxScrollList::updateLine(ix);
}

void
SendQList::updateLine(SendQ* a)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
	fxScrollList::updateLine(ix);
}

fxIMPLEMENT_StructArray(SendQItemArray, SendQItem);

//----------------------------------------------------------------------

SendQListTitle::SendQListTitle(SendQList* l) :
    pageColor(fx_paleBlue),
    textColor(fx_black)
{
    list = l;
    fnt = fxGetFont("Helvetica-Bold", 11);
    minWidth = width = l->getMinimumWidth();
    minHeight = height = fnt->getMaxHeight();
}

SendQListTitle::~SendQListTitle()
{
}

const char* SendQListTitle::className() const { return ("SendQListTitle"); }

fxLayoutConstraint SendQListTitle::getLayoutConstraint(fxOrientation axis)
    { return (axis == horizontal ? variableSize : fixedSize); }

void
SendQListTitle::paint()
{
    pageColor.fill(0,0,width,height);
    u_int descender = fnt->getMaxDescender();
    u_int off = fx_theStyleGuide->horizontalScrollBarPlacement == alignLeft ?
	fx_theStyleGuide->valuatorMinorDimension + 1 : 0;
    textColor.color();
    fnt->imageSpan("Job",    3,	off+1,			descender);
    fnt->imageSpan("Sender", 6,	off+list->senderOffset,	descender);
    fnt->imageSpan("Number", 6,	off+list->destOffset,	descender);
    fnt->imageSpan("Time",   4,	off+list->dateOffset,	descender);
}
