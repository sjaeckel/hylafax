/*	$Header: /usr/people/sam/fax/faxd/RCS/FaxRequest.h,v 1.39 1994/07/05 01:40:41 sam Exp $ */
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
#ifndef _FaxRequest_
#define	_FaxRequest_
/*
 * Fax Send/Poll Request Structure.
 */
#include "StrArray.h"
#include "FaxSendStatus.h"
#include "RegEx.h"
#include <time.h>
#include <stdio.h>

enum FaxSendOp {
    send_fax,			// send prepared file
    send_tiff,			// send tiff file
    send_tiff_saved,		// saved tiff file (converted)
    send_postscript,		// send postscript file
    send_postscript_saved,	// saved postscript file (convert to tiff)
    send_poll,			// make poll request
};
fxDECLARE_PrimArray(FaxSendOpArray, FaxSendOp);
fxDECLARE_PrimArray(DirnumArray, u_short);

/*
 * This structure is passed from the queue manager (faxServerApp)
 * to the fax modem+protocol service (FaxServer) for each send/poll
 * operation to be done.  This class also supports the read and
 * writing of this information to an external file.
 */
class FaxRequest {
public:
    enum FaxNotify {		// notification control
	no_notice = 0,		// no notifications
	when_done = 1,		// notify when send completed
	when_requeued = 2,	// notify if job requeued
    };

    fxStr	qfile;		// associated queue file name
    fxStr	jobid;		// job identifier
    FILE*	fp;		// open+locked queue file
    u_int	lineno;		// line number when reading queue file
    FaxSendStatus status;	// request status indicator
    u_short	totpages;	// total cummulative pages in documents
    u_short	npages;		// total pages sent/received
    u_short	ntries;		// # tries to send current page
    u_short	ndials;		// # consecutive failed tries to call dest
    u_short	totdials;	// total # calls to dest
    u_short	maxdials;	// max # calls to make
    u_short	pagewidth;	// desired output page width (mm)
    u_short	pagelength;	// desired output page length (mm)
    u_short	resolution;	// desired vertical resolution (lpi)
    time_t	tts;		// time to send
    time_t	killtime;	// time to kill job
    fxStr	sender;		// sender's name
    fxStr	mailaddr;	// return mail address
    fxStr	jobtag;		// user-specified job tag
    fxStr	number;		// dialstring for fax machine
    fxStr	external;	// displayable phone number for fax machine
    FaxSendOpArray ops;		// send-related ops to do
    fxStrArray	files;		// associated files to transmit or polling id's
    DirnumArray	dirnums;	// directory of next page in file to send
    FaxNotify	notify;		// email notification indicator
    fxStr	notice;		// message to send for notification
    fxStr	modem;		// outgoing modem to use
    fxStr	pagehandling;	// page analysis information
    fxStr	receiver;	// receiver's identity for cover page generation
    fxStr	company;	// receiver's company for cover page generation
    fxStr	location;	// receiver's location for cover page generation
    fxStr	cover;		// continuation cover page filename

    FaxRequest(const fxStr& qf);
    ~FaxRequest();
    fxBool readQFile(int fd);
    void writeQFile();

    static fxBool isSavedOp(const FaxSendOp&);

    void insertItem(u_int ix, const fxStr& file, u_short dir, FaxSendOp op);
    void appendItem(const fxStr& file, u_short dir, FaxSendOp op);
    void removeItems(u_int ix, u_int n = 1);
private:
    static RegEx jobidPat;

    fxBool isStrCmd(const fxStr& cmd, const fxStr& tag);
    fxBool isShortCmd(const fxStr& cmd, const fxStr& tag);
    fxBool isTimeCmd(const fxStr& cmd, const fxStr& tag);
    fxBool isDocCmd(const fxStr& cmd, const fxStr& tag, fxBool& fileOK);
    void checkNotifyValue(const fxStr& tag);
    fxBool checkDocument(const char* pathname);
    void error(const char* fmt, ...);
};
inline fxBool FaxRequest::isSavedOp(const FaxSendOp& op)
    { return (op == send_tiff_saved || op == send_postscript_saved); }
#endif /* _FaxRequest_ */
