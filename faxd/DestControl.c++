/*	$Id: DestControl.c++ 2 2005-11-11 21:32:03Z faxguy $ */
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

#include "Sys.h"

#include "DestControl.h"
#include "faxQueueApp.h"
#include "FaxTrace.h"

#define	DCI_MAXCONCURRENTCALLS	0x0001
#define	DCI_TIMEOFDAY		0x0002
#define	DCI_MAXSENDPAGES	0x0004
#define	DCI_MAXDIALS		0x0008
#define	DCI_MAXTRIES		0x0010
#define	DCI_USEXVRES		0x0020
#define	DCI_VRES		0x0040

#define	isDefined(b)		(defined & b)
#define	setDefined(b)		(defined |= b)

const DestControlInfo DestControlInfo::defControlInfo;

DestControlInfo::DestControlInfo()		 : pattern("")	{ defined = 0; }
DestControlInfo::DestControlInfo(const char* re) : pattern(re)	{ defined = 0; }
DestControlInfo::DestControlInfo(const DestControlInfo& other)
    : pattern(other.pattern)
    , rejectNotice(other.rejectNotice)
    , modem(other.modem)
    , tod(other.tod)
    , args(other.args)
{
    defined = other.defined;
    maxConcurrentCalls = other.maxConcurrentCalls;
    maxSendPages = other.maxSendPages;
    maxDials = other.maxDials;
    maxTries = other.maxTries;
    usexvres = other.usexvres;
    vres = other.vres;
}
DestControlInfo::~DestControlInfo() {}

// XXX can't sort
int DestControlInfo::compare(const DestControlInfo*) const { return (0); }

static int getNumber(const char* s) { return ((int) strtol(s, NULL, 0)); }

void
DestControlInfo::parseEntry(const char* tag, const char* value, bool quoted)
{
    if (streq(tag, "rejectnotice")) {
	rejectNotice = value;
    } else if (streq(tag, "modem")) {
	modem = value;
    } else if (streq(tag, "maxconcurrentjobs")) {	// backwards compatibility
	maxConcurrentCalls = getNumber(value);
	setDefined(DCI_MAXCONCURRENTCALLS);
    } else if (streq(tag, "maxconcurrentcalls")) {
	maxConcurrentCalls = getNumber(value);
	setDefined(DCI_MAXCONCURRENTCALLS);
    } else if (streq(tag, "maxsendpages")) {
	maxSendPages = getNumber(value);
	setDefined(DCI_MAXSENDPAGES);
    } else if (streq(tag, "maxdials")) {
	maxDials = getNumber(value);
	setDefined(DCI_MAXDIALS);
    } else if (streq(tag, "maxtries")) {
	maxTries = getNumber(value);
	setDefined(DCI_MAXTRIES);
    } else if (streq(tag, "timeofday")) {
	tod.parse(value);
	setDefined(DCI_TIMEOFDAY);
    } else if (streq(tag, "usexvres")) {
	usexvres = getNumber(value);
	setDefined(DCI_USEXVRES);
    } else if (streq(tag, "vres")) {
	vres = getNumber(value);
	setDefined(DCI_VRES);
    } else {
	if( args != "" )
	    args.append('\0');
	const char* quote = quoted ? "\"" : "";
	args.append(fxStr::format("-c%c%s:%s%s%s",
	    '\0', tag, quote, value, quote));
    }
}

u_int
DestControlInfo::getMaxConcurrentCalls() const
{
    if (isDefined(DCI_MAXCONCURRENTCALLS))
	return maxConcurrentCalls;
    else
	return faxQueueApp::instance().getMaxConcurrentCalls();
}

u_int
DestControlInfo::getMaxSendPages() const
{
    if (isDefined(DCI_MAXSENDPAGES))
	return maxSendPages;
    else
	return faxQueueApp::instance().getMaxSendPages();
}

u_int
DestControlInfo::getMaxDials() const
{
    if (isDefined(DCI_MAXDIALS))
	return maxDials;
    else
	return faxQueueApp::instance().getMaxDials();
}

u_int
DestControlInfo::getMaxTries() const
{
    if (isDefined(DCI_MAXTRIES))
	return maxTries;
    else
	return faxQueueApp::instance().getMaxTries();
}

const fxStr&
DestControlInfo::getRejectNotice() const
{
    return rejectNotice;
}

const fxStr&
DestControlInfo::getModem() const
{
    return modem;
}

time_t
DestControlInfo::nextTimeToSend(time_t t) const
{
    if (isDefined(DCI_TIMEOFDAY))
	return tod.nextTimeOfDay(t);
    else
	return faxQueueApp::instance().nextTimeToSend(t);
}

int
DestControlInfo::getUseXVRes() const
{
    if (isDefined(DCI_USEXVRES))
	return usexvres;
    else
	return -1;
}

