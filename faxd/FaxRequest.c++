/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxRequest.c++,v 1.79 1995/04/08 21:30:22 sam Rel $ */
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
#include <osfcn.h>

#include "Sys.h"

#include "FaxRequest.h"
#include "config.h"

/*
 * HylaFAX job request file handling.
 */

FaxRequest::FaxRequest(const fxStr& qf) : qfile(qf)
{
    tts = 0;
    killtime = 0;
    fp = NULL;
    status = send_retry;
    pri = usrpri = FAX_DEFPRIORITY;
    pagewidth = pagelength = resolution = 0;
    npages = totpages = 0;
    ntries = ndials = 0;
    totdials = 0, maxdials = (u_short) FAX_REDIALS;
    tottries = 0, maxtries = (u_short) FAX_RETRIES;
    notify = no_notice;
    jobtype = "facsimile";		// for compatibility w/ old clients
}

FaxRequest::~FaxRequest()
{
    if (fp != NULL)
	::fclose(fp);
}

#define	N(a)		(sizeof (a) / sizeof (a[0]))

static const struct {
    const char* name;
    fxStr FaxRequest::* p;
    fxBool	mustexist;
} strvals[] = {
    // NB: "external" must precede "number"
    { "external",	&FaxRequest::external,		FALSE },
    { "number",		&FaxRequest::number,		TRUE },
    { "mailaddr",	&FaxRequest::mailaddr,		TRUE },
    { "sender",		&FaxRequest::sender,		TRUE },
    { "jobid",		&FaxRequest::jobid,		TRUE },
    { "jobtag",		&FaxRequest::jobtag,		FALSE },
    { "pagehandling",	&FaxRequest::pagehandling,	FALSE },
    { "modem",		&FaxRequest::modem,		TRUE },
    { "receiver",	&FaxRequest::receiver,		FALSE },
    { "company",	&FaxRequest::company,		FALSE },
    { "location",	&FaxRequest::location,		FALSE },
    { "cover",		&FaxRequest::cover,		FALSE },
    { "client",		&FaxRequest::client,		TRUE },
    { "groupid",	&FaxRequest::groupid,		FALSE },
    { "signalrate",	&FaxRequest::sigrate,		FALSE },
    { "dataformat",	&FaxRequest::df,		FALSE },
    { "jobtype",	&FaxRequest::jobtype,		FALSE },
};
static const struct {
    const char* name;
    u_short FaxRequest::* p;
} shortvals[] = {
    { "npages",		&FaxRequest::npages },
    { "totpages",	&FaxRequest::totpages },
    { "ntries",		&FaxRequest::ntries },
    { "ndials",		&FaxRequest::ndials },
    { "totdials",	&FaxRequest::totdials },
    { "maxdials",	&FaxRequest::maxdials },
    { "tottries",	&FaxRequest::tottries },
    { "maxtries",	&FaxRequest::maxtries },
    { "pagewidth",	&FaxRequest::pagewidth },
    { "resolution",	&FaxRequest::resolution },
    { "pagelength",	&FaxRequest::pagelength },
    { "priority",	&FaxRequest::usrpri},
    { "schedpri",	&FaxRequest::pri},
};
static const struct {
    const char* name;
    time_t FaxRequest::* p;
} timevals[] = {
    { "tts",		&FaxRequest::tts },
    { "killtime",	&FaxRequest::killtime },
};
static const struct {
    const char* name;
    fxBool	check;
    FaxSendOp	op;
} docvals[] = {
    { "poll",		FALSE,	FaxRequest::send_poll },
    { "tiff",		TRUE,	FaxRequest::send_tiff },
    { "!tiff",		FALSE,	FaxRequest::send_tiff_saved },
    { "postscript",	TRUE,	FaxRequest::send_postscript },
    { "!postscript",	FALSE,	FaxRequest::send_postscript_saved },
    { "fax",		FALSE,	FaxRequest::send_fax },
    { "data",		TRUE,	FaxRequest::send_data },
    { "!data",		FALSE,	FaxRequest::send_data_saved },
    { "page",		FALSE,	FaxRequest::send_page },
    { "!page",		FALSE,	FaxRequest::send_page_saved },
};
static const char* notifyVals[] = { "none", "when done", "when requeued" };

