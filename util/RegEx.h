/*	$Header: /usr/people/sam/fax/util/RCS/RegEx.h,v 1.2 1994/06/24 04:51:32 sam Exp $ */
/*
 * Copyright (c) 1994 Sam Leffler
 * Copyright (c) 1994 Silicon Graphics, Inc.
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
#ifndef _RegEx_
#define	_RegEx_

#include "regex.h"
#include "Str.h"

/*
 * Reference-counted regular expressions;
 * for use with Ptrs and Arrays.
 */
class RegEx {
public:
    RegEx(const char* pat);
    RegEx(const char* pat, int length);
    ~RegEx();

    const char* pattern() const;

    int Find(const char* text, u_int length, u_int off = 0);
    int Find(const fxStr& s, u_int off = 0)
	{ return Find(s, s.length(), off); }
    int StartOfMatch(u_int subexp = 0);
    int EndOfMatch(u_int subexp = 0);

    void inc();
    void dec();
    u_long getReferenceCount();
protected:
    u_long	referenceCount;
private:
    int		compResult;		// regcomp result
    int		execResult;		// last regexec result
    fxStr	_pattern;		// original regex
    regex_t	c_pattern;		// compiled regex
    regmatch_t*	matches;		// subexpression matches

    void	init();
};
inline const char* RegEx::pattern() const	{ return _pattern; }
inline u_long RegEx::getReferenceCount()	{ return referenceCount; }
inline void RegEx::inc()			{ referenceCount++; }
inline void RegEx::dec()
    { if (--referenceCount <= 0) delete this; }
#endif /* _RegEx_ */
