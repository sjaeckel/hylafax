/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxPoll.c++,v 1.17 1995/04/08 21:30:16 sam Rel $ */
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
#include <stdio.h>
#include "FaxServer.h"
#include "FaxRecvInfo.h"

/*
 * FAX Server Polling Protocol.
 */

/*
 * Initiate a polling receive and invoke the receiving protocol.
 */
fxBool
FaxServer::pollFaxPhaseB(const char* cig, FaxRecvInfoArray& docs, fxStr& emsg)
{
    fxBool pollOK = FALSE;
    changeState(RECEIVING);
    traceProtocol("POLL FAX: begin");
    okToRecv = TRUE;			// we request receive, so it's ok
    recvTSI = cig;			// for writing polled file
    FaxRecvInfo info;
    /*
     * Create the first file ahead of time to avoid timing
     * problems with Class 1 modems.  (Creating the file
     * after recvBegin can cause part of the first page to
     * be lost.)
     */
    TIFF* tif = setupForRecv("POLL FAX", info, docs);
    if (tif) {
	if (modem->pollBegin(cig, emsg)) {
	    pollOK = recvDocuments("POLL FAX", tif, info, docs, emsg);
	    if (!modem->recvEnd(emsg))
		traceProtocol("POLL FAX: %s (end)", (char*) emsg);
	} else
	    traceProtocol("POLL FAX: %s (begin)", (char*) emsg);
    }
    traceProtocol("POLL FAX: end");
    return (pollOK);
}
