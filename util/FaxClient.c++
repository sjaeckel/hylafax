#ident $Header: /usr/people/sam/flexkit/fax/util/RCS/FaxClient.c++,v 1.9 91/06/04 20:10:42 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "FaxClient.h"

#include <libc.h>
#include <osfcn.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#ifndef CYPRESS_XGL
extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

    int socket(int, int, int);
    int close(int);
    int gethostname(char*, int);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

FaxClient::FaxClient() : host("")
{
    fd = -1;
    verbose = FALSE;
    replyChannel = addOutput("faxMessage", fxDT_CharPtr);
    eofChannel = addOutput("faxEof", fxDT_void);
    errorChannel = addOutput("faxError", fxDT_int);
}

FaxClient::FaxClient(const fxStr hostarg) : host(hostarg)
{
    fd = -1;
    verbose = FALSE;
    replyChannel = addOutput("faxMessage", fxDT_CharPtr);
    eofChannel = addOutput("faxEof", fxDT_void);
    errorChannel = addOutput("faxError", fxDT_int);
}

FaxClient::FaxClient(const char* hostarg) : host(hostarg)
{
    fd = -1;
    verbose = FALSE;
    replyChannel = addOutput("faxMessage", fxDT_CharPtr);
    eofChannel = addOutput("faxEof", fxDT_void);
    errorChannel = addOutput("faxError", fxDT_int);
}

FaxClient::~FaxClient()
{
    if (fd > -1)
	::close(fd);
}

const char*
FaxClient::className() const { return ("FaxClient"); }

void
FaxClient::setHost(fxStr hostarg)
{
    host = hostarg;
}

void
FaxClient::setHost(char* hostarg)
{
    host = hostarg;
}

fxStr
FaxClient::getHost()
{
    return host;
}

void
FaxClient::setVerbose(fxBool b)
{
    verbose = b;
}

fxBool
FaxClient::getVerbose()
{
    return verbose;
}

faxRC
FaxClient::callServer()
{
    if (host.length() == 0) {		// if host not specified by -h
	char* cp = getenv("FAXSERVER");
	if (cp && *cp != '\0')
	    host = cp;
	if (host.length() == 0)
	    host = "localhost";
    }
    struct hostent* hp = gethostbyname(host);
    if (!hp)
	fxFatal("%s: Unknown host.", (char*) host);
    fd = ::socket(hp->h_addrtype, SOCK_STREAM, 0);
    if (fd < 0)
	fxFatal("Can not create socket to connect to server.");
    struct sockaddr_in sin;
    bzero(&sin, sizeof (sin));
    sin.sin_family = hp->h_addrtype;
    struct servent* sp = getservbyname("fax", "tcp");
    if (!sp)
	fxFatal("Can not find port for \"fax\" service.");
    sin.sin_port = sp->s_port;
    for (char** cpp = hp->h_addr_list; *cpp; cpp++) {
	bcopy(*cpp, &sin.sin_addr, hp->h_length);
	if (::connect(fd, (struct sockaddr*) &sin, sizeof (sin)) >= 0) {
	    fdOut = fd;
	    return Success;
	}
    }
    fxFatal("Can not reach \"fax\" service at host \"%s\".", (char*) host);
    return Failure;
}

faxRC
FaxClient::hangupServer()
{
    fd_set readers = fx_theExecutive->getReadSet();
    if (FD_ISSET(fd, &readers))
	fx_theExecutive->removeSelectHandler(this);
    if (::close(fd) == -1)
	return Failure;
    else
	return Success;
}

void
FaxClient::setFds(const int in, const int out)
{
    fd = in;
    fdOut = out;
}

void
FaxClient::sendLine(const char* cmd, int v)
{
    char num[20];
    sprintf(num, "%d", v);
    sendLine(cmd, num);
}

void
FaxClient::sendLine(const char* cmd, const char* tag)
{
    char line[2048];
    sprintf(line, "%s:%s\n", cmd, tag);
    sendLine(line);
}

void
FaxClient::sendLine(const char* line)
{
    if (verbose)
	printf("-> %s", line);
    u_int l = strlen(line);
    if (write(fdOut, line, l) != l)
	fxFatal("Server write error; line was \"%s\".", line);
}

void
FaxClient::sendData(const char* type, const char* filename)
{
    int tempfd = ::open(filename, O_RDONLY);
    if (tempfd < 0)
	fxFatal("%s: Can not open.", filename);
    struct stat sb;
    fstat(tempfd, &sb);
    int cc = (int) sb.st_size;
    if (verbose)
	printf("SEND \"%s\" (%s:%d bytes)\n", (char*) filename, type, cc);
    sendLine(type, cc);
    while (cc > 0) {
	char buf[4096];
	int n = fxmin((u_int) cc, sizeof (buf));
	if (read(tempfd, buf, n) != n)
	    fxFatal("Protocol botch (data read.");
	if (write(fdOut, buf, n) != n)
	    fxFatal("Protocol botch (data write).");
	cc -= n;
    }
    ::close(tempfd);
}

void
FaxClient::handleRead()
{
    char buf[1024];
    int n = ::read(fd, buf, sizeof (buf) - 1);
    if (n > 0) {
	buf[n--] = '\0';
	for (char *bp = buf; *bp; ) {
	    char *cp = strchr(bp, '\n');
	    if (cp)
		*cp++ = '\0';
	    else
		cp = strchr(bp, '\0');
	    if (verbose)
		printf("<- %s\n", bp);
	    sendCharPtr(replyChannel, bp);
	    bp = cp;
	}
    } else if (n == 0) {
	if (verbose)
	    printf("<- EOF\n");
	sendVoid(eofChannel);
    } else {
	if (verbose)
	    printf("<- ERROR (errno = %d)\n", errno);
	sendInt(errorChannel, errno);
    }
}
