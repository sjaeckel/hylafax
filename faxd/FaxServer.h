/*	$Header: /usr/people/sam/fax/faxd/RCS/FaxServer.h,v 1.112 1994/07/04 18:37:16 sam Exp $ */
/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
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
#ifndef _FaxServer_
#define	_FaxServer_
/*
 * Fax Modem and Protocol Server.
 */
#include <stdarg.h>

#include "FaxMachineLog.h"
#include "FaxModem.h"
#include "ModemConfig.h"
#include "FaxRequest.h"
#include "FaxTrace.h"
#include "IOHandler.h"

#include <sys/time.h>

class RegExArray;
class fxBoolArray;
class FaxSendOpArray;
class faxServerApp;
class Getty;
class FaxMachineInfo;
class FaxMachineLog;
class UUCPLock;
class FaxRecvInfo;
class FaxRecvInfoArray;
class DialStringRules;

/*
 * This class defines the ``server process'' that manages the fax
 * modem and implements the necessary protocol above the FaxModem
 * driver interface.  When the server is multi-threaded, this class
 * embodies a separate thread.
 */
class FaxServer : public IOHandler {
private:
    enum FaxServerState {
	BASE		= 0,
	RUNNING		= 1,
	MODEMWAIT	= 2,
	LOCKWAIT	= 3,
	GETTYWAIT	= 4,
	SENDING		= 5,
	ANSWERING	= 6,
	RECEIVING	= 7,
    };
    FaxServerState state;
    faxServerApp* app;			// client server app
    Getty*	getty;			// currently active getty
    UUCPLock*	modemLock;		// UUCP interlock
    FILE*	statusFile;		// server status file
    fxBool	abortCall;		// abort current send
    fxBool	deduceComplain;		// if true, complain when can't deduce
    fxBool	changePriority;		// change process priority by state
// generic modem-related stuff
    int		modemFd;		// open modem file
    fxStr	modemDevice;		// name of device to open
    fxStr	modemDevID;		// device identifier
    FaxModem*	modem;			// modem driver
    ModemConfig* modemConfig;		// modem configuration info
    time_t	lastConfigModTime;	// last mod time of configuration file
    SpeakerVolume speakerVolume;	// volume control
    BaudRate	curRate;		// current baud rate
    u_int	consecutiveBadCalls;	// count of consecutive bad phone calls
// server configuration
    u_short	ringsBeforeAnswer;	// # rings to wait
    fxStr	qualifyTSI;		// if set, no recv w/o acceptable tsi
    int		noCarrierRetrys;	// # times to retry on no carrier
    mode_t	recvFileMode;		// protection mode for received files
    mode_t	deviceMode;		// protection mode for modem device
    mode_t	logMode;		// protection mode for log files
    int		tracingLevel;		// tracing level w/o session
    int		logTracingLevel;	// tracing level during session
    int		tracingMask;		// tracing level control mask
    fxStr	gettyArgs;		// getty arguments
    fxStr	contCoverPageTemplate;	// continuation cover page template
    fxBool	adaptiveAnswer;		// answer as data if fax answer fails
    short	answerBias;		// rotor bias applied after good calls
    u_short	answerRotor;		// rotor into possible selections
    u_short	answerRotorSize;	// rotor table size
    AnswerType	answerRotary[3];	// rotary selection of answer types
    u_int	requeueTTS[9];		// requeue intervals[CallStatus code]
    u_int	requeueProto;		// requeue interval after protocol error
    u_int	requeueOther;		// requeue interval after other problem
    u_int	pollModemWait;		// polling interval in modem wait state
    u_int	pollLockWait;		// polling interval in lock wait state
    u_int	maxRecvPages;		// max pages to accept on receive
    u_int	maxSendPages;		// max pages in a send
    u_int	maxConsecutiveBadCalls;	// max consecutive bad phone calls
    u_int	postscriptTimeout;	// timeout on PostScript imager calls
    fxStr	localIdentifier;	// to use in place of FAXNumber
// phone number-related state
    fxStr	longDistancePrefix;	// prefix str for long distance dialing
    fxStr	internationalPrefix;	// prefix str for international dialing
    fxStr	myAreaCode;		// local area code
    fxStr	myCountryCode;		// local country code
    DialStringRules* dialRules;		// dial string rules
    fxStr	FAXNumber;		// phone number
// group 3 protocol-related state
    Class2Params clientCapabilities;	// received client capabilities
    Class2Params clientParams;		// current session parameters
// buffered i/o stuff
    short	rcvCC;			// # bytes pending in rcvBuf
    short	rcvNext;		// next available byte in rcvBuf
    u_char	rcvBuf[1024];		// receive buffering
// for fax reception ...
    fxBool	timeout;		// timeout during data reception
    fxBool	okToRecv;		// ok to accept stuff for this session
    fxStr	recvTSI;		// sender's TSI
    fxStr	hostname;		// host on which fax is received
    u_int	recvPages;		// count of received pages
    time_t	recvStart;		// starting time for document receive
    time_t	lastPatModTime;		// last mod time of patterns file
    RegExArray*	tsiPats;		// recv tsi patterns
    fxBoolArray* acceptTSI;		// accept/reject matched tsi
// send+receive stats
    u_int	npages;			// # pages sent/received
// logging and tracing
    FaxMachineLog* log;			// current log device

    friend class FaxModem;
    friend class faxServerApp;	// XXX for access to setServerStatus, yech

