#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/RecvQList.h,v 1.3 91/05/23 12:13:28 sam Exp $
#ifndef _RecvQList_
#define	_RecvQList_

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "ScrollList.h"
#include "Font.h"
#include "RecvQ.h"

// Scrolling list of received facsimile

class RecvQList : public fxScrollList {
private:
private:
    RecvQPtrArray	items;
    fxFontPtr		senderFnt;	// font for sender identity
    fxFontPtr		otherFnt;	// font for other fields
    u_short		lineHeight;	// height of a list slot
    u_short		dateOffset;	// offset to date field
    u_short		senderOffset;	// offset to subject field
    u_short		pagesOffset;
    u_short		qualityOffset;
    fxGLColor		textColor;	// color of text items
    fxGLColor		highlightColor;	// color of selected items

    friend class RecvQListTitle;	// for sizing header fields

    virtual u_int slotHeight() const;
    void paintLine(u_int ix, int y, fxBool picked);
    virtual void paintLine(u_int ix);
    virtual void sendSlotValue(u_int slot);
public:
    RecvQList(fxLayoutConstraint xc, fxLayoutConstraint yc);
    virtual ~RecvQList();
    virtual const char* className() const;

    virtual void paint();
    void updateLine(u_int ix);
    void updateLine(RecvQ* a);
    fxBool scroll(RecvQ* a);

    // Single selection control
    void setSelection(RecvQ*, fxBool allowUpdate = TRUE);
    RecvQ* getSelection();

    // Multiple selection control
    // NOTE: these do not fire output to the newSelectionChannel,
    // since the caller knows by calling them that the selection is changing.
    void selectItem(RecvQ*, fxBool clearSelectionFirst = FALSE, fxBool allowUpdate = TRUE);
    void deselectItem(RecvQ*, fxBool allowUpdate = TRUE);

    // calling sendSelections() causes the selected items to be sent to
    // the "value" output channel, one at a time.
    // getSelectionCount tells how many items will be sent.
    void getSelections(RecvQPtrArray& result);
    RecvQ* getFirstSelection();

    virtual void add(RecvQ*, fxBool update = TRUE,
		     fxBool picked = FALSE);
    virtual void addUnique(RecvQ*, fxBool update = TRUE,
			   fxBool picked = FALSE);
    virtual void insert(RecvQ*, u_int ix = 0, fxBool update = TRUE,
		        fxBool picked = FALSE);
    virtual void insertUnique(RecvQ*, u_int ix = 0,
			      fxBool update = TRUE, fxBool picked = FALSE);
    virtual void remove(RecvQ*, fxBool update = TRUE);
    virtual void removeAll(fxBool allowUpdate = TRUE);
    fxBool contains(RecvQ* a) const;
    RecvQ* operator[](u_int ix) const;
    u_int length() const;

    u_int itemToSlot(RecvQ*) const;
    RecvQ* slotToItem(u_int slot) const;

    void sort(fxBool allowUpdate = TRUE);
};

inline RecvQ* RecvQList::operator[](u_int ix) const { return items[ix]; }
inline u_int RecvQList::length() const		    { return items.length(); }

class RecvQListTitle : public fxView {
private:
    RecvQList*	list;
    fxFontPtr	fnt;
    fxGLColor	pageColor;
    fxGLColor	textColor;
public:
    RecvQListTitle(RecvQList* list);
    virtual ~RecvQListTitle();
    virtual const char* className() const;

    virtual fxLayoutConstraint getLayoutConstraint(fxOrientation axis);

    virtual void paint();

    void setPageColor(const fxGLColor& c) { pageColor = c; }
    void setTextColor(const fxGLColor& c) { textColor = c; }
};
#endif /* _RecvQList_ */
