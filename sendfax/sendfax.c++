#ident $Header: /usr/people/sam/flexkit/fax/sendfax/RCS/sendfax.c++,v 1.15 91/06/04 20:11:12 sam Exp $

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
#include "Path.h"

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
#include <netdb.h>		// XXX

    int close(int);
    int gethostname(char*, int);
    int access(const char*, int);
}

#include "tiffio.h"
#include <gl/image.h>

enum FileType {
    TYPE_TIFF,
    TYPE_TEXT,
    TYPE_TROFF,
    TYPE_POSTSCRIPT,
    TYPE_IMAGE,
};
struct FileInfo : public fxObj {
    fxStr	name;
    FileType	type;
    fxBool	isTemp;
    fxBool	tagLine;

    FileInfo() { isTemp = FALSE; tagLine = TRUE; type = TYPE_TEXT; }
    ~FileInfo() { if (isTemp) unlink(name); }
};
fxDECLARE_ObjArray(FileInfoArray, FileInfo);

class sendFaxApp : public fxApplication {
private:
    FaxClient	client;
    fxStr	appName;		// for error messages
    fxStrArray	destNames;		// FAX destination names
    fxStrArray	destNumbers;		// FAX destination numbers
    FileInfoArray files;		// files to send (possibly converted)
    fxStr	stdinTemp;		// temp file for collecting from pipe
    fxBool	verbose;
    fxStr	killtime;		// job's time to be killed
    fxStr	sendtime;		// job's time to be sent
    fxBool	coverSheet;		// if TRUE, prepend cover sheet
    int		resolution;		// sending vertical resolution (dpi)
    int		pageWidth;		// sending page width (pixels)
    int		pageLength;		// sending page length (mm)
    Path	searchPath;
    enum FaxNotify {no_notice, when_done, when_requeued} notify;

    fxStr	mailBox;		// user@host return mail address
    fxStr	senderName;		// sender's full name (if available)
    fxStr	comments;		// comments on cover sheet
    fxStr	filterDir;		// place to look for filters

    static fxStr TextConverter;
    static fxStr TroffConverter;
    static fxStr PostScriptConverter;
    static fxStr ImageConverter;
    static fxStr CoverSheetApp;
    static fxStr dbName;

    void copyToTemporary(int fin, FileInfo& info);
    fxStr getCmd(const fxStr& cmd);
    void convertData(FileInfo& info, const fxStr& cmd,
		const fxStr& file, FileType result);
    FileType fileType(const char* filename);
    void handleFile(const char* filename, FileInfo& info);
    void setupUserIdentity();
    fxStr tildeExpand(const fxStr& filename);
    void sendCoverSheet(const fxStr& name, const fxStr& number);
    void sendCoverDef(const char* name, const char* value);
    void sendCoverLine(const char* va_alist ...);
    void printError(const char* va_alist ...);
    void usage();
public:
    sendFaxApp();
    ~sendFaxApp();

    void initialize(int argc, char** argv);
    void open();
    virtual const char *className() const;

    void recvStatus(const char* statusMessage);	// recvStatus wire
    void recvEof();				// recvEof wire
    void recvError(const int err);		// recvError wire

};

static void s0(sendFaxApp* o, char*cp)		{ o->recvStatus(cp); }
static void s1(sendFaxApp* o)			{ o->recvEof(); }
static void s2(sendFaxApp* o, int er)		{ o->recvError(er); }

/*
 * Note that handleFile knows the resultant type
 * of using these filters to convert data!  If
 * you fiddle with these, you need to change the
 * code to reflect any type change in the result.
 */
fxStr sendFaxApp::TextConverter("text2fax");
fxStr sendFaxApp::TroffConverter("dit2fax");
fxStr sendFaxApp::ImageConverter("sgi2fax");
fxStr sendFaxApp::CoverSheetApp("faxcover");
fxStr sendFaxApp::dbName("~/.faxdb");

