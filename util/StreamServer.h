#ident $Header: /d/sam/flexkit/fax/util/RCS/StreamServer.h,v 1.2 91/05/23 12:49:59 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _StreamServer_
#define	_StreamServer_

#ifndef _fx_Exec_
#include "Exec.h"
#endif

class StreamServer : public fxSelectHandler {
protected:
    fxOutputChannel*	yoChannel;
public:
    StreamServer(const char* serviceName, const char* protoName = 0);
    virtual ~StreamServer();
    virtual const char* className() const;
    virtual void handleRead();
};
#endif /* _StreamServer_ */
