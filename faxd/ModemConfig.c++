/*	$Header: /usr/people/sam/fax/faxd/RCS/ModemConfig.c++,v 1.50 1994/07/03 17:29:10 sam Exp $ */
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
#include "ModemConfig.h"
#include "t.30.h"
#include "config.h"

#include <string.h>
#include <syslog.h>

ModemConfig::ModemConfig()
    : type("unknown")
    , dialCmd("DT%s")			// %s = phone number
    , noAutoAnswerCmd("S0=0")
    , echoOffCmd("E0")
    , verboseResultsCmd("V1")
    , resultCodesCmd("Q0")
    , onHookCmd("H0")
    , softResetCmd("Z")
    , waitTimeCmd("S7=30")		// wait time is 30 seconds
    , pauseTimeCmd("S8=2")		// comma pause time is 2 seconds
    , class1Cmd("+FCLASS=1")		// set class 1 (fax)
    , class2CQQueryCmd("+FCQ=?")	// class 2 copy quality query
    , tagLineFmt("From %%n|%c|Page %%p of %%t")
{
    class2XmitWaitForXON = TRUE;	// default suits most Class 2 modems

    // default volume setting commands
    setVolumeCmds("M0 L0M1 L1M1 L2M1 L3M1");

    answerAnyCmd = "A";

    flowControl = FaxModem::FLOW_NONE;	// no flow control
    maxRate = FaxModem::BR19200;	// reasonable for most modems
    sendFillOrder = FILLORDER_LSB2MSB;	// default to CCITT bit order
    recvFillOrder = FILLORDER_LSB2MSB;	// default to CCITT bit order
    frameFillOrder = FILLORDER_LSB2MSB;	// default to most common usage

    resetDelay = 2600;			// 2.6 second delay after reset
    baudRateDelay = 0;			// delay after setting baud rate

    t1Timer = TIMER_T1;			// T.30 T1 timer (ms)
    t2Timer = TIMER_T2;			// T.30 T2 timer (ms)
    t4Timer = TIMER_T4;			// T.30 T4 timer (ms)
    dialResponseTimeout = 3*60*1000;	// dialing command timeout (ms)
    answerResponseTimeout = 3*60*1000;	// answer command timeout (ms)
    pageStartTimeout = 3*60*1000;	// page send/receive timeout (ms)
    pageDoneTimeout = 3*60*1000;	// page send/receive timeout (ms)

    class1TCFResponseDelay = 75;	// 75ms delay between TCF and ack/nak
    class1SendPPMDelay = 75;		// 75ms delay before sending PPM
    class1SendTCFDelay = 75;		// 75ms delay between sending DCS & TCF
    class1TrainingRecovery = 1500;	// 1.5sec delay after failed training
    class1RecvAbortOK = 200;		// 200ms after abort before flushing OK
    class1FrameOverhead = 4;		// flags + station id + 2-byte FCS
    class1RecvIdentTimer = t1Timer;	// default to standard protocol
    class1TCFMinRun = (2*TCF_DURATION)/3;// must be at least 2/3rds of expected
    class1TCFMaxNonZero = 10;		// max 10% non-zero data in TCF burst

    maxPacketSize = 16*1024;		// max write to modem
    interPacketDelay = 0;		// delay between modem writes
    waitForConnect = FALSE;		// unique answer response from modem

    percentGoodLines = 95;		// require 95% good lines
    maxConsecutiveBadLines = 5;		// 5 lines at 98 lpi
}

ModemConfig::~ModemConfig()
{
}

#ifdef streq
#undef streq
#endif
#define	streq(a,b)	(strcasecmp(a,b)==0)

static fxBool getBoolean(const char* cp)
    { return (streq(cp, "on") || streq(cp, "yes")); }

