/*	$Id: SendFaxJob.c++,v 1.22 1997/11/25 08:02:18 guru Rel $ */
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
#include "config.h"
#include "Sys.h"

#include <ctype.h>
#include <string.h>
#include <errno.h>

#if CONFIG_INETTRANSPORT
extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>            // XXX
}
#endif

#include "SendFaxClient.h"
#include "PageSize.h"
#include "FaxConfig.h"

SendFaxJob::SendFaxJob()
{
}
SendFaxJob::SendFaxJob(const SendFaxJob& other)
    : jobtag(other.jobtag)
    , mailbox(other.mailbox)
    , number(other.number)
    , subaddr(other.subaddr)
    , passwd(other.passwd)
    , external(other.external)
    , coverFile(other.coverFile)
    , coverTemplate(other.coverTemplate)
    , name(other.name)
    , location(other.location)
    , company(other.company)
    , comments(other.comments)
    , regarding(other.regarding)
    , voicenumber(other.voicenumber)
    , killTime(other.killTime)
    , sendTime(other.sendTime)
    , tagline(other.tagline)
    , pageSize(other.pageSize)
{
    notify = other.notify;
    autoCover = other.autoCover;
    coverIsTemp = other.coverIsTemp;
    sendTagLine = other.sendTagLine;
    retryTime = other.retryTime;
    hres = other.hres;
    vres = other.vres;
    pageWidth = other.pageWidth;
    pageLength = other.pageLength;
    maxRetries = other.maxRetries;
    maxDials = other.maxDials;
    priority = other.priority;
    minsp = other.minsp;
    desiredbr = other.desiredbr;
    desiredst = other.desiredst;
    desiredec = other.desiredec;
    desireddf = other.desireddf;
    pagechop = other.pagechop;
    chopthreshold = other.chopthreshold;
}
SendFaxJob::~SendFaxJob()
{
    if (coverFile != "" && coverIsTemp)
	Sys::unlink(coverFile);
}

/*
 * Configuration file support.
 */
#define	N(a)	(sizeof (a) / sizeof (a[0]))

const SendFaxJob::SFJ_stringtag SendFaxJob::strings[] = {
{ "tagline",		&SendFaxJob::tagline,		NULL },
{ "sendtime",		&SendFaxJob::sendTime,		NULL },
{ "killtime",		&SendFaxJob::killTime,		FAX_TIMEOUT },
{ "pagesize",		&SendFaxJob::pageSize,		"default" },
{ "jobtag",		&SendFaxJob::jobtag,		NULL },
{ "subaddress",		&SendFaxJob::subaddr,		NULL },
{ "password",		&SendFaxJob::passwd,		NULL },
{ "cover-template",	&SendFaxJob::coverTemplate,	NULL },
{ "cover-comments",	&SendFaxJob::comments,		NULL },
{ "cover-regarding",	&SendFaxJob::regarding,		NULL },
{ "cover-company",	&SendFaxJob::company,		NULL },
{ "cover-location",	&SendFaxJob::location,		NULL },
{ "cover-voice",	&SendFaxJob::voicenumber,	NULL },
};
const SendFaxJob::SFJ_numbertag SendFaxJob::numbers[] = {
{ "maxtries",		&SendFaxJob::maxRetries,	FAX_RETRIES },
{ "maxdials",		&SendFaxJob::maxDials,		FAX_REDIALS },
};
const SendFaxJob::SFJ_floattag SendFaxJob::floats[] = {
{ "hres",		&SendFaxJob::hres,		204. },
{ "vres",		&SendFaxJob::vres,		FAX_DEFVRES },
{ "pagewidth",		&SendFaxJob::pageWidth,		0. },
{ "pagelength",		&SendFaxJob::pageLength,	0. },
{ "chopthreshold",	&SendFaxJob::chopthreshold,	3.0 },
};

