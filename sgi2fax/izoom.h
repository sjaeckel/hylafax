#ident	$Header: /d/sam/flexkit/fax/sgi2fax/RCS/izoom.h,v 1.2 91/05/23 12:36:50 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef IZOOMDEF
#define IZOOMDEF

#define IMPULSE         1
#define BOX             2
#define TRIANGLE        3
#define QUADRATIC       4
#define MITCHELL        5

typedef struct FILTER {
    int n, totw;
    short *dat;
    short *w;
} FILTER;

typedef struct zoom {
    int (*getfunc)();
    short *abuf;
    short *bbuf;
    int anx, any;
    int bnx, bny;
    short **xmap;
    int type;
    int curay;
    int y;
    FILTER *xfilt, *yfilt;      /* stuff for fitered zoom */
    short *tbuf;
    int nrows, clamp, ay;
    short **filtrows;
    int *accrow;
} zoom;

zoom *newzoom();

#endif IZOOMDEF
