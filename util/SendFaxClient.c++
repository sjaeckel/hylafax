/*	$Header: /usr/people/sam/fax/./util/RCS/SendFaxClient.c++,v 1.43 1995/04/08 21:44:21 sam Rel $ */
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
#include <stdarg.h>
#include <ctype.h>

#include <osfcn.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>		// XXX
}

#include "Dispatcher.h"
#include "SendFaxClient.h"
#include "TypeRules.h"
#include "DialRules.h"
#include "PageSize.h"
#include "config.h"

struct FileInfo : public fxObj {
    fxStr	name;
    const TypeRule* rule;
    fxBool	isTemp;
    fxBool	tagLine;

    FileInfo();
    FileInfo(const FileInfo& other);
    ~FileInfo();
};
fxDECLARE_ObjArray(FileInfoArray, FileInfo);


SendFaxClient::SendFaxClient()
{
    typeRules = NULL;
    dialRules = NULL;
    files = new FileInfoArray;
    pollCmd = FALSE;
    coverSheet = TRUE;
    gotPermission = FALSE;
    permission = FALSE;
    verbose = FALSE;
    killtime = FAX_TIMEOUT;	// default time to kill the job
    hres = 204;			// G3 standard
    vres = FAX_DEFVRES;		// default resolution
    pageWidth = 0;
    pageLength = 0;
    totalPages = 0;
    maxDials = FAX_REDIALS;
    maxRetries = FAX_RETRIES;
    priority = FAX_DEFPRIORITY;	// default priority
    notify = FAX_DEFNOTIFY;	// default notification
    setup = FALSE;
}

SendFaxClient::~SendFaxClient()
{
    u_int i;
    for (i = 0; i < coverPages.length(); i++)
	unlink((char*) coverPages[i]);
    for (i = 0; i < tempFiles.length(); i++)
	unlink((char*) tempFiles[i]);
    delete typeRules;
    delete dialRules;
    delete files;
}

fxBool
SendFaxClient::prepareSubmission()
{
    u_int i, n;

    if (!setupSenderIdentity(from))
	return (FALSE);
    if (pageSize == "" && !setPageSize("default"))
	return (FALSE);
    typeRules = TypeRules::read(FAX_LIBDATA "/" FAX_TYPERULES);
    if (!typeRules) {
	printError("Unable to setup file typing and conversion rules");
	return (FALSE);
    }
    typeRules->setVerbose(verbose);
    dialRules = new DialStringRules(FAX_LIBDATA "/" FAX_DIALRULES);
    dialRules->setVerbose(verbose);
    if (!dialRules->parse() && verbose)		// NB: not fatal
	printWarning("unable to setup dialstring rules");
    for (i = 0, n = files->length(); i < n; i++)
	if (!handleFile((*files)[i]))
	    return (FALSE);
    /*
     * Convert dialstrings to a displayable format.  This
     * deals with problems like calling card access codes
     * getting stuck on the cover sheet and/or displayed in
     * status messages.
     */
    externalNumbers.resize(destNumbers.length());
    for (i = 0, n = destNumbers.length(); i < n; i++)
	externalNumbers[i] = dialRules->displayNumber(destNumbers[i]);
    /*
     * Suppress the cover page if we're just doing a poll;
     * otherwise, generate a cover sheet for each destination
     * (We do it now so that we can be sure everything is ready
     * to send before we setup a connection to the server.)
     */
    if (pollCmd && files->length() == 0)
	coverSheet = FALSE;
    if (coverSheet) {
	coverPages.resize(externalNumbers.length());
	for (i = 0, n = externalNumbers.length(); i < n; i++)
	    coverPages[i] = makeCoverPage(destNames[i], externalNumbers[i],
		destCompanys[i], destLocations[i], senderName);
    }
    return (setup = TRUE);
}

#define	CHECK(x)	{ if (!(x)) return (FALSE); }
#define	CHECKSEND(x)	{ if (!(x)) goto failure; }