u_int
DestControlInfo::getVRes() const
{
    if (isDefined(DCI_VRES))
	return vres;
    else
	return 0;
}

/*
 * Destination Control Database File Support.
 */

DestControl::DestControl()
{
    lastModTime = 0;
}
DestControl::~DestControl() {}

void
DestControl::setFilename(const char* s)
{
    if (filename != s) {
	filename = s;
	lastModTime = 0;
    }
}

const DestControlInfo&
DestControl::operator[](const fxStr& canon)
{
    struct stat sb;
    if (Sys::stat(filename, sb) == 0 && sb.st_mtime > lastModTime) {
	info.resize(0);
	readContents();
	lastModTime = sb.st_mtime;
    }
    for (u_int i = 0, n = info.length(); i < n; i++) {
	DestControlInfo& dci = info[i];
	if (dci.pattern.Find(canon))
	    return (dci);
    }
    return (DestControlInfo::defControlInfo);
}

void
DestControl::readContents()
{
    FILE* fp = Sys::fopen(filename, "r");
    if (fp) {
	lineno = 0;
	while (parseEntry(fp))
	    ;
	fclose(fp);
    }
}

bool
DestControl::readLine(FILE* fp, char line[], u_int cc)
{
    if (fgets(line, cc, fp) == NULL)
	return (false);
    lineno++;
    char* cp;
    if ((cp = strchr(line, '#')))
	*cp = '\0';
    if ((cp = strchr(line, '\n')))
	*cp = '\0';
    return (true);
}

static bool
isContinued(FILE* fp)
{
    int c = getc(fp);
    if (c == EOF)
	return (false);
    ungetc(c, fp);
    return (isspace(c) || c == '#');
}

void
DestControl::skipEntry(FILE* fp, char line[], u_int cc)
{
    while (isContinued(fp) && readLine(fp, line, cc))
	;
}

bool
DestControl::parseEntry(FILE* fp)
{
    char line[1024];
    for (;;) {
	if (!readLine(fp, line, sizeof (line)-1))
	    return (false);
	char* cp;
	for (cp = line; *cp && !isspace(*cp); cp++)
	    ;
	if (*cp == '\0')
	    continue;
	if (cp == line) {
	    parseError("Missing regular expression.");
	    skipEntry(fp, line, sizeof (line)-1);
	    return (true);
	}
	*cp++ = '\0';
	DestControlInfo dci(line);
	if (dci.pattern.getErrorCode() > REG_NOMATCH) {
	    RE& re = dci.pattern;
	    fxStr emsg;
	    re.getError(emsg);
	    parseError("Bad regular expression: %s: " | emsg, re.pattern());
	    skipEntry(fp, line, sizeof (line)-1);
	    return (true);
	}
	for (;;) {
	    while (isspace(*cp))
		cp++;
	    if (*cp == '\0') {			// EOL, look for continuation
		if (!isContinued(fp)) {
		    info.append(dci);
		    return (true);
		}
		if (!readLine(fp, line, sizeof (line)-1)) {
		    info.append(dci);
		    return (false);
		}
		cp = line;
		continue;			// go back and skip whitespace
	    }
	    const char* tag = cp;
	    while (isalnum(*cp)) {		// collect tag and lower case
		if (isupper(*cp))
		    *cp = tolower(*cp);
		cp++;
	    }
	    if (cp != tag) {
		if (*cp != '\0')		// terminate tag
		    *cp++ = '\0';
		while (isspace(*cp))		// whitespace after tag
		    cp++;
	    } else {				// syntax error, no tag
		parseError("Missing parameter name.");
		tag = "";
	    }
	    if (*cp != '\0' && *cp != '"' && !isalnum(*cp)) {
		// skip any delimiter & any following whitespace
		for (++cp; isspace(*cp); cp++)
		    ;
	    }
	    const char* value = cp;
	    bool quoted = false;
	    if (*cp == '"') {			// quoted value
		quoted = true;
		value++;
		for (cp++; *cp && *cp != '"'; cp++)
		    ;
		if (*cp != '"') {
		    parseError("Missing \" in tag value \"%s\".", value);
		    // NB: continue parsing
		}
	    } else {				// value delimited by whitespace
		while (*cp && !isspace(*cp))
		    cp++;
	    }
	    if (*cp != '\0')			// terminate value
		*cp++ = '\0';
	    if (tag[0] != '\0')
		dci.parseEntry(tag, value, quoted);
	}
    }
}

/*
 * Report an error encountered while parsing.
 */
void
DestControl::parseError(const char* fmt0 ...)
{
    va_list ap;
    va_start(ap, fmt0);
    vlogError(filename | fxStr::format(": line %u: %s", lineno, fmt0), ap);
    va_end(ap);
}

fxIMPLEMENT_ObjArray(DestControlInfoArray, DestControlInfo)

