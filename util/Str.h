/*	$Id: Str.h,v 1.20 1996/09/25 17:22:27 sam Rel $ */
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
#ifndef _Str_
#define	_Str_

#include "stdlib.h"
#include "stdarg.h"
#include "Obj.h"

// Temporary strings are generated by the concatenation operators.
// They are designed to avoid calls to malloc, and as such are not
// meant to be used directly.
class fxStr;

class fxTempStr {
public:
    fxTempStr(fxTempStr const &other);
    ~fxTempStr();

    // C++ uses these operators to perform the first concatenation
    friend fxTempStr operator|(fxStr const&, fxStr const&);
    friend fxTempStr operator|(fxStr const&, char const*);
    friend fxTempStr operator|(char const*, fxStr const&);

    // Susequent concatenations use these operators.  These operators
    // just append the argument data to the temporary, avoiding malloc
    // when possible.
    // XXX these aren't really const, although they are declared as such
    // XXX to avoid warnings. In a land of very advanced compilers, this
    // XXX strategy may backfire.
    friend fxTempStr& operator|(const fxTempStr&, fxStr const& b);
    friend fxTempStr& operator|(const fxTempStr&, char const* b);

    operator char*() const;
    operator int() const;
    operator float() const;
    operator double() const;

    u_int length() const;
protected:
    char	indata[100];		// inline data, avoiding malloc
    char*	data;			// points at indata or heap
    u_int	slength;		// same rules as fxStr::slength

    friend class fxStr;

    fxTempStr(char const *, u_int, char const *, u_int);
    fxTempStr& concat(char const* b, u_int bl);
};

inline fxTempStr::operator char*() const	{ return data; }
inline fxTempStr::operator int() const		{ return atoi(data); }
inline fxTempStr::operator float() const	{ return float(atof(data)); }
inline fxTempStr::operator double() const	{ return double(atof(data)); }
inline u_int fxTempStr::length() const		{ return slength - 1; }

//----------------------------------------------------------------------

class fxStr {
    friend class fxTempStr;
public:
    fxStr(u_int l=0);
    fxStr(char const *s);
    fxStr(char const *s, u_int len);
    fxStr(fxStr const&);
    fxStr(int, char const* format);
    fxStr(long, char const* format);
    fxStr(float, char const* format);
    fxStr(double, char const* format);
    fxStr(const fxTempStr&);
    ~fxStr();

    static fxStr format(const char* fmt ...);	// sprintf sort of
    static fxStr vformat(const char* fmt, va_list ap);	// vsprintf sort of
    static fxStr null;				// null string for general use
    /////////////////////////////////////////////////////
    u_long hash() const;

    operator char*()
	{ return data; }
    operator const char*() const
	{ return data; }
    operator int() const
	{ return atoi(data); }
    operator float() const
	{ return float(atof(data)); }
    operator double() const
	{ return double(atof(data)); }

    u_int length() const { return slength-1; }

    char& operator[](u_int i) const
    {   fxAssert(i<slength-1,"Invalid Str[] index");
	return data[i]; }
    char& operator[](int i) const
    {   fxAssert((u_int)(i)<slength-1,"Invalid Str[] index");
	return data[i]; }

    void operator=(const fxTempStr& s);
    void operator=(fxStr const& s);
    void operator=(char const *s);

    /////////////////////////////////////////////////////
    // Comparison
    friend fxBool operator==(fxStr const&, fxStr const&);
    friend fxBool operator==(fxStr const&, char const*);
    friend fxBool operator==(char const*, fxStr const&);

    friend fxBool operator!=(fxStr const&, fxStr const&);
    friend fxBool operator!=(fxStr const&, char const*);
    friend fxBool operator!=(char const*, fxStr const&);

    friend fxBool operator>=(fxStr const&, fxStr const&);
    friend fxBool operator>=(fxStr const&, char const*);
    friend fxBool operator>=(char const*, fxStr const&);

    friend fxBool operator<=(fxStr const&, fxStr const&);
    friend fxBool operator<=(fxStr const&, char const*);
    friend fxBool operator<=(char const*, fxStr const&);

    friend fxBool operator>(fxStr const&, fxStr const&);
    friend fxBool operator>(fxStr const&, char const*);
    friend fxBool operator>(char const*, fxStr const&);

    friend fxBool operator<(fxStr const&, fxStr const&);
    friend fxBool operator<(fxStr const&, char const*);
    friend fxBool operator<(char const*, fxStr const&);

