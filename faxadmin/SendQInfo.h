#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/SendQInfo.h,v 1.2 91/05/23 12:13:45 sam Exp $

#ifndef _SendQInfo_
#define	_SendQInfo_

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "DialogWindow.h"

class SendQInfo : public fxDialogWindow {
private:
public:
    SendQInfo(const char* qf);
    ~SendQInfo();
};
#endif /* _SendQInfo_ */
