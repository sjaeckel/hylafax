#ident $Header: /d/sam/flexkit/fax/util/RCS/Path.h,v 1.2 91/05/23 12:49:52 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _Path_
#define	_Path_

#ifndef fx_Str_
#include "Str.h"
#endif

class Path : public fxStr {
protected:
    char	sep;
    friend class PathIterator;
public:
    Path(const char *envVar, char *defPath, const char separator = ':');
    virtual ~Path();

    // search for file name and return pathname in result
    virtual fxBool findFile(const fxStr& name, fxStr& result);
};

class PathIterator {
protected:
    Path&	path;
    u_short	off;
public:
    PathIterator(Path& p) : path(p)	{ off = 0; }
    fxBool notDone()			{ return (off < path.slength); }
    void operator++();
    operator fxStr();
};
#endif _Path_
