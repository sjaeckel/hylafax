/*	$Header: /usr/people/sam/fax/./faxstat/RCS/ServerStatus.h,v 1.12 1995/04/08 21:34:48 sam Rel $ */
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
#ifndef _FaxServerStatus_
#define	_FaxServerStatus_
#include "Array.h"
#include "Str.h"

struct FaxServerStatus : public fxObj {
    fxStr	number;		// server's phone number
    fxStr	status;		// server status
    fxStr	info;		// administrator info
    fxStr	host;		// host on which server resides
    fxStr	modem;		// modem used by server

    FaxServerStatus();
    FaxServerStatus(const FaxServerStatus& other);
    ~FaxServerStatus();

    int compare(const FaxServerStatus* other) const;

    fxBool parse(const char*);
    void serverInfo(const fxStr&);
    void print(FILE*);
};

fxDECLARE_ObjArray(FaxServerStatusArray, FaxServerStatus);
#endif /* _FaxServerStatus_ */
