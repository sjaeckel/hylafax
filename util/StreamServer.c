#ident $Header: /d/sam/flexkit/fax/util/RCS/StreamServer.c++,v 1.3 91/05/23 12:49:57 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Exec.h"
#include "StreamServer.h"

StreamServer(const char* serviceName, const char* protoName)
{
    fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd != -1);
    struct sockadd_in sin;
    bzero(&sin, sizeof (sin));
    sin.sin_family = AF_INET;
    sp = getservbyname(serviceName, protoName);
    if (sp == NULL)
	fxFatal("Do not know \"%s\"/\"%s\" service", serviceName, protoName);
    sin.sin_port = sp->s_port;
    if (bind(fd, &sin, sizeof (sin)) == -1)
	fxFatal("Can not bind socket address");
    assert(listen(fd, 5) != -1);
    yoChannel = addOutput("yo", fxDT_ClientData);
    fxTheExecutive->addSelectHandler(this);
}

StreamServer::~StreamServer()
{
    fxTheExecutive->removeSelectHandler(this);
    ::close(fd);
}

const char* StreamServer::className() const
    { return "StreamServer"; }

void
StreamServer::handleRead()
{
    ClientData* client = new ClientData;
    struct sockaddr_in sin;
    client->sinlen = sizeof (client->sin);
    client->s = accept(fd, &client->sin, &client->sinlen);
    if (client->s != -1)
	sendData(yoChannel, client);
    else
	delete client;
}
