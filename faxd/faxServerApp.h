#ident $Header: /d/sam/flexkit/fax/faxd/RCS/faxServerApp.h,v 1.13 91/05/23 12:26:58 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _faxServerApp_
#define	_faxServerApp_

#include "OrderedGlobal.h"
#include "Application.h"
#include "StrArray.h"
#include <sys/types.h>
#include <stdarg.h>

class FaxServer;
class FIFOServer;
class FaxRequest;
class FaxMachineInfo;
class FaxRecvInfo;

struct Job {
    enum JobStatus {
	no_status,
	done,
	requeued,
	removed,
	timedout,
	no_formatter,
	format_failed,
    };
    Job*	next;		// linked list
    fxStr	file;		// queue file name
    time_t	tts;		// time to send job
    int		pri;		// priority

    Job(const fxStr& s, time_t t) : file(s) { tts = t; pri = 0; }
    Job(const Job& j) : file(j.file) { tts = j.tts; pri = 0; }
    Job(const Job* j) : file(j->file) { tts = j->tts; pri = 0; }
    ~Job() {}
};

class faxServerApp : public fxApplication {
private:
    fxStr		appName;		// program name
    fxStr		device;			// modem device
    fxStr		queueDir;		// spooling directory
    FaxServer*		server;			// active server app
    Job*		queue;			// job queue
    fxBool		okToUse2D;		// if TRUE, 2-d encoding works
    int			requeueInterval;	// time between job retries
    unsigned		currentTimeout;		// time associated with alarm
    time_t		jobStart;		// starting time for job
    time_t		fileStart;		// starting time for file
    fxOutputChannel*	sendChannel;		// to fax server app
    fxOutputChannel*	scanChannel;		// for poking app after timeout
    FIFOServer*		fifo;			// fifo job queue interface
    FIFOServer*		devfifo;		// fifo device interface
    FaxRequest*		request;		// current send request

    static const fxStr fifoName;
    static const fxStr configName;
    static const fxStr sendDir;
    static const fxStr recvDir;
    static const fxStr mailCmd;
    static const fxStr notifyCmd;
    static const fxStr ps2faxCmd;

    void logError(const char* fmt ...);
    void vlogError(const char* fmt, va_list ap);
    void record(const char* cmd, const char* from, const char* to,
	int npages, float time);

    void sendJob(FaxRequest* request);
    void insertJob(Job* job);
    void processJob(Job* job);
    void deleteJob(const fxStr& name);
    Job* removeJob(const fxStr& name);
    void alterJob(const char* s);
    FaxRequest* readQFile(const fxStr& filename, int fd);
    void scanQueueDirectory();
    void deleteRequest(JobStatus why, FaxRequest* req, fxBool force = FALSE);
    void notifySender(JobStatus why, FaxRequest* req);
    JobStatus convertPostScript(const fxStr& inFile, const fxStr& outFile,
	float resolution, u_int pageWidth, float pageLength,
	const FaxMachineInfo& info);
public:
    faxServerApp();
    ~faxServerApp();

    void initialize(int argc, char** argv);
    void open();
    void close();
    virtual const char *className() const;

    void fifoMessage(const char* mesage);
    void notifySendComplete(const char* filename);
    void notifyJobComplete(FaxRequest* req = 0);
    void notifyJobRecvd(const FaxRecvInfo& req);
    void scanQueue();
    void timeout();

    void logInfo(const char* fmt ...);
    void vlogInfo(const char* fmt, va_list ap);
};
#endif
