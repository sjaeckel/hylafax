/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxConfig.h,v 1.11 1995/04/08 21:30:04 sam Rel $ */
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
#ifndef _FaxConfig_
#define	_FaxConfig_
/*
 * HylaFAX Server Configuration Base Class.
 */
#include "Types.h"

class fxStr;

class FaxConfig {
private:
    u_int	lineno;			// line number while parsing
    time_t	lastModTime;		// last modification timestamp
protected:
    FaxConfig();
    FaxConfig(const FaxConfig& other);

    // generic template for matching names since we can't use a union
    typedef struct {
	const char* name;		// tag name (lowercase)
	void* FaxConfig::*p;		// pointer to member of structure
	void* def;			// default value
    } tags;

    // NB: const void* should be const tags* but gcc can't hack it
    static fxBool findTag(const char*, const void*, u_int, u_int&);
    static fxBool findValue(const char*, const char*[], u_int, u_int&);

    static int getNumber(const char*);
    static fxBool getBoolean(const char*);

    virtual fxBool setConfigItem(const char* tag, const char* value) = 0;
    virtual void configError(const char* fmt, ...) = 0;
    virtual void configTrace(const char* fmt, ...) = 0;
    u_int getConfigLineNumber() const;
public:
    virtual ~FaxConfig();

    virtual void readConfig(const fxStr& filename);
    virtual void readConfigItem(const char*);
    virtual void resetConfig();
    virtual fxBool updateConfig(const fxStr& filename);
};
inline u_int FaxConfig::getConfigLineNumber() const { return lineno; }
#endif /* _FaxConfig_ */
