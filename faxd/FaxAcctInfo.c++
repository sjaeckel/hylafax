/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxAcctInfo.c++,v 1.6 1995/04/08 21:30:01 sam Rel $ */
/*
 * Copyright (c) 1990-1995 Sam Leffler
 * Copyright (c) 1991-1995 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics
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
#include "FaxAcctInfo.h"
#include <sys/file.h>

#include "config.h"

extern	const char* fmtTime(time_t);

/*
 * Record a transfer in the transfer log file.
 */
fxBool
FaxAcctInfo::record(const char* cmd) const
{
    FILE* flog = ::fopen(FAX_XFERLOG, "a");
    if (flog != NULL) {
	::flock(fileno(flog), LOCK_EX);
	writeRecord(flog, cmd);
	return (::fclose(flog) == 0);
    } else
	return (FALSE);
}

void
FaxAcctInfo::writeRecord(FILE* fd, const char* cmd) const
{
    char buf[80];
    ::strftime(buf, sizeof (buf), "%D %H:%M", ::localtime(&start));
    ::fprintf(fd,
	"%s\t%s\t%s\t%s\t%s\t\"%s\"\t\"%s\"\t%d\t%s\t%d\t%s\t\"%s\"\n",
	buf, cmd, device, jobid,
	user, dest, csi, sigrate, df, npages, fmtTime(duration), status);
}
