/*	$Header: /usr/people/sam/fax/./faxd/RCS/Job.h,v 1.21 1995/04/08 21:30:49 sam Rel $ */
/*
 * Copyright (c) 1994-1995 Sam Leffler
 * Copyright (c) 1994-1995 Silicon Graphics, Inc.
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
#ifndef _Job_
#define	_Job_
/*
 * Queue Manager Job.
 */
#include "IOHandler.h"
#include "QLink.h"
#include "Str.h"

typedef unsigned int JobStatus;
class Modem;
class Job;
class FaxRequest;

/*
 * NB: These should be private nested classes but various
 *     C++ compilers cannot grok it.
 */
class JobKillHandler : public IOHandler {
private:
    Job& job;
public:
    JobKillHandler(Job&);
    ~JobKillHandler();
    void timerExpired(long, long);
};
class JobTTSHandler : public IOHandler {
private:
    Job& job;
public:
    JobTTSHandler(Job&);
    ~JobTTSHandler();
    void timerExpired(long, long);
};
class JobPrepareHandler : public IOHandler {
private:
    Job& job;
public:
    JobPrepareHandler(Job&);
    ~JobPrepareHandler();
    void childStatus(pid_t, int);
};
class JobSendHandler : public IOHandler {
private:
    Job& job;
public:
    JobSendHandler(Job&);
    ~JobSendHandler();
    void childStatus(pid_t, int);
};

/*
 * Jobs represent outbound requests in the queue.
 */
class Job : public QLink {
private:
    JobKillHandler	killHandler;	// Dispatcher handler for kill timeout
    JobTTSHandler	ttsHandler;	// Dispatcher handler for tts timeout
    JobPrepareHandler	prepareHandler;	// Dispatcher handler for job prep work
    JobSendHandler	sendHandler;	// Dispatcher handler for job send work
public:
    enum {
	no_status	= 0,
	done		= 1,		// job completed successfully
	requeued	= 2,		// job requeued after attempt
	removed		= 3,		// job removed by user command
	timedout	= 4,		// job kill time expired
	no_formatter	= 5,		// PostScript formatter not found
	failed		= 6,		// job completed w/o success
	format_failed	= 7,		// PostScript formatting failed
	poll_rejected	= 8,		// poll rejected by destination
	poll_no_document= 9,		// poll found no documents
	poll_failed	= 10,		// poll failed for unknown reason
	killed		= 11,		// job killed by user command
	blocked		= 12,		// job waiting for resource or event
	rejected	= 13		// job rejected before send attempted
    };
    fxStr	file;		// queue file name
    fxStr	jobid;		// job identifier
    pid_t	pid;		// pid of current subprocess
    int		pri;		// priority
    time_t	tts;		// time to send job
    time_t	killtime;	// time to kill job
    time_t	start;		// time job passed to modem
    fxStr	dest;		// canonical destination identity
    Job*	dnext;		// linked list by destination
    fxStr	device;		// modem to be used
    Modem*	modem;		// modem/server currently assigned to job
    u_short	pagewidth;	// desired output page width (mm)
    u_short	pagelength;	// desired output page length (mm)
    u_short	resolution;	// desired vertical resolution (lpi)
    fxBool	willpoll;	// job has polling request
    fxBool	abortPending;	// user abort pending for job

    Job(const fxStr& filename
	, const fxStr& jobid
	, const fxStr& modem
	, int pri
	, time_t tts
    );
    ~Job();

    static const char* jobStatusName(const JobStatus);

    void startKillTimer(long sec);
    void stopKillTimer();

    void startTTSTimer(long sec);
    void stopTTSTimer();

    void startPrepare(pid_t pid);
    void startSend(pid_t pid);
};

/*
 * Job iterator class for iterating over lists.
 */
class JobIter {
private:
    const QLink* head;
    QLink*	ql;
    QLink*	next;
public:
    JobIter(QLink& q)		{ head = &q; ql = q.next, next = ql->next; }
    ~JobIter() {}

    void operator=(QLink& q)	{ head = &q; ql = q.next; next = ql->next; }
    void operator++()		{ ql = next, next = ql->next; }
    void operator++(int)	{ ql = next, next = ql->next; }
    operator Job&() const	{ return *(Job*)ql; }
    operator Job*() const	{ return (Job*) ql; }
    Job& job() const		{ return *(Job*)ql; }
    fxBool notDone()		{ return ql != head; }
};
#endif /* _Job_ */
