/*	$Header: /usr/people/sam/fax/./faxd/RCS/Modem.c++,v 1.16 1995/04/08 21:30:53 sam Rel $ */
/*
 * Copyright (c) 1990-1995 Sam Leffler
 * Copyright (c) 1991-1995 Silicon Graphics, Inc.
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
#include "Sys.h"

#include <errno.h>

#include "Modem.h"
#include "Job.h"
#include "faxQueueApp.h"
#include "UUCPLock.h"
#include "config.h"

QLink Modem::list;

Modem::Modem(const fxStr& id)
    : devID(id)
    , fifoName(FAX_FIFO "." | id)
{
    state = DOWN;			// modem down until notified otherwise
    canpoll = TRUE;			// be optimistic
    fd = -1;				// force open on first use
    insert(list);			// place on free list
    /*
     * Convert an identifier to the pathname for the
     * device (required by the UUCP lock code).  This
     * is done converting '_'s to '/'s and then prepending
     * DEV_PREFIX.  This is required for SVR4 systems
     * which have their devices in subdirectories!
     */
    fxStr dev = id;
    u_int l;
    while ((l = dev.next(0, '_')) < dev.length())
	dev[l] = '/';
    lock = faxQueueApp::instance().getUUCPLock(DEV_PREFIX | dev);
}

Modem::~Modem()
{
    delete lock;
    if (fd > 0)
	::close(fd);
    remove();
}

/*
 * Given a modem device-id, return a reference
 * to a Modem instance.  If no instance exists,
 * one is created and added to the list of known
 * modems.
 */
Modem&
Modem::getModemByID(const fxStr& id)
{
    for (ModemIter iter(list); iter.notDone(); iter++) {
	Modem& modem = iter;
	if (modem.devID == id)
	    return (modem);
    }
    return (*new Modem(id));
}

fxBool
Modem::modemExists(const fxStr& id)
{
    for (ModemIter iter(list); iter.notDone(); iter++)
	if (iter.modem().devID == id)
	    return (TRUE);
    return (FALSE);
}

fxBool
Modem::assign(Job& job)
{
    if (job.device != MODEM_ANY && job.device != devID)
	return (FALSE);
    // XXX per-modem tod usage for fax, data, voice
    if (job.willpoll && !canpoll)
	return (FALSE);
    if (job.pagewidth && !supportsPageWidthInMM(job.pagewidth))
	return (FALSE);
    if (job.pagelength && !supportsPageLengthInMM(job.pagelength))
	return (FALSE);
    if (job.resolution && !supportsVRes(job.resolution))
	return (FALSE);
    if (lock->lock()) {		// lock modem for use
	state = BUSY;		// mark in use
	job.modem = this;	// assign modem to job
	return (TRUE);
    } else
	return (FALSE);
}

/*
 * Release a previously assigned modem.
 */
void
Modem::release()
{
    lock->unlock();
    /*
     * We must mark the modem READY when releasing the lock
     * because we cannot depend on the faxgetty process 
     * notifying us if/when the modem status changes.  This
     * may result in overzealous scheduling of the modem, but
     * sender apps are expected to stablize the modem before
     * starting work it shouldn't be too bad.
     */
    state = READY;
}

void
Modem::setCapabilities(const char* s)
{
    canpoll = (s[0] == 'P');	// P for polling, other for no support
    caps.decodeCaps(s+1);	//...parse capabilities string...
    state = READY;		// XXX needed for static configuration
}

void
Modem::traceState(const char* state)
{
    faxQueueApp::instance().traceServer("MODEM " | devID | " %s", state);
}

void
Modem::FIFOMessage(const char* cp)
{
    switch (cp[0]) {
    case 'R':			// modem ready, parse capabilities
	traceState("READY");
	setCapabilities(&cp[1]);
	break;
    case 'B':			// modem busy doing something
	traceState("BUSY");
	state = BUSY;
	break;
    case 'D':			// modem to be marked down
	traceState("DOWN");
	state = DOWN;
	break;
    default:
	logError("Unknown Modem FIFO message \"%s\"", cp);
	break;
    }
}

/*
 * Return whether or not the modem supports the
 * specified page width.  We perhaps should accept
 * page width when large page sizes are supported
 * (but then the caller would need to know in order
 * to pad the image to the appropriate width).
 */
fxBool
Modem::supportsPageWidthInMM(u_int w) const
{
    if (w <= 110)		// 864 pixels + slop
	return caps.wd & BIT(WD_864);
    else if (w <= 154)		// 1216 pixels + slop
	return caps.wd & BIT(WD_1216);
    else if (w <= 218)		// 1728 pixels + slop
	return caps.wd & BIT(WD_1728);
    else if (w <= 258)		// 2048 pixels + slop
	return caps.wd & BIT(WD_2048);
    else if (w <= 306)		// 2432 pixels + slop
	return caps.wd & BIT(WD_2432);
    else
	return FALSE;
}

fxBool
Modem::supportsPageWidthInPixels(u_int w) const
{
    if (w <= 880)		// 864 pixels + slop
	return caps.wd & BIT(WD_864);
    else if (w <= 1232)		// 1216 pixels + slop
	return caps.wd & BIT(WD_1216);
    else if (w <= 1744)		// 1728 pixels + slop
	return caps.wd & BIT(WD_1728);
    else if (w <= 2064)		// 2048 pixels + slop
	return caps.wd & BIT(WD_2048);
    else if (w <= 2448)		// 2432 pixels + slop
	return caps.wd & BIT(WD_2432);
    else
	return FALSE;
}

/*
 * Return whether or not the modem supports the
 * specified vertical resolution.  Note that we're
 * rather tolerant because of potential precision
 * problems and general sloppiness on the part of
 * applications writing TIFF files.
 */
fxBool
Modem::supportsVRes(float res) const
{
    if (75 <= res && res < 120)
	return caps.vr & BIT(VR_NORMAL);
    else if (150 <= res && res < 250)
	return caps.vr & BIT(VR_FINE);
    else
	return FALSE;
}

/*
 * Return whether or not the modem supports 2DMR.
 */
fxBool
Modem::supports2D() const
{
    return caps.df & BIT(DF_2DMR);
}

/*
 * Return whether or not the modem supports the
 * specified page length.  As above for vertical
 * resolution we're lenient in what we accept.
 */
fxBool
Modem::supportsPageLengthInMM(u_int l) const
{
    // XXX probably need to be more forgiving with values
    if (270 < l && l <= 330)
	return caps.ln & (BIT(LN_A4)|BIT(LN_INF));
    else if (330 < l && l <= 390)
	return caps.ln & (BIT(LN_B4)|BIT(LN_INF));
    else
	return caps.ln & BIT(LN_INF);
}

void
Modem::broadcast(const fxStr& msg)
{
    for (ModemIter iter(list); iter.notDone(); iter++)
	iter.modem().send(msg);
}

fxBool
Modem::send(const fxStr& msg)
{
again:
    if (fd < 0)
	fd = Sys::open(fifoName, O_WRONLY|O_NDELAY);
    if (fd >= 0) {
	int n = Sys::write(fd, msg, msg.length());
	if (n == -1 && errno == EBADF) {
	    ::close(fd), fd = -1;
	    goto again;
	}
	return (n == msg.length());
    } else
	return (FALSE);
}
