/*	$Header: /usr/people/sam/fax/./faxd/RCS/ModemConfig.c++,v 1.74 1995/04/08 21:30:55 sam Rel $ */
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
#include "ModemConfig.h"

#include "t.30.h"
#include "config.h"

#define	N(a)	(sizeof (a) / sizeof (a[0]))

ModemConfig::ModemConfig()
{
    setupConfig();
}

ModemConfig::~ModemConfig()
{
}

const fxStr&
ModemConfig::getFlowCmd(FlowControl f) const
{
    if (f == ClassModem::FLOW_RTSCTS)
	return (hardFlowCmd);
    else if (f == ClassModem::FLOW_XONXOFF)
	return (softFlowCmd);
    else if (f == ClassModem::FLOW_NONE)
	return (noFlowCmd);
    else
	return (fxStr::null);
}

/*
 * The following tables map configuration parameter names to
 * pointers to class ModemConfig members and provide default
 * values that are forced when the configuration is reset
 * prior to reading a configuration file.
 */

/*
 * Note that all of the Class 2/2.0 parameters except
 * Class2CQQueryCmd are initialized at the time the
 * modem is setup based on whether the modem is Class 2
 * or Class 2.0.
 */
static const struct {
    const char*		 name;
    fxStr ModemConfig::* p;
    const char*		 def;		// NULL is shorthand for ""
} atcmds[] = {
{ "modemanswercmd",		&ModemConfig::answerAnyCmd,	"ATA" },
{ "modemansweranycmd",		&ModemConfig::answerAnyCmd },
{ "modemanswerfaxcmd",		&ModemConfig::answerFaxCmd },
{ "modemanswerdatacmd",		&ModemConfig::answerDataCmd },
{ "modemanswervoicecmd",	&ModemConfig::answerVoiceCmd },
{ "modemanswerfaxbegincmd",	&ModemConfig::answerFaxBeginCmd },
{ "modemanswerdatabegincmd",	&ModemConfig::answerDataBeginCmd },
{ "modemanswervoicebegincmd",	&ModemConfig::answerVoiceBeginCmd },
{ "modemresetcmds",		&ModemConfig::resetCmds },
{ "modemresetcmd",		&ModemConfig::resetCmds },
{ "modemdialcmd",		&ModemConfig::dialCmd,		"ATDT%s" },
{ "modemnoflowcmd",		&ModemConfig::noFlowCmd },
{ "modemsoftflowcmd",		&ModemConfig::softFlowCmd },
{ "modemhardflowcmd",		&ModemConfig::hardFlowCmd },
{ "modemsetupaacmd",		&ModemConfig::setupAACmd },
{ "modemsetupdtrcmd",		&ModemConfig::setupDTRCmd },
{ "modemsetupdcdcmd",		&ModemConfig::setupDCDCmd },
{ "modemnoautoanswercmd",	&ModemConfig::noAutoAnswerCmd,	"ATS0=0" },
{ "modemechooffcmd",		&ModemConfig::echoOffCmd,	"ATE0" },
{ "modemverboseresultscmd",	&ModemConfig::verboseResultsCmd,"ATV1" },
{ "modemresultcodescmd",	&ModemConfig::resultCodesCmd,	"ATQ0" },
{ "modemonhookcmd",		&ModemConfig::onHookCmd,	"ATH0" },
{ "modemsoftresetcmd",		&ModemConfig::softResetCmd,	"ATZ" },
{ "modemwaittimecmd",		&ModemConfig::waitTimeCmd,	"ATS7=60" },
{ "modemcommapausetimecmd",	&ModemConfig::pauseTimeCmd,	"ATS8=2" },
{ "modemmfrquerycmd",		&ModemConfig::mfrQueryCmd },
{ "modemmodelquerycmd",		&ModemConfig::modelQueryCmd },
{ "modemrevquerycmd",		&ModemConfig::revQueryCmd },
{ "modemsendbegincmd",		&ModemConfig::sendBeginCmd },
{ "modemclassquerycmd",		&ModemConfig::classQueryCmd,	"AT+FCLASS=?" },
{ "class0cmd",			&ModemConfig::class0Cmd,	"AT+FCLASS=0" },
{ "class1cmd",			&ModemConfig::class1Cmd,	"AT+FCLASS=1" },
{ "class1nflocmd",		&ModemConfig::class1NFLOCmd },
{ "class1sflocmd",		&ModemConfig::class1SFLOCmd },
{ "class1hflocmd",		&ModemConfig::class1HFLOCmd },
{ "class2cmd",			&ModemConfig::class2Cmd },
{ "class2borcmd",		&ModemConfig::class2BORCmd },
{ "class2relcmd",		&ModemConfig::class2RELCmd },
{ "class2cqcmd",		&ModemConfig::class2CQCmd },
{ "class2abortcmd",		&ModemConfig::class2AbortCmd },
{ "class2cqquerycmd",    	&ModemConfig::class2CQQueryCmd,	"AT+FCQ=?" },
{ "class2dccquerycmd",		&ModemConfig::class2DCCQueryCmd },
{ "class2tbccmd",		&ModemConfig::class2TBCCmd },
{ "class2crcmd",		&ModemConfig::class2CRCmd },
{ "class2phctocmd",		&ModemConfig::class2PHCTOCmd },
{ "class2bugcmd",		&ModemConfig::class2BUGCmd },
{ "class2lidcmd",		&ModemConfig::class2LIDCmd },
{ "class2dcccmd",		&ModemConfig::class2DCCCmd },
{ "class2discmd",		&ModemConfig::class2DISCmd },
{ "class2ddiscmd",		&ModemConfig::class2DDISCmd },
{ "class2cigcmd",		&ModemConfig::class2CIGCmd },
{ "class2ptscmd",		&ModemConfig::class2PTSCmd },
{ "class2splcmd",		&ModemConfig::class2SPLCmd },
{ "class2piecmd",		&ModemConfig::class2PIECmd },
{ "class2nrcmd",		&ModemConfig::class2NRCmd },
{ "class2nflocmd",		&ModemConfig::class2NFLOCmd },
{ "class2sflocmd",		&ModemConfig::class2SFLOCmd },
{ "class2hflocmd",		&ModemConfig::class2HFLOCmd },
};
static const struct {
    const char*		 name;
    fxStr ModemConfig::* p;
    const char*		 def;		// NULL is shorthand for ""
} strcmds[] = {
{ "modemtype",			&ModemConfig::type,		"unknown" },
{ "taglinefont",		&ModemConfig::tagLineFontFile },
{ "taglineformat",		&ModemConfig::tagLineFmt,
  "From %%n|%c|Page %%p of %%t" },
{ "class2recvdatatrigger",	&ModemConfig::class2RecvDataTrigger },
{ "ringdata",			&ModemConfig::ringData },
{ "ringfax",			&ModemConfig::ringFax },
{ "ringvoice",			&ModemConfig::ringVoice },
{ "cidname",			&ModemConfig::cidName },
{ "cidnumber",			&ModemConfig::cidNumber },
};
static const struct {
    const char*		 name;
    u_int ModemConfig::* p;
    u_int		 def;
} fillorders[] = {
{ "modemrecvfillorder",	 &ModemConfig::recvFillOrder,  FILLORDER_LSB2MSB },
{ "modemsendfillorder",	 &ModemConfig::sendFillOrder,  FILLORDER_LSB2MSB },
{ "modemframefillorder", &ModemConfig::frameFillOrder, FILLORDER_LSB2MSB },
};
static const struct {
    const char*		 name;
    u_int ModemConfig::* p;
    u_int		 def;
} numbers[] = {
{ "percentgoodlines",		&ModemConfig::percentGoodLines,	     95 },
{ "maxconsecutivebadlines",	&ModemConfig::maxConsecutiveBadLines,5 },
{ "modemresetdelay",		&ModemConfig::resetDelay,	     2600 },
{ "modembaudratedelay",		&ModemConfig::baudRateDelay,	     0 },
{ "modemmaxpacketsize",		&ModemConfig::maxPacketSize,	     16*1024 },
{ "modeminterpacketdelay",	&ModemConfig::interPacketDelay,	     0 },
{ "faxt1timer",			&ModemConfig::t1Timer,		     TIMER_T1 },
{ "faxt2timer",			&ModemConfig::t2Timer,		     TIMER_T2 },
{ "faxt4timer",			&ModemConfig::t4Timer,		     TIMER_T4 },
{ "modemdialresponsetimeout",	&ModemConfig::dialResponseTimeout,   3*60*1000},
{ "modemanswerresponsetimeout",	&ModemConfig::answerResponseTimeout, 3*60*1000},
{ "modempagestarttimeout",	&ModemConfig::pageStartTimeout,	     3*60*1000},
{ "modempagedonetimeout",	&ModemConfig::pageDoneTimeout,	     3*60*1000},
{ "class1tcfrecvtimeout",	&ModemConfig::class1TCFRecvTimeout,  4500 },
{ "class1tcfresponsedelay",	&ModemConfig::class1TCFResponseDelay,75 },
{ "class1sendppmdelay",		&ModemConfig::class1SendPPMDelay,    75 },
{ "class1sendtcfdelay",		&ModemConfig::class1SendTCFDelay,    75 },
{ "class1trainingrecovery",	&ModemConfig::class1TrainingRecovery,1500 },
{ "class1recvabortok",		&ModemConfig::class1RecvAbortOK,     200 },
{ "class1frameoverhead",	&ModemConfig::class1FrameOverhead,   4 },
{ "class1recvIdentTimer",	&ModemConfig::class1RecvIdentTimer,  TIMER_T1 },
{ "class1tcfmaxnonzero",	&ModemConfig::class1TCFMaxNonZero,   10 },
{ "class1tcfminrun",		&ModemConfig::class1TCFMinRun,
  (2*TCF_DURATION)/3 },
};

