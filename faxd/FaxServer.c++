#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxServer.c++,v 1.16 91/05/23 12:26:00 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <osfcn.h>
#include <ctype.h>
#include <sys/termio.h>
#include <sys/fcntl.h>
#include <errno.h>

#include "t.30.h"

#include "FaxServer.h"
#include "FaxRequest.h"
#include "FaxRecvInfo.h"
#include "RegExArray.h"

int FaxServer::pageWidthCodes[4] = {
    1728,	// 1728 along 215 mm line
    2432,	// 1728 (215 mm), 2048 (255 mm), or 2432 (303 mm)
    2048,	// 1728 (215 mm), 2048 (255 mm)
    2432,	// Invalid, but must be interpreted as #1
};
int FaxServer::pageLengthCodes[4] = {
    297,	// A4 paper (mm)
    -1,		// unlimited
    364,	// can handle both A4 and B4 (mm)
    0		// invalid
};
// the first column is for T3.85, the second for T7.7
int FaxServer::minScanlineTimeCodes[8][2] = {
    { 20, 20 },		// 20 ms at 3.85 l/mm; T7.7 = T3.85
    { 40, 40 },		// 40 ms at 3.85 l/mm; T7.7 = T3.85
    { 10, 10 },		// 10 ms at 3.85 l/mm; T7.7 = T3.85
    {  5,  5 },		//  5 ms at 3.85 l/mm; T7.7 = T3.85
    { 10,  5 },		// 10 ms at 3.85 l/mm; T7.7 = 1/2 T3.85
    { 20, 10 },		// 20/2 ms at 3.85 l/mm; T7.7 = 1/2 T3.85
    { 40, 20 },		// 40/2 ms at 3.85 I/mm; T7.7 = 1/2 T3.85
    {  0,  0 },		// 0 ms at 3.85 I/mm; T7.7 = T3.85
};

static void s0(FaxServer* o, FaxRequest* fax)	{ o->sendFax(fax); }
static void s1(FaxServer* o)			{ o->recvFax(); }

FaxServer::FaxServer(const fxStr& devName) :
    modemDevice(devName)
{
    addInput("sendFax",			fxDT_FaxRequest,this, (fxStubFunc) s0);
    addInput("recvFax",			fxDT_void,	this, (fxStubFunc) s1);

    jobCompleteChannel	= addOutput("jobComplete",	fxDT_FaxRequest);
    sendCompleteChannel	= addOutput("sendComplete",	fxDT_CharPtr);
    jobRecvdChannel	= addOutput("jobRecvd",		fxDT_FaxRecvd);
    recvCompleteChannel	= addOutput("recvComplete",	fxDT_void);
    sendStatusChannel	= addOutput("sendStatus",	fxDT_int);
    traceChannel	= addOutput("trace",		fxDT_CharPtr);

    modemFd = -1;
    modem = 0;
    rcvNext = rcvCC = 0;
    speakerVolume = FaxModem::QUIET;
    modemType = "Abaton";
    ringsBeforeAnswer = 0;
    tracingLevel = 0;
    toneDialing = TRUE;
    waitTimeForCarrier = 30;		// modem's default value
    commaPauseTime = 2;			// modem's default value
    rcvHandler = 0;
    okToReceive2D = TRUE;
    recvFileMode = 0600;		// default protection mode
    qualifyTSI = FALSE;
    lastPatModTime = 0;
    tsiPats = 0;
}

FaxServer::~FaxServer()
{
    delete rcvHandler;
    delete modem;
    delete tsiPats;
}

const char* FaxServer::className() const { return "FaxServer"; }

void
FaxServer::open()
{
    if (opened)
	return;
    opened = TRUE;
    ::umask(077);				// keep all temp files private
    startTimeout(3);
    modemFd = ::open(modemDevice, O_RDWR);
    stopTimeout("opening");
    if (modemFd >= 0 && setBaudRate(B1200)) {
	if (setupModem(modemType)) {
	    rcvHandler = new ModemListener(modemFd);
	    rcvHandler->connect("ring", this, "recvFax");
	    fx_theExecutive->addSelectHandler(rcvHandler);
	} else
	    traceStatus(FAXTRACE_SERVER,
		"\"%s\": Cannot deduce or handle modem type.",
		(char*) modemDevice);
    } else
	traceStatus(FAXTRACE_SERVER, "\"%s\": Can not open modem.",
	    (char*) modemDevice);
}
fxBool FaxServer::openSucceeded() const	{ return modem != 0; }

void
FaxServer::close()
{
    if (rcvHandler)
	fx_theExecutive->removeSelectHandler(rcvHandler);
    if (modemFd >= 0)
	::close(modemFd);
    fxApplication::close();
}

void
FaxServer::initialize(int argc, char** argv)
{
    updateTSIPatterns();
}

