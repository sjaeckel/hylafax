#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxModem.c++,v 1.6 91/05/23 12:25:41 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <ctype.h>

#include "Everex.h"
#include "Class2.h"
#include "FaxModem.h"
#include "FaxServer.h"
#include "Str.h"

#include "t.30.h"

#include <sys/termio.h>
#include <libc.h>

const char* FaxModem::callStatus[6] = {
    "call successful",				// OK
    "busy signal detected",			// BUSY
    "no carrier detected",			// NOCARRIER
    "no local dialtone",			// NODIALTONE
    "invalid dialing command",			// ERROR
    "unknown problem (check modem power)",	// FAILURE
};

FaxModem*
FaxModem::getModemByType(ModemType t, FaxServer& s)
{
    FaxModem* modem = 0;
    switch (t) {
    case ABATON:
    case EV958:
    case EV968:
	modem = new EverexModem(s);
	break;
    case CLASS2:
	modem = new Class2Modem(s);
	break;
    case CLASS1:
    case UNKNOWN:
	break;
    }
    if (modem && modem->getType() == UNKNOWN) {
	delete modem;
	modem = 0;
    }
    return (modem);
}

FaxModem*
FaxModem::getModemByName(const char* name, FaxServer& s)
{
    if (strcasecmp(name, "Abaton") == 0)
	return getModemByType(ABATON, s);
    else if (strcasecmp(name, "Ev958") == 0)
	return getModemByType(EV958, s);
    else if (strcasecmp(name, "Ev968") == 0)
	return getModemByType(EV968, s);
    else if (strcasecmp(name, "Class1") == 0)
	return getModemByType(CLASS1, s);
    else if (strcasecmp(name, "Class2") == 0)
	return getModemByType(CLASS2, s);
    return (0);
}

FaxModem::FaxModem(FaxServer& s, ModemType t) : server(s)
{
    type = t;
}

FaxModem::~FaxModem()
{
}

ModemType FaxModem::getType() const	{ return (type); }

const char* FaxModem::signallingNames[4] = {
     "V.27 at 2400 baud",
     "V.27 at 4800 baud",
     "V.29 at 9600 baud",
     "V.29 at 7200 baud",
};
const char* FaxModem::getSignallingRateName(u_int rate) const
    { return (rate < 4 ? signallingNames[rate] : "???"); }

u_int FaxModem::getBestScanlineTime() const { return DISMINSCAN_0MS; }

int FaxModem::getModemLine(char buf[], int timer)
    { return server.getModemLine(buf, timer); }
fxBool FaxModem::getTimedModemLine(char buf[], int timer)
    { return server.getTimedModemLine(buf, timer); }
int FaxModem::getModemChar(int timer)
    { return server.getModemChar(timer); }

void FaxModem::flushModemInput()
    { server.flushModemInput(); }
fxBool FaxModem::putModem(void* d, int n, int t)
    { return server.putModem(d, n, t); }
void FaxModem::putModemLine(const char* cp)
    { server.putModemLine(cp); }

void FaxModem::startTimeout(int seconds) { server.startTimeout(seconds); }
void FaxModem::stopTimeout(const char* w){ server.stopTimeout(w); }

/*
 * Select max baud rate supported by the modem.
 * We require at least 4800 baud since the signalling
 * rate constrains our operating speed for transferring
 * raw facsimile data.
 */
fxBool
FaxModem::selectBaudRate()
{
    for (int rate = B19200; rate > B4800; rate--) {
	server.setBaudRate(rate);
	if (reset())
	    return (TRUE);
    }
    return (FALSE);
}

fxBool FaxModem::sendBreak(fxBool pause)
    { return server.sendBreak(pause); }
fxBool FaxModem::setBaudRate(int r, fxBool b)
    { return server.setBaudRate(r,b); }
fxBool FaxModem::setInputFlowControl(fxBool b, fxBool f)
    { return server.setInputFlowControl(b,f); }

void FaxModem::beginTimedTransfer()	{ server.timeout = FALSE; }
void FaxModem::endTimedTransfer()	{}
fxBool FaxModem::wasTimeout()		{ return server.timeout; }

void FaxModem::resetPages()		{ server.npages = 0; }
void FaxModem::countPage()		{ server.npages++; }

int
FaxModem::fromHex(char* cp, int n)
{
    int v = 0;
    while (n-- > 0) {
	int c = *cp++;
	if (isxdigit(c)) {
	    if (isdigit(c))
		c -= '0';
	    else
		c = (c - 'A') + 10;
	    v = (v << 4) + c;
	}
    }
    return (v);
}

fxStr
FaxModem::toHex(int v, int ndigits)
{
    char buf[9];
    assert(ndigits <= 8);
    for (int i = ndigits-1; i >= 0; i--) {
	int n = v & 0xf;
	v >>= 4;
	buf[i] = (n < 10 ? '0' + n : 'A' + (n-10));
    }
    return (fxStr(buf, ndigits));
}

/* 
 * Hayes-style modem manipulation support.
 */
fxBool
FaxModem::reset()
{
    return atCmd("V1Q0H0E0");
}

fxBool
FaxModem::abort()
{
    return reset();
}

fxBool
FaxModem::sync()
{
    char rbuf[1024];
    while (getModemLine(rbuf)) {
	if (streq(rbuf, "OK", 2))
	    return (TRUE);
	if (streq(rbuf, "ERROR", 5) || streq(rbuf, "PHONE OFF-HOOK", 5))
	    break;
    }
    return (FALSE);
}

fxBool
FaxModem::atCmd(char cmd, char arg, fxBool waitForOK)
{
    char buf[3];
    buf[0] = cmd;
    buf[1] = arg;
    buf[2] = '\0';
    return (atCmd(buf, waitForOK));
}

fxBool
FaxModem::atCmd(const fxStr& cmd, fxBool waitForOK)
{
    putModemLine("AT" | cmd);
    return (waitForOK ? sync() : TRUE);
}

void
FaxModem::setEcho(fxBool on)
{
    atCmd('E', on ? '1' : '0');
}

void
FaxModem::setSpeakerVolume(SpeakerVolume l)
{
    if (l != OFF) {
	atCmd('L', '0' + l - 1);
	atCmd('M', '2');
    } else
	atCmd('M', '0');
}

void
FaxModem::hangup()
{
    atCmd("H0");
    sleep(1);
}

fxBool
FaxModem::waitForRings(int n)
{
    char rbuf[1024];
    while (n > 0 && getTimedModemLine(rbuf, 10) && streq(rbuf, "RING", 4))
	n--;
    return (n <= 0);
}

void FaxModem::sendBegin() {}
void FaxModem::sendSetupPhaseB() {}
void FaxModem::sendEnd() {}

void FaxModem::recvEnd() {}

void
FaxModem::setWaitTimeForCarrier(int secs)
{
    atCmd("S8=" | fxStr(secs, "%d"));
}

void
FaxModem::setCommaPauseTime(int secs)
{
    atCmd("S8=" | fxStr(secs, "%d"));
}
