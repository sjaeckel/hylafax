/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxMachineInfo.c++,v 1.43 1995/04/08 21:30:08 sam Rel $ */
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
#include <osfcn.h>
#include <sys/file.h>

#include "Sys.h"

#include "FaxMachineInfo.h"
#include "class2.h"
#include "config.h"

const fxStr FaxMachineInfo::infoDir(FAX_INFODIR);

FaxMachineInfo::FaxMachineInfo()
{
    changed = FALSE;
    resetConfig();
}
FaxMachineInfo::FaxMachineInfo(const FaxMachineInfo& other)
    : FaxConfig(other)
    , file(other.file)
    , csi(other.csi)
    , lastSendFailure(other.lastSendFailure)
    , lastDialFailure(other.lastDialFailure)
    , pagerPassword(other.pagerPassword)
{
    locked = other.locked;

    supportsHighRes = other.supportsHighRes;
    supports2DEncoding = other.supports2DEncoding;
    supportsPostScript = other.supportsPostScript;
    calledBefore = other.calledBefore;
    maxPageWidth = other.maxPageWidth;
    maxPageLength = other.maxPageLength;
    maxSignallingRate = other.maxSignallingRate;
    minScanlineTime = other.minScanlineTime;
    sendFailures = other.sendFailures;
    dialFailures = other.dialFailures;

    pagerMaxMsgLength = other.pagerMaxMsgLength;

    changed = other.changed;
}
FaxMachineInfo::~FaxMachineInfo() { writeConfig(); }

int
FaxMachineInfo::getMaxPageWidthInMM() const
{
    return (int)(maxPageWidth/(204.0f/25.4f));
}

#include <ctype.h>

fxBool
FaxMachineInfo::updateConfig(const fxStr& number)
{
    fxStr canon(number);
    for (u_int i = 0; i < canon.length(); i++)
	if (!isdigit(canon[i]))
	    canon.remove(i);
    if (file == "")
	file = infoDir | "/" | canon;
    return FaxConfig::updateConfig(file);
}

void
FaxMachineInfo::resetConfig()
{
    supportsHighRes = TRUE;		// assume 196 lpi support
    supports2DEncoding = TRUE;		// assume 2D-encoding support
    supportsPostScript = FALSE;		// no support for Adobe protocol
    calledBefore = FALSE;		// never called before
    maxPageWidth = 2432;		// max required width
    maxPageLength = -1;			// infinite page length
    maxSignallingRate = BR_14400;	// T.17 14.4KB
    minScanlineTime = ST_0MS;		// 0ms/0ms
    sendFailures = 0;
    dialFailures = 0;

    pagerMaxMsgLength = (u_int) -1;	// unlimited length
    pagerPassword = "";			// no password string

    locked = 0;
}

extern void vlogError(const char* fmt, va_list ap);

void
FaxMachineInfo::error(const char* fmt0 ...)
{
    va_list ap;
    va_start(ap, fmt0);
    vlogError(file | ": " | fmt0, ap);
    va_end(ap);
}

/*
 * Report an error encountered while parsing the info file.
 */
void
FaxMachineInfo::vconfigError(const char* fmt0, va_list ap)
{
    vlogError(file |
	fxStr::format(": line %u: %s", getConfigLineNumber(), fmt0), ap);
}
void
FaxMachineInfo::configError(const char* fmt0 ...)
{
    va_list ap;
    va_start(ap, fmt0);
    vconfigError(fmt0, ap);
    va_end(ap);
}
void
FaxMachineInfo::configTrace(const char* fmt0 ...)
{
    va_list ap;
    va_start(ap, fmt0);
    vconfigError(fmt0, ap);
    va_end(ap);
}

#define	N(a)		(sizeof (a) / sizeof (a[0]))

static const char* brnames[] =
   { "2400", "4800", "7200", "9600", "12000", "14400" };
#define	NBR	N(brnames)
static const char* stnames[] =
   { "0ms", "5ms", "10ms/5ms", "10ms",
     "20ms/10ms", "20ms", "40ms/20ms", "40ms" };
#define	NST	N(stnames)

#define	HIRES	0
#define	G32D	1
#define	PS	2
#define	WD	3
#define	LN	4
#define	BR	5
#define	ST	6

#define	setLocked(b,ix)	locked |= b<<ix

fxBool
FaxMachineInfo::setConfigItem(const char* tag, const char* value)
{
    int b = (tag[0] == '&' ? 1 : 0);	// locked down indicator
    if (b) tag++;
    if (streq(tag, "supportshighres")) {
	supportsHighRes = getBoolean(value);
	setLocked(b, HIRES);
    } else if (streq(tag, "supports2dencoding")) {
	supports2DEncoding = getBoolean(value);
	setLocked(b, G32D);
    } else if (streq(tag, "supportspostscript")) {
	supportsPostScript = getBoolean(value);
	setLocked(b, PS);
    } else if (streq(tag, "calledbefore")) {
	calledBefore = getBoolean(value);
    } else if (streq(tag, "maxpagewidth")) {
	maxPageWidth = getNumber(value);
	setLocked(b, WD);
    } else if (streq(tag, "maxpagelength")) {
	maxPageLength = getNumber(value);
	setLocked(b, LN);
    } else if (streq(tag, "sendfailures")) {
	sendFailures = getNumber(value);
    } else if (streq(tag, "dialfailures")) {
	dialFailures = getNumber(value);
    } else if (streq(tag, "remotecsi")) {
	csi = value;
    } else if (streq(tag, "lastsendfailure")) {
	lastSendFailure = value;
    } else if (streq(tag, "lastdialfailure")) {
	lastDialFailure = value;
    } else if (streq(tag, "maxsignallingrate")) {
	u_int ix;
	if (findValue(value, brnames, N(brnames), ix)) {
	    maxSignallingRate = ix;
	    setLocked(b, BR);
	}
    } else if (streq(tag, "minscanlinetime")) {
	u_int ix;
	if (findValue(value, stnames, N(stnames), ix)) {
	    minScanlineTime = ix;
	    setLocked(b, ST);
	}
    } else if (streq(tag, "pagermaxmsglength")) {
	pagerMaxMsgLength = getNumber(value);
    } else if (streq(tag, "pagerpassword")) {
	pagerPassword = value;
    } else
	return (FALSE);
    return (TRUE);
}

