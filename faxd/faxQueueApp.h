/*	$Id: faxQueueApp.h,v 1.50 1996/07/16 01:36:17 sam Rel $ */
/*
 * Copyright (c) 1990-1996 Sam Leffler
 * Copyright (c) 1991-1996 Silicon Graphics, Inc.
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
#ifndef _faxQueueApp_
#define	_faxQueueApp_
/*
 * HylaFAX Queue Manager Application.
 */
#include "faxApp.h"
#include "FaxConfig.h"
#include "IOHandler.h"
#include "Job.h"
#include "DestInfo.h"
#include "DestControl.h"
#include "StrDict.h"

typedef	struct tiff TIFF;

class Class2Params;
class DialStringRules;
class UUCPLock;
class faxRequest;
class Modem;
class Trigger;

/*
 * This class represents a thread of control that manages the
 * job queue, deals with system-related notification (sends
 * complete, facsimile received, errors), and delivers external
 * commands passed in through the command FIFOs.  This class is
 * also responsible for preparing documents for sending by
 * doing formatting tasks such as converting PostScript to TIFF.
 */
class faxQueueApp : public FaxConfig, public faxApp, public IOHandler {
public:
    struct stringtag {
	const char*	 name;
	fxStr faxQueueApp::* p;
	const char*	 def;		// NULL is shorthand for ""
    };
    struct numbertag {
	const char*	 name;
	u_int faxQueueApp::*p;
	u_int		 def;
    };
private:
    class SchedTimeout : public IOHandler {
    private:
	fxBool	started;
    public:
	SchedTimeout();
	~SchedTimeout();
	void timerExpired(long, long);

	void start();
    };
// configuration stuff
    fxStr	configFile;		// configuration filename
    fxStr	contCoverPageTemplate;	// continuation cover page template
    u_int	postscriptTimeout;	// timeout on PostScript imager calls
    u_int	maxConcurrentJobs;	// max parallel jobs to a destination
    u_int	maxSendPages;		// max pages in a send job
    u_int	maxDials;		// max times to dial the phone for a job
    u_int	maxTries;		// max transmits tried for a job
    TimeOfDay	tod;			// time of day restrictions on sends
    DestControl	destCtrls;		// destination control database
    fxStr	longDistancePrefix;	// prefix str for long distance dialing
    fxStr	internationalPrefix;	// prefix str for international dialing
    fxStr	areaCode;		// local area code
    fxStr	countryCode;		// local country code
    DialStringRules* dialRules;		// dial string rules
    fxStr	uucpLockType;		// UUCP lock file type
    fxStr	uucpLockDir;		// UUCP lock file directory
    mode_t	uucpLockMode;		// UUCP lock file creation mode
    u_int	uucpLockTimeout;	// UUCP stale lock file timeout
    u_int	pollLockWait;		// polling interval in lock wait state
    u_int	tracingLevel;		// tracing level w/o session
    u_int	tracingMask;		// tracing level control mask
    fxStr	logFacility;		// syslog facility to direct trace msgs
    u_int	requeueInterval;	// job requeue interval
    fxBool	use2D;			// ok to use 2D-encoded data
    u_int	pageChop;		// default page chop handling
    float	pageChopThreshold;	// minimum space before page chop
    fxStr	notifyCmd;		// external command for notification
    fxStr	ps2faxCmd;		// external command for ps imager
    fxStr	pcl2faxCmd;		// external command for pcl imager
    fxStr	tiff2faxCmd;		// external command for TIFF converter
    fxStr	coverCmd;		// external command for cont cover pages
    fxStr	sendFaxCmd;		// external command for fax transmits
    fxStr	sendPageCmd;		// external command for page transmits
    fxStr	sendUUCPCmd;		// external command for UUCP calls
    fxStr	wedgedCmd;		// external command for wedged modems

    static const stringtag strings[];
    static const numbertag numbers[];
// runtime state
    fxBool	timeout;		// timeout occurred
    fxBool	abortPrepare;		// job preparation should be aborted
    fxBool	quit;			// terminate server
    int		fifo;			// fifo job queue interface
#define	NQHASH		16
    QLink	runqs[NQHASH];		// jobs ready to run
    QLink	sleepq;			// jobs waiting for time-to-send
    QLink	activeq;		// jobs actively being processed
    QLink	deadq;			// jobs waiting to be reaped
    QLink	suspendq;		// jobs suspending from scheduling
    SchedTimeout schedTimeout;		// timeout for running scheduler
    DestInfoDict destJobs;		// jobs organized by destination
    fxStrDict	pendingDocs;		// documents waiting for removal

    static faxQueueApp* _instance;

    static const fxStr sendDir;		// sendq directory name
    static const fxStr docDir;		// docq directory name
    static const fxStr clientDir;	// client directory name

