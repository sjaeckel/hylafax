#ident $Header: /usr/people/sam/flexkit/fax/faxcomp/RCS/faxcomp.c++,v 1.3 91/08/04 13:08:26 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "VisualApplication.h"
#include "StrArray.h"
#include "promptWindow.h"
#include "config.h"
#include "FaxDB.h"
#include "ConfirmDialog.h"
#include "BusyCursor.h"
#include "XICCCM.h"

#include <osfcn.h>
#include <getopt.h>
#include <ctype.h>

class faxComposeApp : public fxVisualApplication, public BusyCursor {
private:
    fxStr	appName;		// for error messages
    fxStr	senderName;		// sender's full name (if available)
    fxStr	host;			// server hostname
    fxStrArray	docs;			// documents to send
    fxStr	dbName;			// fax machine database filename
    fxStr	sendCmd;		// send facsimile command
    fxStr	pageCount;		// total page count
    promptWindow* w;			// dialog window
    fxBool	haveCover;		// if true, have cover sheet temp file
    fxBool	haveStdin;		// if true, have stdin temp file

    static fxStr CoverSheetApp;

    fxStr tildeExpand(const fxStr& filename);
    void copyToTemporary(int fin);
    fxBool makeCoverSheet();
    void setupUserIdentity();
    void cleanupCover();
    void usage();
public:
    faxComposeApp();
    ~faxComposeApp();
    virtual const char *className() const;

    void initialize(int argc, char** argv);
    void open();

    void faxSend();
    void faxCancel();
};

fxStr faxComposeApp::CoverSheetApp("faxcover");

fxAPPINIT(faxComposeApp, 0);

static void s1(faxComposeApp* o)         { o->faxSend(); }
static void s2(faxComposeApp* o)         { o->faxCancel(); }

faxComposeApp::faxComposeApp() :
    dbName("~/.faxdb"),
    sendCmd(FAX_SENDFAX)
{
    addInput("::faxSend",	fxDT_void,	this,	(fxStubFunc) s1);
    addInput("::faxCancel",	fxDT_void,	this,	(fxStubFunc) s2);

    haveCover = FALSE;
    haveStdin = FALSE;
}

faxComposeApp::~faxComposeApp()
{
    cleanupCover();
    if (haveStdin)
	unlink(docs[0]);
}

const char* faxComposeApp::className() const { return ("faxComposeApp"); }

void
faxComposeApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    w = new promptWindow();
    w->connect("send", this, "::faxSend");
    w->connect("cancel", this, "::faxCancel");
    while ((c = getopt(argc, argv, "d:h:p:DRmln")) != -1)
	switch (c) {
	case 'h':			// server's host
	    host = optarg;
	    break;
	case 'd':
	    dbName = optarg;
	    break;
	case 'D':
	    w->setNotifyDone(TRUE);
	    break;
	case 'R':
	    w->setNotifyRequeue(TRUE);
	    break;
	case 'm':
	    w->setResolution(196.);
	    break;
	case 'l':
	    w->setResolution(98.);
	    break;
	case 'n':
	    w->setCoverPage(FALSE);
	    break;
	case 'p':
	    pageCount = optarg;
	    break;
	case '?':
	    usage();
	}
    setupUserIdentity();
    if (optind < argc) {
	for (; optind < argc; optind++)
	    docs.append(argv[optind]);
    } else {
	// read from stdin if no docs on command line
	copyToTemporary(fileno(stdin));
	haveStdin = TRUE;
    }
    w->setFaxDB(new FaxDB(tildeExpand(dbName)));
}

void
faxComposeApp::usage()
{
    fxFatal("usage: %s [-h server-host] docs...", (char*) appName);
}

void
faxComposeApp::open()
{
    add(w);
    w->open();
    fxVisualApplication::open();
    enableICCCM(fxMakeXICCCM());
}

void
faxComposeApp::setupUserIdentity()
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
	fxFatal("Can not determine your user name");
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
	fxFatal("Bad (null) user name");
}

extern "C" int mkstemp(char*);

