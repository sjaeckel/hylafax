/*	$Id$ */
/*
 * Copyright (c) 1994-1996 Sam Leffler
 * Copyright (c) 1994-1996 Silicon Graphics, Inc.
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
#include <ctype.h>
#include <stdlib.h>

#include "ModemServer.h"
#include "FaxTrace.h"
#include "Sys.h"

/*
 * Call status description strings.
 */
const char* ClassModem::callStatus[9] = {
    "Call successful",				// OK
    "Busy signal detected",			// BUSY
    "No carrier detected",			// NOCARRIER
    "No answer from remote",			// NOANSWER
    "No local dialtone",			// NODIALTONE
    "Invalid dialing command",			// ERROR
    "Unknown problem (check modem power)",	// FAILURE
    "Carrier established, but Phase A failure",	// NOFCON
    "Data connection established (wanted fax)",	// DATACONN
};
/*
 * Service class descriptions.  The first three
 * correspond to the EIA/TIA definitions.  The
 * voice class is for ZyXEL modems.
 */
const char* ClassModem::serviceNames[9] = {
    "\"Data\"",			// SERVICE_DATA
    "\"Class 1\"",		// SERVICE_CLASS1
    "\"Class 2\"",		// SERVICE_CLASS2
    "\"Class 2.0\"",		// SERVICE_CLASS20 (XXX 3)
    "",				// 4
    "",				// 5
    "",				// 6
    "",				// 7
    "\"Voice\"",		// SERVICE_VOICE
};
const char* ClassModem::ATresponses[13] = {
    "Nothing",			// AT_NOTHING
    "OK",			// AT_OK
    "Connection established",	// AT_CONNECT
    "No answer or ring back",	// AT_NOANSWER
    "No carrier",		// AT_NOCARRIER
    "No dial tone",		// AT_NODIALTONE
    "Busy",			// AT_BUSY
    "Phone off-hook",		// AT_OFFHOOK
    "Ring",			// AT_RING
    "Command error",		// AT_ERROR
    "<Empty line>",		// AT_EMPTYLINE
    "<Timeout>",		// AT_TIMEOUT
    "<Unknown response>"	// AT_OTHER
};
const char* ClassModem::callTypes[5] = {
    "unknown",
    "data",
    "fax",
    "voice",
    "error"
};
const char* ClassModem::answerTypes[4] = {
    "any",
    "fax",
    "data",
    "voice"
};

static fxStr
stripAT(const fxStr& a0)
{
    fxStr s(a0);
    if (s.length() >= 2 && s.head(2) == "AT")
	s.remove(0, 2);
    return s;
}

ClassModem::ClassModem(ModemServer& s, const ModemConfig& c)
    : server(s)
    , conf(c)
    , mfrQueryCmd(c.mfrQueryCmd)
    , modelQueryCmd(c.modelQueryCmd)
    , revQueryCmd(c.revQueryCmd)
{
    /*
     * The modem drivers and main server code require:
     *
     * echoOff		command echo disabled
     * verboseResults	verbose command result strings
     * resultCodes	result codes enabled
     * onHook		modem initially on hook (hung up)
     * noAutoAnswer	no auto-answer (we do it manually)
     *
     * In addition the following configuration is included
     * in the reset command set:
     *
     * flowControl	DCE-DTE flow control method
     * setupDTR		DTR management technique
     * setupDCD		DCD management technique
     * pauseTime	time to pause for "," when dialing
     * waitTime		time to wait for carrier when dialing
     *
     * Any other modem-specific configuration that needs to
     * be done at reset time should be implemented by overriding
     * the ClassModem::reset method.
     */
    // XXX: workaround yet another GCC bug (sigh)
    const fxStr& flow = conf.getFlowCmd(conf.flowControl);
    resetCmds = "AT"
	      | stripAT(conf.resetCmds)		// prepend to insure our needs
	      | stripAT(conf.echoOffCmd)
	      | stripAT(conf.verboseResultsCmd)
	      | stripAT(conf.resultCodesCmd)
	      | stripAT(conf.noAutoAnswerCmd)
	      | stripAT(conf.onHookCmd)
	      | "\nAT"
	      | stripAT(conf.pauseTimeCmd)
	      | stripAT(conf.waitTimeCmd)
	      | stripAT(flow)
	      | stripAT(conf.setupDTRCmd)
	      | stripAT(conf.setupDCDCmd)
	      ;
    modemServices = 0;
    rate = BR0;
    flowControl = conf.flowControl;
    iFlow = FLOW_NONE;
    oFlow = FLOW_NONE;
    setupDefault(mfrQueryCmd,   conf.mfrQueryCmd,       "ATI3");
    setupDefault(modelQueryCmd, conf.modelQueryCmd,     "ATI0");
    setupDefault(revQueryCmd,   conf.revQueryCmd,       ""); // No "standard" way? -- dbely
}