void
ModemConfig::setupConfig()
{
    int i;

    for (i = N(atcmds)-1; i >= 0; i--)
	(*this).*atcmds[i].p = (atcmds[i].def ? atcmds[i].def : "");
    for (i = N(strcmds)-1; i >= 0; i--)
	(*this).*strcmds[i].p = (strcmds[i].def ? strcmds[i].def : "");
    for (i = N(fillorders)-1; i >= 0; i--)
	(*this).*fillorders[i].p = fillorders[i].def;
    for (i = N(numbers)-1; i >= 0; i--)
	(*this).*numbers[i].p = numbers[i].def;

    flowControl		= ClassModem::FLOW_NONE;// no flow control
    maxRate		= ClassModem::BR19200;	// reasonable for most modems
    waitForConnect	= FALSE;		// unique modem answer response
    class2XmitWaitForXON = TRUE;		// default per Class 2 spec
    class2SendRTC	= FALSE;		// default per Class 2 spec
    setVolumeCmds("ATM0 ATL0M1 ATL1M1 ATL2M1 ATL3M1");
}

void
ModemConfig::resetConfig()
{
    FaxConfig::resetConfig();
    setupConfig();
}

#define	valeq(a,b)	(::strcasecmp(a,b)==0)

BaudRate
ModemConfig::findRate(const char* cp)
{
    static const struct {
	const char* name;
	BaudRate    br;
    } rates[] = {
	{    "300", ClassModem::BR300 },
	{   "1200", ClassModem::BR1200 },
	{   "2400", ClassModem::BR2400 },
	{   "4800", ClassModem::BR4800 },
	{   "9600", ClassModem::BR9600 },
	{  "19200", ClassModem::BR19200 },
	{  "38400", ClassModem::BR38400 },
	{  "57600", ClassModem::BR57600 },
	{  "76800", ClassModem::BR76800 },
	{ "115200", ClassModem::BR115200 },
    };
    for (int i = N(rates)-1; i >= 0; i--)
	if (streq(cp, rates[i].name))
	    return (rates[i].br);
    return (ClassModem::BR0);
}

