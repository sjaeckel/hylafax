/*	$Header: /usr/people/sam/fax/faxstat/RCS/ServerStatus.c++,v 1.7 1994/06/06 21:35:57 sam Exp $ */
/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
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
#include "ServerStatus.h"

FaxServerStatus::FaxServerStatus() { }
FaxServerStatus::~FaxServerStatus() { }

int
FaxServerStatus::compare(const FaxServerStatus* other) const
{
    return number.compare(&other->number);
}

fxBool
FaxServerStatus::parse(const char* tag)
{
    const char* cp = strchr(tag, ':');
    if (cp) {
	number.append(tag, cp-tag);
	const char* tp = strchr(++cp, ':');
	if (tp) {
	    modem.append(cp, tp-cp);
	    status = tp+1;
	} else {
	    modem = "";
	    status = cp;
	}
    } else {
	number = tag;
	modem = "";
	status = "Running";
    }
    return (TRUE);
}

void
FaxServerStatus::serverInfo(const fxStr& s)
{
    info.append(s | "\n");
}

void
FaxServerStatus::print(FILE* fp)
{
    if (modem != "")
	fprintf(fp, "Server on %s:%s for %s: %s.\n",
	    (char*) host, (char*) modem, (char*) number, (char*) status);
    else
	fprintf(fp, "Server on \"%s\" for %s: %s.\n",
	    (char*) host, (char*) number, (char*) status);
    if (info != "")
	fprintf(fp, "%s", (char*) info);
}

fxIMPLEMENT_ObjArray(FaxServerStatusArray, FaxServerStatus);