ClassModem::~ClassModem()
{
}

/*
 * Default methods for modem driver interface.
 */

bool
ClassModem::dataService()
{
    return atCmd(conf.class0Cmd);
}

CallStatus
ClassModem::dial(const char* number, fxStr& emsg)
{
    protoTrace("DIAL %s", number);
    char buf[256];
    sprintf(buf, (const char*) conf.dialCmd, number);
    emsg = "";
    CallStatus cs = (atCmd(buf, AT_NOTHING) ? dialResponse(emsg) : FAILURE);
    if (cs != OK && emsg == "")
	emsg = callStatus[cs];
    return (cs);
}

/*
 * Status messages to ignore when dialing.
 */
bool
ClassModem::isNoise(const char* s)
{
    static const char* noiseMsgs[] = {
	"CED",		// RC32ACL-based modems send this before +FCON
	"DIALING",
	"RRING",	// Telebit
	"RINGING",	// ZyXEL
	"+FHR:",	// Intel 144e
    };
#define	NNOISE	(sizeof (noiseMsgs) / sizeof (noiseMsgs[0]))

    for (u_int i = 0; i < NNOISE; i++)
	if (strneq(s, noiseMsgs[i], strlen(noiseMsgs[i])))
	    return (true);
    return (false);
}
#undef NNOISE

/*
 * Set of status codes we expect to receive
 * from a modem in response to an A (answer
 * the phone) command.
 */
static const AnswerMsg answerMsgs[] = {
{ "CONNECT FAX",11,
   ClassModem::AT_NOTHING, ClassModem::OK,	  ClassModem::CALLTYPE_FAX },
{ "CONNECT",     7,
   ClassModem::AT_NOTHING, ClassModem::OK,	  ClassModem::CALLTYPE_DATA },
{ "NO ANSWER",   9,
   ClassModem::AT_NOTHING, ClassModem::NOANSWER,  ClassModem::CALLTYPE_ERROR },
{ "NO CARRIER", 10,
   ClassModem::AT_NOTHING, ClassModem::NOCARRIER, ClassModem::CALLTYPE_ERROR },
{ "NO DIALTONE",11,
   ClassModem::AT_NOTHING, ClassModem::NODIALTONE,ClassModem::CALLTYPE_ERROR },
{ "ERROR",       5,
   ClassModem::AT_NOTHING, ClassModem::ERROR,     ClassModem::CALLTYPE_ERROR },
{ "+FHNG:",	6,
   ClassModem::AT_NOTHING, ClassModem::NOCARRIER, ClassModem::CALLTYPE_ERROR },
{ "+FHS:",	5,
   ClassModem::AT_NOTHING, ClassModem::NOCARRIER, ClassModem::CALLTYPE_ERROR },
{ "FAX",	 3,
   ClassModem::AT_CONNECT, ClassModem::OK,	  ClassModem::CALLTYPE_FAX },
{ "DATA",	 4,
   ClassModem::AT_CONNECT, ClassModem::OK,	  ClassModem::CALLTYPE_DATA },
};
#define	NANSWERS	(sizeof (answerMsgs) / sizeof (answerMsgs[0]))

const AnswerMsg*
ClassModem::findAnswer(const char* s)
{
    for (u_int i = 0; i < NANSWERS; i++)
	if (strneq(s, answerMsgs[i].msg, answerMsgs[i].len))
	    return (&answerMsgs[i]);
    return (NULL);
}
#undef NANSWERS

/*
 * Deduce connection kind: fax, data, or voice.
 */
