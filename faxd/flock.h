#ident $Header: /d/sam/flexkit/fax/faxd/RCS/flock.h,v 1.2 91/05/23 12:27:00 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FLOCK_
#define	_FLOCK_
/* XXX can't include <sys/file.h> because of conflicts */
/*
 * Flock call.
 */
#define LOCK_SH        1    /* shared lock */
#define LOCK_EX        2    /* exclusive lock */
#define LOCK_NB        4    /* don't block when locking */
#define LOCK_UN        8    /* unlock */

extern "C" int flock(int, int);
#endif