void
SendFaxJob::setupConfig()
{
    int i;

    for (i = N(strings)-1; i >= 0; i--)
	(*this).*strings[i].p = (strings[i].def ? strings[i].def : "");
    for (i = N(numbers)-1; i >= 0; i--)
	(*this).*numbers[i].p = numbers[i].def;
    for (i = N(floats)-1; i >= 0; i--)
	(*this).*floats[i].p = floats[i].def;

    autoCover = TRUE;
    sendTagLine = FALSE;		// default is to use server config
    notify = FAX_DEFNOTIFY;		// default notification
    mailbox = "";
    priority = FAX_DEFPRIORITY;		// default transmit priority
    minsp = (u_int) -1;
    desiredbr = (u_int) -1;
    desiredst = (u_int) -1;
    desiredec = (u_int) -1;
    desireddf = (u_int) -1;
    retryTime = (u_int) -1;
    pagechop = chop_default;
}

fxBool
SendFaxJob::setConfigItem(const char* tag, const char* value)
{
    u_int ix;
    if (FaxConfig::findTag(tag, (const FaxConfig::tags*) strings, N(strings), ix)) {
	(*this).*strings[ix].p = value;
	switch (ix) {
	case 0:	sendTagLine = TRUE; break;
	}
    } else if (FaxConfig::findTag(tag, (const FaxConfig::tags*) numbers, N(numbers), ix)) {
	(*this).*numbers[ix].p = atoi(value);
    } else if (FaxConfig::findTag(tag, (const FaxConfig::tags*) floats, N(floats), ix)) {
	(*this).*floats[ix].p = atof(value);
    } else if (streq(tag, "autocoverpage"))
	setAutoCoverPage(FaxConfig::getBoolean(value));
    else if (streq(tag, "notify") || streq(tag, "notification"))
	setNotification(value);
    else if (streq(tag, "mailaddr"))
	setMailbox(value);
    else if (streq(tag, "priority"))
	setPriority(value);
    else if (streq(tag, "minspeed"))
	setMinSpeed(value);
    else if (streq(tag, "desiredspeed"))
	setDesiredSpeed(value);
    else if (streq(tag, "desiredmst"))
	setDesiredMST(value);
    else if (streq(tag, "desiredec"))
	setDesiredEC(FaxConfig::getBoolean(value));
    else if (streq(tag, "desireddf"))
	setDesiredDF(value);
    else if (streq(tag, "retrytime"))
	setRetryTime(value);
    else if (streq(tag, "pagechop"))
	setChopHandling(value);
    else
	return (FALSE);
    return (TRUE);
}
#undef N

#define	valeq(a,b)	(strcasecmp(a,b)==0)
#define	valneq(a,b,n)	(strncasecmp(a,b,n)==0)

fxBool
SendFaxJob::setNotification(const char* v0)
{
    const char* v = v0;
    if (valneq(v, "when", 4)) {
	for (v += 4; isspace(*v); v++)
	    ;
    }
    if (valeq(v, "done"))
	notify = when_done;
    else if (valneq(v, "req", 3))
	notify = when_requeued;
    else if (valeq(v, "none") || valeq(v, "off"))
	notify = no_notice;
    else if (valeq(v, "default"))
	notify = FAX_DEFNOTIFY;
    else
	return (FALSE);
    return (TRUE);
}
void SendFaxJob::setNotification(FaxNotify n)		{ notify = n; }
/*
 * Create the mail address for a local user.
 */
void
SendFaxJob::setMailbox(const char* user)
{
    fxStr acct(user);
    if (acct != "" && acct.next(0, "@!") == acct.length()) {
	static fxStr domainName;
	if (domainName == "") {
	    char hostname[64];
	    (void) gethostname(hostname, sizeof (hostname));
#if CONFIG_INETTRANSPORT
	    struct hostent* hp = gethostbyname(hostname);
	    domainName = (hp ? hp->h_name : hostname);
#else
	    domainName = hostname;
#endif
	}
	mailbox = acct | "@" | domainName;
    } else
	mailbox = acct;
    // strip leading & trailing white space
    mailbox.remove(0, mailbox.skip(0, " \t"));
    mailbox.resize(mailbox.skipR(mailbox.length(), " \t"));
}
void SendFaxJob::setJobTag(const char* s)		{ jobtag = s; }

