/*	$Id: TriggerRef.c++,v 1.2 1996/06/24 03:00:44 sam Rel $ */
/*
 * Copyright (c) 1995-1996 Sam Leffler
 * Copyright (c) 1995-1996 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */
#include "Trigger.h"
#include "TriggerRef.h"

/*
 * Reference-counted trigger references.
 */
TriggerRef::TriggerRef(Trigger& tr) : ref(tr) { tr.refs++; }
TriggerRef::~TriggerRef() { ref.refs--; }

/*
 * Purge a list of references.  If any of these
 * triggers are referenced only by this list, then
 * purge the trigger as well.
 */
void
TriggerRef::purge(QLink& head)
{
    QLink* next;
    for (QLink* ql = head.next; ql != &head; ql = next) {
	next = ql->next;
	TriggerRef* tr = (TriggerRef*) ql;
	Trigger* t = &tr->ref;
	delete tr;
	if (t->refs == 0)
	    delete t;
    }
}

/*
 * Purge references to a specific trigger.
 */
void
TriggerRef::purge(QLink& head, Trigger* t)
{
    QLink* next;
    for (QLink* ql = head.next; ql != &head; ql = next) {
	next = ql->next;
	TriggerRef* tr = (TriggerRef*) ql;
	if (&tr->ref == t) {
	    tr->remove();		// decrements refs as a side effect
	    delete tr;
	    if (t->refs == 0)
		return;
	}
    }
}
