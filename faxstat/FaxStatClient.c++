/*	$Header: /usr/people/sam/fax/./faxstat/RCS/FaxStatClient.c++,v 1.17 1995/04/08 21:34:40 sam Rel $ */
/*
 * Copyright (c) 1990-1995 Sam Leffler
 * Copyright (c) 1991-1995 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "FaxStatClient.h"
#include "SendStatus.h"
#include "RecvStatus.h"
#include "ServerStatus.h"

#include "config.h"

// FaxStatClient public methods

int FaxStatClient::clientsDone = 0;

FaxStatClient::FaxStatClient(const char* host,
    FaxSendStatusArray& sa, FaxRecvStatusArray& ra, FaxServerStatusArray& va)
    : FaxClient(host)
    , sendStatus(sa)
    , recvStatus(ra)
    , serverStatus(va)
{
    iamDone = FALSE;
}

FaxStatClient::~FaxStatClient()
{
    if (iamDone)
	clientsDone--;		// reset so that repeated use works correctly
}

fxBool
FaxStatClient::start(fxBool debug)
{
    if (debug)
	setFds(0, 1);		// use stdin, stdout instead of socket
    else if (!callServer()) {
	printError("Could not call server on host %s", (char*) getHost());
	return (FALSE);
    }
    return (TRUE);
}

void
FaxStatClient::printError(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    ::vfprintf(stderr, fmt, ap);
    va_end(ap);
    ::fputs("\n", stderr);
}

void
FaxStatClient::printWarning(const char* fmt ...)
{
    va_list ap;
    va_start(ap, fmt);
    ::fprintf(stderr, "Warning, ");
    ::vfprintf(stderr, fmt, ap);
    va_end(ap);
    ::fputs("\n", stderr);
}

#define	isCmd(s)	(::strcasecmp(s, cmd) == 0)

void
FaxStatClient::recvConf(const char* cmd, const char* tag)
{
    if (isCmd("server")) {
	FaxServerStatus stat;
	if (stat.parse(tag)) {
	    stat.host = getHost();
	    serverStatus.append(stat);
	}
    } else if (isCmd("serverInfo")) {
	fxStr line(tag);
	fxStr modem(line.head(line.next(0,':')));
	fxStr info(line.tail(line.length() - (modem.length()+1)));
	for (u_int i = 0; i < serverStatus.length(); i++) {
	    FaxServerStatus& stat = serverStatus[i];
	    if (stat.host == getHost() &&
	      (modem == MODEM_ANY || modem == stat.modem)) {
		stat.serverInfo(info);
		break;
	    }
	}
    } else if (isCmd("jobStatus")) {
	if (!parseSendStatus(tag)) {
	    printError("Malformed statusMessage \"%s\"\n", tag);
	    goto bad;
	}
    } else if (isCmd("jobAtStatus")) {
	if (!parseSendAtStatus(tag)) {
	    printError("Malformed statusMessage \"%s\"\n", tag);
	    goto bad;
	}
    } else if (isCmd("recvJob")) {
	if (!parseRecvStatus(tag)) {
	    printError("Malformed statusMessage \"%s\"\n", tag);
	    goto bad;
	}
    } else if (isCmd("error")) {
	::printf("%s\n", tag);
    } else if (isCmd(".")) {
bad:
	handleEof(0, FALSE);
    } else {
	printError("Unknown status message \"%s:%s\"\n", cmd, tag);
    }
}

fxBool
FaxStatClient::parseSendStatus(const char* tag)
{
    FaxSendStatus stat;

    if (stat.parse(tag)) {
	stat.host = getHost();
	sendStatus.append(stat);
	return (TRUE);
    } else
	return (FALSE);
}

fxBool
FaxStatClient::parseSendAtStatus(const char* tag)
{
    FaxSendAtStatus stat;

    if (stat.parse(tag)) {
	stat.host = getHost();
	sendStatus.append(stat);
	return (TRUE);
    } else
	return (FALSE);
}

fxBool
FaxStatClient::parseRecvStatus(const char* tag)
{
    FaxRecvStatus stat;

    if (stat.parse(tag)) {
	stat.host = getHost();
	recvStatus.append(stat);
	return (TRUE);
    } else
	return (FALSE);
}

void
FaxStatClient::recvEof()
{
    handleEof(0, FALSE);
}

void
FaxStatClient::recvError(const int err)
{
    handleEof(err, TRUE);
}

// FaxStatClient private methods

void
FaxStatClient::handleEof(const int err, const fxBool isError)
{
    if (isError)
	printError("Socket read error: %s\n", strerror(err));
    (void) hangupServer();
    clientsDone++;
    iamDone = TRUE;
}

fxIMPLEMENT_PtrArray(FaxStatClientArray, FaxStatClient*);