CallType
ClassModem::answerResponse(fxStr& emsg)
{
    CallStatus cs = FAILURE;
    ATResponse r;
    time_t start = Sys::now();

    do {
	r = atResponse(rbuf, conf.answerResponseTimeout);
again:
	if (r == AT_TIMEOUT)
	    break;
	const AnswerMsg* am = findAnswer(rbuf);
	if (am != NULL) {
	    if (am->expect != AT_NOTHING && conf.waitForConnect) {
		/*
		 * Response string is an intermediate result that
		 * is only meaningful if followed by AT response
		 * am->next.  Read the next response from the modem
		 * and if it's the expected one, use the message
		 * to intuit the call type.  Otherwise, discard
		 * the intermediate response string and process the
		 * call according to the newly read response.
		 * This is intended to deal with modems that send
		 *   <something>
		 *   CONNECT
		 * (such as the Boca 14.4).
		 */
		r = atResponse(rbuf, conf.answerResponseTimeout);
		if (r != am->expect)
		    goto again;
	    }
	    if (am->status == OK)		// success
		return (am->type);
	    cs = am->status;
	    break;
	}
	if (r == AT_EMPTYLINE) {
	    emsg = callStatus[cs];
	    return (CALLTYPE_ERROR);
	}
    } while (Sys::now()-start < conf.answerResponseTimeout);
    emsg = "Ring detected without successful handshake";
    return (CALLTYPE_ERROR);
}

CallType
ClassModem::answerCall(AnswerType atype, fxStr& emsg)
{
    CallType ctype = CALLTYPE_ERROR;
    /*
     * If the request has no type-specific commands
     * to use, then just use the normal commands
     * intended for answering any type of call.
     */
    fxStr answerCmd;
    switch (atype) {
    case ANSTYPE_FAX:	answerCmd = conf.answerFaxCmd; break;
    case ANSTYPE_DATA:	answerCmd = conf.answerDataCmd; break;
    case ANSTYPE_VOICE:	answerCmd = conf.answerVoiceCmd; break;
    }
    if (answerCmd == "")
	answerCmd = conf.answerAnyCmd;
    if (atCmd(answerCmd, AT_NOTHING)) {
	ctype = answerResponse(emsg);
	if (ctype == CALLTYPE_UNKNOWN) {
	    /*
	     * The response does not uniquely identify the type
	     * of call; assume the type corresponds to the type
	     * of the answer request.
	     */
	    static CallType unknownCall[] = {
		CALLTYPE_FAX,		// ANSTYPE_ANY (default)
		CALLTYPE_DATA,		// ANSTYPE_DATA
		CALLTYPE_FAX,		// ANSTYPE_FAX
		CALLTYPE_VOICE,		// ANSTYPE_VOICE
		CALLTYPE_UNKNOWN,	// ANSTYPE_EXTERN
	    };
	    ctype = unknownCall[atype];
	}
	answerCallCmd(ctype);
    }
    return (ctype);
} 

/*
 * Send any configured commands to the modem once the
 * type of the call has been established.  These commands
 * normally configure flow control and buad rate for
 * modems that, for example, require a fixed baud rate
 * and flow control scheme when receiving fax.
 */ 
void
ClassModem::answerCallCmd(CallType ctype)
{
    fxStr beginCmd;
    switch (ctype) {
    case CALLTYPE_FAX:	beginCmd = conf.answerFaxBeginCmd; break;
    case CALLTYPE_DATA:	beginCmd = conf.answerDataBeginCmd; break;
    case CALLTYPE_VOICE:beginCmd = conf.answerVoiceBeginCmd; break;
    }
    if (beginCmd != "")
	(void) atCmd(beginCmd);
}

/*
 * Set data transfer timeout and adjust according
 * to the negotiated bit rate.
 */
void
ClassModem::setDataTimeout(long secs, u_int br)
{
    dataTimeout = secs*1000;	// 9600 baud timeout/data write (ms)
    switch (br) {
    case BR_2400:	dataTimeout *= 4; break;
    case BR_4800:	dataTimeout *= 2; break;
    case BR_9600:	dataTimeout = (4*dataTimeout)/3; break;
    // could shrink timeout for br > 9600
    }
}

fxStr
ClassModem::getCapabilities() const
{
    return fxStr("");
}

/*
 * Tracing support.
 */

/*
 * Trace a MODEM-communication-related activity.
 */
void
ClassModem::modemTrace(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    server.vtraceStatus(FAXTRACE_MODEMCOM, fmt, ap);
    va_end(ap);
}

