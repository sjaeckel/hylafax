#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxRecvdData.c++,v 1.2 91/05/23 12:25:48 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "FaxRecvInfo.h"
#include "OrderedGlobal.h"
#include "Sequencer.h"

fxDataType fxDT_FaxRecvd;
fxINIT(FaxRecvd_init, {fxDT_FaxRecvd = (*fx_nextDataType)();}, {}, 0);

fxIMPLEMENT_Data(FaxRecvdData, FaxRecvInfo, fxDT_FaxRecvd);
