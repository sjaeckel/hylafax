/*	$Header: /usr/people/sam/fax/./faxalter/RCS/faxalter.c++,v 1.23 1995/04/08 21:28:17 sam Rel $ */
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
#include "FaxClient.h"
#include "StrArray.h"
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

class faxAlterApp : public FaxClient {
private:
    fxStr	appName;		// for error messages
    fxBool	groups;			// group or job id's
    fxStr	notify;
    fxStr	tts;
    fxStr	killtime;
    fxStr	maxdials;
    fxStr	modem;
    fxStr	priority;
    fxStrArray	jobids;

    const char* alterCmd(const char* param);
    void usage();
    void printError(const char* fmt, ...);
    void printWarning(const char* fmt, ...);
public:
    faxAlterApp();
    ~faxAlterApp();

    void initialize(int argc, char** argv);
    void open();

    void recvConf(const char* cmd, const char* tag);
    void recvEof();
    void recvError(const int err);
};
faxAlterApp::faxAlterApp() { groups = FALSE; }
faxAlterApp::~faxAlterApp() {}

void
faxAlterApp::initialize(int argc, char** argv)
{
    int c, n;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "a:h:k:m:n:P:t:DQRgpv")) != -1)
	switch (c) {
	case 'D':			// set notification to when done
	    notify = "when done";
	    break;
	case 'Q':			// no notification (quiet)
	    notify = "none";
	    break;
	case 'R':			// set notification to when requeued
	    notify = "when requeued";
	    break;
	case 'a':			// send at specified time
	    tts = optarg;
	    break;
	case 'g':			// apply to groups, not jobs
	    groups = TRUE;
	    break;
	case 'h':			// server's host
	    setHost(optarg);
	    break;
	case 'k':			// kill job at specified time
	    killtime = optarg;
	    break;
	case 'm':			// modem
	    modem = optarg;
	    break;
	case 'n':			// set notification
	    if (strcmp(optarg, "done") == 0)
		notify = "when done";
	    else if (strcmp(optarg, "requeued") == 0)
		notify = "when requeued";
	    else
		notify = optarg;
	    break;
	case 'p':			// send now (push)
	    tts = "now";
	    break;
	case 'P':			// scheduling priority
	    priority = optarg;
	    if ((u_int) atoi(priority) > 255)
		fxFatal("Invalid job priority %s;"
		    " values must be in the range [0,255]", optarg);
	    break;
	case 't':			// set max number of retries
	    n = atoi(optarg);
	    if (n < 0)
		fxFatal("Bad number of retries for -t option: %s", optarg);
	    maxdials = fxStr(n, "%d");
	    break;
	case 'v':			// trace protocol
	    setVerbose(TRUE);
	    break;
	case '?':
	    usage();
	}
    if (optind >= argc)
	usage();
    if (tts == "" && notify == "" && killtime == "" &&
      maxdials == "" && modem == "" && priority == "")
	fxFatal("No job parameters specified for alteration.");
    setupUserIdentity();
    for (; optind < argc; optind++)
	jobids.append(argv[optind]);
}

void
faxAlterApp::usage()
{
    fxFatal("usage: %s"
      " [-h server-host]"
      " [-a time]"
      " [-k time]"
      " [-m modem]"
      " [-n notify]"
      " [-P priority]"
      " [-t tries]"
      " [-p]"
      " [-g]"
      " [-DQR]"
      " jobID...", (char*) appName);
}

const char*
faxAlterApp::alterCmd(const char* param)
{
    static char cmd[80];
    ::sprintf(cmd, "alter%s%s", groups ? "Group" : "", param);
    return (cmd);
}

void
faxAlterApp::open()
{
    if (callServer()) {
	for (int i = 0; i < jobids.length(); i++) {
	    fxStr line = jobids[i] | ":" | getSenderName();
	    // do notify first 'cuz setting tts causes q rescan
	    if (notify != "")
		sendLine(alterCmd("Notify"), (char*)(line | ":" | notify));
	    if (tts != "")
		sendLine(alterCmd("TTS"), (char*)(line | ":" | tts));
	    if (killtime != "")
		sendLine(alterCmd("KillTime"), (char*)(line | ":" | killtime));
	    if (maxdials != "")
		sendLine(alterCmd("MaxDials"), (char*)(line | ":" | maxdials));
	    if (priority != "")
		sendLine(alterCmd("Priority"), (char*)(line | ":" | priority));
	    if (modem != "")
		sendLine(alterCmd("Modem"), (char*)(line | ":" | modem));
	}
	sendLine(".\n");
	startRunning();
    } else
	fxFatal("Could not call server.");
}

void
faxAlterApp::printError(const char* va_alist ...)
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
faxAlterApp::printWarning(const char* va_alist ...)
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

#define isCmd(s)	(strcasecmp(s, cmd) == 0)

void
faxAlterApp::recvConf(const char* cmd, const char* tag)
{
    if (isCmd("altered")) {
//	printf("Job %s altered.\n", tag);
    } else if (isCmd("notQueued")) {
	printf("Job %s not altered: it was not queued.\n", tag);
    } else if (isCmd("jobLocked")) {
	printf("Job %s not altered: it is being sent.\n", tag);
    } else if (isCmd("openFailed")) {
	printf("Job %s not altered: open failed; check SYSLOG.\n", tag);
    } else if (isCmd("sOwner")) {
	printf(
	"Job %s not altered: could not establish server process owner.\n",
	    tag);
    } else if (isCmd("jobOwner")) {
	printf(
	"Job %s not altered: you neither own it nor are the fax user.\n",
	    tag);
    } else if (isCmd("error")) {
	printf("%s\n", tag);
    } else {
	printf("Unknown status message \"%s:%s\"\n", cmd, tag);
    }
}

void
faxAlterApp::recvEof()
{
    stopRunning();
}

void
faxAlterApp::recvError(const int err)
{
    printf("Fatal socket read error: %s\n", strerror(err));
    stopRunning();
}

#include "Dispatcher.h"

int
main(int argc, char** argv)
{
    faxAlterApp app;
    app.initialize(argc, argv);
    app.open();
    while (app.isRunning())
	Dispatcher::instance().dispatch();
    return 0;
}