fxBool
SendFaxClient::submitJob()
{
    u_int i;

    CHECK(setup && callServer())
    startRunning();
    /*
     * Explicitly check for permission to submit a job
     * before sending the input documents.  This way we
     * we avoid sending a load of stuff just to find out
     * that the user/host is not permitted to submit jobs.
     */
    CHECKSEND(sendLine("checkPerm", "send"))
    permission = gotPermission = FALSE;
    while (isRunning() && !getPeerDied() && !gotPermission)
	Dispatcher::instance().dispatch();
    CHECK(permission)

    /*
     * Transfer the document files first so that they
     * can be referenced multiple times for different
     * destinations.
     */
    for (i = 0; i < files->length(); i++) {
	const FileInfo& info = (*files)[i];
	if (info.rule->getResult() == TypeRule::TIFF)
	    CHECKSEND(sendData("tiff", info.name))
	else
	    CHECKSEND(sendLZWData("zpostscript", info.name))
    }
    if (pollCmd)
	CHECKSEND(sendLine("poll", ""))
    for (i = 0; i < destNumbers.length(); i++) {
	CHECKSEND(sendLine("begin", i))
	CHECKSEND(sendLine("jobtype", "facsimile"))
	if (sendtime != "")
	    CHECKSEND(sendLine("sendAt", sendtime))
	if (maxDials >= 0)
	    CHECKSEND(sendLine("maxdials", maxDials))
	if (maxRetries >= 0)
	    CHECKSEND(sendLine("maxtries", maxRetries))
	if (killtime != "")
	    CHECKSEND(sendLine("killtime", killtime))
	CHECKSEND(sendLine("priority", priority));
	/*
	 * If the dialstring is different from the
	 * displayable number then pass both.
	 */
	if (destNumbers[i] != externalNumbers[i]) {
	    /*
	     * XXX
	     * We should bump the protocol version and
	     * warn the submitter if this isn't supported
	     * by the server.
	     */
	    CHECKSEND(sendLine("external", externalNumbers[i]))
	}
	CHECKSEND(sendLine("number", destNumbers[i]))
	CHECKSEND(sendLine("sender", senderName))
	CHECKSEND(sendLine("mailaddr", mailbox))
	CHECKSEND(sendLine("receiver", destNames[i]))
	CHECKSEND(sendLine("company", destCompanys[i]))
	CHECKSEND(sendLine("location", destLocations[i]))
	if (jobtag != "")
	    CHECKSEND(sendLine("jobtag", jobtag))
	CHECKSEND(sendLine("resolution", (int) vres))
	CHECKSEND(sendLine("pagewidth", (int) pageWidth))
	CHECKSEND(sendLine("pagelength", (int) pageLength))
	if (notify == when_done)
	    CHECKSEND(sendLine("notify", "when done"))
	else if (notify == when_requeued)
	    CHECKSEND(sendLine("notify", "when requeued"))
	if (coverSheet)
	    CHECKSEND(sendLZWData("zcover", coverPages[i]))
	CHECKSEND(sendLine("end", i))
    }
    CHECKSEND(sendLine(".\n"));
    return (TRUE);
failure:
    if (getPeerDied()) {
	/*
	 * Look for the reason the peer died.
	 */
	while (isRunning())
	    Dispatcher::instance().dispatch();
    }
    return (FALSE);
}
#undef CHECKSEND
#undef CHECK

u_int SendFaxClient::getNumberOfDestinations() const
   { return destNames.length(); }
u_int SendFaxClient::getNumberOfFiles() const	{ return files->length(); }
u_int SendFaxClient::getTotalPages() const	{ return totalPages; }

