#ident $Header: /d/sam/flexkit/fax/faxstat/RCS/faxstat.c++,v 1.7 91/05/23 12:31:03 sam Exp $

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

#include <libc.h>
#include <osfcn.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <netdb.h>		// XXX

    int close(int);
    int gethostname(char*, int);
}

class faxStatClient : public fxObj {
private:
    int			serverCount;
    fxOutputChannel*	doneChannel;

    void handleEof(const int, const fxBool);
    void initMembers();
    fxBool parseSendStatus(char* tag, fxBool isLocked);
    fxBool parseRecvStatus(char* tag);
public:
    FaxClient		client;
    int			nclients;		// XXX

    faxStatClient();
    faxStatClient(char* hostname);
    ~faxStatClient();
    virtual const char *className() const;
    fxBool start(fxBool debug);

    void recvStatus(char* statusMessage);	// recvStatus wire
    void recvEof();				// recvEof wire
    void recvError(int err);			// recvError wire
};

fxDECLARE_Ptr(faxStatClient);

fxDECLARE_ObjArray(faxStatArray, faxStatClientPtr);

class faxStatApp : public fxApplication {
private:
    faxStatArray	clients;
    fxStr		appName;	// for error messages
    fxStrArray		senderNames;	// sender's full name (if available)
    fxStrArray		jobs;
    fxBool		allJobs;
    fxBool		recvJobs;
    fxBool		debug;
    int			clientsDone;

    void setupUserIdentity();
    void addAllHosts();
public:
    faxStatApp();
    ~faxStatApp();

    void initialize(int argc, char** argv);
    void open();
    virtual const char *className() const;

    void recvDone();
};

static void s0(faxStatClient* o, char*cp)	{ o->recvStatus(cp); }
static void s1(faxStatClient* o)		{ o->recvEof(); }
static void s2(faxStatClient* o, int er)	{ o->recvError(er); }

static void s10(faxStatApp* o)			{ o->recvDone(); }

// faxStatApp public methods

faxStatApp::faxStatApp()
{
    allJobs = FALSE;
    recvJobs = TRUE;
    debug = FALSE;
    clientsDone = 0;

    addInput("recvDone",	fxDT_void,	this, (fxStubFunc) s10);
}

faxStatApp::~faxStatApp()
{
}

void
faxStatApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "h:u:arstD")) != -1)
	switch (c) {
	case 'h':			// server's host
	    if (debug)
		fxFatal("Debug incompatible with host specification.");
	    clients.append(new faxStatClient(optarg));
	    break;
	case 'r':
	    recvJobs = TRUE;
	    break;
	case 's':
	    recvJobs = FALSE;
	    break;
	case 't':
	    if (debug)
		fxFatal("Debug incompatible with host specification.");
	    addAllHosts();
	    break;
	case 'u':
	    senderNames.append(optarg);
	    break;
	case 'a':			// all jobs
	    allJobs = recvJobs = TRUE;
	    break;
	case 'D':			// debug
	    if (clients.length() > 0)
		fxFatal("Debug incompatible with host specification.");
	    debug = TRUE;
	    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	    break;
	case '?':
	    fxFatal("usage: %s"
		" [-h server-host]"
		" [-u user-name]"
		" [-ars]"
		" [jobs]",
		(char*) appName);
	}

    if (clients.length() == 0)
	clients.append(new faxStatClient);
    if (senderNames.length() == 0)
	setupUserIdentity();
    if (optind < argc) {
	// list of jobs to check on
	jobs.resize(argc-optind);
	for (u_int i = 0; optind < argc; i++, optind++)
	    jobs[i] = argv[optind];
    }
}

void
faxStatApp::open()
{
    int n = clients.length();
    for (int i = 0; i < n; i++) {
	faxStatClient* c = clients[i];
	if (c->start(debug)) {
	    c->nclients = n;
	    c->connect("done", this, "recvDone");
	    c->client.sendLine("serverStatus:\n");
	    if (jobs.length() > 0) {
		for (int j = 0; j < jobs.length(); j++)
		    c->client.sendLine("jobStatus", jobs[j]);
	    } else if (allJobs) {
		c->client.sendLine("allStatus:\n");
	    } else {
		for (int j = 0; j < senderNames.length(); j++)
		    c->client.sendLine("userStatus", senderNames[j]);
	    }
	    if (recvJobs)
		c->client.sendLine("recvStatus:\n");
	    c->client.sendLine(".\n");
	}
    }
}

