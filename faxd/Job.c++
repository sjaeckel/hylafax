/*	$Header: /usr/people/sam/fax/./faxd/RCS/Job.c++,v 1.18 1995/04/08 21:30:48 sam Rel $ */
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
#include "faxQueueApp.h"
#include "Dispatcher.h"
#include "Sys.h"
#include "config.h"

JobKillHandler::JobKillHandler(Job& j) : job(j) {}
JobKillHandler::~JobKillHandler() {}
void JobKillHandler::timerExpired(long, long)
    { faxQueueApp::instance().timeoutJob(job); }

JobTTSHandler::JobTTSHandler(Job& j) : job(j) {}
JobTTSHandler::~JobTTSHandler() {}
void JobTTSHandler::timerExpired(long, long)
    { faxQueueApp::instance().runJob(job); }

JobPrepareHandler::JobPrepareHandler(Job& j) : job(j) {}
JobPrepareHandler::~JobPrepareHandler() {}
void JobPrepareHandler::childStatus(pid_t, int status)
    { faxQueueApp::instance().prepareJobDone(job, status); }

JobSendHandler::JobSendHandler(Job& j) : job(j) {}
JobSendHandler::~JobSendHandler() {}
void JobSendHandler::childStatus(pid_t, int status)
    { faxQueueApp::instance().sendJobDone(job, status); }

Job::Job(const fxStr& s, const fxStr& id, const fxStr& m, int p, time_t t)
    : killHandler(*this)
    , ttsHandler(*this)
    , prepareHandler(*this)
    , sendHandler(*this)
    , file(s)
    , jobid(id)
    , device(m)
{
    tts = t;
    pri = p;
    killtime = 0;
    dnext = NULL;
    modem = NULL;
    abortPending = FALSE;
}

Job::~Job()
{
    stopKillTimer();
    stopTTSTimer();
}

void
Job::startKillTimer(long sec)
{
    killtime = sec;
    Dispatcher::instance().startTimer(sec - Sys::now(), 0, &killHandler);
}

void
Job::stopKillTimer()
{
    Dispatcher::instance().stopTimer(&killHandler);
}

void
Job::startTTSTimer(long sec)
{
    tts = sec;
    Dispatcher::instance().startTimer(sec - Sys::now(), 0, &ttsHandler);
}

void
Job::stopTTSTimer()
{
    Dispatcher::instance().stopTimer(&ttsHandler);
}

void
Job::startPrepare(pid_t p)
{
    Dispatcher::instance().startChild(pid = p, &prepareHandler);
}

void
Job::startSend(pid_t p)
{
    Dispatcher::instance().startChild(pid = p, &sendHandler);
}

const char*
Job::jobStatusName(const JobStatus status)
{
    static const char* names[] = {
	"no_status",
	"done",
	"requeued",
	"removed",
	"timedout",
	"no_formatter",
	"failed",
	"format_failed",
	"poll_rejected",
	"poll_no_document",
	"poll_failed",
	"killed",
	"blocked",
	"rejected",
    };
#define	N(a)	(sizeof (a) / sizeof (a[0]))
    if ((u_int) status >= N(names)) {
	static char s[30];
	::sprintf(s, "status_%u", (u_int) status);
	return (s);
    } else
	return (names[status]);
}
#undef N