    friend class JobTTSHandler;		// for access to runJob
    friend class JobKillHandler;	// for acccess to timeoutJob
    friend class JobPrepareHandler;	// for acccess to prepareJobDone
    friend class JobSendHandler;	// for acccess to sendJobDone
    friend class faxQueueApp::SchedTimeout;// for access to runScheduler
    friend class ModemLockWaitHandler;	// for access to pollForModemLock

// configuration support
    void	setupConfig();
    void	resetConfig();
    fxBool	setConfigItem(const char* tag, const char* value);
    void	configError(const char* fmt, ...);
    void	configTrace(const char* fmt, ...);
    void	setDialRules(const char* name);
    fxStr	canonicalizePhoneNumber(const fxStr& ds);
// modem support
    void	scanForModems();
    fxBool	assignModem(Job& job);
    void	releaseModem(Job& job);
    void	notifyModemWedged(Modem&);
    void	pollForModemLock(Modem& modem);
// diagnostic and tracing interfaces
    void	traceServer(const char* fmt ...);
    void	vtraceServer(const char* fmt, va_list ap);
    void	jobError(const Job& job, const char* fmt ...);
    void	traceQueue(const Job&, const char* fmt ...);
    void	traceJob(const Job&, const char* fmt ...);
    void	traceQueue(const char* fmt ...);
    void	traceModem(const Modem&, const char* fmt ...);
// miscellaneous stuff
    void	startTimeout(long ms);
    void	stopTimeout(const char* whichdir);
    const fxStr& pickCmd(const FaxRequest& req);
// FIFO-related stuff
    int		inputReady(int);		// Dispatcher hook
    void	openFIFOs();
    void	closeFIFOs();
    void	FIFOMessage(const char*);
    void	FIFOMessage(char cmd, const fxStr& id, const char* args);
    void	FIFOModemMessage(const fxStr& devid, const char* msg);
    void	FIFOJobMessage(const fxStr& jobid, const char* msg);
    void	FIFORecvMessage(const fxStr& devid, const char* msg);
    void	scanClientDirectory();
// qfile request stuff
    void	scanQueueDirectory();
    FaxRequest*	readRequest(Job&);
    void	updateRequest(FaxRequest& req, Job& job);
    void	deleteRequest(Job&, FaxRequest* req, JobStatus why,
		    fxBool force, const char* duration = "");
    void	deleteRequest(Job&, FaxRequest& req, JobStatus why,
		    fxBool force, const char* duration = "");
    void	notifySender(Job&, JobStatus, const char* = "");
// job management interfaces
    void	processJob(Job& job);
    void	processJob(Job&, FaxRequest* req,
		    DestInfo&, const DestControlInfo&);
    void	sendJobStart(Job&, FaxRequest*, const DestControlInfo&);
    void	sendJobDone(Job& job, int status);
    void	blockJob(Job&, FaxRequest&, const char*);
    void	delayJob(Job&, FaxRequest&, const char*, time_t);
    void	rejectJob(Job& job, FaxRequest& req, const fxStr& reason);
    fxBool	alterJob(const char*);
    fxBool	submitJob(const fxStr& jobid, fxBool checkState = FALSE);
    fxBool	suspendJob(const fxStr& jobid, fxBool abortActive);
    void	rejectSubmission(Job&, FaxRequest&, const fxStr& reason);

    void	setReadyToRun(Job& job);
    void	setSleep(Job& job, time_t tts);
    void	setDead(Job& job);
    void	setActive(Job& job);
    void	setSuspend(Job& job);
    fxBool	submitJob(FaxRequest&, fxBool checkState = FALSE);
    fxBool	submitJob(Job& job, FaxRequest& req, fxBool checkState = FALSE);
    fxBool	suspendJob(Job& job, fxBool abortActive);
    fxBool	terminateJob(const fxStr& filename, JobStatus why);
    void	timeoutJob(Job& job);
    void	timeoutJob(Job& job, FaxRequest&);
    void	runJob(Job& job);

    void	runScheduler();
    void	pokeScheduler();
// job preparation stuff
    fxBool	prepareJobNeeded(Job&, FaxRequest&, JobStatus&);
    static void prepareCleanup(int s);
    void	prepareJobStart(Job&, FaxRequest*,
		    FaxMachineInfo&, const DestControlInfo&);
    void	prepareJobDone(Job&, int status);
    JobStatus	prepareJob(Job& job, FaxRequest& req,
		    const FaxMachineInfo&, const DestControlInfo&);
    JobStatus	convertDocument(Job&,
		    const faxRequest& input, const fxStr& outFile,
		    const Class2Params& params,
		    const DestControlInfo& dci, fxStr& emsg);
    JobStatus	runConverter(Job& job, const char* app, char* const* argv,
		    fxStr& emsg);
    fxBool	runConverter1(Job& job, int fd, fxStr& output);
    void	makeCoverPage(Job&, FaxRequest&, const Class2Params&,
		    const DestControlInfo&);
    fxBool	preparePageHandling(FaxRequest&,
		    const FaxMachineInfo&, const DestControlInfo&, fxStr& emsg);
    void	preparePageChop(const FaxRequest&,
		    TIFF*, const Class2Params&, fxStr&);
    void	setupParams(TIFF*, Class2Params&, const FaxMachineInfo&);
// document reference counting support
    void	unrefDoc(const fxStr& file);
public:
    faxQueueApp();
    ~faxQueueApp();

    static faxQueueApp& instance();

    void	initialize(int argc, char** argv);
    void	open();

    // NB: public for use by Modem class
    UUCPLock*	getUUCPLock(const fxStr& deviceName);

    // used by DestControlInfo for default values
    u_int	getTracingLevel() const;
    u_int	getMaxConcurrentJobs() const;
    u_int	getMaxSendPages() const;
    u_int	getMaxDials() const;
    u_int	getMaxTries() const;
    time_t	nextTimeToSend(time_t) const;
};
#endif
