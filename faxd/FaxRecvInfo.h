#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxRecvInfo.h,v 1.2 91/05/23 12:25:46 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FaxRecvInfo_
#define	_FaxRecvInfo_

#include "Str.h"

struct FaxRecvInfo {
    fxStr	qfile;		// file containing data
    u_short	npages;		// number of total pages
    u_short	pagewidth;	// page width (pixels)
    float	pagelength;	// page length (mm)
    float	resolution;	// resolution (dpi)
    fxStr	sender;		// sender's TSI
    float	time;		// time on the phone
};

#include "Data.h"
extern fxDataType fxDT_FaxRecvd;
fxDECLARE_Data(FaxRecvdData, FaxRecvInfo);
#endif _FaxRecvInfo_