void
SendFaxJob::setRetryTime(const char* v)
{
    char* cp;
    u_int t = (u_int) strtoul(v, &cp, 10);
    if (cp) {
	while (isspace(*cp))
	    ;
	if (strncasecmp(cp, "min", 3) == 0)
	    t *= 60;
	else if (strncasecmp(cp, "hour", 4) == 0)
	    t *= 60*60;
	else if (strncasecmp(cp, "day", 3) == 0)
	    t *= 24*60*60;
    }
    retryTime = t;
}
void SendFaxJob::setRetryTime(u_int v)			{ retryTime = v; }
void SendFaxJob::setKillTime(const char* s)		{ killTime = s; }
void SendFaxJob::setSendTime(const char* s)		{ sendTime = s; }
void SendFaxJob::setMaxRetries(u_int n)			{ maxRetries = n; }
void SendFaxJob::setMaxDials(u_int n)			{ maxDials = n; }
void
SendFaxJob::setPriority(const char* pri)
{
    if (valeq(pri, "default") || valeq(pri, "normal"))
	priority = FAX_DEFPRIORITY;
    else if (valeq(pri, "bulk") || valeq(pri, "junk"))
	priority = FAX_DEFPRIORITY + 4*16;
    else if (valeq(pri, "high"))
	priority = FAX_DEFPRIORITY - 4*16;
    else
	priority = atoi(pri);
}
void SendFaxJob::setPriority(int p)			{ priority = p; }

void SendFaxJob::setDialString(const char* s)		{ number = s; }
void SendFaxJob::setSubAddress(const char* s)		{ subaddr = s; }
void SendFaxJob::setPassword(const char* s)		{ passwd = s; }
void SendFaxJob::setExternalNumber(const char* s)	{ external = s; }

void SendFaxJob::setAutoCoverPage(fxBool b)		{ autoCover = b; }
void
SendFaxJob::setCoverPageFile(const char* s, fxBool removeOnExit)
{
    if (coverFile != "" && removeOnExit)
	Sys::unlink(coverFile);
    coverFile = s;
    coverIsTemp = removeOnExit;
}
void SendFaxJob::setCoverTemplate(const char* s)	{ coverTemplate = s; }
void SendFaxJob::setCoverName(const char* s)		{ name = s; }
void SendFaxJob::setCoverLocation(const char* s)	{ location = s; }
void SendFaxJob::setCoverCompany(const char* s)		{ company = s; }
void SendFaxJob::setCoverComments(const char* s)	{ comments = s; }
void SendFaxJob::setCoverRegarding(const char* s)	{ regarding = s; }
void SendFaxJob::setCoverVoiceNumber(const char* s)	{ voicenumber = s; }

fxBool
SendFaxJob::setPageSize(const char* name)
{
    PageSizeInfo* info = PageSizeInfo::getPageSizeByName(name);
    if (info) {
	pageWidth = info->width();
	pageLength = info->height();
	pageSize = name;
	delete info;
	return (TRUE);
    } else
	return (FALSE);
}
void SendFaxJob::setVResolution(float r)		{ vres = r; }
void SendFaxJob::setHResolution(float r)		{ hres = r; }

