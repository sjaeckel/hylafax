#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FIFOServer.c++,v 1.4 91/05/23 12:25:29 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <errno.h>
#include <osfcn.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "Exec.h"
#include "FIFOServer.h"

FIFOServer::FIFOServer(const char* fifoName, int mode, fxBool okToExist)
{
    if (mknod(fifoName, (mode & 0777) | S_IFIFO, 0) < 0)
	if (errno != EEXIST || !okToExist)
	    fxFatal("Could not create FIFO \"%s\".", fifoName);
    fd = ::open(fifoName, O_RDONLY|O_NDELAY, 0);
    assert(fd != -1);
    yoChannel = addOutput("message", fxDT_CharPtr);
    fx_theExecutive->addSelectHandler(this);
}

FIFOServer::~FIFOServer()
{
    fx_theExecutive->removeSelectHandler(this);
    ::close(fd);
}

const char* FIFOServer::className() const { return "FIFOServer"; }

void
FIFOServer::handleRead()
{
    char buf[1024];
    int n = ::read(fd, buf, sizeof (buf));
    if (n > 0) {
	buf[n--] = '\0';
	if (buf[n] == '\n')		// make echo "mumble" > FIFO ok
	    buf[n] = '\0';
	sendCharPtr(yoChannel, buf);
    }
}