    static int pageWidthCodes[8];	// map wd to pixel count
    static int pageLengthCodes[4];	// map ln to mm length

// FAX transmission protocol support
    void	sendFax(FaxRequest& fax, FaxMachineInfo&, const fxStr& number);
    fxBool	sendPrepareFax(FaxRequest&, FaxMachineInfo&, fxStr& emsg);
    fxBool	sendClientCapabilitiesOK(FaxMachineInfo&, u_int nsf, fxStr&);
    fxBool	sendFaxPhaseB(FaxRequest&, u_int ix, FaxMachineInfo&);
    void	sendPoll(FaxRequest& fax, fxBool remoteHasDoc);
    FaxSendStatus sendSetupParams(TIFF*, Class2Params&, FaxMachineInfo&,fxStr&);
    FaxSendStatus sendSetupParams1(TIFF*, Class2Params&,FaxMachineInfo&,fxStr&);
    void	sendFailed(FaxRequest& fax,
		    FaxSendStatus, const char* notice, u_int tts = 0);
// FAX reception support
    fxBool	recvFax();
    TIFF*	setupForRecv(const char* op, FaxRecvInfo&, FaxRecvInfoArray&);
    fxBool	recvDocuments(const char* op, TIFF*,
		    FaxRecvInfo&, FaxRecvInfoArray&, fxStr& emsg);
    fxBool	recvFaxPhaseB(TIFF* tif, int& ppm, fxStr& emsg);
    void	recvComplete(FaxRecvInfo&, time_t, const fxStr& emsg);
    fxBool	pollFaxPhaseB(const char* cig, FaxRecvInfoArray&, fxStr& emsg);
    void	updateTSIPatterns();
    void	readTSIPatterns(FILE*, RegExArray*&, fxBoolArray*&);
// miscellaneous stuff
    void	setupConfig();
    fxBool	setupCall(AnswerType atype, CallType ctype,
		    fxBool& waitForProcess, fxStr& emsg);
    fxBool	runGetty(const char* args);
    void	setAnswerRotary(const fxStr& value);
// state-related stuff
    static const char* stateNames[8];
    static const char* stateStatus[8];

    fxBool	setupModem();
    fxBool	openDevice(const char* dev);
    fxBool	reopenDevice();
    void	discardModem(fxBool dropDTR);
    void	changeState(FaxServerState, long timeout = 0);
    void	setServerStatus(const char* fmt, ...);
    fxBool	abortRequested();
    void	setProcessPriority(FaxServerState s);
// modem i/o support
    int		inputReady(int);
    void	timerExpired(long, long);
    void	childStatus(pid_t, int);
    int		getModemLine(char buf[], u_int bufSize, long ms = 0);
    int		getModemChar(long ms = 0);
    void	flushModemInput();
    fxBool	putModem(const void* data, int n, long ms = 0);
    fxBool	putModem1(const void* data, int n, long ms = 0);
    void	startTimeout(long ms);
    void	stopTimeout(const char* whichdir);
// modem line control
    fxBool	sendBreak(fxBool pause);
    fxBool	setBaudRate(BaudRate rate);
    fxBool	setBaudRate(BaudRate rate, FlowControl i, FlowControl o);
    fxBool	setXONXOFF(FlowControl i, FlowControl o, SetAction act);
    fxBool	setDTR(fxBool on);
    fxBool	setInputBuffering(fxBool on);
    fxBool	modemStopOutput();
    void	modemFlushInput();

// general trace interface (used by modem classes)
    void	traceStatus(int kind, const char* fmt ...);
    void	vtraceStatus(int kind, const char* fmt, va_list ap);
    void	traceModemIO(const char* dir, const u_char* buf, u_int cc);
// FAX receiving protocol support (used by modem classes)
    void	recvDCS(const Class2Params&);
    void	recvNSF(u_int);
    fxBool	recvCheckTSI(const fxStr& tsi);
    void	recvSetupPage(TIFF* tif, long group3opts, int fillOrder);
public:
    FaxServer(faxServerApp* app, const fxStr& deviceName, const fxStr& devID);
    ~FaxServer();

    void open();
    void close();
    void initialize(int argc, char** argv);

    void setDialRules(const char* filename);
    fxStr canonicalizePhoneNumber(const fxStr& number);
    fxStr prepareDialString(const fxStr& ds);

    void restoreState(const fxStr& filename);
    void restoreStateItem(const char* line);

    void setModemNumber(const fxStr& number);	// fax phone number
    const fxStr& getModemNumber();
    void setLocalIdentifier(const fxStr& number);
    const fxStr& getLocalIdentifier();

    fxBool modemReady() const;
    fxBool modemSupports2D() const;
    fxBool modemSupportsEOLPadding() const;
    fxBool modemSupportsVRes(float res) const;
    fxBool modemSupportsPageWidth(u_int w) const;
    fxBool modemSupportsPageLength(u_int l) const;
    fxBool modemSupportsPolling() const;

    fxBool serverBusy() const;

    void setSessionTracing(int level);		// tracing during send+receive
    int getSessionTracing();
    void setServerTracing(int level);		// tracing outside send+receive
    int getServerTracing();
    void setModemSpeakerVolume(SpeakerVolume);	// speaker volume
    SpeakerVolume getModemSpeakerVolume();
    void setRingsBeforeAnswer(int rings);	// rings to wait before answer
    int getRingsBeforeAnswer();

    const fxStr& getContCoverPage() const;	// cont. cover page template

    void sendFax(FaxRequest&, FaxMachineInfo&);	// client-server send interface
    void answerPhone(AnswerType, fxBool force);	// client-server recv interface
    void abortSession();			// abort send/receive session
};
#endif /* _FaxServer_ */