int
SendFaxJob::getSpeed(const char* value) const
{
    switch (atoi(value)) {
    case 2400:	return (0);
    case 4800:	return (1);
    case 7200:	return (2);
    case 9600:	return (3);
    case 12000:	return (4);
    case 14400:	return (5);
    }
    return (-1);
}
void SendFaxJob::setMinSpeed(int v)			{ minsp = v; }
void SendFaxJob::setMinSpeed(const char* v)		{ minsp = getSpeed(v); }
void SendFaxJob::setDesiredSpeed(int v)			{ desiredbr = v; }
void SendFaxJob::setDesiredSpeed(const char* v)		{ desiredbr = getSpeed(v); }
void
SendFaxJob::setDesiredMST(const char* v)
{
    if (valeq(v, "0ms"))
	desiredst = 0;
    else if (valeq(v, "5ms"))
	desiredst = 1;
    else if (valeq(v, "10ms2"))
	desiredst = 2;
    else if (valeq(v, "10ms"))
	desiredst = 3;
    else if (valeq(v, "20ms2"))
	desiredst = 4;
    else if (valeq(v, "20ms"))
	desiredst = 5;
    else if (valeq(v, "40ms2"))
	desiredst = 6;
    else if (valeq(v, "40ms"))
	desiredst = 7;
    else
	desiredst = atoi(v);
}
void SendFaxJob::setDesiredMST(int v)			{ desiredst = v; }
void SendFaxJob::setDesiredEC(fxBool b)			{ desiredec = b; }
void
SendFaxJob::setDesiredDF(const char* v)
{
    if (strcasecmp(v, "1d") == 0 || strcasecmp(v, "1dmr") == 0)
	desireddf = 0;
    else if (strcasecmp(v, "2d") == 0 || strcasecmp(v, "2dmr") == 0)
	desireddf = 1;
    else if (strcasecmp(v, "2dmruncomp") == 0)
	desireddf = 1;				// NB: force 2D w/o uncompressed
    else if (strcasecmp(v, "2dmmr") == 0)
	desireddf = 3;
    else
	desireddf = atoi(v);
}
void SendFaxJob::setDesiredDF(int df)			{ desireddf = df; }

void
SendFaxJob::setTagLineFormat(const char* v)
{
    tagline = v;
    sendTagLine = TRUE;
}

void
SendFaxJob::setChopHandling(const char* v)
{
    if (strcasecmp(v, "none") == 0)
	pagechop = chop_none;
    else if (strcasecmp(v, "all") == 0)
	pagechop = chop_all;
    else if (strcasecmp(v, "last") == 0)
	pagechop = chop_last;
    else
	pagechop = atoi(v);
}
void SendFaxJob::setChopHandling(u_int v)		{ pagechop = v; }
void SendFaxJob::setChopThreshold(float v)		{ chopthreshold = v; }

extern int
parseAtSyntax(const char* s, const struct tm& ref, struct tm& at0, fxStr& emsg);

#define	CHECK(x)	{ if (!(x)) goto failure; }
#define	CHECKCMD(x)	CHECK(client.command(x) == COMPLETE)
#define	CHECKPARM(a,b)	CHECK(client.jobParm(a,b))
#define	IFPARM(a,b,v)	{ if ((b) != (v)) CHECKPARM(a,b) }

