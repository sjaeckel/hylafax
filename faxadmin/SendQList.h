#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/SendQList.h,v 1.3 91/05/23 12:13:51 sam Exp $
#ifndef _SendQList_
#define	_SendQList_

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
#include "Array.h"

// Scrolling lists of send-q jobs

class SendQ;
class SendQPtrArray;

class SendQList : public fxScrollList {
private:
    struct SendQItem {
	SendQ*	entry;		// q entry reference
	u_short	senderLen;	// calculated sender field length
	u_short	destLen;	// calculated destination field length
	SendQItem(SendQ* e, u_short sl, u_short dl)
	    { entry = e; senderLen = sl; destLen = dl; } 
    };
    fxDECLARE_Array(SendQItemArray, SendQItem)
private:
    SendQItemArray	items;
    fxFontPtr		senderFnt;	// font for sender identity
    fxFontPtr		otherFnt;	// font for other fields
    u_short		lineHeight;	// height of a list slot
    u_short		dateOffset;	// offset to date field
    u_short		senderMaxWidth;	// subject field width (pixels)
    u_short		senderOffset;	// offset to subject field
    char*		senderBuf;	// overflow construction buffer
    u_short		destMaxWidth;	// author field width (pixels)
    u_short		destOffset;	// offset to author field
    fxGLColor		textColor;	// color of text items
    fxGLColor		highlightColor;	// color of selected items

    friend class SendQListTitle;	// for sizing header fields

    virtual u_int slotHeight() const;
    void paintLine(u_int ix, int y, fxBool picked);
    virtual void paintLine(u_int ix);
    virtual void sendSlotValue(u_int slot);
    short getStrLen(const fxStr&, fxFont*, u_short maxWidth);
public:
    SendQList(fxLayoutConstraint xc, fxLayoutConstraint yc);
    virtual ~SendQList();
    virtual const char* className() const;

    virtual void paint();
    void updateLine(u_int ix);
    void updateLine(SendQ* a);
    fxBool scroll(SendQ* a);

    // Single selection control
    void setSelection(SendQ*, fxBool allowUpdate = TRUE);
    SendQ* getSelection();

    // Multiple selection control
    // NOTE: these do not fire output to the newSelectionChannel,
    // since the caller knows by calling them that the selection is changing.
    void selectItem(SendQ*, fxBool clearSelectionFirst = FALSE, fxBool allowUpdate = TRUE);
    void deselectItem(SendQ*, fxBool allowUpdate = TRUE);

    // calling sendSelections() causes the selected items to be sent to
    // the "value" output channel, one at a time.
    // getSelectionCount tells how many items will be sent.
    void getSelections(SendQPtrArray& result);
    SendQ* getFirstSelection();

    virtual void add(SendQ*, fxBool update = TRUE,
		     fxBool picked = FALSE);
    virtual void addUnique(SendQ*, fxBool update = TRUE,
			   fxBool picked = FALSE);
    virtual void insert(SendQ*, u_int ix = 0, fxBool update = TRUE,
		        fxBool picked = FALSE);
    virtual void insertUnique(SendQ*, u_int ix = 0,
			      fxBool update = TRUE, fxBool picked = FALSE);
    virtual void remove(SendQ*, fxBool update = TRUE);
    virtual void removeAll(fxBool allowUpdate = TRUE);
    fxBool contains(SendQ* a) const;
    SendQ* operator[](u_int ix) const;
    u_int length() const;

    u_int itemToSlot(SendQ*) const;
    SendQ* slotToItem(u_int slot) const;

    void sort(fxBool allowUpdate = TRUE);
};

inline SendQ* SendQList::operator[](u_int ix) const
    { return items[ix].entry; }
inline u_int SendQList::length() const
    { return items.length(); }

class SendQListTitle : public fxView {
private:
    SendQList*	list;
    fxFontPtr	fnt;
    fxGLColor	pageColor;
    fxGLColor	textColor;
public:
    SendQListTitle(SendQList* list);
    virtual ~SendQListTitle();
    virtual const char* className() const;

    virtual fxLayoutConstraint getLayoutConstraint(fxOrientation axis);

    virtual void paint();

    void setPageColor(const fxGLColor& c) { pageColor = c; }
    void setTextColor(const fxGLColor& c) { textColor = c; }
};
#endif /* _SendQList_ */
