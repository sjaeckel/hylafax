/*	$Header: /usr/people/sam/fax/./faxrm/RCS/faxrm.c++,v 1.25 1995/04/08 21:34:31 sam Rel $ */
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
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include "FaxClient.h"
#include "StrArray.h"
#include "config.h"

class faxRmApp : public FaxClient {
private:
    fxStr	appName;		// for error messages
    fxStrArray	jobids;
    fxStr	request;

    void usage();
    void printError(const char* fmt, ...);
    void printWarning(const char* fmt, ...);
public:
    faxRmApp();
    ~faxRmApp();

    void initialize(int argc, char** argv);
    void open();

    void recvConf(const char* cmd, const char* tag);
    void recvEof();
    void recvError(const int err);
};

faxRmApp::faxRmApp() : request("remove") {}
faxRmApp::~faxRmApp() {}

void
faxRmApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;
    fxBool groups = FALSE;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "h:gkv")) != -1)
	switch (c) {
	case 'h':			// server's host
	    setHost(optarg);
	    break;
	case 'k':
	    request = "kill";
	    break;
	case 'g':
	    groups = TRUE;
	    break;
	case 'v':
	    setVerbose(TRUE);
	    break;
	case '?':
	    usage();
	}
    if (optind >= argc)
	usage();
    setupUserIdentity();
    for (; optind < argc; optind++)
	jobids.append(argv[optind]);
    if (groups)
	request.append("Group");
}

void
faxRmApp::usage()
{
    fxFatal("usage: %s [-h server-host] [-kv] jobID...", (char*) appName);
}

void
faxRmApp::open()
{
    if (callServer()) {
	for (int i = 0; i < jobids.length(); i++) {
	    fxStr line = jobids[i] | ":" | getSenderName();
	    sendLine(request, line);
	}
	sendLine(".\n");
	startRunning();
    } else
	fxFatal("Could not call server.");
}

void
faxRmApp::printError(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: ", (char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(-1);
}
#undef fmt

void
faxRmApp::printWarning(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: Warning, ", (char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
}
#undef fmt

#define isCmd(s)	(strcasecmp(cmd, s) == 0)

void
faxRmApp::recvConf(const char* cmd, const char* tag)
{
    if (isCmd("removed")) {
	printf("Job %s removed.\n", tag);
    } else if (isCmd("notQueued")) {
	printf("Job %s not removed: it was not queued.\n", tag);
    } else if (isCmd("jobLocked")) {
	printf("Job %s not removed: it is being sent.\n", tag);
    } else if (isCmd("openFailed")) {
	printf("Job %s not removed: open failed; check SYSLOG.\n", tag);
    } else if (isCmd("unlinkFailed")) {
	printf("Job %s not removed: unlink failed; check SYSLOG.\n", tag);
    } else if (isCmd("docUnlinkFailed")) {
	printf(
	    "Document for job %s not removed: unlink failed; check SYSLOG.\n",
	    tag);
    } else if (isCmd("sOwner")) {
	printf(
	    "Job %s not removed: could not establish server process owner.\n",
	    tag);
    } else if (isCmd("jobOwner")) {
	printf("Job %s not removed: you neither own it nor are the fax user.\n",
	    tag);
    } else if (isCmd("error")) {
	printf("%s\n", tag);
    } else {
	printf("Unknown status message \"%s:%s\"\n", cmd, tag);
    }
}

void
faxRmApp::recvEof()
{
    stopRunning();
}

void
faxRmApp::recvError(const int err)
{
    printf("Fatal socket read error: %s\n", strerror(err));
    stopRunning();
}

#include "Dispatcher.h"

int
main(int argc, char** argv)
{
    faxRmApp app;
    app.initialize(argc, argv);
    app.open();
    while (app.isRunning())
	Dispatcher::instance().dispatch();
    return 0;
}
