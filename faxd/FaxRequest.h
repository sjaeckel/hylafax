#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxRequest.h,v 1.10 91/05/23 12:25:52 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FaxRequest_
#define	_FaxRequest_

#include "StrArray.h"
#include "BoolArray.h"
#include <time.h>
#include <stdio.h>

struct FaxRequest {
    enum FaxNotify {		// notification control
	no_notice,		// no notifications
	when_done,		// notify when send completed
	when_requeued		// notify if job requeued
    };

    fxStr	qfile;		// associated queue file name
    FILE*	fp;		// open+locked queue file
    fxBool	status;		// if TRUE, request completed
    u_short	npages;		// number of total pages
    u_short	pagewidth;	// desired output page width (pixels)
    float	pagelength;	// desired output page length (mm)
    float	resolution;	// desired output resolution (dpi)
    time_t	tts;		// time to send
    fxStr	killtime;	// time to kill job
    fxStr	killjob;	// job ID from "at"
    fxStr	sendjob;	// job ID from "at"
    fxStr	sender;		// sender's name
    fxStr	mailaddr;	// return mail address
    fxStr	number;		// phone number of fax machine
    fxStrArray	files;		// array of files to transmit
    fxBoolArray formats;	// if true, postscript, otherwise g3 tiff
    FaxNotify	notify;		// email notification indicator
    fxStr	notice;		// message to send for notification

    FaxRequest(const fxStr& qf);
    ~FaxRequest();
    fxBool readQFile(int fd);
    void writeQFile();
};

#include "Data.h"
extern fxDataType fxDT_FaxRequest;
fxDECLARE_Data(FaxRequestData, FaxRequest*);
#endif /* _FaxRequest_ */