/*
 * Trace a modem capability.
 */
void
ClassModem::modemCapability(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    static const fxStr modem("MODEM: ");
    server.vtraceStatus(FAXTRACE_MODEMCAP, modem | fmt, ap);
    va_end(ap);
}

/*
 * Indicate a modem supports some capability.
 */
void
ClassModem::modemSupports(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    static const fxStr supports("MODEM Supports ");
    server.vtraceStatus(FAXTRACE_MODEMCAP, supports | fmt, ap);
    va_end(ap);
}

/*
 * Trace a protocol-related activity.
 */
void
ClassModem::protoTrace(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    server.vtraceStatus(FAXTRACE_PROTOCOL, fmt, ap);
    va_end(ap);
}

/*
 * Trace a server-level activity.
 */
void
ClassModem::serverTrace(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    server.vtraceStatus(FAXTRACE_SERVER, fmt, ap);
    va_end(ap);
}

/*
 * Trace a modem capability bit mask.
 */
void
ClassModem::traceBits(u_int bits, const char* bitNames[])
{
    for (u_int i = 0; bits; i++)
	if (BIT(i) & bits) {
	    modemSupports(bitNames[i]);
	    bits &= ~BIT(i);
	}
}

/*
 * Modem i/o support.
 */

int
ClassModem::getModemLine(char buf[], u_int bufSize, long ms)
{
    int n = server.getModemLine(buf, bufSize, ms);
    if (n > 0)
	trimModemLine(buf, n);
    return (n);
}
int ClassModem::getModemChar(long ms) { return server.getModemChar(ms); }
int ClassModem::getModemDataChar()    { return server.getModemChar(dataTimeout); }

bool
ClassModem::putModemDLEData(const u_char* data, u_int cc, const u_char* bitrev, long ms)
{
    u_char dlebuf[2*1024];
    while (cc > 0) {
	if (wasTimeout() || abortRequested())
	    return (false);
	/*
	 * Copy to temp buffer, doubling DLE's.
	 */
	u_int i, j;
	u_int n = fxmin((size_t) cc, sizeof (dlebuf)/2);
	for (i = 0, j = 0; i < n; i++, j++) {
	    dlebuf[j] = bitrev[data[i]];
	    if (dlebuf[j] == DLE)
		dlebuf[++j] = DLE;
	}
	if (!putModem(dlebuf, j, ms))
	    return (false);
	data += n;
	cc -= n;
    }
    return (true);
}

void ClassModem::flushModemInput()
    { server.modemFlushInput(); }
bool ClassModem::putModem(void* d, int n, long ms)
    { return server.putModem(d, n, ms); }
bool ClassModem::putModemData(void* d, int n)
    { return server.putModem(d, n, dataTimeout); }

bool
ClassModem::putModemLine(const char* cp)
{
    u_int cc = strlen(cp);
    server.traceStatus(FAXTRACE_MODEMCOM, "<-- [%u:%s\\r]", cc+1, cp);
    static const char CR = '\r';
    return (server.putModem1(cp, cc) && server.putModem1(&CR, 1));
}

void ClassModem::startTimeout(long ms) { server.startTimeout(ms); }
void ClassModem::stopTimeout(const char* w){ server.stopTimeout(w); }

const u_int MSEC_PER_SEC = 1000;

#include <sys/time.h>
#if HAS_SELECT_H
#include <sys/select.h>
#endif

void
ClassModem::pause(u_int ms)
{
    if (ms == 0)
	return;
    protoTrace("DELAY %u ms", ms);
    struct timeval tv;
    tv.tv_sec = ms / MSEC_PER_SEC;
    tv.tv_usec = (ms % MSEC_PER_SEC) * 1000;
    (void) select(0, 0, 0, 0, &tv);
}

void
ClassModem::setupDefault(fxStr& s, const fxStr& configured, const char* def)
{
    if (configured == "")
	s = def;
    else
	s = configured;
}

/*
 * Reset the modem and set the DTE-DCE rate.
 */
bool
ClassModem::selectBaudRate(BaudRate br, FlowControl i, FlowControl o)
{
    rate = br;
    iFlow = i;
    oFlow = o;
    return (reset(5*1000) || reset(5*1000));	// NB: try at most twice
}

bool ClassModem::sendBreak(bool pause)
    { return server.sendBreak(pause); }
