/*	$Header: /usr/people/sam/fax/util/RCS/RegEx.c++,v 1.1 1994/06/23 00:27:19 sam Exp $ */
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

/*
 * Regular expression support.
 */
#include <RegEx.h>

RegEx::RegEx(const char* pat) : _pattern(pat)
{
    init();
}

RegEx::RegEx(const char* pat, int length) : _pattern(pat, length)
{
    init();
}

RegEx::~RegEx()
{
    regfree(&c_pattern);
    delete matches;
}

void
RegEx::init()
{
    referenceCount = 0;
    compResult = regcomp(&c_pattern, _pattern, REG_EXTENDED);
    if (compResult == 0) {
	matches = new regmatch_t[c_pattern.re_nsub+1];
	execResult = REG_NOMATCH;
    } else {
	matches = NULL;
	execResult = compResult;
    }
}

int
RegEx::Find(const char* text, u_int length, u_int off)
{
    if (compResult != 0)
	return (compResult);
    /*
     * These two checks are for compatibility with the old
     * InterViews code; yech (but the DialRules logic needs it).
     */
    if (off >= length || (off != 0 && _pattern[0] == '^'))
	return (execResult = REG_NOMATCH);
    matches[0].rm_so = off;
    matches[0].rm_eo = length;
    return (execResult = regexec(&c_pattern, text,
	c_pattern.re_nsub+1, matches, REG_STARTEND) == REG_NOMATCH);
}

int
RegEx::StartOfMatch(u_int ix)
{
    if (execResult != 0)
	return (execResult);
    return (ix <= c_pattern.re_nsub ? matches[ix].rm_so : -1);
}

int
RegEx::EndOfMatch(u_int ix)
{
    if (execResult != 0)
	return (execResult);
    return (ix <= c_pattern.re_nsub ? matches[ix].rm_eo : -1);
}
