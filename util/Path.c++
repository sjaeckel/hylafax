#ident $Header: /d/sam/flexkit/fax/util/RCS/Path.c++,v 1.2 91/05/23 12:49:51 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Path.h"
#include <osfcn.h>

Path::Path(const char *envVar, char *defPath, const char separator) :
    fxStr(defPath ? defPath : "")
{
    char* cp = getenv(envVar);
    if (cp && *cp != '\0')
	fxStr::operator=(cp);
    sep = separator;
}
Path::~Path() {}

fxBool
Path::findFile(const fxStr& name, fxStr& result)
{
    if (name[0] == '/' && access(name, 0) == 0) {
	result = name;
	return (1);
    }
    for (PathIterator it(*this); it.notDone(); it++) {
	char* pathname = it | "/" | name;
	if (access(pathname, 0) == 0) {
	    result = pathname;
	    return (TRUE);
	}
    }
    return (FALSE);
}

void PathIterator::operator++()
{
    off = path.next(off, path.sep)+1;
}

PathIterator::operator fxStr()
{
    return (path.extract(off, path.next(off, path.sep) - off));
}