sendFaxApp::sendFaxApp() :
    filterDir(FAX_FILTERDIR),
    searchPath("PATH", FAX_FILTERDIR)
{
    verbose = FALSE;
    killtime = "now + 1 day";	// default time to kill the job
    coverSheet = TRUE;
    resolution = 98;		// default is low resolution
    pageWidth = 1728;
    pageLength = 297;		// default is A4
    notify = no_notice;		// default is no email notification

    addInput("recvStatus",	fxDT_CharPtr,	this, (fxStubFunc) s0);
    addInput("recvEof",		fxDT_void,	this, (fxStubFunc) s1);
    addInput("recvError",	fxDT_int,	this, (fxStubFunc) s2);

    client.connect("faxMessage",	this, "recvStatus");
    client.connect("faxEof",		this, "recvEof");
    client.connect("faxError",		this, "recvError");
}

sendFaxApp::~sendFaxApp()
{
    if (stdinTemp.length())
	unlink(stdinTemp);
}

void
sendFaxApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "a:c:d:f:h:k:lmnvDR")) != -1)
	switch (c) {
	case 'c':
	    comments = optarg;
	    break;
	case 'd':
	    char* cp = strchr(optarg, '@');
	    if (cp) {
		destNames.append(fxStr(optarg, cp-optarg));
		destNumbers.append(cp+1);
	    } else {
		destNames.append("");
		destNumbers.append(optarg);
	    }
	    break;
	case 'h':			// server's host
	    client.setHost(optarg);
	    break;
	case 'l':			// low resolution
	    resolution = 98;
	    break;
	case 'm':			// medium resolution
	    resolution = 196;
	    break;
	case 'n':			// no cover sheet
	    coverSheet = FALSE;
	    break;
	case 'R':			// notify when requeued or done
	    notify = when_requeued;
	    break;
	case 'D':			// notify when done
	    notify = when_done;
	    break;
	case 'k':
	    killtime = optarg;
	    break;
	case 'a':
	    sendtime = optarg;
	    break;
	case 'f':
	    filterDir = optarg;
	    break;
	case 'v':
	    verbose = TRUE;
	    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
	    break;
	case '?':
	    usage();
	    /*NOTREACHED*/
	}
    if (destNumbers.length() == 0) {
	printError("No destination specified");
	usage();
    }
    setupUserIdentity();
    if (optind < argc) {
	files.resize(argc-optind);
	for (u_int i = 0; optind < argc; i++, optind++)
	    handleFile(argv[optind], files[i]);
    } else {
	files.resize(1);
	copyToTemporary(fileno(stdin), files[0]);
    }
}

void
sendFaxApp::usage()
{
    fxFatal("usage: %s"
	" [-d destination]"
	" [-h server-host]"
	" [-k kill-time]\n"
	"    "
	" [-a time-to-send]"
	" [-c comments]"
	" [-lmnvDR]"
	" [files]\n",
	(char*) appName);
}

fxStr
sendFaxApp::tildeExpand(const fxStr& filename)
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
sendFaxApp::setupUserIdentity()
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
    mailBox = pwd->pw_name;
    mailBox.append('@');
    char hostname[64];
    (void) gethostname(hostname, sizeof (hostname));
    struct hostent* hp = gethostbyname(hostname);
    mailBox.append(hp ? hp->h_name : hostname);
}

void
sendFaxApp::handleFile(const char* filename, FileInfo& info)
{
    switch (info.type = fileType(filename)) {
    case TYPE_TIFF:
	info.name = filename;
	info.isTemp = FALSE;
	break;
    case TYPE_POSTSCRIPT:
	info.name = filename;
	info.isTemp = FALSE;
	break;
    case TYPE_TEXT:
	convertData(info, TextConverter, filename, TYPE_POSTSCRIPT);
	break;
    case TYPE_TROFF:
	convertData(info, TroffConverter, filename, TYPE_POSTSCRIPT);
	break;
    case TYPE_IMAGE:
	convertData(info, ImageConverter, filename, TYPE_TIFF);
	break;
    default:
	fxFatal("Unknown file type 0x%x for \"%s\"", info.type, filename);
	/*NOTREACHED*/
    }
}