#define	isLocked(b)	(locked & (1<<b))

#define	checkLock(ix, member, value)	\
    if (!isLocked(ix)) {		\
	member = value;			\
	changed = TRUE;			\
    }

void FaxMachineInfo::setSupportsHighRes(fxBool b)
    { checkLock(HIRES, supportsHighRes, b); }
void FaxMachineInfo::setSupports2DEncoding(fxBool b)
    { checkLock(G32D, supports2DEncoding, b); }
void FaxMachineInfo::setSupportsPostScript(fxBool b)
    { checkLock(PS, supportsPostScript, b); }
void FaxMachineInfo::setMaxPageWidthInPixels(int v)
    { checkLock(WD, maxPageWidth, v); }
void FaxMachineInfo::setMaxPageLengthInMM(int v)
    { checkLock(LN, maxPageLength, v); }
void FaxMachineInfo::setMaxSignallingRate(int v)
    { checkLock(BR, maxSignallingRate, v); }
void FaxMachineInfo::setMinScanlineTime(int v)
    { checkLock(ST, minScanlineTime, v); }

void
FaxMachineInfo::setCalledBefore(fxBool b)
{
    calledBefore = b;
    changed = TRUE;
}

#define	checkChanged(member, value)	\
    if (member != value) {		\
	member = value;			\
	changed = TRUE;			\
    }

void FaxMachineInfo::setCSI(const fxStr& v)
    { checkChanged(csi, v); }
void FaxMachineInfo::setLastSendFailure(const fxStr& v)
    { checkChanged(lastSendFailure, v); }
void FaxMachineInfo::setLastDialFailure(const fxStr& v)
    { checkChanged(lastDialFailure, v); }
void FaxMachineInfo::setSendFailures(int v)
    { checkChanged(sendFailures, v); }
void FaxMachineInfo::setDialFailures(int v)
    { checkChanged(dialFailures, v); }

/*
 * Rewrite the file if the contents have changed.
 */
void
FaxMachineInfo::writeConfig()
{
    if (changed) {
	mode_t omask = ::umask(022);
	int fd = Sys::open(file, O_WRONLY|O_CREAT, 0644);
	(void) ::umask(omask);
	if (fd >= 0) {
	    FILE* fp = ::fdopen(fd, "w");
	    if (fp != NULL) {
		writeConfig(fp);
		::ftruncate(fd, ::ftell(fp));
		::fclose(fp);		// XXX check for write error
		return;
	    } else
		error("fdopen: %m");
	    ::close(fd);
	} else
	    error("open: %m");
	changed = FALSE;
    }
}

static void
putBoolean(FILE* fp, const char* tag, fxBool locked, fxBool b)
{
    ::fprintf(fp, "%s%s:%s\n", locked ? "&" : "", tag, b ? "yes" : "no");
}

static void
putDecimal(FILE* fp, const char* tag, fxBool locked, int v)
{
    ::fprintf(fp, "%s%s:%d\n", locked ? "&" : "", tag, v);
}

static void
putString(FILE* fp, const char* tag, fxBool locked, const char* v)
{
    ::fprintf(fp, "%s%s:\"%s\"\n", locked ? "&" : "", tag, v);
}

static void
putIfString(FILE* fp, const char* tag, fxBool locked, const char* v)
{
    if (*v != '\0')
	::fprintf(fp, "%s%s:\"%s\"\n", locked ? "&" : "", tag, v);
}

void
FaxMachineInfo::writeConfig(FILE* fp)
{
    putBoolean(fp, "supportsHighRes", isLocked(HIRES), supportsHighRes);
    putBoolean(fp, "supports2DEncoding", isLocked(G32D),supports2DEncoding);
    putBoolean(fp, "supportsPostScript", isLocked(PS), supportsPostScript);
    putBoolean(fp, "calledBefore", FALSE, calledBefore);
    putDecimal(fp, "maxPageWidth", isLocked(WD), maxPageWidth);
    putDecimal(fp, "maxPageLength", isLocked(LN), maxPageLength);
    putString(fp, "maxSignallingRate", isLocked(BR),
	brnames[fxmin(maxSignallingRate, BR_14400)]);
    putString(fp, "minScanlineTime", isLocked(ST),
	stnames[fxmin(minScanlineTime, ST_40MS)]);
    putString(fp, "remoteCSI", FALSE, csi);
    putDecimal(fp, "sendFailures", FALSE, sendFailures);
    putIfString(fp, "lastSendFailure", FALSE, lastSendFailure);
    putDecimal(fp, "dialFailures", FALSE, dialFailures);
    putIfString(fp, "lastDialFailure", FALSE, lastDialFailure);
}
