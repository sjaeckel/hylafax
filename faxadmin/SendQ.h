#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/SendQ.h,v 1.4 91/05/23 12:13:38 sam Exp $

#ifndef _SendQ_
#define	_SendQ_

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "StrArray.h"
#include "BoolArray.h"
#include <time.h>

class fxVisualApplication;

struct SendQ {
    enum SendNotify {		// notification control
	no_notice,		// no notifications
	when_done,		// notify when send completed
	when_requeued		// notify if job requeued
    };

    fxStr	qfile;		// associated queue file name
    u_short	jobid;		// job identifier
    u_short	npages;		// number of pages
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
    SendNotify	notify;		// email notification indicator

    SendQ(const char* qf);
    ~SendQ();
    fxBool readQFile(int fd, fxVisualApplication* app);
};

class SendQPtrArray : public fxArray {
public:
    fxArrayHeader(SendQPtrArray,SendQ*)
protected:
    virtual void createElements(void *, u_int);
    virtual int compareElements(void const*, void const*) const;
};
#endif /* _SendQ_ */