void SendFaxClient::setCoverComments(const char* s)	{ comments = s; }
const fxStr& SendFaxClient::getCoverComments() const	{ return comments; }
void SendFaxClient::setCoverRegarding(const char* s)	{ regarding = s; }
const fxStr& SendFaxClient::getCoverRegarding() const	{ return regarding; }
void SendFaxClient::setCoverSheet(fxBool b)		{ coverSheet = b; }
void SendFaxClient::setResolution(float r)		{ vres = r; }
void SendFaxClient::setPollRequest(fxBool b)		{ pollCmd = b; }
fxBool SendFaxClient::getPollRequest() const		{ return pollCmd; }
void SendFaxClient::setNotification(FaxNotify n)	{ notify = n; }
void SendFaxClient::setKillTime(const char* s)		{ killtime = s; }
void SendFaxClient::setSendTime(const char* s)		{ sendtime = s; }
void SendFaxClient::setFromIdentity(const char* s)	{ from = s; }
void SendFaxClient::setJobTag(const char* s)		{ jobtag = s; }
const fxStr& SendFaxClient::getFromIdentity() const	{ return from; }
void SendFaxClient::setMaxRetries(int n)		{ maxRetries = n; }
void SendFaxClient::setMaxDials(int n)			{ maxDials = n; }
void SendFaxClient::setVerbose(fxBool b)		{ verbose = b; }
fxBool SendFaxClient::getVerbose() const		{ return verbose; }

int SendFaxClient::getPriority() const			{ return priority; }
void SendFaxClient::setPriority(int p)			{ priority = p; }

fxBool
SendFaxClient::setPageSize(const char* name)
{
    PageSizeInfo* info = PageSizeInfo::getPageSizeByName(name);
    if (info) {
	pageWidth = info->width();
	pageLength = info->height();
	pageSize = name;
	delete info;
	return (TRUE);
    } else {
	printError("Unknown page size \"%s\"", name);
	return (FALSE);
    }
}
const fxStr& SendFaxClient::getPageSize() const	{ return pageSize; }
float SendFaxClient::getPageWidth() const	{ return pageWidth; }
float SendFaxClient::getPageLength() const	{ return pageLength; }

/*
 * Add a new destination name and number.
 */
fxBool
SendFaxClient::addDestination(
    const char* person,
    const char* faxnum,
    const char* company,
    const char* location
)
{
    if (!faxnum || faxnum[0] == '\0') {
	printError("No fax number specified");
	return (FALSE);
    }
    destNumbers.append(faxnum);
    destNames.append(person ? person : "");
    destLocations.append(location ? location : "");
    destCompanys.append(company ? company : "");
    return (TRUE);
}

/*
 * Add a new file to send to each destination.
 */
void
SendFaxClient::addFile(const char* filename)
{
    u_int i = files->length();
    files->resize(i+1);
    (*files)[i].name = filename;
}

/*
 * Create the mail address for a local user.
 */
void
SendFaxClient::setMailbox(const char* user)
{
    fxStr acct(user);
    if (acct.next(0, "@!") == acct.length()) {
	char hostname[64];
	(void) gethostname(hostname, sizeof (hostname));
	struct hostent* hp = gethostbyname(hostname);
	mailbox = acct | "@" | (hp ? hp->h_name : hostname);
    } else
	mailbox = acct;
}
const fxStr& SendFaxClient::getMailbox() const		{ return mailbox; }

/*
 * Setup the sender's identity.
 */
fxBool
SendFaxClient::setupSenderIdentity(const fxStr& from)
{
    FaxClient::setupUserIdentity();		// client identity

    if (from != "") {
	u_int l = from.next(0, '<');
	if (l == from.length()) {
	    l = from.next(0, '(');
	    if (l != from.length()) {		// joe@foobar (Joe Schmo)
		mailbox = from.head(l);
		l++; senderName = from.token(l, ')');
	    } else {				// joe
		setMailbox(from);
		if (from == getUserName())
		    senderName = FaxClient::getSenderName();
		else
		    senderName = "";
	    }
	} else {				// Joe Schmo <joe@foobar>
	    senderName = from.head(l);
	    l++; mailbox = from.token(l, '>');
	}
	if (senderName == "" && mailbox != "") {
	    /*
	     * Mail address, but no "real name"; construct one from
	     * the account name.  Do this by first stripping anything
	     * to the right of an '@' and then stripping any leading
	     * uucp patch (host!host!...!user).
	     */
	    senderName = mailbox;
	    senderName.resize(senderName.next(0, '@'));
	    senderName.remove(0, senderName.nextR(senderName.length(), '!'));
	}

	// strip and leading&trailing white space
	senderName.remove(0, senderName.skip(0, " \t"));
	senderName.resize(senderName.skipR(senderName.length(), " \t"));
	mailbox.remove(0, mailbox.skip(0, " \t"));
	mailbox.resize(mailbox.skipR(mailbox.length(), " \t"));
	if (senderName == "" || mailbox == "") {
	    printError("Malformed (null) sender name or mail address");
	    return (FALSE);
	}
    } else {
	senderName = FaxClient::getSenderName();
	setMailbox(getUserName());
    }
    return (TRUE);
}
const fxStr& SendFaxClient::getSenderName() const	{ return senderName; }

