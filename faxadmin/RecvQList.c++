#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/RecvQList.c++,v 1.4 91/05/23 12:13:24 sam Exp $

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
#include "RecvQList.h"
#include "RecvQ.h"
#include <libc.h>

RecvQList::RecvQList(fxLayoutConstraint xc, fxLayoutConstraint yc) :
    fxScrollList(xc, yc, TRUE),
    textColor(fx_black),
    highlightColor(fx_uiGray)
{
    otherFnt = fxGetFont("Helvetica", 11);
    senderFnt = fxGetFont("Helvetica-Oblique", 11);
    pageColor = fx_lightGray;

    dateOffset = 1;
    senderOffset = dateOffset + otherFnt->getWidth("31 Sep 23:59") + 8;
    pagesOffset = senderOffset + 18*senderFnt->getWidth('a') + 8;
    qualityOffset = pagesOffset + otherFnt->getWidth("Pages") + 8;

    lineHeight = fxmax((u_int) senderFnt->getMaxHeight(),
			(u_int) otherFnt->getMaxHeight());
    setupSize(qualityOffset + otherFnt->getWidth("Quality"), 0);
}

RecvQList::~RecvQList()
{
}

const char* RecvQList::className() const { return ("RecvQList"); }

u_int
RecvQList::slotHeight() const
{
    return (lineHeight);
}

void
RecvQList::setSelection(RecvQ* a, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
	fxScrollList::selectItem(ix, TRUE, allowUpdate);
}

RecvQ*
RecvQList::getSelection()
{
    if (pickSlot == fxInvalidNameIndex)
	return (0);
    else
	return (items[pickSlot]);
}

void
RecvQList::selectItem(RecvQ* a, fxBool clearSelectionFirst, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
        fxScrollList::selectItem(ix, clearSelectionFirst, allowUpdate);
}

void
RecvQList::deselectItem(RecvQ* a, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
        fxScrollList::deselectItem(ix, allowUpdate);
}

//----------------------------------------------------------------------

void
RecvQList::add(RecvQ* e, fxBool allowUpdate, fxBool picked)
{
    items.append(e);
    fxScrollList::add(allowUpdate, picked);
}

void
RecvQList::insert(RecvQ* e, u_int ix, fxBool allowUpdate, fxBool picked)
{
    items.insert(e, ix);
    fxScrollList::insert(ix, allowUpdate, picked);
}

void
RecvQList::insertUnique(RecvQ* a, u_int ix, fxBool allowUpdate, fxBool picked)
{
    if (!contains(a))
	insert(a, ix, allowUpdate, picked);
}

void
RecvQList::addUnique(RecvQ* a, fxBool allowUpdate, fxBool picked)
{
    if (!contains(a))
	add(a, allowUpdate, picked);
}

void
RecvQList::remove(RecvQ* a, fxBool allowUpdate)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex) {
	items.remove(ix);
	fxScrollList::remove(ix, allowUpdate);
    }
}

void
RecvQList::removeAll(fxBool allowUpdate)
{
    items.remove(0, items.length());
    fxScrollList::removeAll(allowUpdate);
}

fxBool
RecvQList::contains(RecvQ* a) const
{
    return (itemToSlot(a) != fxInvalidNameIndex);
}

void
RecvQList::sort(fxBool allowUpdate)
{
    items.qsort();
    if (opened && allowUpdate)
	update();
}

//----------------------------------------------------------------------

void
RecvQList::paint()
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
RecvQList::paintLine(u_int ix)
{
    int y = height + viewingPixelOffset - (ix - firstVisibleName) * lineHeight;
    if (y > 0)
	paintLine(ix, y - lineHeight, ispicked[ix] && enabled);
}

void
RecvQList::paintLine(u_int ix, int y, fxBool picked)
{
    fxGLColor* bg = picked ? &highlightColor : &pageColor;
    bg->fill(0, y, width, lineHeight);
    const RecvQ& q = *items[ix];
    char buf[80];
    textColor.color();
    int descender = otherFnt->getMaxDescender();
    cftime(buf, "%d %h %R", &q.arrival);
    otherFnt->imageSpan(buf, strlen(buf), dateOffset, y+descender);
    senderFnt->imageSpan(q.number, q.number.length(),
	senderOffset, y+senderFnt->getMaxDescender());
    sprintf(buf, "%5d", q.npages);
    otherFnt->imageSpan(buf, strlen(buf), pagesOffset, y+descender);
    const char* quality = q.resolution < 150 ? "regular" : "fine";
    otherFnt->imageSpan(quality, strlen(quality), qualityOffset, y+descender);
}

fxBool
RecvQList::scroll(RecvQ* a)
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
RecvQList::sendSlotValue(u_int slot)
{
}

void
RecvQList::getSelections(RecvQPtrArray& result)
{
    result.resize(0);
    if (selectCount > 0) {
	for (int i = beginPick; i <= endPick; i++)
	    if (ispicked[i])
		result.append(items[i]);
    }
}

RecvQ*
RecvQList::getFirstSelection()
{
    if (pickSlot == fxInvalidNameIndex) {
	if (selectCount > 0) {
	    for (u_int i = beginPick; i <= endPick; i++)
		if (ispicked[i])
		    return (items[i]);
	}
	return (0);
    } else
	return (items[pickSlot]);
}

//----------------------------------------------------------------------

u_int
RecvQList::itemToSlot(RecvQ* a) const
{
    if (pickSlot != fxInvalidNameIndex && items[pickSlot] == a)
	return (pickSlot);
    for (u_int i = 0, n = items.length(); i < n; i++)
	if (items[i] == a)
	    return (i);
    return (fxInvalidNameIndex);
}

RecvQ*
RecvQList::slotToItem(u_int slot) const
{
    if (slot < items.length())
	return (items[slot]);
    else
	return (0);
}

void
RecvQList::updateLine(u_int ix)
{
    fxScrollList::updateLine(ix);
}

void
RecvQList::updateLine(RecvQ* a)
{
    u_int ix = itemToSlot(a);
    if (ix != fxInvalidNameIndex)
	fxScrollList::updateLine(ix);
}

//----------------------------------------------------------------------

RecvQListTitle::RecvQListTitle(RecvQList* l) :
    pageColor(fx_paleBlue),
    textColor(fx_black)
{
    list = l;
    fnt = fxGetFont("Helvetica-Bold", 11);
    minWidth = width = l->getMinimumWidth();
    minHeight = height = fnt->getMaxHeight();
}

RecvQListTitle::~RecvQListTitle()
{
}

const char* RecvQListTitle::className() const { return ("RecvQListTitle"); }

fxLayoutConstraint RecvQListTitle::getLayoutConstraint(fxOrientation axis)
    { return (axis == horizontal ? variableSize : fixedSize); }

void
RecvQListTitle::paint()
{
    pageColor.fill(0,0,width,height);
    u_int descender = fnt->getMaxDescender();
    u_int off = fx_theStyleGuide->horizontalScrollBarPlacement == alignLeft ?
	fx_theStyleGuide->valuatorMinorDimension + 1 : 0;
    textColor.color();
    fnt->imageSpan("Received",8, off+list->dateOffset,	descender);
    fnt->imageSpan("Sender",  6, off+list->senderOffset,descender);
    fnt->imageSpan("Pages",   5, off+list->pagesOffset,	descender);
    fnt->imageSpan("Quality", 7, off+list->qualityOffset,descender);
}
