/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxConfig.c++,v 1.11 1995/04/08 21:30:03 sam Rel $ */
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

/*
 * HylaFAX Server Configuration Base Class.
 */
#include "FaxConfig.h"
#include "Str.h"

#include <ctype.h>

#include "Sys.h"

FaxConfig::FaxConfig()
{
    lineno = 0;
    lastModTime = 0;
}
FaxConfig::FaxConfig(const FaxConfig& other)
{
    lineno = other.lineno;
    lastModTime = other.lastModTime;
}
FaxConfig::~FaxConfig() {}

void
FaxConfig::readConfig(const fxStr& filename)
{
    FILE* fd = Sys::fopen(filename, "r");
    if (fd) {
	char line[1024];
	while (::fgets(line, sizeof (line)-1, fd))
	    readConfigItem(line);
	::fclose(fd);
    }
}
void FaxConfig::resetConfig() { lineno = 0; }

fxBool
FaxConfig::updateConfig(const fxStr& filename)
{
    struct stat sb;
    if (Sys::stat(filename, sb) == 0 && sb.st_mtime > lastModTime) {
	resetConfig();
	readConfig(filename);
	lastModTime = sb.st_mtime;
	return (TRUE);
    } else
	return (FALSE);
}


fxBool
FaxConfig::findTag(const char* tag, const void* names0, u_int n, u_int& ix)
{
    const tags* names = (const tags*) names0;

    for (int i = n-1; i >= 0; i--) {
	const char* cp = names[i].name;
	if (cp[0] == tag[0] && streq(cp, tag)) {
	    ix = i;
	    return (TRUE);
	}
    }
    return (FALSE);
}

fxBool
FaxConfig::findValue(const char* value, const char* values[], u_int n, u_int& ix)
{
    for (u_int i = 0; i < n; i++) {
	const char* cp = values[i];
	if (cp[0] == value[0] && streq(cp, value)) {
	    ix = i;
	    return (TRUE);
	}
    }
    return (FALSE);
}

int
FaxConfig::getNumber(const char* s)
{
    return ((int) ::strtol(s, NULL, 0));
}

fxBool
FaxConfig::getBoolean(const char* cp)
{
    return (streq(cp, "on") || streq(cp, "yes"));
}

void
FaxConfig::readConfigItem(const char* b)
{
    char buf[2048];
    char* cp;

    lineno++;
    ::strncpy(buf, b, sizeof (buf));
    for (cp = buf; isspace(*cp); cp++)		// skip leading white space
	;
    if (*cp == '#')
	return;
    const char* tag = cp;			// start of tag
    while (*cp && *cp != ':') {			// skip to demarcating ':'
	if (isupper(*cp))
	    *cp = tolower(*cp);
	cp++;
    }
    if (*cp != ':') {
	configError("Syntax error, missing ':' in \"%s\"", b);
	return;
    }
    for (*cp++ = '\0'; isspace(*cp); cp++)	// skip white space again
	;
    const char* value;
    if (*cp == '"') {				// "..." value
	int c;
	/*
	 * Parse quoted string and deal with \ escapes.
	 */
	char* dp = ++cp;
	for (value = dp; (c = *cp) != '"'; cp++) {
	    if (c == '\0') {			// unmatched quote mark
		configError("Syntax error, missing quote mark in \"%s\"", b);
		return;
	    }
	    if (c == '\\') {
		c = *++cp;
		if (isdigit(c)) {		// \nnn octal escape
		    int v = c - '0';
		    if (isdigit(c = cp[1])) {
			cp++, v = (v << 3) + (c - '0');
			if (isdigit(c = cp[1]))
			    cp++, v = (v << 3) + (c - '0');
		    }
		    c = v;
		} else {			// \<char> escapes
		    for (const char* tp = "n\nt\tr\rb\bf\fv\013"; *tp; tp += 2)
			if (c == tp[0]) {
			    c = tp[1];
			    break;
			}
		}
	    }
	    *dp++ = c;
	}
	*dp = '\0';
    } else {					// value up to 1st non-ws
	for (value = cp; *cp && !isspace(*cp); cp++)
	    ;
	*cp = '\0';
    }
    if (!setConfigItem(tag, value))
	configTrace("Unknown configuration parameter \"%s\" ignored", tag);
}
