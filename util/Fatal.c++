#ident $Header: /d/sam/flexkit/fax/util/RCS/Fatal.c++,v 1.1 91/05/23 12:52:44 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <stdio.h>
#include <stdarg.h>

void
fxFatal(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(-1);
}
#undef fmt
