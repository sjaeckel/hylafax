/*	$Header: /usr/people/sam/fax/./faxd/RCS/faxGettyApp.c++,v 1.30 1995/04/08 21:31:15 sam Rel $ */
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
#include <sys/types.h>
#include <ctype.h>
#include <osfcn.h>
#include <errno.h>
#include <pwd.h>
#include <math.h>
#include <limits.h>
#include <sys/file.h>

#include "Sys.h"

#include "Dispatcher.h"

#include "FaxRecvInfo.h"
#include "FaxMachineInfo.h"
#include "FaxAcctInfo.h"
#include "faxGettyApp.h"
#include "UUCPLock.h"
#include "Getty.h"
#include "RegExArray.h"
#include "BoolArray.h"
#include "config.h"

/*
 * HylaFAX Spooling and Command Agent.
 */

const fxStr faxGettyApp::fifoName	= FAX_FIFO;
const fxStr faxGettyApp::recvDir	= FAX_RECVDIR;

extern	const char* fmtTime(time_t);

faxGettyApp* faxGettyApp::_instance = NULL;

faxGettyApp::faxGettyApp(const fxStr& devName, const fxStr& devID)
    : FaxServer(devName, devID)
{
    devfifo = -1;
    modemLock = NULL;
    debug = FALSE;
    lastCIDModTime = 0;
    cidPats = NULL;
    acceptCID = NULL;
    setupConfig();

    fxAssert(_instance == NULL, "Cannot create multiple faxGettyApp instances");
    _instance = this;
}

faxGettyApp::~faxGettyApp()
{
    delete acceptCID;
    delete cidPats;
    delete modemLock;
}

faxGettyApp& faxGettyApp::instance() { return *_instance; }

void
faxGettyApp::initialize(int argc, char** argv)
{
    for (GetoptIter iter(argc, argv, getOpts()); iter.notDone(); iter++)
	switch (iter.option()) {
	case 'D':
	    detachFromTTY();
	    break;
	case 'd':
	    debug = TRUE;
	    break;
	}
    FaxServer::initialize(argc, argv);
    faxApp::initialize(argc, argv);
    serverPID = fxStr::format("%u", ::getpid());
    modemLock = getUUCPLock(getModemDevice());
    sendQueuer("D");			// notify that modem exists
}

void
faxGettyApp::open()
{
    traceServer("OPEN " | getModemDevice());
    faxApp::open();
    FaxServer::open();
}

void
faxGettyApp::close()
{
    if (isRunning()) {
	sendQueuer("D");
	traceServer("CLOSE " | getModemDevice());
	faxApp::close();
	FaxServer::close();
    }
}

fxBool faxGettyApp::lockModem()		{ return modemLock->lock(); }
void faxGettyApp::unlockModem()		{ modemLock->unlock(); }
fxBool faxGettyApp::isModemLocked()	{ return modemLock->isLocked(); }

fxBool
faxGettyApp::setupModem()
{
    /*
     * Reread the configuration file if it has been
     * changed.  We do this before each setup operation
     * since we are a long-running process and it should
     * not be necessary to restart the process to have
     * config file changes take effect.
     */
    updateConfig(getConfigFile());
    if (FaxServer::setupModem()) {
	/*
	 * Setup modem for receiving.
	 */
	FaxModem* modem = (FaxModem*) ModemServer::getModem();
	modem->setupReceive();
	/*
	 * If the server is configured, listen for incoming calls.
	 */
	setRingsBeforeAnswer(ringsBeforeAnswer);
	return (TRUE);
    } else
	return (FALSE);
}

/*
 * Discard any handle on the modem.
 */
void
faxGettyApp::discardModem(fxBool dropDTR)
{
    int fd = getModemFd();
    if (fd >= 0) {
	if (Dispatcher::instance().handler(fd, Dispatcher::ReadMask))
	    Dispatcher::instance().unlink(fd);
    }
    FaxServer::discardModem(dropDTR);
}

/*
 * Answer the telephone in response to data from the modem
 * (e.g. a "RING" message) or an explicit command from the
 * user (sending an "ANSWER" command through the FIFO).
 */