const char* faxStatApp::className() const { return ("faxStatApp"); }

void
faxStatApp::recvDone()
{
    if (++clientsDone >= clients.length())
	close();				// terminate app
}

// faxStatApp private methods

void
faxStatApp::setupUserIdentity()
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

    fxStr namestr;
    if (pwd->pw_gecos) {
	if (pwd->pw_gecos[0] == '&') {
	    namestr = pwd->pw_gecos+1;
	    namestr.insert(pwd->pw_name);
	    if (islower(namestr[0]))
		namestr[0] = toupper(namestr[0]);
	} else
	    namestr = pwd->pw_gecos;
	namestr.resize(namestr.next(0,','));
    } else
	namestr = pwd->pw_name;
    if (namestr.length() == 0)
	fxFatal("Bad (null) user name.");
    senderNames.append(namestr);
}

void
faxStatApp::addAllHosts()
{
    char* cp = getenv("FAXSERVER");
    fxBool foundEnv = (cp != 0);
    if (foundEnv)
	clients.append(new faxStatClient(cp));

    if (::chdir(FAX_SPOOLDIR) < 0) {
nofiles:
	if (!foundEnv)
	    fxFatal("No FAXSERVER environment variable and no spool directory:\n"
		"What hosts do you want to query?"
	    );
	else
	    return;
    }
    DIR* dirp = opendir(FAX_ETCDIR);
    if (!dirp)
	goto nofiles;
    for (dirent* dp = readdir(dirp); dp; dp = readdir(dirp)) {
	if (strncmp(dp->d_name, FAX_REMOTEPREF, strlen(FAX_REMOTEPREF))
	== 0) {
	    cp = strchr(dp->d_name, '.');
	    if (cp && *++cp)
		clients.append(new faxStatClient(cp));
	} else
	if (!foundEnv
	&&  strncmp(dp->d_name, FAX_CONFIGPREF, strlen(FAX_CONFIGPREF))
	== 0) {
	    // XXX should probably use something in the gethostbyname
	    // family to decide whether FAXSERVER == localhost
	    // and append localhost if there are config files and
	    // FAXSERVER != localhost.
	    clients.append(new faxStatClient("localhost"));
	    foundEnv = TRUE;
	}
    }
    closedir(dirp);
}

// faxStatClient public methods

faxStatClient::faxStatClient()
{
    initMembers();
}

faxStatClient::faxStatClient(char* hostname) : client(hostname)
{
    initMembers();
}

faxStatClient::~faxStatClient()
{
}

const char* faxStatClient::className() const { return ("faxStatClient"); }

fxBool
faxStatClient::start(fxBool debug)
{
    if (debug)
	client.setFds(0, 1);	// use stdin, stdout instead of socket
    else if (client.callServer() == client.Failure) {
	fprintf(stderr,
	    "Could not call server on host %s", (char*) client.getHost());
	return FALSE;
    }
    fx_theExecutive->addSelectHandler(&client);
    return TRUE;
}

#define	isCmd(cmd)	(strcasecmp(cp, cmd) == 0)

#define nextTag(what)						\
    tag = strchr(cp, ':');					\
    if (!tag) { what; }						\
    *tag++ = '\0';						\
    while (isspace(*tag))					\
	tag++;

