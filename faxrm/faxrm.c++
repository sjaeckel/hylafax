#ident $Header: /usr/people/sam/flexkit/fax/faxrm/RCS/faxrm.c++,v 1.6 91/05/31 11:20:56 sam Exp $

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

class faxRmApp : public fxApplication {
private:
    FaxClient	client;
    fxStr	appName;		// for error messages
    fxStr	senderName;		// sender's full name (if available)
    fxStrArray	jobids;

    void setupUserIdentity();
    void usage();
public:
    faxRmApp();
    ~faxRmApp();

    void initialize(int argc, char** argv);
    void open();
    virtual const char *className() const;

    void recvConf(const char* confMessage);	// recvConf wire
    void recvEof();				// recvEof wire
    void recvError(const int err);		// recvError wire
};

static void s0(faxRmApp* o, char* cp)		{ o->recvConf(cp); }
static void s1(faxRmApp* o)			{ o->recvEof(); }
static void s2(faxRmApp* o, int er)		{ o->recvError(er); }

faxRmApp::faxRmApp()
{
    addInput("recvConf",	fxDT_CharPtr,	this, (fxStubFunc) s0);
    addInput("recvEof",		fxDT_void,	this, (fxStubFunc) s1);
    addInput("recvError",	fxDT_int,	this, (fxStubFunc) s2);

    client.connect("faxMessage",	this, "recvConf");
    client.connect("faxEof",		this, "recvEof");
    client.connect("faxError",		this, "recvError");
}

faxRmApp::~faxRmApp()
{
}

const char* faxRmApp::className() const { return ("faxRmApp"); }

void
faxRmApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "h:")) != -1)
	switch (c) {
	case 'h':			// server's host
	    client.setHost(optarg);
	    break;
	case '?':
	    usage();
	}
    if (optind >= argc) {
	usage();
    }
    setupUserIdentity();
    for (; optind < argc; optind++)
	jobids.append(argv[optind]);
}

void
faxRmApp::usage()
{
    fxFatal("usage: %s [-h server-host] jobID...", (char*) appName);
}

void
faxRmApp::setupUserIdentity()
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
faxRmApp::open()
{
    if (client.callServer() == client.Failure)
	fxFatal("Could not call server.");
    fx_theExecutive->addSelectHandler(&client);
    for (int i = 0; i < jobids.length(); i++) {
	fxStr line = jobids[i] | ":" | senderName;
	client.sendLine("remove", line);
    }
    client.sendLine(".\n");
}

#define isCmd(cmd)	(strcasecmp(confMessage, cmd) == 0)

void
faxRmApp::recvConf(const char* confMessage)
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
	printf(
	"Job %s not removed: you neither own it nor are the fax user.\n",
	    tag);
    } else if (isCmd("error")) {
	printf("%s\n", tag);
    } else {
	printf("Unknown status message \"%s:%s\"\n", confMessage, tag);
    }
}

void
faxRmApp::recvEof()
{
    (void) client.hangupServer();
    close();			// terminate app;
}

void
faxRmApp::recvError(const int err)
{
    printf("Fatal socket read error: %s\n", strerror(err));
    (void) client.hangupServer();
    close();			// terminate app;
}

fxAPPINIT(faxRmApp, 0);