void
faxGettyApp::answerPhone(AnswerType atype, fxBool force)
{
    if (lockModem()) {
	sendQueuer("B");
	changeState(ANSWERING);
	CallerID cid;
	CallType ctype = ClassModem::CALLTYPE_UNKNOWN;
	if (force || modemWaitForRings(ringsBeforeAnswer, ctype, cid)) {
	    beginSession(FAXNumber);
	    /*
	     * Answer the phone according to atype.  If this is
	     * ``any'', then pick a more specific type.  If
	     * adaptive-answer is enabled and ``any'' is requested,
	     * then rotate through the set of possibilities.
	     */
	    fxBool callResolved;
	    fxBool advanceRotary = TRUE;
	    fxStr emsg;
	    if (!isCIDOk(cid.number)) {		// check Caller ID if present
		/*
		 * Call was rejected based on Caller ID information.
		 */
		traceServer("ANSWER: CID REJECTED: NUMBER \"%s\" NAME \"%s\"",
		    (char*) cid.number, (char*) cid.name);
		callResolved = FALSE;
		advanceRotary = FALSE;
	    } else if (ctype != ClassModem::CALLTYPE_UNKNOWN) {
		/*
		 * Distinctive ring or other means has already identified
		 * the type of call.  If we're to answer the call in a
		 * different way, then treat this as an error and don't
		 * answer the phone.  Otherwise answer according to the
		 * deduced call type.
		 */
		if (atype != ClassModem::ANSTYPE_ANY && ctype != atype) {
		    traceServer("ANSWER: Call deduced as %s,"
			 "but told to answer as %s; call ignored",
			 ClassModem::callTypes[ctype],
			 ClassModem::answerTypes[atype]);
		    callResolved = FALSE;
		    advanceRotary = FALSE;
		} else {
		    ctype = modemAnswerCall(ctype, emsg);
		    callResolved = processCall(ctype, emsg);
		}
	    } else if (atype == ClassModem::ANSTYPE_ANY) {
		int r = answerRotor;
		do {
		    atype = answerRotary[r];
		    ctype = modemAnswerCall(atype, emsg);
		    callResolved = processCall(ctype, emsg);
		    r = (r+1) % answerRotorSize;
		} while (!callResolved && adaptiveAnswer && r != answerRotor);
	    } else {
		ctype = modemAnswerCall(atype, emsg);
		callResolved = processCall(ctype, emsg);
	    }
	    /*
	     * Call resolved.  If we were able to recognize the call
	     * type and setup a session, then reset the answer rotary
	     * state if there is a bias toward a specific answer type.
	     * Otherwise, if the call failed, advance the rotor to
	     * the next answer type in preparation for the next call.
	     */
	    if (callResolved) {
		if (answerBias != (u_int) -1)
		    answerRotor = answerBias;
	    } else if (advanceRotary) {
		if (adaptiveAnswer)
		    answerRotor = 0;
		else
		    answerRotor = (answerRotor+1) % answerRotorSize;
	    }
	    endSession();
	} else
	    modemFlushInput();
	/*
	 * If we still have a handle on the modem, then force a
	 * hangup and discard the handle.  We do this explicitly
	 * because some modems are impossible to safely hangup in the
	 * event of a problem.  Forcing a close on the device so that
	 * the modem will see DTR drop (hopefully) should clean up any
	 * bad state its in.  We then immediately try to setup the modem
	 * again so that we can be ready to answer incoming calls again.
	 *
	 * NB: the modem may have been discarded if a child process
	 *     was invoked to handle the inbound call.
	 */
	if (modemReady()) {
	    modemHangup();
	    discardModem(TRUE);
	}
	fxBool isSetup;
	if (isModemLocked() || lockModem()) {
	    isSetup = setupModem();
	    unlockModem();
	} else
	    isSetup = FALSE;
	if (isSetup)
	    changeState(RUNNING);
	else
	    changeState(MODEMWAIT, pollModemWait);
    } else {
	/*
	 * The modem is in use to call out, or by way of an incoming
	 * call.  If we're not sending or receiving, discard our handle
	 * on the modem and change to MODEMWAIT state where we wait
	 * for the modem to come available again.
	 */
	if (force)				// eliminate noise messages
	    traceServer("ANSWER: Can not lock modem device");
	if (state != SENDING && state != ANSWERING && state != RECEIVING) {
	    discardModem(FALSE);
	    changeState(LOCKWAIT, pollLockWait);
	}
    }
}

/*
 * Process an inbound call after the phone's been answered.
 * Calls may either be handled within the process or through
 * an external application.  Fax calls are handled internally.
 * Other types of calls are handled with external apps.  The
 * modem is conditioned for service, the process is started
 * with the open file descriptor passed on stdin+stdout+stderr,
 * and the local handle on the modem is discarded so that SIGHUP
 * is delivered to the subprocess (group) on last close.  This
 * process waits for the subprocess to terminate, at which time
 * it removes the modem lock file and reconditions the modem for
 * incoming calls (if configured).
 */