fxBool
FaxServer::setupModem(const char* name)
{
    modem = FaxModem::getModemByName(name, *this);
    if (!modem)
	return (FALSE);
    modem->reset();
    modem->setSpeakerVolume(speakerVolume);
    modem->setCommaPauseTime(commaPauseTime);
    modem->setWaitTimeForCarrier(waitTimeForCarrier);
    modem->setLID(canonicalizePhoneNumber(FAXNumber));

    traceStatus(FAXTRACE_SERVER, "MODEM \"%s\"", modem->getName());
    return (TRUE);
}

/*
 * Convert a phone number to a canonical format:
 *	+<country><areacode><number>
 * This involves, possibly, stripping off leading
 * dialing prefixes for long distance and/or
 * international dialing.
 */
fxStr
FaxServer::canonicalizePhoneNumber(const fxStr& number)
{
    fxStr canon(number);
    // strip everything but digits
    for (int i = canon.length()-1; i >= 0; i--)
	if (!isdigit(canon[i]))
	    canon.remove(i);
    if (number[number.skip(0, " \t")] != '+') {
	// form canonical phone number by removing
	// any long-distance and/or international
	// dialing stuff and by prepending local
	// area code and country code -- as appropriate
	fxStr prefix = canon.extract(0, internationalPrefix.length());
	if (prefix != internationalPrefix) {
	    prefix = canon.extract(0, longDistancePrefix.length());
	    if (prefix != longDistancePrefix)
		canon.insert(myAreaCode);
	    else
		canon.remove(0, longDistancePrefix.length());
	    canon.insert(myCountryCode);
	} else
	    canon.remove(0, internationalPrefix.length());
    }
    canon.insert('+');
    return (canon);
}

/*
 * Convert a canonical phone number to one that
 * reflects local dialing characteristics.  This
 * means stripping country and area codes, when
 * local, and inserting dialing prefixes.
 */
fxStr
FaxServer::localizePhoneNumber(const fxStr& canon)
{
    fxStr number(canon);
    if (number.extract(1, myCountryCode.length()) == myCountryCode) {
	number.remove(0, myCountryCode.length()+1);
	if (number.extract(0, myAreaCode.length()) == myAreaCode)
	    number.remove(0, myAreaCode.length());
	else
	    number.insert(longDistancePrefix);	
    } else {
	number.remove(0);		// remove "+"
	number.insert(internationalPrefix);
    }
    if (useDialPrefix)
	number.insert(dialPrefix);
    return (number);
}

void
FaxServer::setModemNumber(const fxStr& number)
{
    FAXNumber = number;
    if (modem)
	modem->setLID(canonicalizePhoneNumber(number));
}
const fxStr& FaxServer::getModemNumber()	{ return (FAXNumber); }

void FaxServer::setOkToReceive2D(fxBool on)	{ okToReceive2D = on; }
fxBool FaxServer::getOkToReceive2D()		{ return (okToReceive2D); }
void FaxServer::setQualifyTSI(fxBool on)	{ qualifyTSI = on; }
fxBool FaxServer::getQualifyTSI()		{ return (qualifyTSI); }

void FaxServer::setTracing(int level)		{ tracingLevel = level; }
int FaxServer::getTracing()			{ return (tracingLevel); }
void FaxServer::setToneDialing(fxBool on)	{ toneDialing = on; }
fxBool FaxServer::getToneDialing()		{ return (toneDialing); }
void FaxServer::setRingsBeforeAnswer(int rings)	{ ringsBeforeAnswer = rings; }
int FaxServer::getRingsBeforeAnswer()		{ return (ringsBeforeAnswer); }
void FaxServer::setRecvFileMode(int mode)	{ recvFileMode = mode; }
int FaxServer::getRecvFileMode()		{ return (recvFileMode); }

void
FaxServer::setModemSpeakerVolume(SpeakerVolume lev)
{
    speakerVolume = lev;
    if (modem)
	modem->setSpeakerVolume(lev);
}
SpeakerVolume FaxServer::getModemSpeakerVolume(){ return (speakerVolume); }

void
FaxServer::setCommaPauseTime(int secs)
{
    commaPauseTime = secs;
    if (modem)
	modem->setCommaPauseTime(secs);
}
int FaxServer::getCommaPauseTime()		{ return (commaPauseTime); }

void
FaxServer::setWaitTimeForCarrier(int secs)
{
    waitTimeForCarrier = secs;
    if (modem)
	modem->setWaitTimeForCarrier(secs);
}
int FaxServer::getWaitTimeForCarrier()		{ return (waitTimeForCarrier); }

// XXX need preferences database

static const char* putBoolean(fxBool b)
    { return (b ? "yes" : "no"); }
static fxBool getBoolean(const char* cp)
    { return (strcasecmp(cp, "on") == 0 || strcasecmp(cp, "yes") == 0); }