extern "C" int mkstemp(char*);

void
sendFaxApp::copyToTemporary(int fin, FileInfo& info)
{
    fxStr templ("/usr/tmp/sndfaxXXXXXX");
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
    handleFile(templ, info);
    /*
     * If the temp file contained data that was converted, then
     * we can unlink it now.  Otherwise, it'll be unlinked when
     * we exit as a consequence of our marking it ``temporary''.
     */
    if (info.name != templ)
	unlink(templ);
    else
	info.isTemp = TRUE;
}

fxStr
sendFaxApp::getCmd(const fxStr& cmd)
{
    fxStr pathname;

    if (cmd[0] == '/' && access(cmd, 1) == 0)
	return (cmd);
    if (filterDir.length() > 0) {
	pathname = filterDir | "/" | cmd;
	if (access(pathname, 1) == 0)
	    return (pathname);
    }
    for (PathIterator it(searchPath); it.notDone(); it++) {
	pathname = it | "/" | cmd;
	if (access(pathname, 1) == 0)
	    return (pathname);
    }
    pathname = "";
    return (pathname);
}

void
sendFaxApp::convertData(FileInfo& info, const fxStr& cmd,
		const fxStr& file, FileType result)
{
    fxStr sysCmd = getCmd(cmd);
    if (sysCmd.length() > 0) {
	fxStr temp("/usr/tmp/sndfaxXXXXXX");
	mktemp(temp);
	sysCmd = sysCmd | (resolution == 98 ? " -l" : " -m") |
	     " -o " | temp | " " | file;
	if (verbose)
	    printf("CONVERT \"%s\"\n", (char*) sysCmd);
	if (system(sysCmd) != 0) {
	    unlink(temp);
	    fxFatal("Error converting data; command was \"%s\"", (char*)sysCmd);
	}
	info.name = temp;
	info.type = result;
	info.isTemp = TRUE;
    } else
	fxFatal("Can not convert %s", (char*) file);
}

FileType
sendFaxApp::fileType(const char* filename)
{
    struct stat sb;
    int fd = ::open(filename, O_RDONLY);
    if (fd < 0 || fstat(fd, &sb) < 0)
	fxFatal("%s: Can not determine file type", filename);
    if ((sb.st_mode & S_IFMT) != S_IFREG)
	fxFatal("%s: Not a regular file", filename);
    union {
	int	l;
	short	w;
	char	b[512];
    } buf;
    bzero(&buf, sizeof (buf));
    int cc = read(fd, buf.b, sizeof (buf));
    if (cc == 0)
	fxFatal("%s: Empty file", filename);
    if (cc >= 2) {
	switch (buf.w) {
	case TIFF_BIGENDIAN:
	case TIFF_LITTLEENDIAN:
	    return (TYPE_TIFF);
	case IMAGIC:
	    return (TYPE_IMAGE);
	}
    }
    for (u_int i = 0; i < cc; i++) {
	char c = buf.b[i];
	if (!isascii(c))
	    fxFatal("%s: Non-ascii stuff and not a known type", filename);
    }
    if (buf.b[0] == '%' && buf.b[1] == '!')
	return (TYPE_POSTSCRIPT);
    if (strncmp(buf.b, "x T psc\n", 8) == 0)
	return (TYPE_TROFF);
    return (TYPE_TEXT);
}