bool
ClassModem::setBaudRate(BaudRate r)
{
    if (server.setBaudRate(r)) {
	if (conf.baudRateDelay)
	    pause(conf.baudRateDelay);
	return (true);
    } else
	return (false);
}
bool
ClassModem::setBaudRate(BaudRate r, FlowControl i, FlowControl o)
{
    iFlow = i;
    oFlow = o;
    rate = r;
    if (server.setBaudRate(r,i,o)) {
	if (conf.baudRateDelay)
	    pause(conf.baudRateDelay);
	return (true);
    } else
	return (false);
}
bool
ClassModem::setXONXOFF(FlowControl i, FlowControl o, SetAction a)
{
    iFlow = i;
    oFlow = o;
    return server.setXONXOFF(i, o, a);
}

bool ClassModem::setDTR(bool onoff)
    { return server.setDTR(onoff); }
bool ClassModem::setInputBuffering(bool onoff)
    { return server.setInputBuffering(onoff); }
bool ClassModem::modemStopOutput()
    { return server.modemStopOutput(); }

/*
 * Miscellaneous server interfaces hooks.
 */

bool ClassModem::abortRequested()
    { return server.abortRequested(); }

void ClassModem::beginTimedTransfer()		{ server.timeout = false; }
void ClassModem::endTimedTransfer()		{}
bool ClassModem::wasTimeout()			{ return server.timeout; }
void ClassModem::setTimeout(bool b)		{ server.timeout = b; }

/*
 * Parsing support routines.
 */

/*
 * Cleanup a response line from the modem.  This removes
 * leading white space and any prefixing "+F<mumble>=" crap
 * that some Class 2 modems put at the front, as well as
 * any trailing white space.
 */
void
ClassModem::trimModemLine(char buf[], int& cc)
{
    // trim trailing white space
    if (cc > 0 && isspace(buf[cc-1])) {
	do {
	    cc--;
	} while (cc > 0 && isspace(buf[cc-1]));
	buf[cc] = '\0';
    }
    if (cc > 0) {
	u_int i = 0;
	// leading white space
	while (i < cc && isspace(buf[i]))
	    i++;
	// check for a leading +F<mumble>=
	if (i+1 < cc && buf[i] == '+' && buf[i+1] == 'F') {
	    u_int j = i;
	    for (i += 2; i < cc && buf[i] != '='; i++)
		 ;
	    if (i < cc) {	// trim more white space
		for (i++; i < cc && isspace(buf[i]); i++)
		    ;
	    } else		// no '=', back out
		i = j;
	}
	cc -= i;
	memmove(buf, buf+i, cc+1);
    }
}

/* 
 * Hayes-style modem manipulation support.
 */
bool
ClassModem::reset(long ms)
{
    setDTR(false);
    pause(conf.resetDelay);		// pause so modem can do reset
    setDTR(true);
#ifndef CONFIG_NOREOPEN
    /*
     * On some systems lowering and raising DTR is not done
     * properly (DTR is not raised when requested); thus we
     * reopen the device to insure that DTR is reasserted.
     */
    server.reopenDevice();
#endif
    if (!setBaudRate(rate, iFlow, oFlow)) {
        return (false);
    }
    flushModemInput();
    /*
     * Perform a soft reset as well to ensure the modem
     * is in a stable state before sending the additional
     * reset commands.
     */
    return atCmd(conf.softResetCmd, AT_OK, conf.resetDelay)
            && atCmd(resetCmds, AT_OK, ms);
}

bool
ClassModem::sync(long ms)
{
    return waitFor(AT_OK, ms);
}

