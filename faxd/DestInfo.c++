/*	$Header: /usr/people/sam/fax/./faxd/RCS/DestInfo.c++,v 1.7 1995/04/08 21:29:59 sam Rel $ */
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
#include "DestInfo.h"
#include "Job.h"
#include "Str.h"

DestInfo::DestInfo()
{
    activeCount = 0;
    blockedCount = 0;
    running = NULL;
}

DestInfo::DestInfo(const DestInfo& other)
    : QLink(other)
    , info(other.info)
{
    activeCount = other.activeCount;
    blockedCount = other.blockedCount;
    running = other.running;
}

DestInfo::~DestInfo() {}

FaxMachineInfo&
DestInfo::getInfo(const fxStr& number)
{
    info.updateConfig(number);			// update as necessary
    return info;
}

void
DestInfo::updateConfig()
{
    info.writeConfig();				// update as necessary
}

fxBool
DestInfo::isActive(Job& job) const
{
    if (running == NULL)
	return (FALSE);
    else if (running == &job)
	return (TRUE);
    else {
	for (Job* jp = running->dnext; jp != NULL; jp = jp->dnext)
	    if (jp == &job)
		return (TRUE);
	return (FALSE);
    }
}

void
DestInfo::active(Job& job)
{
    if (running == NULL) {			// list empty
	running = &job;
	job.dnext = NULL;
	activeCount++;
    } else if (running == &job) {		// job on list already
	return;
    } else {					// general case
	Job* jp;
	for (Job** jpp = &running->dnext; (jp = *jpp) != NULL; jpp = &jp->dnext)
	    if (jp == &job)
		return;
	*jpp = &job;
	job.dnext = NULL;
	activeCount++;
    }
}

void
DestInfo::done(Job& job)
{
    if (running == &job) {			// job at head of list
	running = job.dnext;
	job.dnext = NULL;
	activeCount--;
    } else if (running == NULL) {		// list empty
	return;
    } else {					// general case
	Job* jp;
	for (Job** jpp = &running->dnext; (jp = *jpp) != NULL; jpp = &jp->dnext)
	    if (jp == &job) {
		*jpp = job.dnext;
		job.dnext = NULL;
		activeCount--;
		break;
	    }
    }
}

void
DestInfo::block(Job& job)
{
    job.insert(*this);
    blockedCount++;
}

Job*
DestInfo::nextBlocked()
{
    if (next != this) {
	Job* job = (Job*) next;
	job->remove();
	blockedCount--;
	return (job);
    } else
	return (NULL);
}

Job*
DestInfo::unblock(const fxStr& filename)
{
    for (JobIter iter(*this); iter.notDone(); iter++) {
	Job& job = iter;
	if (job.file == filename) {
	    job.remove();
	    blockedCount--;
	    return (&job);
	}
    }
    return (NULL);
}

fxIMPLEMENT_StrKeyObjValueDictionary(DestInfoDict, DestInfo);