    int compare(fxStr const *a) const { return ::compare(*this, *a); }
    friend int compare(fxStr const&, fxStr const&);
    friend int compare(fxStr const&, char const*);
    friend int compare(char const*, fxStr const&);

    /////////////////////////////////////////////////////
    // Concatenation
    friend fxTempStr& operator|(const fxTempStr&, fxStr const&);
    friend fxTempStr& operator|(const fxTempStr&, char const*);

    friend fxTempStr operator|(fxStr const&, fxStr const&);
    friend fxTempStr operator|(fxStr const&, char const*);
    friend fxTempStr operator|(char const*, fxStr const&);

    /////////////////////////////////////////////////////
    // Misc
    fxStr copy() const;
    fxStr extract(u_int start,u_int len) const;
    fxStr cut(u_int start,u_int len);
    fxStr head(u_int) const;
    fxStr tail(u_int) const;
    void lowercase(u_int posn=0, u_int len=0);
    void raisecase(u_int posn=0, u_int len=0);

    void remove(u_int posn,u_int len=1);

    void resize(u_int len, fxBool reallocate = FALSE);
    void setMaxLength(u_int maxlen);

    void append(char a);
    void append(char const *s, u_int len=0);
    void append(const fxTempStr& s)
	{ append((const char*)s, s.slength-1); }
    void append(fxStr const& s)
	{ append((const char*)s, s.slength-1); }

    void insert(char a, u_int posn=0);
    void insert(char const *, u_int posn=0, u_int len=0);
    void insert(const fxTempStr& s, u_int posn=0)
	{ insert((const char*)s, posn, s.slength-1); }
    void insert(fxStr const& s, u_int posn=0)
	{ insert((const char*)s, posn, s.slength-1); }

    /////////////////////////////////////////////////////
    // Parsing
    u_int next(u_int posn, char delimiter) const;
    u_int next(u_int posn, char const *delimiters, u_int len=0) const;
    u_int next(u_int posn, fxStr const& delimiters) const
	{ return next(posn, (const char*)delimiters, delimiters.slength-1); }
			    
    u_int nextR(u_int posn, char delimiter) const;
    u_int nextR(u_int posn, char const*, u_int len=0) const;
    u_int nextR(u_int posn, fxStr const& delimiters) const
	{ return nextR(posn, (const char*)delimiters, delimiters.slength-1); }

    u_int find(u_int posn, char const* str, u_int len=0) const;
    u_int find(u_int posn, fxStr const& str) const
	{ return find(posn, str, str.slength-1); }

    u_int findR(u_int posn, char const* str, u_int len=0) const;
    u_int findR(u_int posn, fxStr const& str) const
	{ return findR(posn, str, str.slength-1); }

    u_int skip(u_int posn, char a) const; 
    u_int skip(u_int posn, char const *, u_int len=0) const;
    u_int skip(u_int posn, fxStr const& delimiters) const
	{ return skip(posn, (const char*)delimiters, delimiters.slength-1); }

    u_int skipR(u_int posn, char a) const;
    u_int skipR(u_int posn, char const *, u_int len=0) const;
    u_int skipR(u_int posn, fxStr const& delimiters) const
	{ return skipR(posn, (const char*)delimiters, delimiters.slength-1); }

    fxStr token(u_int & posn, char delimiter) const;
    fxStr token(u_int & posn, char const * delimiters,
	u_int delimiters_len = 0) const;
    fxStr token(u_int & posn, fxStr const & delimiters) const
	{ return token(posn, delimiters.data, delimiters.slength-1); }

    fxStr tokenR(u_int & posn, char delimiter) const;
    fxStr tokenR(u_int & posn, char const * delimiters,
	u_int delimiters_len = 0) const;
    fxStr tokenR(u_int & posn, fxStr const & delimiters) const
	{ return tokenR(posn, delimiters.data, delimiters.slength-1); }

protected:
    // slength is one greater than the true length of the data.
    // This is because the data is null-terminated. However, the
    // data may contain nulls; they will be ignored. This is to
    // provide compatibility with ordinary C-style strings, and
    // with arbitrary data. slength is always positive.
    u_int slength;

    // data points to the actual data. It is always a valid pointer.
    char * data; 

    // zero-length string support
    // resizeInternal doesn't update slength or handle null termination
    static char emptyString;
    void resizeInternal(u_int);

    int findEndBuffer(const char *, u_int buflen) const;
    int findBuffer(const char *buf, u_int buflen) const;
    void bracketBuffer(const char *, u_int buflen, int &, int &) const;
};
#endif /* _Str_ */
