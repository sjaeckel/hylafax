#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FIFOServer.h,v 1.2 91/05/23 12:25:32 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FIFOServer_
#define	_FIFOServer_

#ifndef _fx_Exec_
#include "Exec.h"
#endif

class FIFOServer : public fxSelectHandler {
protected:
    fxOutputChannel*	yoChannel;
public:
    FIFOServer(const char* fifoName, int mode, fxBool okToExist = FALSE);
    virtual ~FIFOServer();
    virtual const char* className() const;
    virtual void handleRead();
};
#endif /* _FIFOServer_ */