void
faxStatClient::recvStatus(char* statusMessage)
{
    char* cp;
    char* tag;

    if (cp = strchr(statusMessage, '\n'))
	*cp = '\0';
    cp = statusMessage;
    if (isCmd(".")) {
bad:
	(void) client.hangupServer();
	sendVoid(doneChannel);
	return;
    }
    nextTag(fprintf(stderr, "Malformed statusMessage \"%s\"\n", cp); goto bad);

    if (isCmd("server")) {
	serverCount++;
	printf("Server active on host \"%s\" for %s\n",
	    (char*) client.getHost(), tag);
    } else if (isCmd("jobStatus")) {
	if (!parseSendStatus(tag, FALSE)) {
	    fprintf(stderr, "Malformed statusMessage \"%s\"\n", tag);
	    goto bad;
	}
    } else if (isCmd("jobLocked")) {
	if (!parseSendStatus(tag, TRUE)) {
	    fprintf(stderr, "Malformed statusMessage \"%s\"\n", tag);
	    goto bad;
	}
    } else if (isCmd("recvJob")) {
	if (!parseRecvStatus(tag)) {
	    fprintf(stderr, "Malformed statusMessage \"%s\"\n", tag);
	    goto bad;
	}
    } else if (isCmd("error")) {
	printf("%s\n", tag);
    } else {
	printf("Unknown status message \"%s:%s\"\n", statusMessage, tag);
    }
}

fxBool
faxStatClient::parseSendStatus(char* tag, fxBool isLocked)
{
    char* cp;
    char* jobname;
    char* sender;
    time_t tts;

    jobname = cp = tag; nextTag(return FALSE);
    sender = cp = tag; nextTag(return FALSE);
    cp = tag; nextTag(return FALSE);
    tts = atoi(cp);
    printf("Job %s for %s to %s ", jobname, sender, tag);
    if (nclients > 1)
	printf("(on %s) ", (char*) client.getHost());
    if (!isLocked) {
	printf("is queued ");
	char buf[80];
	if (tts != 0)
	    cftime(buf, "%D at %R", &tts);
	else
	    strcpy(buf, "immediately");
	printf("(send %s)\n", buf);
    } else
	printf("is being processed\n");
    return TRUE;
}

fxBool
faxStatClient::parseRecvStatus(char* tag)
{
    int beingReceived, pageWidth, pageLength, npages;
    float resolution;
    time_t arrival;

    if (sscanf(tag, "%d:%d:%d:%f:%d:%d",
      &beingReceived, &pageWidth, &pageLength, &resolution,
      &npages, &arrival) != 6)
	return (FALSE);
    char* cp = strchr(tag, '\0');
    while (cp > tag && *cp != ':')
	cp--;
    printf("%sFax from %s", beingReceived ? "*" : "", cp+1);
    if (!beingReceived) {
	char buf[80];
	cftime(buf, " received %D at %R", &arrival);
	printf("%s", buf);
    }
    if (nclients > 1)
	printf(" on %s", (char*) client.getHost());
    printf(", %d", npages);
    if (resolution == 0)
	resolution = 98;
    float mm = pageLength / resolution;
    if ((1720 <= pageWidth && pageWidth <= 1800) &&
      (280 <= mm && mm < 300))
	printf(" A4");
    printf(" pages");
    if (resolution > 150)
	printf(" (fine quality)");
    printf("\n");
    return (TRUE);
}
#undef nextTag

void
faxStatClient::recvEof()
{
    handleEof(0, FALSE);
}

void
faxStatClient::recvError(int err)
{
    handleEof(err, TRUE);
}

// faxStatClient private methods

void
faxStatClient::initMembers()
{
    serverCount = 0;

    doneChannel = addOutput("done", fxDT_void);

    addInput("recvStatus",	fxDT_CharPtr,	this, (fxStubFunc) s0);
    addInput("recvEof",		fxDT_void,	this, (fxStubFunc) s1);
    addInput("recvError",	fxDT_int,	this, (fxStubFunc) s2);

    client.connect("faxMessage",	this, "recvStatus");
    client.connect("faxEof",		this, "recvEof");
    client.connect("faxError",		this, "recvError");
}

void
faxStatClient::handleEof(const int err, const fxBool isError)
{
    if (serverCount == 0)
	printf("no servers active on %s\n", (char*) client.getHost());
    if (isError)
	printf("socket read error: %s\n", strerror(err));
    (void) client.hangupServer();
    sendVoid(doneChannel);
}

// faxStatApp creation

fxIMPLEMENT_ObjArray(faxStatArray, faxStatClientPtr);

fxAPPINIT(faxStatApp, 0);
