/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxRequest.h,v 1.59 1995/04/08 21:30:23 sam Rel $ */
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
#ifndef _FaxRequest_
#define	_FaxRequest_
/*
 * HylaFAX Job Request Structure.
 */
#include "FaxSendStatus.h"
#include "faxRequest.h"
#include "StrDict.h"
#include <time.h>
#include <stdio.h>

/*
 * This structure is passed from the queue manager
 * to the fax modem+protocol service for each job
 * to be processed.  This class also supports the
 * reading and writing of this information to an
 * external file.
 */
class FaxRequest {
private:
    fxBool isStrCmd(const fxStr& cmd, const fxStr& tag);
    fxBool isShortCmd(const fxStr& cmd, const fxStr& tag);
    fxBool isTimeCmd(const fxStr& cmd, const fxStr& tag);
    fxBool isDocCmd(const fxStr& cmd, const fxStr& tag, fxBool& fileOK);
    void checkNotifyValue(const fxStr& tag);
    fxBool checkDocument(const char* pathname);
    void error(const char* fmt, ...);

    static fxStrDict pendingDocs;

    void recordPendingDoc(const fxStr& file);
    void expungePendingDocs(const fxStr& file);
public:
    enum {
	send_fax,		// send prepared file via fax
	send_tiff,		// send tiff file via fax
	send_tiff_saved,	// saved tiff file (converted)
	send_postscript,	// send postscript file via fax
	send_postscript_saved,	// saved postscript file (convert to tiff)
	send_data,		// send untyped data file
	send_data_saved,	// send untyped data file (converted)
	send_poll,		// make fax poll request
	send_page,		// send pager message (converted)
	send_page_saved,	// send pager message
	send_uucp		// send file via uucp
    };
    enum FaxNotify {		// notification control
	no_notice = 0,		// no notifications
	when_done = 1,		// notify when send completed
	when_requeued = 2	// notify if job requeued
    };

    fxStr	qfile;		// associated queue file name
    fxStr	jobid;		// job identifier
    fxStr	groupid;	// group identifier
    FILE*	fp;		// open+locked queue file
    u_int	lineno;		// line number when reading queue file
    FaxSendStatus status;	// request status indicator
    u_short	totpages;	// total cummulative pages in documents
    u_short	npages;		// total pages sent/received
    u_short	ntries;		// # tries to send current page
    u_short	ndials;		// # consecutive failed tries to call dest
    u_short	totdials;	// total # calls to dest
    u_short	maxdials;	// max # times to dial the phone
    u_short	tottries;	// total # attempts to deliver
    u_short	maxtries;	// max # attempts to deliver (answered calls)
    u_short	pagewidth;	// desired output page width (mm)
    u_short	pagelength;	// desired output page length (mm)
    u_short	resolution;	// desired vertical resolution (lpi)
    u_short	usrpri;		// user-requested scheduling priority
    u_short	pri;		// current scheduling priority
    time_t	tts;		// time to send
    time_t	killtime;	// time to kill job
    fxStr	sender;		// sender's name
    fxStr	mailaddr;	// return mail address
    fxStr	jobtag;		// user-specified job tag
    fxStr	number;		// dialstring for fax machine
    fxStr	external;	// displayable phone number for fax machine
    FaxNotify	notify;		// email notification indicator
    fxStr	notice;		// message to send for notification
    fxStr	modem;		// outgoing modem to use
    fxStr	pagehandling;	// page analysis information
    fxStr	receiver;	// receiver's identity for cover page generation
    fxStr	company;	// receiver's company for cover page generation
    fxStr	location;	// receiver's location for cover page generation
    fxStr	cover;		// continuation cover page filename
    fxStr	client;		// identity of machine that submitted job
    fxStr	sigrate;	// negotiated signalling rate
    fxStr	df;		// negotiated data format
    fxStr	jobtype;	// job type for selecting send command
    faxRequestArray requests;	// set of requests

    FaxRequest(const fxStr& qf);
    ~FaxRequest();
    fxBool readQFile(int fd, fxBool& rejectJob);
    void writeQFile();
    u_int findRequest(FaxSendOp, u_int start = 0) const;

    void insertFax(u_int ix, const fxStr& file);
    void removeItems(u_int ix, u_int n = 1);
};
#endif /* _FaxRequest_ */