void
faxComposeApp::copyToTemporary(int fin)
{
    fxStr templ("/usr/tmp/faxdocXXXXXX");
    int fd = mkstemp(templ);
    if (fd < 0)
	fxFatal("%s: Can not create temporary file", (char*) templ);
    int cc, total = 0;
    char buf[16*1024];
    while ((cc = read(fin, buf, sizeof (buf))) > 0) {
	if (write(fd, buf, cc) != cc) {
	    unlink(templ);
	    fxFatal("%s: write error", (char*) templ);
	}
	total += cc;
    }
    ::close(fd);
    if (total == 0) {
	unlink(templ);
	fxFatal("No input data; tranmission aborted");
    }
    docs.append(templ);
}

void
faxComposeApp::faxSend()
{
    if (!confirmRequest(this, "Send facsimile, confirm?"))
	return;
    beginBusy(w);
    if (w->getCoverPage() && !makeCoverSheet()) {
	endBusy(w);
	return;
    }
    fxStr cmd = sendCmd | " -n -d \"" | w->getFax() | "\"";
    if (host.length())
	cmd.append(" -h " | host);
    cmd.append(w->getResolution() == 98 ? " -l" : " -m");
    if (w->getNotifyDone())
	cmd.append(" -D");
    if (w->getNotifyRequeued())
	cmd.append(" -R");
    for (u_int i = 0; i < docs.length(); i++)
	cmd.append(" " | docs[i]);
    FILE* fp = popen(cmd, "r");
    if (fp) {
	char line[1024];
	fxStr info;
	while (fgets(line, sizeof (line), fp))
	    info.append(line);
	pclose(fp);
	notifyUser(this,
	    "Job successfully submitted:\n  %s", (char*) info);
    } else {
	notifyUser(this,
	    "There was a problem submitting the job!\n(%s)", (char*) cmd);
    }
    /*
     * Remove cover sheet since we may regenerate
     * a new one if the destination is changed and
     * another send is done!
     */
    cleanupCover();
    endBusy(w);
}

fxBool
faxComposeApp::makeCoverSheet()
{
    fxStr cmd(CoverSheetApp);
    cmd.append(" -f \"" | senderName | "\"");
    cmd.append(" -n \"" | w->getFax() | "\"");
    cmd.append(" -t \"" | w->getDestName() | "\"");
    cmd.append(" -l \"" | w->getLocation() | "\"");
    cmd.append(" -v \"" | w->getPhone() | "\"");
    cmd.append(" -c \"" | w->getComments() | "\"");
    if (pageCount != "")
	cmd.append(" -p " | pageCount);
    fxStr templ("/usr/tmp/faxcoverXXXXXX");
    int fd = mkstemp(templ);
    if (fd < 0) {
	notifyUser(this, "%s: Can not create temporary file", (char*) templ);
	return (FALSE);
    }
    FILE* fp = popen(cmd, "r");
    if (fp != NULL) {
	char line[1024];
	while (fgets(line, sizeof (line)-1, fp))
	    (void) write(fd, line, strlen(line));
	if (pclose(fp) == 0) {
	    ::close(fd);
	    docs.insert(templ, 0);		// cover sheet goes first
	    haveCover = TRUE;
	    return (TRUE);
	}
    }
    ::close(fd);
    unlink(templ);
    notifyUser(this, "Problem generating cover sheet!");
    return (FALSE);
}

void
faxComposeApp::faxCancel()
{
    if (confirmRequest(this, "Cancel facsimile and exit?"))
	close();
}

fxStr
faxComposeApp::tildeExpand(const fxStr& filename)
{
    fxStr path(filename);
    if (filename.length() > 1 && filename[0] == '~') {
	path.remove(0);
	char* cp = getenv("HOME");
	if (!cp || *cp == '\0') {
	    struct passwd* pwd = getpwuid(getuid());
	    if (!pwd)
		fxFatal("Can not figure out who you are.");
	    cp = pwd->pw_dir;
	}
	path.insert(cp);
    }
    return (path);
}

void
faxComposeApp::cleanupCover()
{
    if (haveCover) {
	unlink(docs[0]);
	docs.remove(0);
	haveCover = FALSE;
    }
}