fxBool
FaxRequest::readQFile(int fd, fxBool& rejectJob)
{
    lineno = 0;
    fp = ::fdopen(fd, "r+w");
    if (fp == NULL) {
	error("open: %%m");
	return (FALSE);
    }
    rejectJob = FALSE;
    char line[2048];
    while (::fgets(line, sizeof (line) - 1, fp)) {
	lineno++;
	if (line[0] == '#')
	    continue;
	fxStr cmd(line);
	cmd.resize(cmd.next(0, '\n'));
	u_int l = cmd.next(0,':');
	if (l >= cmd.length()) {
	    error("Syntax error, missing ':' in line \"%s\"", line);
	    continue;
	}
	// split tag from command
	fxStr tag(cmd.tail(cmd.length()-(l+1)));
	tag.remove(0, tag.skip(0, " \t"));
	// cleanup command and convert to lower case
	cmd.resize(l);
	cmd.lowercase();
	if (isStrCmd(cmd, tag)) {
	    ;
	} else if (isShortCmd(cmd, tag)) {
	    ;
	} else if (isTimeCmd(cmd, tag)) {
	    if (cmd == "tts" && tts == 0)
		tts = Sys::now();	// distinguish "now" from unset
	} else if (cmd == "notify") {	// email notification
	    checkNotifyValue(tag);
	} else if (cmd == "status") {
	    // check for strings w/ continued values (lines end with ``\'')
	    if (tag.length() && tag[tag.length()-1] == '\\') {
		fxBool cont = FALSE;
		fxStr s;
		do {
		    if (::fgets(line, sizeof (line)-1, fp) == NULL)
			break;
		    lineno++;
		    s = line;
		    s.resize(s.next(0,'\n'));
		    cont = (s.length() && s[s.length()-1] == '\\');
		    if (cont)
			s.resize(s.length()-1);
		    tag.append(s);
		} while (cont);
	    }
	    notice = tag;
	} else {
	    fxBool fileOK;
	    if (!isDocCmd(cmd, tag, fileOK))
		error("Ignoring unknown line \"%s\"", line);
	    else if (!fileOK)
		rejectJob = TRUE;
	}
    }
    for (int i = N(strvals)-1; i >= 0; i--)
	if (strvals[i].mustexist && (*this).*strvals[i].p == "") {
	    error("Null or missing %s in job request", strvals[i].name);
	    rejectJob = TRUE;
	}
    return (TRUE);
}

#define	DUMP(fp, vals, fmt, cast) {					\
    for (int i = N(vals)-1; i >= 0; i--)				\
	::fprintf(fp, fmt, vals[i].name, cast((*this).*vals[i].p));	\
}

void
FaxRequest::writeQFile()
{
    ::rewind(fp);
    DUMP(fp, timevals,	"%s:%u\n",);
    DUMP(fp, shortvals,	"%s:%d\n",);
    DUMP(fp, strvals,	"%s:%s\n", (char*));
    { fxStr s(notice);;
      for (u_int l = 0; (l = s.next(l,'\n')) < s.length(); l += 2)
	s.insert('\\', l);
      ::fprintf(fp, "status:%s\n", (char*) s);
    }
    ::fprintf(fp, "notify:%s\n", notifyVals[int(notify)]);
    for (u_int i = 0; i < requests.length(); i++) {
	const faxRequest& req = requests[i];
	for (int j = N(docvals)-1; j >= 0; j--)
	    if (req.op == docvals[j].op) {
		::fprintf(fp, "%s:%u:%s\n", docvals[j].name,
		    req.dirnum, (char*) req.item);
		break;
	    }
    }
    ::fflush(fp);
    ::ftruncate(fileno(fp), ftell(fp));
    // XXX maybe should fsync, but not especially portable
}
#undef DUMP

fxBool
FaxRequest::isStrCmd(const fxStr& cmd, const fxStr& tag)
{
    for (int i = N(strvals)-1; i >= 0; i--)
	if (strvals[i].name == cmd) {
	    (*this).*strvals[i].p = tag;
	    return (TRUE);
	}
    return (FALSE);
}

fxBool
FaxRequest::isShortCmd(const fxStr& cmd, const fxStr& tag)
{
    for (int i = N(shortvals)-1; i >= 0; i--)
	if (shortvals[i].name == cmd) {
	    (*this).*shortvals[i].p = ::atoi(tag);
	    return (TRUE);
	}
    return (FALSE);
}

fxBool
FaxRequest::isTimeCmd(const fxStr& cmd, const fxStr& tag)
{
    for (int i = N(timevals)-1; i >= 0; i--)
	if (timevals[i].name == cmd) {
	    (*this).*timevals[i].p = ::atoi(tag);
	    return (TRUE);
	}
    return (FALSE);
}