BaudRate
ModemConfig::getRate(const char* cp)
{
    BaudRate br = findRate(cp);
    if (br == ClassModem::BR0) {
	configError("Unknown baud rate \"%s\", using 19200", cp);
	br = ClassModem::BR19200;		// default
    }
    return (br);
}

u_int
ModemConfig::getFill(const char* cp)
{
    if (valeq(cp, "LSB2MSB"))
	return (FILLORDER_LSB2MSB);
    else if (valeq(cp, "MSB2LSB"))
	return (FILLORDER_MSB2LSB);
    else {
	configError("Unknown fill order \"%s\"", cp);
        return ((u_int) -1);
    }
}

FlowControl
ModemConfig::getFlow(const char* cp)
{
    if (valeq(cp, "xonxoff"))
	return (ClassModem::FLOW_XONXOFF);
    else if (valeq(cp, "rtscts"))
	return (ClassModem::FLOW_RTSCTS);
    else if (valeq(cp, "none"))
	return (ClassModem::FLOW_NONE);
    else {
	configError("Unknown flow control \"%s\", using xonxoff", cp);
	return (ClassModem::FLOW_XONXOFF);	// default
    }
}

void
ModemConfig::setVolumeCmds(const fxStr& tag)
{
    u_int l = 0;
    for (int i = ClassModem::OFF; i <= ClassModem::HIGH; i++)
	setVolumeCmd[i] = parseATCmd(tag.token(l, " \t"));
}

