/*	$Header: /usr/people/sam/fax/./port/RCS/writev.c,v 1.4 1995/04/08 21:42:40 sam Rel $
/*
 * Copyright (c) 1994-1995 Sam Leffler
 * Copyright (c) 1994-1995 Silicon Graphics, Inc.
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
 * writev emulation
 */
#include "port.h"
#include <sys/types.h>
#include <sys/uio.h>

int
writev(int fildes, struct iovec* iov, int iovcnt)
{
    int cc = 0;

    for (; iovcnt > 0; iovcnt--, iov++) {
	unsigned len = (unsigned) iov->iov_len;
	caddr_t base = (caddr_t) iov->iov_base;
	while (len > 0) {
	    int n = write(fildes, base, len);
	    if (n < 0 && cc == 0)
		return (-1);
	    if (n <= 0)
		break;
	    len -= n, base += n;
	    cc += n;
	}
    }
    return (cc);
}