fxBool
faxGettyApp::processCall(CallType ctype, fxStr& emsg)
{
    fxBool callHandled = FALSE;

    switch (ctype) {
    case ClassModem::CALLTYPE_FAX:
	traceServer("ANSWER: FAX CONNECTION");
	changeState(RECEIVING);
	callHandled = recvFax();
	break;
    case ClassModem::CALLTYPE_DATA:
	traceServer("ANSWER: DATA CONNECTION");
	if (gettyArgs != "")
	    runGetty("GETTY", OSnewGetty, gettyArgs, TRUE);
	else
	    traceServer("ANSWER: Data connections are not permitted");
	callHandled = TRUE;
	break;
    case ClassModem::CALLTYPE_VOICE:
	traceServer("ANSWER: VOICE CONNECTION");
	if (vgettyArgs != "")
	    runGetty("VGETTY", OSnewVGetty, vgettyArgs, TRUE);
	else
	    traceServer("ANSWER: Voice connections are not permitted");
	callHandled = TRUE;
	break;
    case ClassModem::CALLTYPE_ERROR:
	traceServer("ANSWER: %s", (char*) emsg);
	break;
    }
    return (callHandled);
}

/*
 * Run a getty subprocess and wait for it to terminate.
 * The speed parameter is passed to use in establishing
 * a login session.
 */
void
faxGettyApp::runGetty(
    const char* what,
    Getty* (*newgetty)(const fxStr&, const fxStr&),
    const char* args,
    fxBool keepLock
)
{
    fxStr prefix(DEV_PREFIX);
    fxStr dev(getModemDevice());
    if (dev.head(prefix.length()) == prefix)
	dev.remove(0, prefix.length());
    Getty* getty = (*newgetty)(dev, fxStr::format("%u", getModemRate()));
    if (getty == NULL) {
	traceServer("%s: could not create", what);
	return;
    }
    getty->setupArgv(args);
    /*
     * The getty process should not inherit the lock file.
     * Remove it here before the fork so that our state is
     * correct (so further unlock calls will do nothing).
     * Note that we remove the lock here because apps such
     * as ppp and slip that install their own lock cannot
     * cope with finding a lock in place (even if it has
     * their pid in it).  This creates a potential window
     * during which outbound jobs might grab the modem
     * since they won't find a lock file in place.
     */
    if (!keepLock)
	unlockModem();
    fxBool parentIsInit = (::getppid() == 1);
    pid_t pid = ::fork();
    if (pid == -1) {
	traceServer("%s: can not fork", what);
	delete getty;
	return;
    }
    if (pid == 0) {			// child, start getty session
	setProcessPriority(BASE);	// remove any high priority
	if (keepLock)
	    /*
	     * The getty process should inherit the lock file.
	     * Force the UUCP lock owner so that apps find their
	     * own pid in the lock file.  Otherwise they abort
	     * thinking some other process already has control
	     * of the modem.  Note that doing this creates a
	     * potential window for stale lock removal between
	     * the time the login process terminates and the
	     * parent retakes ownership of the lock file (see below).
	     */
	    modemLock->setOwner(::getpid());
	if (::setegid(::getgid()) < 0)
	    traceServer("runGetty::setegid: %m");
	if (::seteuid(::getuid()) < 0)
	    traceServer("runGetty::seteuid (child): %m");
	getty->run(getModemFd(), parentIsInit);
	::_exit(127);
	/*NOTREACHED*/
    }
    traceServer("%s: START \"%s\", pid %lu", what,
	(const char*) getty->getCmdLine(), (u_long) pid);
    getty->setPID(pid);
    /*
     * Purge existing modem state because the getty+login
     * processe will change everything and because we must
     * close the descriptor so that the getty will get
     * SIGHUP on last close.
     */
    discardModem(FALSE);
    changeState(GETTYWAIT);
    int status;
    getty->wait(status, TRUE);		// wait for getty/login work to complete
    /*
     * Retake ownership of the modem.  Note that there's
     * a race in here (another process could come along
     * and purge the lock file thinking it was stale because
     * the pid is for the process that just terminated);
     * the only way to avoid it is to use a real locking
     * mechanism (e.g. flock on the lock file).
     */
    if (keepLock)
	modemLock->setOwner(0);		// NB: 0 =>'s use setup pid
    uid_t euid = ::geteuid();
    if (::seteuid(::getuid()) < 0)	// Getty::hangup assumes euid is root
	 traceServer("runGetty: seteuid (parent): %m");
    getty->hangup();			// cleanup getty-related stuff
    ::seteuid(euid);
    traceServer("%s: exit status %#o", what, status);
    delete getty;
}