static BaudRate
findRate(const char* cp)
{
    static const struct {
	const char* name;
	BaudRate    br;
    } rates[] = {
	{   "300", FaxModem::BR300 },
	{  "1200", FaxModem::BR1200 },
	{  "2400", FaxModem::BR2400 },
	{  "4800", FaxModem::BR4800 },
	{  "9600", FaxModem::BR9600 },
	{ "19200", FaxModem::BR19200 },
	{ "38400", FaxModem::BR38400 },
	{ "57600", FaxModem::BR57600 },
	{ "76800", FaxModem::BR76800 },
    };

#define	N(a)	(sizeof (a) / sizeof (a[0]))
    for (int i = N(rates)-1; i >= 0; i--)
	if (streq(cp, rates[i].name))
	    return (rates[i].br);
    return (FaxModem::BR0);
#undef	N
}

static BaudRate
getRate(const char* cp)
{
    BaudRate br = findRate(cp);
    if (br == FaxModem::BR0) {
	syslog(LOG_ERR, "Unknown baud rate \"%s\", using 19200", cp);
	br = FaxModem::BR19200;			// default
    }
    return (br);
}

static u_int
getFill(const char* cp)
{
    if (streq(cp, "LSB2MSB"))
	return (FILLORDER_LSB2MSB);
    else if (streq(cp, "MSB2LSB"))
	return (FILLORDER_MSB2LSB);
    else {
	syslog(LOG_ERR, "Unknown fill order \"%s\"", cp);
        return ((u_int) -1);
    }
}

static FlowControl
getFlow(const char* cp)
{
    if (streq(cp, "xonxoff"))
	return (FaxModem::FLOW_XONXOFF);
    else if (streq(cp, "rtscts"))
	return (FaxModem::FLOW_RTSCTS);
    else if (streq(cp, "none"))
	return (FaxModem::FLOW_NONE);
    else {
	syslog(LOG_ERR, "Unknown flow control \"%s\", using xonxoff", cp);
	return (FaxModem::FLOW_XONXOFF);	// default
    }
}

void
ModemConfig::setVolumeCmds(const fxStr& tag)
{
    u_int l = 0;
    for (int i = FaxModem::OFF; i <= FaxModem::HIGH; i++) {
	fxStr tmp = tag.token(l, " \t");		// NB: for gcc
	setVolumeCmd[i] = parseATCmd(tmp);
    }
}

/*
 * Scan AT command strings and convert <...> escape
 * commands into single-byte escape codes that are
 * interpreted by FaxModem::atCmd.  Note that the
 * baud rate setting commands are carefully ordered
 * so that the desired baud rate can be extracted
 * from the low nibble.
 */
fxStr
ModemConfig::parseATCmd(const char* cp)
{
    fxStr cmd(cp);
    u_int pos = 0;
    while ((pos = cmd.next(pos, '<')) != cmd.length()) {
	u_int epos = pos+1;
	fxStr esc = cmd.token(epos, '>');
	esc.lowercase();

	char ecode;
	if (esc == "xon")
	    ecode = ESC_XONFLOW;
	else if (esc == "rts")
	    ecode = ESC_RTSFLOW;
	else if (esc == "none")
	    ecode = ESC_NOFLOW;
	else if (esc == "")		// NB: "<>" => <
	    ecode = '<';
	else {
	    BaudRate br = findRate(esc);
	    if (br == FaxModem::BR0) {
		syslog(LOG_ERR, "Unknown AT escape code \"%s\"", (char*) esc);
		pos = epos;
		continue;
	    }
	    ecode = 0x80|ord(br);
	}
	cmd.remove(pos, epos-pos);
	cmd.insert(ecode, pos);
    }
    return (cmd);
}

/*
 * The following tables map configuration parameter names to
 * pointers to class ModemConfig members.
 */
