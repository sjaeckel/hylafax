/*	$Id: lockname.c,v 1.7 1996/08/21 01:35:32 sam Rel $ */
/*
 * Copyright (c) 1993-1996 Sam Leffler
 * Copyright (c) 1993-1996 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics, Inc.
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
/*
 * Program invoked by faxaddmodem and probemodem to
 * generate a UUCP lock filename according to SVR4
 * conventions
 */
#include "port.h"
#include <stdio.h>
#if HAS_MKDEV
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#endif

int
main(int ac, char* av[])
{
#if HAS_MKDEV
    struct stat sb;

    if (stat(av[1], &sb) >= 0 && S_ISCHR(sb.st_mode)) {
	printf("LK.%03d.%03d.%03d\n",
	    major(sb.st_dev),
	    major(sb.st_rdev),
	    minor(sb.st_rdev));
	return (0);
    } else
#endif
	return (-1);
}
