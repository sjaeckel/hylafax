#ident	$Header: /d/sam/flexkit/fax/sgi2fax/RCS/hipass.h,v 1.2 91/05/23 12:36:45 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef HIPASSDEF
#define HIPASSDEF

typedef struct highpass {
    int active, y;
    int xsize, ysize;
    int extrapval;
    short *blurrows[3];
    short *pastrows[2];
    short *acc;
    int (*getfunc)();
} highpass;

highpass *newhp();

#endif HIPASSDEF