static fxBool
hasDotDot(const char* pathname)
{
    const char* cp = pathname;
    while (cp) {
	if (cp[0] == '.')		// NB: good enough
	    return (TRUE);
	if (cp = ::strchr(cp, '/'))
	    cp++;
    }
    return (FALSE);
}

fxBool
FaxRequest::checkDocument(const char* pathname)
{
    /*
     * Scan full pathname to disallow access to
     * files outside the spooling hiearchy.
     */
    if (pathname[0] == '/' || hasDotDot(pathname)) {
	error("Invalid document file \"%s\"", pathname);
	return (FALSE);
    }
    int fd = Sys::open(pathname, 0);
    if (fd == -1) {
	error("Can not access document file \"%s\": %%m", pathname);
	return (FALSE);
    }
    ::close(fd);
    return (TRUE);
}

fxBool
FaxRequest::isDocCmd(const fxStr& cmd, const fxStr& tag, fxBool& fileOK)
{
    for (int i = N(docvals)-1; i >= 0; i--)
	if (docvals[i].name == cmd) {
	    fxStr file(tag);			// need to extract dirnum
	    u_short dirnum;
	    u_int l = file.next(0,':');
	    if (l != file.length()) {		// extract directory index
		dirnum = (u_short) ::atoi(file);
		file.remove(0,l+1);
	    } else				// none present, zero
		dirnum = 0;
	    if (fileOK = (!docvals[i].check || checkDocument(file)))
		requests.append(faxRequest(docvals[i].op, dirnum, file));
	    return (TRUE);
	}
    return (FALSE);
}

void
FaxRequest::checkNotifyValue(const fxStr& tag)
{
    for (int i = N(notifyVals)-1; i >= 0; i--)
	 if (tag == notifyVals[i]) {
	    notify = FaxNotify(i);
	    return;
	}
    error("Invalid notify value \"%s\"", (char*) tag);
}

u_int
FaxRequest::findRequest(FaxSendOp op, u_int ix) const
{
    while (ix < requests.length()) {
	if (requests[ix].op == op)
	    return (ix);
	ix++;
    }
    return fx_invalidArrayIndex;
}

void
FaxRequest::insertFax(u_int ix, const fxStr& file)
{
    requests.insert(faxRequest(send_fax, 0, file), ix);
}

void
FaxRequest::removeItems(u_int ix, u_int n)
{
    for (u_int i = 0; i < n; i++) {
	const faxRequest& req = requests[ix+i];
	if (req.op == send_poll)
	    continue;
	struct stat sb;
	if (req.op != send_fax && i+1 < n && Sys::stat(req.item, sb) == 0) {
	    /*
	     * If there are multiple links to this document, then
	     * delay removing the imaged version in case other
	     * jobs can reuse it.  Otherwise, this is the last job
	     * to send this document and we should purge all imaged
	     * versions that have been kept around.
	     */
	    const faxRequest& pending = requests[ix+i+1];
	    if (sb.st_nlink > 1) {
		recordPendingDoc(pending.item);
		i++;			// don't remove imaged version
	    } else if (pending.op == send_fax)
		expungePendingDocs(pending.item);
	}
	Sys::unlink(req.item);
    }
    requests.remove(ix, n);
}

extern void vlogError(const char* fmt, va_list ap);

void
FaxRequest::error(const char* fmt0 ...)
{
    char fmt[128];
    ::sprintf(fmt, "%s: line %u: %s", (const char*) qfile, lineno, fmt0);
    va_list ap;
    va_start(ap, fmt0);
    vlogError(fmt, ap);
    va_end(ap);
}

fxStrDict FaxRequest::pendingDocs;

static fxStr
docid(const fxStr& file)
{
    u_int l = file.nextR(file.length(), ':');
    return (file.head(l > 0 ? l-1 : l));
}

void
FaxRequest::recordPendingDoc(const fxStr& file)
{
    // XXX suppress duplicates
    pendingDocs[docid(file)].append(" " | file);
}

void
FaxRequest::expungePendingDocs(const fxStr& file)
{
    const fxStr* others = pendingDocs.find(docid(file));
    if (others != NULL) {
	u_int l = 0;
	do
	    Sys::unlink(others->token(l, ' '));
	while (l < others->length());
    }
}
