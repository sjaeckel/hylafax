#ident $Header: /usr/people/sam/flexkit/fax/util/RCS/FaxClient.h,v 1.5 91/06/04 20:11:05 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Types.h"
#include "Str.h"
#include "Exec.h"

class FaxClient : public fxSelectHandler {
protected:
    fxStr		host;		// server's host
    fxBool		verbose;	// print data as sent or received
    int			fdOut;		// normally same as SelectHandler.fd
    fxOutputChannel*	replyChannel;
    fxOutputChannel*	eofChannel;
    fxOutputChannel*	errorChannel;
public:
    enum faxRC {Failure = -1, Success = 0};
    FaxClient();
    FaxClient(const fxStr hostarg);
    FaxClient(const char* hostarg);
    ~FaxClient();
    virtual const char *className() const;

    // bookkeeping
    virtual void setHost(fxStr);
    virtual void setHost(char*);
    virtual fxStr getHost();
    virtual faxRC callServer();
    virtual faxRC hangupServer();
    virtual void setFds(const int in, const int out);

    void setVerbose(fxBool);
    fxBool getVerbose();

    // output
    virtual void sendData(const char* type, const char* filename);
    virtual void sendLine(const char* cmd);
    virtual void sendLine(const char* cmd, int v);
    virtual void sendLine(const char* cmd, const fxStr& s)
	{ sendLine(cmd, (char*) s); }
    virtual void sendLine(const char* cmd, const char* tag);

    // input
    virtual void handleRead();
};
