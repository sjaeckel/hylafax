#ident $Header: /usr/people/sam/flexkit/fax/faxcover/RCS/faxcover.c++,v 1.7 91/06/04 16:52:03 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "OrderedGlobal.h"
#include "Application.h"
#include "StrArray.h"
#include "FaxDB.h"
#include "config.h"

#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <libc.h>
#include <time.h>
#include <osfcn.h>
#include <pwd.h>
#include <sys/file.h>

class faxCoverApp : public fxApplication {
private:
    fxStr	appName;		// for error messages
    fxStr	cover;			// prototype cover sheet
    fxStr	filterDir;		// filter programs directory
    FaxDB*	db;			// fax machine database
    fxStr	destName;
    fxStr	destNumber;
    fxStr	comments;
    fxStr	sender;
    fxStr	voiceNumber;
    fxStr	location;
    fxStr	pageCount;

    static fxStr dbName;

    fxStr tildeExpand(const fxStr& filename);
    void coverDef(const char* tag, const char* value);
    void makeCoverSheet();
    void usage();
    void printError(const char* va_alist ...);
public:
    faxCoverApp();
    ~faxCoverApp();

    void initialize(int argc, char** argv);
    void open();
    virtual const char *className() const;
};

fxAPPINIT(faxCoverApp,0);

fxStr faxCoverApp::dbName("~/.faxdb");

faxCoverApp::faxCoverApp() :
    filterDir(FAX_FILTERDIR),
    cover(FAX_COVER)
{
    db = 0;
}

faxCoverApp::~faxCoverApp()
{
}

const char* faxCoverApp::className() const { return "faxCoverApp"; }

void
faxCoverApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "n:t:f:c:p:l:v:")) != -1)
	switch (c) {
	case 'n':			// fax number
	    destNumber = optarg;
	    break;
	case 't':			// to identity
	    destName = optarg;
	    break;
	case 'f':			// from identity
	    sender = optarg;
	    break;
	case 'c':			// comments
	    comments = optarg;
	    break;
	case 'p':			// page count
	    pageCount = optarg;
	    break;
	case 'l':			// to's location
	    location = optarg;
	    break;
	case 'v':			// to's voice phone number
	    voiceNumber = optarg;
	    break;
	case '?':
	    usage();
	}
    if (sender == "" || destNumber == "")
	usage();
}

void
faxCoverApp::open()
{
    if (!db)
	db = new FaxDB(tildeExpand(dbName));
    makeCoverSheet();
    exit(0);
}

void
faxCoverApp::usage()
{
    fxFatal("usage: %s"
	" [-t to]"
	" [-c comments]"
	" [-p #pages]"
	" [-l to-location]"
	" [-v to-voice-number]"
	" -f from"
	" -n fax-number"
	, (char*) appName);
}

void
faxCoverApp::makeCoverSheet()
{
    int fd;
    if (cover.length() > 0 && cover[0] != '/') {
	fd = ::open(tildeExpand("~/" | cover), O_RDONLY);
	if (fd < 0)
	    fd = ::open(filterDir | "/" | cover, O_RDONLY);
    } else
	fd = ::open(cover, O_RDONLY);
    if (fd < 0) {
	printError( "Could not locate prototype cover sheet \"%s\";"
	    " no cover sheet sent", (char*) cover);
	return;
    }
    FaxDBRecord* rec;
    printf("%%!PS-Adobe-2.0 EPSF-2.0\n");
    printf("%%%%Creator: faxcover\n");
    printf("%%%%Title: FlexFAX Cover Sheet\n");
    time_t t = time(0);
    printf("%%%%CreationDate: %s", ctime(&t));
    printf("%%%%Origin: 0 0\n");
    printf("%%%%BoundingBox: 0 0 %d %d\n", 11*72, (int)(8.5*72));
    printf("%%%%Pages: 1 +1\n");
    printf("%%%%EndComments\n");
    printf("100 dict begin\n");
    if (destName != "") {
	rec = db->find(destName);
	coverDef("to", rec->find(FaxDB::nameKey));
	coverDef("to-company", rec->find("Company"));
	if (location == "")
	    coverDef("to-location", rec->find("Location"));
	else
	    coverDef("to-location", location);
	if (voiceNumber == "") {
	    voiceNumber = rec->find("Voice-Number");
	    if (voiceNumber != "") {
		fxStr areaCode(rec->find("Area-Code"));
		if (areaCode != "")
		    voiceNumber.insert("1-" | areaCode | "-");
	    }
	}
    } else {
	coverDef("to", "<unknown>");
	coverDef("to-company", "");
	coverDef("to-location", location);
    }
    coverDef("to-voice-number", voiceNumber);
    coverDef("to-fax-number", destNumber);
    coverDef("comments", comments);
    coverDef("page-count", pageCount);
    coverDef("from", sender);
    rec = db->find(sender);
    if (rec) {
	coverDef("from-company", rec->find("Company"));
	coverDef("from-location", rec->find("Location"));
	fxStr areaCode(rec->find("Area-Code"));
	fxStr number(rec->find(FaxDB::numberKey));
	if (number != "")
	    if (areaCode != "")
		number.insert("1-" | areaCode | "-");
	coverDef("from-fax-number", number);
	number = rec->find("Voice-Number");
	if (number != "")
	    if (areaCode != "")
		number.insert("1-" | areaCode | "-");
	coverDef("from-voice-number", number);
    } else {
	coverDef("from-company", "");
	coverDef("from-location", "");
	coverDef("from-fax-number", "");
	coverDef("from-voice-number", "");
    }
    printf("%%%%EndProlog\n");
    printf("%%%%Page: 1 1\n");
    // copy prototype cover page
    char buf[16*1024];
    int n;
    while ((n = read(fd, buf, sizeof (buf))) > 0) 
	fwrite(buf, n, 1, stdout);
    ::close(fd);
    printf("end\n");
}

void
faxCoverApp::coverDef(const char* tag, const char* value)
{
    if (*value == '\0')
	return;
    printf("/%s (", tag);
    for (const char* cp = value; *cp; cp++) {
	if (*cp == '(' || *cp == ')')
	    putchar('\\');
	putchar(*cp);
    }
    printf(") def\n");
}

fxStr
faxCoverApp::tildeExpand(const fxStr& filename)
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
faxCoverApp::printError(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: ", (char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
}
#undef fmt