void
FaxServer::restoreState(const fxStr& filename)
{
    FILE* fd = fopen(filename, "r");
    if (!fd)
	return;
    char line[512];
    while (fgets(line, sizeof (line)-1, fd))
	restoreStateItem(line);
    fclose(fd);
}

void
FaxServer::restoreStateItem(const char* b)
{
    char buf[512];

    strncpy(buf, b, sizeof (buf));
    char* cp = strchr(buf, '#');
    if (!cp)
	cp = strchr(buf, '\n');
    if (cp)
	*cp = '\0';
    cp = strchr(buf, ':');
    if (cp) {
	*cp++ = '\0';
	while (isspace(*cp))
	    cp++;
	if (strcasecmp(buf, "RingsBeforeAnswer") == 0)
	    ringsBeforeAnswer = atoi(cp);
	else if (strcasecmp(buf, "WaitForCarrier") == 0)
	    waitTimeForCarrier = atoi(cp);
	else if (strcasecmp(buf, "CommaPauseTime") == 0)
	    commaPauseTime = atoi(cp);
	else if (strcasecmp(buf, "RecvFileMode") == 0)
	    recvFileMode = (int) strtol(cp, 0, 8);
	else if (strcasecmp(buf, "SpeakerVolume") == 0)
	    speakerVolume = (SpeakerVolume) atoi(cp);
	else if (strcasecmp(buf, "ToneDialing") == 0)
	    toneDialing = getBoolean(cp);
	else if (strcasecmp(buf, "ProtocolTracing") == 0)
	    tracingLevel = atoi(cp);
	else if (strcasecmp(buf, "FAXNumber") == 0)
	    setModemNumber(cp);
	else if (strcasecmp(buf, "AreaCode") == 0)
	    myAreaCode = cp;
	else if (strcasecmp(buf, "CountryCode") == 0)
	    myCountryCode = cp;
	else if (strcasecmp(buf, "DialPrefix") == 0)
	    dialPrefix = cp;
	else if (strcasecmp(buf, "LongDistancePrefix") == 0)
	    longDistancePrefix = cp;
	else if (strcasecmp(buf, "InternationalPrefix") == 0)
	    internationalPrefix = cp;
	else if (strcasecmp(buf, "UseDialPrefix") == 0)
	    useDialPrefix = getBoolean(cp);
	else if (strcasecmp(buf, "ModemType") == 0)
	    modemType = cp;
	else if (strcasecmp(buf, "QualifyTSI") == 0)
	    qualifyTSI = getBoolean(cp);
    }
}

#include <stdarg.h>

void
FaxServer::traceStatus(int kind, const char* va_alist ...)
#define	fmt va_alist
{
    if (tracingLevel & kind) {
	va_list ap;
	va_start(ap, fmt);
	if (traceChannel->getNumberOfConnections() == 0) {
	    vfprintf(stderr, fmt, ap);
	    fprintf(stderr, "\n");
	} else {
	    char buf[1024];
	    vsprintf(buf, fmt, ap);
	    sendCharPtr(traceChannel, buf, fxObj::sync);
	}
	va_end(ap);
    }
}
#undef fmt

int
FaxServer::modemDIS()
{
    int DIS = DIS_T4RCVR |
	DIS_7MMVRES |
	(DISWIDTH_2432 << 6) |
	(DISLENGTH_UNLIMITED << 4) |
	(modem->getBestSignallingRate() << 12) |
	(modem->getBestScanlineTime() << 1);
    // XXX this is optional 'cuz 2d encoding may be busted??
    if (okToReceive2D)
	DIS |= DIS_2DENCODE;
    return (DIS << 8);
}

static int baudRates[] = {
    0,		// B0
    50,		// B50
    75,		// B75
    110,	// B110
    134,	// B134
    150,	// B150
    200,	// B200
    300,	// B300
    600,	// B600
    1200,	// B1200
    1800,	// B1800
    2400,	// B2400
    4800,	// B4800
    9600,	// B9600
    19200,	// B19200
    38400,	// B38400
};

/*
 * Device manipulation.
 */
fxBool
FaxServer::setBaudRate(int rate, fxBool enableFlow)
{
    struct termio term;
    if (ioctl(modemFd, TCGETA, &term) != 0)
	return (FALSE);
    rate &= CBAUD;
    term.c_iflag = 0;
    if (enableFlow)
	term.c_iflag |= IXON;
    term.c_oflag = 0;
    term.c_lflag = 0;
    term.c_cflag = rate | CS8 | CREAD;
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    traceStatus(FAXTRACE_MODEMOPS,
	"MODEM line set to %d baud (input XON/XOFF %s)",
	baudRates[rate], enableFlow ? "enabled" : "disabled");
    flushModemInput();
    return (ioctl(modemFd, TCSETAF, &term) == 0);
}

