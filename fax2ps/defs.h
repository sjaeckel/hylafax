/* $Header: /usr/people/sam/tiff/contrib/fax2ps/RCS/defs.h,v 1.18 1995/03/16 18:56:30 sam Exp $ */

/*
 * Copyright (c) 1991, 1992, 1993, 1994 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */

#ifndef _DEFS_
#define	_DEFS_
#include "tiffiop.h"

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif

#include "tif_fax3.h"

typedef struct {
    long	row;
    int		cc;
    u_char*	buf;
    u_char*	bp;
    u_char*	scanline;
    u_char	pass;
    u_char	is2d;
    u_short	options;
    Fax3BaseState b;
} Fax3DecodeState;
extern	Fax3DecodeState fax;

#if USE_PROTOTYPES
extern	int FaxPreDecode(TIFF* tif);
extern	int Fax3DecodeRow(TIFF* tif, int npels);
extern	int Fax4DecodeRow(TIFF* tif, int npels);
#else
extern	int FaxPreDecode();
extern	int Fax3DecodeRow();
extern	int Fax4DecodeRow();
#endif
#endif	/* _DEFS_ */