/*
 * Scan AT command strings and convert <...> escape
 * commands into single-byte escape codes that are
 * interpreted by ClassModem::atCmd.  Note that the
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

	u_char ecode;
	u_int delay;
	if (esc == "xon")
	    ecode = ESC_XONFLOW;
	else if (esc == "rts")
	    ecode = ESC_RTSFLOW;
	else if (esc == "none")
	    ecode = ESC_NOFLOW;
	else if (esc.length() > 5 && esc.head(5) == "delay") {
	    delay = (u_int) ::atoi(&esc[5]);
	    if (delay > 255) {
		configError("Bad AT delay value \"%s\"", (char*) esc);
		pos = epos;
		continue;
	    }
	    ecode = ESC_DELAY;
	} else if (esc == "")		// NB: "<>" => <
	    ecode = '<';
	else {
	    BaudRate br = findRate(esc);
	    if (br == ClassModem::BR0) {
		configError("Unknown AT escape code \"%s\"", (char*) esc);
		pos = epos;
		continue;
	    }
	    ecode = 0x80|ord(br);
	}
	cmd.remove(pos, epos-pos);
	cmd.insert(ecode, pos);
	if (ecode == ESC_DELAY)
	    cmd.insert(delay, pos+1);
    }
    return (cmd);
}

void
ModemConfig::parseCID(const char* rbuf, CallerID& cid) const
{
    if (strneq(rbuf, cidName, cidName.length()))
	cid.name = rbuf+cidName.length();
    else if (strneq(rbuf, cidNumber, cidNumber.length()))
	cid.number = rbuf+cidNumber.length();
}

fxBool
ModemConfig::setConfigItem(const char* tag, const char* value)
{
    u_int ix;
    if (findTag(tag, (const tags*)atcmds, N(atcmds), ix))
	(*this).*atcmds[ix].p = parseATCmd(value);
    else if (findTag(tag, (const tags*)strcmds, N(strcmds), ix))
	(*this).*strcmds[ix].p = value;
    else if (findTag(tag, (const tags*)fillorders, N(fillorders), ix))
	(*this).*fillorders[ix].p = getFill(value);
    else if (findTag(tag, (const tags*)numbers, N(numbers), ix))
	(*this).*numbers[ix].p = ::atoi(value);

    else if (streq(tag, "modemsetvolumecmd"))
	setVolumeCmds(value);
    else if (streq(tag, "modemflowcontrol"))
	flowControl = getFlow(value);
    else if (streq(tag, "modemrate"))
	maxRate = getRate(value);
    else if (streq(tag, "modemwaitforconnect"))
	waitForConnect = getBoolean(value);
    else if (streq(tag, "class2xmitwaitforxon"))
	class2XmitWaitForXON = getBoolean(value);
    else if (streq(tag, "class2sendrtc"))
	class2SendRTC = getBoolean(value);
    else
	return (FALSE);
    return (TRUE);
}
#undef N