static const struct {
    const char*		 name;
    fxStr ModemConfig::* p;
} atcmds[] = {
    { "ModemAnswerCmd",			&ModemConfig::answerAnyCmd },
    { "ModemAnswerAnyCmd",		&ModemConfig::answerAnyCmd },
    { "ModemAnswerFaxCmd",		&ModemConfig::answerFaxCmd },
    { "ModemAnswerDataCmd",		&ModemConfig::answerDataCmd },
    { "ModemAnswerVoiceCmd",		&ModemConfig::answerVoiceCmd },
    { "ModemAnswerFaxBeginCmd",		&ModemConfig::answerFaxBeginCmd },
    { "ModemAnswerDataBeginCmd",	&ModemConfig::answerDataBeginCmd },
    { "ModemAnswerVoiceBeginCmd",	&ModemConfig::answerVoiceBeginCmd },
    { "ModemResetCmds",			&ModemConfig::resetCmds },
    { "ModemResetCmd",			&ModemConfig::resetCmds },
    { "ModemDialCmd",			&ModemConfig::dialCmd },
    { "ModemFlowControlCmd",		&ModemConfig::flowControlCmd },
    { "ModemSetupAACmd",		&ModemConfig::setupAACmd },
    { "ModemSetupDTRCmd",		&ModemConfig::setupDTRCmd },
    { "ModemSetupDCDCmd",		&ModemConfig::setupDCDCmd },
    { "ModemNoAutoAnswerCmd",		&ModemConfig::noAutoAnswerCmd },
    { "ModemEchoOffCmd",		&ModemConfig::echoOffCmd },
    { "ModemVerboseResultsCmd",		&ModemConfig::verboseResultsCmd },
    { "ModemResultCodesCmd",		&ModemConfig::resultCodesCmd },
    { "ModemOnHookCmd",			&ModemConfig::onHookCmd },
    { "ModemSoftResetCmd",		&ModemConfig::softResetCmd },
    { "ModemWaitTimeCmd",		&ModemConfig::waitTimeCmd },
    { "ModemCommaPauseTimeCmd",		&ModemConfig::pauseTimeCmd },
    { "ModemMfrQueryCmd",		&ModemConfig::mfrQueryCmd },
    { "ModemModelQueryCmd",		&ModemConfig::modelQueryCmd },
    { "ModemRevQueryCmd",		&ModemConfig::revQueryCmd },
    { "ModemSendBeginCmd",		&ModemConfig::sendBeginCmd },
    { "Class1Cmd",			&ModemConfig::class1Cmd },
    { "Class2Cmd",			&ModemConfig::class2Cmd },
    { "Class2BORCmd",			&ModemConfig::class2BORCmd },
    { "Class2RELCmd",			&ModemConfig::class2RELCmd },
    { "Class2CQCmd",			&ModemConfig::class2CQCmd },
    { "Class2AbortCmd",			&ModemConfig::class2AbortCmd },
    { "Class2CQQueryCmd",		&ModemConfig::class2CQQueryCmd },
    { "Class2DCCQueryCmd",		&ModemConfig::class2DCCQueryCmd },
    { "Class2TBCCmd",			&ModemConfig::class2TBCCmd },
    { "Class2CRCmd",			&ModemConfig::class2CRCmd },
    { "Class2PHCTOCmd",			&ModemConfig::class2PHCTOCmd },
    { "Class2BUGCmd",			&ModemConfig::class2BUGCmd },
    { "Class2LIDCmd",			&ModemConfig::class2LIDCmd },
    { "Class2DCCCmd",			&ModemConfig::class2DCCCmd },
    { "Class2DISCmd",			&ModemConfig::class2DISCmd },
    { "Class2DDISCmd",			&ModemConfig::class2DDISCmd },
    { "Class2CIGCmd",			&ModemConfig::class2CIGCmd },
    { "Class2PTSCmd",			&ModemConfig::class2PTSCmd },
    { "Class2SPLCmd",			&ModemConfig::class2SPLCmd },
    { "Class2PIECmd",			&ModemConfig::class2PIECmd },
    { "Class2NRCmd",			&ModemConfig::class2NRCmd },
    { "TagLineFont",			&ModemConfig::tagLineFontFile },
    { "TagLineFormat",			&ModemConfig::tagLineFmt },
};
static const struct {
    const char*		 name;
    u_int ModemConfig::* p;
} fillorders[] = {
    { "ModemRecvFillOrder",		&ModemConfig::recvFillOrder },
    { "ModemSendFillOrder",		&ModemConfig::sendFillOrder },
    { "ModemFrameFillOrder",		&ModemConfig::frameFillOrder },
};
static const struct {
    const char*		 name;
    u_int ModemConfig::* p;
} numbers[] = {
    { "ModemResetDelay",		&ModemConfig::resetDelay },
    { "ModemBaudRateDelay",		&ModemConfig::baudRateDelay },
    { "ModemMaxPacketSize",		&ModemConfig::maxPacketSize },
    { "ModemInterPacketDelay",		&ModemConfig::interPacketDelay },
    { "FaxT1Timer",			&ModemConfig::t1Timer },
    { "FaxT2Timer",			&ModemConfig::t2Timer },
    { "FaxT4Timer",			&ModemConfig::t4Timer },
    { "ModemDialResponseTimeout",	&ModemConfig::dialResponseTimeout },
    { "ModemAnswerResponseTimeout",	&ModemConfig::answerResponseTimeout },
    { "ModemPageStartTimeout",		&ModemConfig::pageStartTimeout },
    { "ModemPageDoneTimeout",		&ModemConfig::pageDoneTimeout },
    { "Class1TCFResponseDelay",		&ModemConfig::class1TCFResponseDelay },
    { "Class1SendPPMDelay",		&ModemConfig::class1SendPPMDelay },
    { "Class1SendTCFDelay",		&ModemConfig::class1SendTCFDelay },
    { "Class1TrainingRecovery",		&ModemConfig::class1TrainingRecovery },
    { "Class1RecvAbortOK",		&ModemConfig::class1RecvAbortOK },
    { "Class1FrameOverhead",		&ModemConfig::class1FrameOverhead },
    { "Class1RecvIdentTimer",		&ModemConfig::class1RecvIdentTimer },
    { "Class1TCFMaxNonZero",		&ModemConfig::class1TCFMaxNonZero },
    { "Class1TCFMinRun",		&ModemConfig::class1TCFMinRun },
    { "PercentGoodLines",		&ModemConfig::percentGoodLines },
    { "MaxConsecutiveBadLines",		&ModemConfig::maxConsecutiveBadLines },
};