/*
 * Process a file submitted for transmission.
 */
fxBool
SendFaxClient::handleFile(FileInfo& info)
{
    info.rule = fileType(info.name);
    if (!info.rule)
	return (FALSE);
    if (info.rule->getCmd() != "") {	// conversion required
	fxStr temp(_PATH_TMP "faxsndXXXXXX");
	tempFiles.append(temp);
	mktemp((char*) temp);
	fxStr sysCmd = info.rule->getFmtdCmd(info.name, temp,
		hres, vres, "1", pageSize);
	if (verbose)
	    printf("CONVERT \"%s\"\n", (char*) sysCmd);
	if (system((char*) sysCmd) != 0) {
	    unlink((char*) temp);
	    u_int ix = tempFiles.find(temp);
	    if (ix != fx_invalidArrayIndex)
		tempFiles.remove(ix);
	    printError("Error converting data; command was \"%s\"",
		(char*) sysCmd);
	    return (FALSE);
	}
	info.name = temp;
	info.isTemp = TRUE;
    } else				// already postscript or tiff
	info.isTemp = FALSE;
    switch (info.rule->getResult()) {
    case TypeRule::TIFF:
	countTIFFPages(info.name);
	break;
    case TypeRule::POSTSCRIPT:
	estimatePostScriptPages(info.name);
	break;
    }
    return (TRUE);
}

/*
 * Return a TypeRule for the specified file.
 */
const TypeRule*
SendFaxClient::fileType(const char* filename)
{
    struct stat sb;
    int fd = ::open(filename, O_RDONLY);
    if (fd < 0) {
	printError("%s: Can not open file", filename);
	return (NULL);
    }
    if (fstat(fd, &sb) < 0) {
	printError("%s: Can not stat file", filename);
	::close(fd);
	return (NULL);
    }
    if ((sb.st_mode & S_IFMT) != S_IFREG) {
	printError("%s: Not a regular file", filename);
	::close(fd);
	return (NULL);
    }
    char buf[512];
    int cc = read(fd, buf, sizeof (buf));
    ::close(fd);
    if (cc == 0) {
	printError("%s: Empty file", filename);
	return (NULL);
    }
    const TypeRule* tr = typeRules->match(buf, cc);
    if (!tr) {
	printError("%s: Can not determine file type", filename);
	return (NULL);
    }
    if (tr->getResult() == TypeRule::ERROR) {
	fxStr emsg(tr->getErrMsg());
	printError("%s: %s", filename, (char*) emsg);
	return (NULL);
    }
    return tr;   
}

#include "tiffio.h"

/*
 * Count the number of ``pages'' in a TIFF file.
 */
void
SendFaxClient::countTIFFPages(const char* filename)
{
    TIFF* tif = TIFFOpen(filename, "r");
    if (tif) {
	do {
	    totalPages++;
	} while (TIFFReadDirectory(tif));
	TIFFClose(tif);
    }
}

/*
 * Count the number of pages in a PostScript file.
 * We can really only estimate the number as we
 * depend on the DSC comments to figure this out.
 */
