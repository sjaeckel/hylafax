#ident	$Header: /d/sam/flexkit/fax/sgi2fax/RCS/lum.h,v 1.2 91/05/23 12:36:51 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef LUMDEF
#define LUMDEF

#define RLUM	(0.3086)
#define GLUM	(0.6094)
#define BLUM	(0.0820)

#define _RED	(79)
#define _GREEN	(156)
#define _BLUE	(21)

#ifdef OLDWAY
#define RLUM	(0.299)
#define GLUM	(0.587)
#define BLUM	(0.114)

#define _RED	(77)
#define _GREEN	(150)
#define _BLUE	(29)
#endif

#define ILUM(r,g,b)	((_RILUM*(r)+_GILUM*(g)+_BILUM*(b))>>8)
#define LUM(r,g,b)	(RLUM*(r)+GLUM*(g)+BLUM*(b))

extern int _RILUM, _GILUM, _BILUM;

#endif LUMDEF