/*
 * Set the number of rings to wait before answering
 * the telephone.  If there is a modem setup, then
 * configure the dispatcher to reflect whether or not
 * we need to listen for data from the modem (the RING
 * status messages).
 */
void
faxGettyApp::setRingsBeforeAnswer(int rings)
{
    ringsBeforeAnswer = rings;
    if (modemReady()) {
	int fd = getModemFd();
	IOHandler* h =
	    Dispatcher::instance().handler(fd, Dispatcher::ReadMask);
	if (rings > 0 && h == NULL)
	    Dispatcher::instance().link(fd, Dispatcher::ReadMask, this);
	else if (rings == 0 && h != NULL)
	    Dispatcher::instance().unlink(fd);
    }
}

fxBool
faxGettyApp::isCIDOk(const fxStr& cid)
{
    updatePatterns(qualifyCID, cidPats, acceptCID, lastCIDModTime);
    return (qualifyCID == "" ? TRUE : checkACL(cid, cidPats, *acceptCID));
}

/*
 * Notification handlers.
 */

/*
 * Handle notification that the modem device has become
 * available again after a period of being unavailable.
 */
void
faxGettyApp::notifyModemReady()
{
    sendQueuer("R" | getModemCapabilities());
}

#ifdef notdef
/*
 * Handle notification that the modem device looks to
 * be in a state that requires operator intervention.
 */
void
faxQueueApp::notifyModemWedged()
{
    logError("MODEM \"%s\" appears to be wedged", (char*) getModemDevice());
    fxStr cmd(wedgedCmd | " " | getModemDeviceID());
    runCmd(cmd);
}
#endif

/*
 * Handle notification that a document has been received.
 */
void
faxGettyApp::notifyRecvDone(const FaxRecvInfo& ri)
{
    recordRecv(ri);
    // hand to delivery/notification command
    runCmd(faxRcvdCmd
	 | " " | ri.qfile
	 | " " | fxStr::format("%.2f %u", ri.time / 60., ri.sigrate)
	 | " \"" | ri.protocol | "\""
	 | " \"" | ri.reason | "\""
	 | " " | getModemDeviceID()
	 , TRUE);
}

void faxGettyApp::notifyDocumentSent(FaxRequest&, u_int)		{}
void faxGettyApp::notifyPollRecvd(FaxRequest&, const FaxRecvInfo&)	{}
void faxGettyApp::notifyPollDone(FaxRequest&, u_int)			{}

/*
 * Send a message to the central queuer process.
 */
void
faxGettyApp::sendQueuer(const fxStr& msg0)
{
    int fifo;
#ifdef FIFOSELECTBUG
    /*
     * We try multiple times to open the appropriate FIFO
     * file because the system has a kernel bug that forces
     * the server to close+reopen the FIFO file descriptors
     * for each message received on the FIFO (yech!).
     */
    int tries = 0;
    do {
	if (tries > 0)
	    ::sleep(1);
	fifo = Sys::open(fifoName, O_WRONLY|O_NDELAY);
    } while (fifo == -1 && errno == ENXIO && ++tries < 5);
#else
    fifo = Sys::open(fifoName, O_WRONLY|O_NDELAY);
#endif
    if (fifo != -1) {
	/*
	 * Turn off O_NDELAY so that write will block if FIFO is full.
	 */
	if (::fcntl(fifo, F_SETFL, ::fcntl(fifo, F_GETFL, 0) &~ O_NDELAY) <0)
	    logError("fcntl: %m");
	fxStr msg("+" | getModemDeviceID() | ":" | msg0);
	if (Sys::write(fifo, msg, msg.length()) != msg.length())
	    logError("FIFO write failed: %m");
	::close(fifo);
    } else if (debug)
	logError(fifoName | ": Can not open: %m");
}

/*
 * FIFO-related support.
 */

/*
 * Open the requisite FIFO special files.
 */