ATResponse
ClassModem::atResponse(char* buf, long ms)
{
    bool prevTimeout = wasTimeout();
    int n = getModemLine(buf, sizeof (rbuf), ms);
    if (!prevTimeout && wasTimeout())
	lastResponse = AT_TIMEOUT;
    else if (n <= 0)
	lastResponse = AT_EMPTYLINE;
    else {
	lastResponse = AT_OTHER;
	switch (buf[0]) {
	case 'B':
	    if (strneq(buf, "BUSY", 4))
		lastResponse = AT_BUSY;
	    break;
	case 'C':
	    if (strneq(buf, "CONNECT", 7))
		lastResponse = AT_CONNECT;
	    break;
	case 'E':
	    if (strneq(buf, "ERROR", 5))
		lastResponse = AT_ERROR;
	    break;
	case 'N':
	    if (strneq(buf, "NO CARRIER", 10))
		lastResponse = AT_NOCARRIER;
	    else if (strneq(buf, "NO DIAL", 7))	// NO DIALTONE or NO DIAL TONE
		lastResponse = AT_NODIALTONE;
	    else if (strneq(buf, "NO ANSWER", 9))
		lastResponse = AT_NOANSWER;
	    break;
	case 'O':
	    if (strneq(buf, "OK", 2))
		lastResponse = AT_OK;
	    break;
	case 'P':
	    if (strneq(buf, "PHONE OFF-HOOK", 14))
		lastResponse = AT_OFFHOOK;
	    break;
	case 'R':
	    if (streq(buf, "RING"))		// NB: avoid match of RINGING
		lastResponse = AT_RING;
	    break;
	}
    }
    return lastResponse;
}

#define	isLineBreak(c)	((c) == '\n' || (c) == '\r')
#define	isEscape(c)	((c) & 0x80)
/*
 * Send an AT command string to the modem and, optionally
 * wait for status responses.  This routine breaks multi-line
 * strings (demarcated by embedded \n's) and waits for each
 * intermediate response.  Embedded escape sequences for
 * changing the DCE-DTE communication rate and/or host-modem
 * flow control scheme are also recognized and handled.
 */
bool
ClassModem::atCmd(const fxStr& cmd, ATResponse r, long ms)
{
    u_int cmdlen = cmd.length();
    u_int pos = 0;
    bool respPending = false;

    /*
     * Scan string for line breaks and escape codes (byte w/ 0x80 set).
     * A line break causes the current string to be sent to the modem
     * and a return status string parsed (and possibly compared to an
     * expected response).  An escape code terminates scanning,
     * with any pending string flushed to the modem before the
     * associated commands are carried out.
     */
    u_int i = 0;
    while (i < cmdlen) {
	if (isLineBreak(cmd[i]) && !(i+1 < cmdlen && isEscape(cmd[i+1]))) {
	    /*
	     * No escape code follows, send partial string
	     * to modem and await status string if necessary.
	     */
	    if (conf.atCmdDelay)
		pause(conf.atCmdDelay);
	    if (!putModemLine(cmd.extract(pos, i-pos)))
		return (false);
	    pos = ++i;			// next segment starts after line break
	    if (r != AT_NOTHING) {
		if (!waitFor(r, ms))
		    return (false);
	    } else {
		if (!waitFor(AT_OK, ms))
		    return (false);
	    }
	    respPending = false;
	} else if (isEscape(cmd[i])) {
	    /*
	     * Escape code; flush any partial line, process
	     * escape codes and carry out their actions.
	     */
	    ATResponse resp = AT_NOTHING;
	    if (i > pos) {
		if (conf.atCmdDelay)
		    pause(conf.atCmdDelay);
		if (isLineBreak(cmd[i-1])) {
		    /*
		     * Send data with a line break and arrange to
		     * collect the expected response (possibly
		     * specified through a <waitfor> escape processed
		     * below).  Note that we use putModemLine, as
		     * above, so that the same line break is sent
		     * to the modem for all segments (i.e. \n is
		     * translated to \r).
		     */
		    if (!putModemLine(cmd.extract(pos, i-1-pos)))
			return (false);
		    // setup for expected response
		    resp = (r != AT_NOTHING ? r : AT_OK);
		} else {
		    /*
		     * Flush any data as-is, w/o adding a line
		     * break or expecting a response.  This is
		     * important for sending, for example, a
		     * command escape sequence such as "+++".
		     */
		    u_int cc = i-pos;
		    const char* cp = &cmd[pos];
		    server.traceStatus(FAXTRACE_MODEMCOM, "<-- [%u:%s]", cc,cp);
		    if (!server.putModem1(cp, cc))
			return (false);
		}
		respPending = true;
	    }
	    /*
	     * Process escape codes.
	     */
	    BaudRate br = rate;
	    FlowControl flow = flowControl;
	    u_int delay = 0;
	    do {
		switch (cmd[i] & 0xff) {
		case ESC_SETBR:			// set host baud rate
		    br = (u_char) cmd[++i];
		    if (br != rate) {
			setBaudRate(br);
			rate = br;
		    }
		    break;
		case ESC_SETFLOW:		// set host flow control
		    flow = (u_char) cmd[++i];
		    if (flow != flowControl) {
			setBaudRate(br, flow, flow);
			flowControl = flow;
		    }
		    break;
		case ESC_DELAY:			// host delay
		    delay = (u_char) cmd[++i];
		    if (delay != 0)
			pause(delay*10);	// 10 ms granularity
		    break;
		case ESC_WAITFOR:		// wait for response
		    resp = (u_char) cmd[++i];
		    if (resp != AT_NOTHING) {
			// XXX check return?
			(void) waitFor(resp, ms);	// XXX ms
			respPending = false;
		    }
		    break;
		case ESC_FLUSH:			// flush input
		    flushModemInput();
		    break;
		}
	    } while (++i < cmdlen && isEscape(cmd[i]));
	    pos = i;				// next segment starts here
	    if (respPending) {
		/*
		 * If a segment with a line break was flushed
		 * but no explicit <waitfor> escape followed
		 * then collect the response here so that it
		 * does not get lost.
		 */
		if (resp != AT_NOTHING && !waitFor(resp, ms))
		    return (false);
		respPending = false;
	    }
	} else
	    i++;
    }
    /*
     * Flush any pending string to modem.
     */
    if (i > pos) {
	if (conf.atCmdDelay)
	    pause(conf.atCmdDelay);
	if (!putModemLine(cmd.extract(pos, i-pos)))
	    return (false);
	respPending = true;
    }
    /*
     * Wait for any pending response.
     */
    if (respPending) {
	if (r != AT_NOTHING && !waitFor(r, ms))
	    return (false);
    }
    return (true);
}
#undef	isEscape
#undef	isLineBreak