fxBool
ModemConfig::parseItem(const char* tag, const char* value)
{
    int i;

#define	N(a)	(sizeof (a) / sizeof (a[0]))
    for (i = N(atcmds)-1; i >= 0; i--)
	if (streq(tag, atcmds[i].name)) {
	    (*this).*atcmds[i].p = parseATCmd(value);
	    return (TRUE);
	}
    for (i = N(fillorders)-1; i >= 0 ; i--)
	if (streq(tag, fillorders[i].name)) {
	    (*this).*fillorders[i].p = getFill(value);
	    return (TRUE);
	}
    for (i = N(numbers)-1; i >= 0 ; i--)
	if (streq(tag, numbers[i].name)) {
	    (*this).*numbers[i].p = atoi(value);
	    return (TRUE);
	}
#undef N
    fxBool recognized = TRUE;
    if (streq(tag, "ModemType"))
	type = value;
    else if (streq(tag, "ModemSetVolumeCmd"))
	setVolumeCmds(value);
    else if (streq(tag, "ModemFlowControl"))
	flowControl = getFlow(value);
    else if (streq(tag, "ModemMaxRate") || streq(tag, "ModemRate"))
	maxRate = getRate(value);
    else if (streq(tag, "ModemWaitForConnect"))
	waitForConnect = getBoolean(value);

	// Class 2-specific configuration controls
    else if (streq(tag, "Class2RecvDataTrigger"))
	class2RecvDataTrigger = value;
    else if (streq(tag, "Class2XmitWaitForXON"))
	class2XmitWaitForXON = getBoolean(value);

    else
	recognized = FALSE;
    return (recognized);
}