void
faxGettyApp::openFIFOs()
{
    devfifo = openFIFO(fifoName | "." | getModemDeviceID(), 0600, TRUE);
    Dispatcher::instance().link(devfifo, Dispatcher::ReadMask, this);
}

void
faxGettyApp::closeFIFOs()
{
    ::close(devfifo), devfifo = -1;
}

/*
 * Respond to input on the specified file descriptor.
 */
int
faxGettyApp::inputReady(int fd)
{
    if (fd == devfifo)
	return FIFOInput(fd);
    else {
	answerPhone(ClassModem::ANSTYPE_ANY, FALSE);
	return (0);
    }
}

/*
 * Process a message received through a FIFO.
 */
void
faxGettyApp::FIFOMessage(const char* cp)
{
    switch (cp[0]) {
    case 'A':				// answer the phone
	if (cp[1] != '\0') {
	    traceServer("ANSWER %s", cp+1);
	    if (streq(cp+1, "fax"))
		answerPhone(ClassModem::ANSTYPE_FAX, TRUE);
	    else if (streq(cp+1, "data"))
		answerPhone(ClassModem::ANSTYPE_DATA, TRUE);
	    else if (streq(cp+1, "voice"))
		answerPhone(ClassModem::ANSTYPE_VOICE, TRUE);
	} else {
	    traceServer("ANSWER");
	    answerPhone(ClassModem::ANSTYPE_ANY, TRUE);
	}
	break;
    case 'C':				// configuration control
	traceServer("CONFIG \"%s\"", cp+1);
	readConfigItem(cp+1);
	break;
    case 'H':				// HELLO from queuer
	traceServer("HELLO");
	if (state == FaxServer::RUNNING)
	    notifyModemReady();		// sends capabilities also
	else
	    sendQueuer("D");		// good enough
	break;
    case 'Q':				// quit
	traceServer("QUIT");
	close();
	break;
    case 'Z':				// abort send/receive
	FaxServer::abortSession();
	break;
    default:
	faxApp::FIFOMessage(cp);
	break;
    }
}

/*
 * Miscellaneous stuff.
 */

void
faxGettyApp::recordRecv(const FaxRecvInfo& ri)
{
    char type[80];
    if (ri.pagelength == 297 || ri.pagelength == (u_int) -1)
	::strcpy(type, "A4");
    else if (ri.pagelength == 364)
	::strcpy(type, "B4");
    else
	::sprintf(type, "(%u x %u)", ri.pagewidth, ri.pagelength);
    traceServer("RECV: %s from %s, %d %s pages, %u dpi, %s, %s at %u bps",
	(char*) ri.qfile, (char*) ri.sender,
	ri.npages, type, ri.resolution,
	(char*) ri.protocol, fmtTime((time_t) ri.time), ri.sigrate);

    FaxAcctInfo ai;
    ai.user = "fax";
    ai.duration = (time_t) ri.time;
    ai.start = Sys::now() - ai.duration;
    ai.device = getModemDeviceID();
    ai.dest = getModemNumber();
    ai.csi = ri.sender;
    ai.npages = ri.npages;
    ai.sigrate = ri.sigrate;
    ai.df = ri.protocol;
    ai.status = ri.reason;
    ai.jobid = "";
    account("RECV", ai);
}

/*
 * Record a transfer in the transfer log file.
 */
void
faxGettyApp::account(const char* cmd, const FaxAcctInfo& ai)
{
    if (!ai.record(cmd))
	logError("Problem writing %s accounting record, dest=%s",
	    cmd, (const char*) ai.dest);
}

/*
 * Configuration support.
 */

void
faxGettyApp::resetConfig()
{
    FaxServer::resetConfig();
    setupConfig();
}

#define	N(a)	(sizeof (a) / sizeof (a[0]))

const faxGettyApp::stringtag faxGettyApp::strings[] = {
{ "qualifycid",		&faxGettyApp::qualifyCID },
{ "gettyargs",		&faxGettyApp::gettyArgs },
{ "vgettyargs",		&faxGettyApp::vgettyArgs },
{ "notifycmd",		&faxGettyApp::notifyCmd,	FAX_NOTIFYCMD },
{ "faxrcvdcmd",		&faxGettyApp::faxRcvdCmd,	FAX_FAXRCVDCMD },
};
const faxGettyApp::numbertag faxGettyApp::numbers[] = {
{ "answerbias",		&faxGettyApp::answerBias,	(u_int) -1 },
};