void
sendFaxApp::open()
{
    if (client.callServer() == client.Failure)
	fxFatal("Could not call server");
    fx_theExecutive->addSelectHandler(&client);
    for (u_int i = 0; i < files.length(); i++) {
	const FileInfo& info = files[i];
	switch (info.type) {
	case TYPE_TIFF:		client.sendData("tiff", info.name); break;
	case TYPE_POSTSCRIPT:	client.sendData("postscript", info.name); break;
	default:
	    fxFatal("Unexpected file type 0x%x for \"%s\"",
		info.type, (char*) info.name);
	}
    }
    for (i = 0; i < destNumbers.length(); i++) {
	client.sendLine("begin", i);
	if (sendtime.length() > 0)
	    client.sendLine("sendAt", sendtime);
	client.sendLine("killtime", killtime);
	client.sendLine("number", destNumbers[i]);
	client.sendLine("sender", senderName);
	client.sendLine("mailaddr", mailBox);
	client.sendLine("resolution", resolution);
	client.sendLine("pagewidth", pageWidth);
	client.sendLine("pagelength", pageLength);
	if (notify == when_done)
	    client.sendLine("notify", "when done");
	else if (notify == when_requeued)
	    client.sendLine("notify", "when requeued");
	if (coverSheet)
	    sendCoverSheet(destNames[i], destNumbers[i]);
	client.sendLine("end", i);
    }
    client.sendLine(".\n");
}

void
sendFaxApp::sendCoverSheet(const fxStr& name, const fxStr& number)
{
    fxStr cmd(CoverSheetApp);
    cmd.append(" -n \"" | number | "\"");
    cmd.append(" -t \"" | name | "\"");
    cmd.append(" -f \"" | senderName | "\"");
    cmd.append(" -c \"" | comments | "\"");
//    cmd.append(" -p " | pagecount);
    if (verbose)
	printf("COVER SHEET \"%s\"\n", (char*) cmd);
    FILE* fp = popen(cmd, "r");
    if (fp != NULL) {
	client.sendLine("cover", 1);	// cover sheet sub-protocol version 1
	// copy prototype cover page
	char line[1024];
	while (fgets(line, sizeof (line)-1, fp)) {
	    char* cp = strchr(line, '\n');
	    if (cp)
		*cp = '\0';
	    sendCoverLine("%s", line);
	}
	pclose(fp);
	client.sendLine("..\n");
    } else
	printError("Could not generate cover sheet; none sent");
}

void
sendFaxApp::sendCoverLine(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    char buf[4096];
    vsprintf(buf, fmt, ap);
    va_end(ap);
    client.sendLine("!", buf);
}
#undef fmt

void
sendFaxApp::printError(const char* va_alist ...)
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

#define	isCmd(cmd)	(strcasecmp(statusMessage, cmd) == 0)

void
sendFaxApp::recvStatus(const char* statusMessage)
{
    char* cp;
    if (cp = strchr(statusMessage, '\n'))
	*cp = '\0';
    char* tag = strchr(statusMessage, ':');
    if (!tag) {
	printError("Malformed statusMessage \"%s\"", statusMessage);
	(void) client.hangupServer();
	close();				// terminate app
    }
    *tag++ = '\0';
    while (isspace(*tag))
	tag++;

    if (isCmd("job")) {
	int len = files.length();
	printf("request id is %s for host %s (%d %s)\n",
	    tag, (char*) client.getHost(),
	    len, len > 1 ? "files" : "file");
    } else if (isCmd("error")) {
	printf("%s\n", tag);
    } else
	printf("Unknown status message \"%s:%s\"\n", statusMessage, tag);
}

void
sendFaxApp::recvEof()
{
    (void) client.hangupServer();
    close();					// terminate app
}

void
sendFaxApp::recvError(const int err)
{
    printError("Fatal socket read error: %s", strerror(err));
    (void) client.hangupServer();
    close();					// terminate app
}

const char* sendFaxApp::className() const { return ("sendFaxApp"); }

fxIMPLEMENT_ObjArray(FileInfoArray, FileInfo);

fxAPPINIT(sendFaxApp, 0);