/*
 * Wait (carefully) for some response from the modem.
 */
bool
ClassModem::waitFor(ATResponse wanted, long ms)
{
    for (;;) {
	ATResponse response = atResponse(rbuf, ms);
	if (response == wanted)
	    return (true);
	switch (response) {
	case AT_TIMEOUT:
	case AT_EMPTYLINE:
	case AT_ERROR:
	case AT_NOCARRIER:
	case AT_NODIALTONE:
	case AT_NOANSWER:
	case AT_OFFHOOK:
	    modemTrace("MODEM %s", ATresponses[response]);
	    return (false);
	}
    }
}

/*
 * Process a manufacturer/model/revision query.
 */
bool
ClassModem::doQuery(const fxStr& queryCmd, fxStr& result, long ms)
{
    if (queryCmd == "")
	return (true);
    if (queryCmd[0] == '!') {
	/*
	 * ``!mumble'' is interpreted as "return mumble";
	 * this means that you can't send ! to the modem.
	 */
	result = queryCmd.tail(queryCmd.length()-1);
	return (true);
    }
    return (atQuery(queryCmd, result, ms));
}

/*
 * Return modem manufacturer.
 */
bool
ClassModem::setupManufacturer(fxStr& mfr)
{
    return doQuery(mfrQueryCmd, mfr);
}

/*
 * Return modem model identification.
 */
bool
ClassModem::setupModel(fxStr& model)
{
    return doQuery(modelQueryCmd, model);
}

/*
 * Return modem firmware revision.
 */
bool
ClassModem::setupRevision(fxStr& rev)
{
    return doQuery(revQueryCmd, rev);
}

/*
 * Send AT<what>? and get a string response.
 */
bool
ClassModem::atQuery(const char* what, fxStr& v, long ms)
{
    ATResponse r = AT_ERROR;
    if (atCmd(what, AT_NOTHING)) {
	v.resize(0);
	while ((r = atResponse(rbuf, ms)) != AT_OK) {
	    if (r == AT_ERROR || r == AT_TIMEOUT || r == AT_EMPTYLINE)
		break;
	    if (v.length())
		v.append('\n');
	    v.append(rbuf);
	}
    }
    return (r == AT_OK);
}

/*
 * Send AT<what>? and get a range response.
 */
bool
ClassModem::atQuery(const char* what, u_int& v, long ms)
{
    char response[1024];
    if (atCmd(what, AT_NOTHING) && atResponse(response) == AT_OTHER) {
	sync(ms);
	return parseRange(response, v);
    }
    return (false);
}

/*
 * Parsing support routines.
 */