void
SendFaxClient::estimatePostScriptPages(const char* filename)
{
    FILE* fd = fopen(filename, "r");
    if (fd != NULL) {
	char line[2048];
	if (fgets(line, sizeof (line)-1, fd) != NULL) {
	    /*
	     * We only consider ``conforming'' PostScript documents.
	     */
	    if (line[0] == '%' && line[1] == '!') {
		int npagecom = 0;	// # %%Page comments
		int npages = 0;		// # pages according to %%Pages comments
		while (fgets(line, sizeof (line)-1, fd) != NULL) {
		    int n;
		    if (strncmp(line, "%%Page:", 7) == 0)
			npagecom++;
		    else if (sscanf(line, "%%%%Pages: %u", &n) == 1)
			npages += n;
		}
		/*
		 * Believe %%Pages comments over counting of %%Page comments.
		 */
		if (npages > 0)
		    totalPages += npages;
		else if (npagecom > 0)
		    totalPages += npagecom;
	    }
	}
	fclose(fd);
    }
}

/*
 * Invoke the cover page generation program.
 */
fxStr
SendFaxClient::makeCoverPage(
    const fxStr& name,
    const fxStr& number,
    const fxStr& company,
    const fxStr& location,
    const fxStr& sender
)
{
    fxStr templ(_PATH_TMP "sndfaxXXXXXX");
    tempFiles.append(templ);
    int fd = mkstemp((char*) templ);
    if (fd >= 0) {
	fxStr cmd("faxcover");
	if (getPageSize() != "default")
	    cmd.append(" -s " | getPageSize());
	cmd.append(" -n \"" | number | "\"");
	cmd.append(" -t \"" | name | "\"");
	cmd.append(" -f \"" | sender | "\"");
	if (getCoverComments() != "") {
	    cmd.append(" -c \"");
	    cmd.append(getCoverComments());
	    cmd.append("\"");
	}
	if (getCoverRegarding() != "") {
	    cmd.append(" -r \"");
	    cmd.append(getCoverRegarding());
	    cmd.append("\"");
	}
	if (location != "") {
	    cmd.append(" -l \"");
	    cmd.append(location);
	    cmd.append("\"");
	}
	if (company != "") {
	    cmd.append(" -x \"");
	    cmd.append(company);
	    cmd.append("\"");
	}
	if (getTotalPages() > 0) {
	    fxStr pages((int) getTotalPages(), "%u");
	    cmd.append(" -p " | pages);
	}
	if (getVerbose())
	    printf("COVER SHEET \"%s\"\n", (char*) cmd);
	FILE* fp = popen(cmd, "r");
	if (fp != NULL) {
	    // copy prototype cover page
	    char buf[16*1024];
	    int cc;
	    while ((cc = fread(buf, 1, sizeof (buf)-1, fp)) > 0)
		(void) write(fd, buf, cc);
	    if (pclose(fp) == 0) {
		::close(fd);
		return (templ);
	    }
	}
	printError("Error creating cover sheet; command was \"%s\"",
	    (char*) cmd);
    } else
	printError("%s: Can not create temporary file for cover sheet",
	    (char*) templ);
    unlink((char*) templ);
    templ = "";
    return (templ);
}

fxBool
SendFaxClient::sendCoverLine(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    char buf[4096];
    vsprintf(buf, fmt, ap);
    va_end(ap);
    return sendLine("!", buf);
}
#undef fmt

#define	isCmd(s)	(strcasecmp(s, cmd) == 0)

void
SendFaxClient::recvConf(const char* cmd, const char* tag)
{
    if (isCmd("permission")) {
	gotPermission = TRUE;
	permission = (strcasecmp(tag, "granted") == 0);
    } else
	printError("Unknown status message received: \"%s:%s\"", cmd, tag);
}

void
SendFaxClient::recvEof()
{
    stopRunning();
}

void
SendFaxClient::recvError(const int err)
{
    printError("Socket read error: %s", strerror(err));
    stopRunning();
}

FileInfo::FileInfo()
{
    isTemp = FALSE;
    tagLine = TRUE;
    rule = NULL;
}
FileInfo::FileInfo(const FileInfo& other)
    : fxObj(other)
    , name(other.name)
    , rule(other.rule)
{
    isTemp = other.isTemp;
    tagLine = other.tagLine;
}
FileInfo::~FileInfo()
{
    if (isTemp)
	unlink((char*) name);
}
fxIMPLEMENT_ObjArray(FileInfoArray, FileInfo);