fxBool
SendFaxJob::createJob(SendFaxClient& client, fxStr& emsg)
{
    if (!client.setCurrentJob("DEFAULT")) {	// inherit from default
	emsg = client.getLastResponse();
	return (FALSE);
    }
    if (!client.newJob(jobid, groupid, emsg))	// create new job on server
	return (FALSE);

    time_t now = Sys::now();

    CHECKPARM("FROMUSER", client.getSenderName())

    struct tm tts;
    if (sendTime != "") {
	if (!parseAtSyntax(sendTime, *localtime(&now), tts, emsg)) {
	    emsg.insert(sendTime | ": ");
	    return (FALSE);
	}
	now = mktime(&tts);
	// NB: must send time relative to GMT
	CHECK(client.jobSendTime(*gmtime(&now)))
    } else
	tts = *localtime(&now);
    if (killTime != "") {
	struct tm when;
	if (!parseAtSyntax(killTime, tts, when, emsg)) {
	    emsg.insert(killTime | ": ");
	    return (FALSE);
	}
	CHECK(client.jobLastTime(mktime(&when) - now))
    }
    if (retryTime != (u_int) -1)
	CHECK(client.jobRetryTime(retryTime))
    IFPARM("MODEM", client.getModem(), "");	// XXX should be per-job state
    IFPARM("MAXDIALS", maxDials, (u_int) -1)
    IFPARM("MAXTRIES", maxRetries, (u_int) -1)
    CHECKPARM("SCHEDPRI", priority)
    /*
     * If the dialstring is different from the
     * displayable number then pass both.
     */
    IFPARM("EXTERNAL", external, number)
    CHECKPARM("DIALSTRING", number)
    IFPARM("SUBADDR", subaddr, "")
    IFPARM("PASSWD", passwd, "")
    CHECKPARM("NOTIFYADDR", mailbox)
    IFPARM("TOUSER", name, "")
    IFPARM("TOCOMPANY", company, "")
    IFPARM("TOLOCATION", location, "")
    IFPARM("JOBINFO", jobtag, "")
    CHECKPARM("VRES", (u_int) vres)
    CHECKPARM("PAGEWIDTH", (u_int) pageWidth)
    CHECKPARM("PAGELENGTH", (u_int) pageLength)
    IFPARM("MINBR", minsp, (u_int) -1)
    IFPARM("BEGBR", desiredbr, (u_int) -1)
    IFPARM("BEGST", desiredst, (u_int) -1)
    if (desiredec != (u_int) -1)
	CHECKPARM("USEECM", (fxBool) desiredec)
    if (desireddf != (u_int) -1) {
	CHECKPARM("DATAFORMAT",
	    desireddf == 0	? "g31d" :
	    desireddf == 1	? "g32d" :
	    desireddf == 2	? "g32dunc" :
	    desireddf == 3	? "g4"   :
				  "g31d")
    }
    if (sendTagLine) {
	CHECKPARM("USETAGLINE", TRUE)
	CHECKPARM("TAGLINE", tagline)
    }
    CHECKPARM("NOTIFY",
	notify == when_done	? "done" :
	notify == when_requeued	? "done+requeue" :
				  "none")
    CHECKPARM("PAGECHOP", 
	pagechop == chop_default? "default" :
	pagechop == chop_none	? "none" :
	pagechop == chop_all	? "all" :
				  "last")
    IFPARM("CHOPTHRESHOLD", chopthreshold, -1)
    if (coverFile != "") {
	int fd = Sys::open(coverFile, O_RDONLY);
	if (fd < 0) {
	    emsg = fxStr::format("%s: Can not open: %s",
		(const char*) coverFile, strerror(errno));
	    return (FALSE);			// XXX
	}
	fxStr coverDoc;
	fxBool fileSent = 
	       client.setFormat(FaxClient::FORM_PS)
	    && client.setType(FaxClient::TYPE_I)	// XXX??? TYPE_A
	    && client.sendZData(fd, FaxClient::storeTemp, coverDoc, emsg);
	Sys::close(fd);
	if (!fileSent) {
	    if (emsg == "")
		emsg = "Document transfer failed: " | client.getLastResponse();
	    return (FALSE);
	}
	CHECK(client.jobCover(coverDoc))
    }
    /*
     * Append documents and polling requests.
     */
    u_int i, n;
    for (i = 0, n = client.getNumberOfFiles(); i < n; i++)
	CHECK(client.jobDocument(client.getFileDocument(i)))
    for (i = 0, n = client.getNumberOfPollRequests(); i < n; i++) {
	fxStr sep, pwd;
	client.getPollRequest(i, sep, pwd);
	CHECK(client.jobPollRequest(sep, pwd))
    }
    return (TRUE);
failure:
    emsg = client.getLastResponse();
    return (FALSE);
}
#undef CHECKPARM
#undef IFPARM
#undef CHECKCMD
#undef CHECK

fxIMPLEMENT_ObjArray(SendFaxJobArray, SendFaxJob)
