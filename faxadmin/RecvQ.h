#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/RecvQ.h,v 1.3 91/05/23 12:13:20 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _RecvQ_
#define	_RecvQ_

#include "Str.h"
#include <time.h>

class fxVisualApplication;

struct RecvQ {
    fxStr	qfile;		// associated queue file name
    u_short	npages;		// number of pages
    u_short	pagewidth;	// page width (pixels)
    float	pagelength;	// page length (mm)
    float	resolution;	// resolution (dpi)
    fxStr	number;		// phone number of fax machine
    time_t	arrival;	// arrival time

    RecvQ(const char* qf);
    ~RecvQ();
    fxBool readQFile(int fd, fxVisualApplication* app);
};

#include "Array.h"
class RecvQPtrArray : public fxArray {
public:
    fxArrayHeader(RecvQPtrArray,RecvQ*)
protected:
    virtual void createElements(void *, u_int);
    virtual int compareElements(void const*, void const*) const;
};
#endif /* _RecvQ_ */
