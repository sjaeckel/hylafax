#ident	$Header: /d/sam/flexkit/fax/sgi2fax/RCS/lut.h,v 1.2 91/05/23 12:36:53 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef LUTDEF
#define LUTDEF

typedef struct lut {
    int insteps;
    int outsteps;
    int stoclut;
    unsigned short *stab;
    float *flow;
    float *fhigh;
} lut;

lut *makelut();

#endif