const char OPAREN = '(';
const char CPAREN = ')';
const char COMMA = ',';
const char SPACE = ' ';

/*
 * Parse a Class 2 parameter range string.  This is very
 * forgiving because modem vendors do not exactly follow
 * the syntax specified in the "standard".  Try looking
 * at some of the responses given by rev ~4.04 of the
 * ZyXEL firmware (for example)!
 *
 * NB: We accept alphanumeric items but don't return them
 *     in the parsed range so that modems like the ZyXEL 2864
 *     that indicate they support ``Class Z'' are handled.
 */
bool
ClassModem::vparseRange(const char* cp, int nargs ... )
{
    bool b = true;
    va_list ap;
    va_start(ap, nargs);
    while (nargs-- > 0) {
	while (cp[0] == SPACE)
	    cp++;
	char matchc;
	bool acceptList;
	if (cp[0] == OPAREN) {				// (<items>)
	    matchc = CPAREN;
	    acceptList = true;
	    cp++;
	} else if (isalnum(cp[0])) {			// <item>
	    matchc = COMMA;
	    acceptList = (nargs == 0);
	} else {					// skip to comma
	    b = false;
	    break;
	}
	int mask = 0;
	while (cp[0] && cp[0] != matchc) {
	    if (cp[0] == SPACE) {			// ignore white space
		cp++;
		continue;
	    }
	    if (!isalnum(cp[0])) {
		b = false;
		goto done;
	    }
	    int v;
	    if (isdigit(cp[0])) {
		v = 0;
		do {
		    v = v*10 + (cp[0] - '0');
		} while (isdigit((++cp)[0]));
	    } else {
		v = -1;					// XXX skip item below
		while (isalnum((++cp)[0]))
		    ;
	    }
	    int r = v;
	    if (cp[0] == '-') {				// <low>-<high>
		cp++;
		if (!isdigit(cp[0])) {
		    b = false;
		    goto done;
		}
		r = 0;
		do {
		    r = r*10 + (cp[0] - '0');
		} while (isdigit((++cp)[0]));
	    } else if (cp[0] == '.') {			// <d.b>
		cp++;
		while (isdigit(cp[0]))			// XXX
		    cp++;
		v++, r++;				// XXX 2.0 -> 3
	    }
	    if (v != -1) {				// expand range or list
		r = fxmin(r, 31);			// clamp to valid range
		for (; v <= r; v++)
		    mask |= 1<<v;
	    }
	    if (acceptList && cp[0] == COMMA)		// (<item>,<item>...)
		cp++;
	}
	*va_arg(ap, int*) = mask;
	if (cp[0] == matchc)
	    cp++;
	if (matchc == CPAREN && cp[0] == COMMA)
	    cp++;
    }
done:
    va_end(ap);
    return (b);
}

/*
 * Parse a single Class X range specification
 * and return the resulting bit mask.
 */
bool
ClassModem::parseRange(const char* cp, u_int& a0)
{
    return vparseRange(cp, 1, &a0);
}

void
ClassModem::setSpeakerVolume(SpeakerVolume l)
{
    atCmd(conf.setVolumeCmd[l]);
}

void
ClassModem::hangup()
{
    atCmd(conf.onHookCmd, AT_OK, 5000);
}

bool
ClassModem::waitForRings(u_int n, CallType& type, CallerID& cinfo)
{
    if (n > 0) {
	time_t timeout = n*5;			// 5 second/ring
	time_t start = Sys::now();
	do {
	    switch (atResponse(rbuf, 5000)) {
	    case AT_OTHER:			// check distinctive ring
		 if (streq(conf.ringData, rbuf))
		    type = CALLTYPE_DATA;
		else if (streq(conf.ringFax, rbuf))
		    type = CALLTYPE_FAX;
		else if (streq(conf.ringVoice, rbuf))
		    type = CALLTYPE_VOICE;
		else {
		    conf.parseCID(rbuf, cinfo);
		    break;
		}
		/* fall thru... */
	    case AT_RING:			// normal ring
		n--;
		break;
	    case AT_NOANSWER:
	    case AT_NOCARRIER:
	    case AT_NODIALTONE:
	    case AT_ERROR:
		return (false);
	    }
	} while (n > 0 && Sys::now()-start < timeout);
    }
    return (n == 0);
}