void
faxGettyApp::setupConfig()
{
    int i;

    for (i = N(strings)-1; i >= 0; i--)
	(*this).*strings[i].p = (strings[i].def ? strings[i].def : "");
    for (i = N(numbers)-1; i >= 0; i--)
	(*this).*numbers[i].p = numbers[i].def;
    ringsBeforeAnswer = 0;		// default is not to answer phone
    adaptiveAnswer = FALSE;		// no adaptive answer support
    setAnswerRotary("any");		// answer calls as ``any''
}

fxBool
faxGettyApp::setConfigItem(const char* tag, const char* value)
{
    u_int ix;
    if (findTag(tag, (const tags*) strings, N(strings), ix)) {
	(*this).*strings[ix].p = value;
    } else if (findTag(tag, (const tags*)numbers, N(numbers), ix)) {
	(*this).*numbers[ix].p = getNumber(value);
	switch (ix) {
	case 0: answerBias = fxmin(answerBias, (u_int) 2); break;
	}
    } else if (streq(tag, "ringsbeforeanswer"))
	setRingsBeforeAnswer(getNumber(value));
    else if (streq(tag, "adaptiveanswer"))
	adaptiveAnswer = getBoolean(value);
    else if (streq(tag, "answerrotary"))
	setAnswerRotary(value);
    else
	return (FaxServer::setConfigItem(tag, value));
    return (TRUE);
}
#undef	N

/*
 * Process an answer rotary spec string.
 */
void
faxGettyApp::setAnswerRotary(const fxStr& value)
{
    u_int l = 0;
    for (u_int i = 0; i < 3 && l < value.length(); i++) {
	fxStr type(value.token(l, " \t"));
	type.raisecase();
	if (type == "FAX")
	    answerRotary[i] = ClassModem::ANSTYPE_FAX;
	else if (type == "DATA")
	    answerRotary[i] = ClassModem::ANSTYPE_DATA;
	else if (type == "VOICE")
	    answerRotary[i] = ClassModem::ANSTYPE_VOICE;
	else {
	    if (type != "ANY")
		configError("Unknown answer type \"%s\"", (char*) type);
	    answerRotary[i] = ClassModem::ANSTYPE_ANY;
	}
    }
    if (i == 0)				// void string
	answerRotary[i++] = ClassModem::ANSTYPE_ANY;
    answerRotor = 0;
    answerRotorSize = i;
}

static void
usage(const char* appName)
{
    fxFatal("usage: %s [-q queue-dir] [-Ddpx] modem-device", appName);
}

static void
sigCleanup(int s)
{
    logError("CAUGHT SIGNAL %d", s);
    faxGettyApp::instance().close();
    ::_exit(-1);
}

int
main(int argc, char** argv)
{
    faxApp::setupLogging("FaxGetty");

    fxStr appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    faxGettyApp::setupPermissions();

    faxApp::setOpts("q:Ddpx");			// p+x are for FaxServer

    fxStr queueDir(FAX_SPOOLDIR);
    fxStr device;
    GetoptIter iter(argc, argv, faxApp::getOpts());
    for (; iter.notDone(); iter++)
	switch (iter.option()) {
	case 'm': device = iter.optArg(); break;	// compatibility
	case 'q': queueDir = iter.optArg(); break;
	case '?': usage(appName);
	}
    if (device == "") {
	device = iter.getArg();
	if (device == "")
	    usage(appName);
    }
    if (device[0] != '/')				// for getty
	device.insert(DEV_PREFIX);
    if (Sys::chdir(queueDir) < 0)
	fxFatal(queueDir | ": Can not change directory");

    /*
     * Construct an identifier for the device special
     * file by stripping a leading prefix (DEV_PREFIX)
     * and converting all remaining '/'s to '_'s.  This
     * is required for SVR4 systems which have their
     * devices in subdirectories!
     */
    fxStr devID = device;
    fxStr prefix(DEV_PREFIX);
    u_int pl = prefix.length();
    if (devID.length() > pl && devID.head(pl) == prefix)
	devID.remove(0, pl);
    while ((l = devID.next(0, '/')) < devID.length())
	devID[l] = '_';

    faxGettyApp* app = new faxGettyApp(device, devID);

    ::signal(SIGTERM, fxSIGHANDLER(sigCleanup));
    ::signal(SIGINT, fxSIGHANDLER(sigCleanup));

    app->initialize(argc, argv);
    app->open();
    while (app->isRunning())
	Dispatcher::instance().dispatch();
    app->close();
    delete app;

    return 0;
}
