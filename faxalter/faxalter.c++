#ident $Header: /usr/people/sam/flexkit/fax/faxalter/RCS/faxalter.c++,v 1.1 91/05/31 12:51:08 sam Exp $

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
#include "OrderedGlobal.h"
#include "Application.h"
#include "StrArray.h"
#include "config.h"

#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>

#include <stdio.h>
#include <string.h>

#include <libc.h>
#include <osfcn.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>		// XXX

    int close(int);
    int gethostname(char*, int);
}

class faxAlterApp : public fxApplication {
private:
    FaxClient	client;
    fxStr	appName;		// for error messages
    fxStr	senderName;		// sender's full name (if available)
    fxStr	notify;
    fxStr	tts;
    fxStrArray	jobids;

    void setupUserIdentity();
    void usage();
public:
    faxAlterApp();
    ~faxAlterApp();

    void initialize(int argc, char** argv);
    void open();
    virtual const char *className() const;

    void recvConf(const char* confMessage);	// recvConf wire
    void recvEof();				// recvEof wire
    void recvError(const int err);		// recvError wire
};

static void s0(faxAlterApp* o, char* cp)	{ o->recvConf(cp); }
static void s1(faxAlterApp* o)			{ o->recvEof(); }
static void s2(faxAlterApp* o, int er)		{ o->recvError(er); }

faxAlterApp::faxAlterApp()
{
    addInput("recvConf",	fxDT_CharPtr,	this, (fxStubFunc) s0);
    addInput("recvEof",		fxDT_void,	this, (fxStubFunc) s1);
    addInput("recvError",	fxDT_int,	this, (fxStubFunc) s2);

    client.connect("faxMessage",	this, "recvConf");
    client.connect("faxEof",		this, "recvEof");
    client.connect("faxError",		this, "recvError");
}

faxAlterApp::~faxAlterApp()
{
}

const char* faxAlterApp::className() const { return ("faxAlterApp"); }

void
faxAlterApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "a:h:n:DQRpv")) != -1)
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
	case 'h':			// server's host
	    client.setHost(optarg);
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
	    tts = "0";
	    break;
	case 'v':			// trace protocol
	    client.setVerbose(TRUE);
	    break;
	case '?':
	    usage();
	}
    if (optind >= argc)
	usage();
    if (tts == "" && notify == "")
	fxFatal("No job parameters specified for alteration.");
    setupUserIdentity();
    for (; optind < argc; optind++)
	jobids.append(argv[optind]);
}

void
faxAlterApp::usage()
{
    fxFatal("usage: %s [-h server-host] [-a time] [-n notify] [-p] [-DQR] jobID...", (char*) appName);
}

void
faxAlterApp::setupUserIdentity()
{
    struct passwd* pwd = NULL;
    char* name = cuserid(NULL);
    if (!name) {
	name = getlogin();
	if (name)
	    pwd = getpwnam(name);
    }
    if (!pwd)
	pwd = getpwuid(getuid());
    if (!pwd)
	fxFatal("Can not determine your user name.");
    if (pwd->pw_gecos) {
	if (pwd->pw_gecos[0] == '&') {
	    senderName = pwd->pw_gecos+1;
	    senderName.insert(pwd->pw_name);
	    if (islower(senderName[0]))
		senderName[0] = toupper(senderName[0]);
	} else
	    senderName = pwd->pw_gecos;
	senderName.resize(senderName.next(0,','));
    } else
	senderName = pwd->pw_name;
    if (senderName.length() == 0)
	fxFatal("Bad (null) user name.");
}

void
faxAlterApp::open()
{
    if (client.callServer() == client.Failure)
	fxFatal("Could not call server.");
    fx_theExecutive->addSelectHandler(&client);
    for (int i = 0; i < jobids.length(); i++) {
	fxStr line = jobids[i] | ":" | senderName;
	// do notify first 'cuz setting tts causes q rescan
	if (notify != "")
	    client.sendLine("alterNotify", (char*)(line | ":" | notify));
	if (tts != "")
	    client.sendLine("alterTTS", (char*)(line | ":" | tts));
    }
    client.sendLine(".\n");
}

#define isCmd(cmd)	(strcasecmp(confMessage, cmd) == 0)

void
faxAlterApp::recvConf(const char* confMessage)
{
    char* cp;
    if (cp = strchr(confMessage, '\n'))
	*cp = '\0';
    char* tag = strchr(confMessage, ':');
    if (!tag) {
	fprintf(stderr, "Malformed confirmation message \"%s\"", confMessage);
	client.hangupServer();
	close();			// terminate app
    }
    *tag++ = '\0';
    while (isspace(*tag))
	tag++;

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
	printf("Unknown status message \"%s:%s\"\n", confMessage, tag);
    }
}

void
faxAlterApp::recvEof()
{
    (void) client.hangupServer();
    close();			// terminate app;
}

void
faxAlterApp::recvError(const int err)
{
    printf("Fatal socket read error: %s\n", strerror(err));
    (void) client.hangupServer();
    close();			// terminate app;
}

fxAPPINIT(faxAlterApp, 0);