fxBool
FaxServer::setInputFlowControl(fxBool enableFlow, fxBool flush)
{
    struct termio term;
    if (ioctl(modemFd, TCGETA, &term) == 0) {
	if (enableFlow)
	    term.c_iflag |= IXON;
	else
	    term.c_iflag &= ~IXON;
	traceStatus(FAXTRACE_MODEMOPS,
	    "MODEM input XON/XOFF %s", enableFlow ? "enabled" : "disabled");
	if (flush)
	    flushModemInput();
	if (ioctl(modemFd, flush ? TCSETAF : TCSETA, &term) == 0)
	    return (TRUE);
    }
    return (FALSE);
}

fxBool
FaxServer::sendBreak(fxBool pause)
{
    traceStatus(FAXTRACE_MODEMOPS, "<-- break");
    flushModemInput();
    if (pause) {
	struct termio term;
	/*
	 * NB: TCSBRK is supposed to wait for output to drain,
	 * but modem appears loses data if we don't do this
	 * ioctl trick.
	 */
	if (ioctl(modemFd, TCGETA, &term) != -1)
	    (void) ioctl(modemFd, TCSETAF, &term);
    }
    return (ioctl(modemFd, TCSBRK, 0) == 0);
}

static fxBool timerExpired = FALSE;
static void sigAlarm() { timerExpired = TRUE; }

void
FaxServer::startTimeout(int t)
{
    timeout = timerExpired = FALSE;
    signal(SIGALRM, (sig_type) sigAlarm);
    traceStatus(FAXTRACE_TIMEOUTS, "start %d second timer", t);
    alarm(t);
}

void
FaxServer::stopTimeout(const char* whichdir)
{
    alarm(0);
    traceStatus(FAXTRACE_TIMEOUTS,
	"stop timer%s", timerExpired ? ", timer expired" : "");
    if (timeout = timerExpired)
	traceStatus(FAXTRACE_MODEMOPS, "TIMEOUT: %s modem", whichdir);
}

fxBool
FaxServer::getTimedModemLine(char rbuf[], int timer)
{
    return (getModemLine(rbuf, timer) != 0 && !timeout);
}

const int CAN = 030;

int
FaxServer::getModemLine(char rbuf[], int timer)
{
    int c;
    int cc = 0;
    do {
	while ((c = getModemChar(timer)) != EOF && c != '\n')
	    if (c != '\0' && c != '\r')
		rbuf[cc++] = c;
    } while (cc == 0 && c != EOF);
    rbuf[cc] = '\0';
    traceStatus(FAXTRACE_MODEMCOM, "--> [%d:%s]", cc, rbuf);
    return (cc);
}

int
FaxServer::getModemChar(int timer)
{
    if (rcvNext >= rcvCC) {
	int n = 0;
	if (timer) startTimeout(timer);
	do
	    rcvCC = ::read(modemFd, rcvBuf, sizeof (rcvBuf));
	while (n++ < 5 && rcvCC == 0);
	if (timer) stopTimeout("reading from");
	if (rcvCC <= 0) {
	    if (rcvCC < 0) {
		extern int errno;
		if (errno != EINTR)
		    traceStatus(FAXTRACE_MODEMOPS,
			"error %d reading from modem", errno);
		else
		    timeout = timerExpired;
	    } else
		traceStatus(FAXTRACE_MODEMOPS,
		    "too many zero-length reads from modem");
	    return (EOF);
	}
	rcvNext = 0;
    }
    return (rcvBuf[rcvNext++]);
}

void
FaxServer::modemFlushInput()
{
    char rbuf[1024];
    while (getTimedModemLine(rbuf, 1))
	;
}

void
FaxServer::flushModemInput()
{
    rcvCC = rcvNext = 0;
}

fxBool
FaxServer::putModem(void* data, int n, int timer)
{
    traceStatus(FAXTRACE_MODEMCOM, "<-- data [%d]", n);
    if (timer)
	startTimeout(timer);
    else
	timeout = FALSE;
    n -= ::write(modemFd, (char*) data, n);
    if (timer)
	stopTimeout("writing to");
    return (!timeout && n == 0);
}

void
FaxServer::putModemLine(const char* cp)
{
    int n = strlen(cp);
    traceStatus(FAXTRACE_MODEMCOM, "<-- [%d:%s]", n, cp);
    ::write(modemFd, cp, n);
    static char CR = '\r';
    ::write(modemFd, &CR, 1);
}

ModemListener::ModemListener(int f)
{
    fd = f;
    ringChannel = addOutput("ring", fxDT_void);
}
ModemListener::~ModemListener()			{}
const char* ModemListener::className() const	{ return ("ModemListener"); }
void ModemListener::handleRead()		{ sendVoid(ringChannel); }
