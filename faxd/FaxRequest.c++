/*	$Header: /usr/people/sam/fax/faxd/RCS/FaxRequest.c++,v 1.50 1994/07/04 18:37:16 sam Exp $ */
/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
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
#include <unistd.h>
#include <syslog.h>
#include <osfcn.h>
#include <fcntl.h>

#include "FaxRequest.h"
#include "config.h"

RegEx FaxRequest::jobidPat(FAX_QFILEPREF);

FaxRequest::FaxRequest(const fxStr& qf) : qfile(qf)
{
    tts = 0;
    killtime = 0;
    (void) jobidPat.Find(qf);
    int qfileTail = jobidPat.EndOfMatch();
    jobid = qf.extract(qfileTail, qf.length()-qfileTail);
    fp = NULL;
    status = send_retry;
    npages = 0;
    totpages = 0;
    ntries = 0;
    ndials = 0;
    totdials = 0;
    maxdials = (u_short) FAX_RETRIES;
    notify = no_notice;
}

FaxRequest::~FaxRequest()
{
    if (fp)
	fclose(fp);
}

#define	N(a)		(sizeof (a) / sizeof (a[0]))

static const struct {
    const char* name;
    fxStr FaxRequest::* p;
} strvals[] = {
    { "external",	&FaxRequest::external },	// NB: must precede number
    { "number",		&FaxRequest::number },
    { "mailaddr",	&FaxRequest::mailaddr },
    { "sender",		&FaxRequest::sender },
    { "jobtag",		&FaxRequest::jobtag },
    { "pagehandling",	&FaxRequest::pagehandling },
    { "modem",		&FaxRequest::modem },
    { "receiver",	&FaxRequest::receiver },
    { "company",	&FaxRequest::company },
    { "location",	&FaxRequest::location },
    { "cover",		&FaxRequest::cover },
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
    { "pagewidth",	&FaxRequest::pagewidth },
    { "resolution",	&FaxRequest::resolution },
    { "pagelength",	&FaxRequest::pagelength },
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
    { "poll",		FALSE,	send_poll },
    { "tiff",		TRUE,	send_tiff },
    { "!tiff",		FALSE,	send_tiff_saved },
    { "postscript",	TRUE,	send_postscript },
    { "!postscript",	FALSE,	send_postscript_saved },
    { "fax",		FALSE,	send_fax },
};
static const char* notifyVals[] = { "none", "when done", "when requeued" };

fxBool
FaxRequest::readQFile(int fd)
{
    if (fd > -1)
	fp = fdopen(fd, "r+w");
    else
	fp = fopen((char*) qfile, "r+w");
    if (fp == NULL) {
	syslog(LOG_ERR, "%s: open: %m", (char*) qfile);
	return (FALSE);
    }
    char line[2048];
    lineno = 0;
    while (fgets(line, sizeof (line) - 1, fp)) {
	lineno++;
	if (line[0] == '#')
	    continue;
	fxStr cmd(line);
	cmd.resize(cmd.next(0, '\n'));
	u_int l = cmd.next(0,':');
	if (l >= cmd.length()) {
	    error("Malformed line \"%s\"", line);
	    return (FALSE);
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
		tts = time(0);		// distinguish "now" from unset
	} else if (cmd == "notify") {	// email notification
	    checkNotifyValue(tag);
	} else if (cmd == "status") {
	    // check for strings w/ continued values (lines end with ``\'')
	    if (tag.length() && tag[tag.length()-1] == '\\') {
		fxBool cont = FALSE;
		fxStr s;
		do {
		    if (fgets(line, sizeof (line)-1, fp) == NULL)
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
	    if (isDocCmd(cmd, tag, fileOK)) {
		if (!fileOK)
		    return (FALSE);
	    } else
		error("Ignoring unknown qfile line \"%s\"", line);
	}
    }
    if (tts == 0) {
	error("Invalid time-to-send (zero)");
	return (FALSE);
    }
    if (number == "" || sender == "" || mailaddr == "") {
	error("Malformed job description, missing number|sender|mailaddr");
	return (FALSE);
    }
    if (killtime == 0) {
	error("No kill time, or time is zero");
	return (FALSE);
    }
    if (files.length() == 0) {
	error("No files to send");
	return (FALSE);
    }
    if (modem == "")
	modem = MODEM_ANY;
    return (TRUE);
}

#define	DUMP(fp, vals, fmt, cast) {					\
    for (int i = N(vals)-1; i >= 0; i--)				\
	fprintf(fp, fmt, vals[i].name, cast((*this).*vals[i].p));	\
}

void
FaxRequest::writeQFile()
{
    rewind(fp);
    DUMP(fp, timevals,	"%s:%u\n",);
    DUMP(fp, shortvals,	"%s:%d\n",);
    DUMP(fp, strvals,	"%s:%s\n", (char*));
    { fxStr s(notice);;
      for (u_int l = 0; (l = s.next(l,'\n')) < s.length(); l += 2)
	s.insert('\\', l);
      fprintf(fp, "status:%s\n", (char*) s);
    }
    fprintf(fp, "notify:%s\n", notifyVals[int(notify)]);
    for (u_int i = 0; i < files.length(); i++)
	for (int j = N(docvals)-1; j >= 0; j--)
	    if (ops[i] == docvals[j].op) {
		fprintf(fp, "%s:%u:%s\n", docvals[j].name, dirnums[i],
		    (char*) files[i]);
		break;
	    }
    fflush(fp);
    if (ftruncate(fileno(fp), ftell(fp)) < 0)
	syslog(LOG_ERR, "%s: truncate failed: %m", (char*) qfile);
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
	    (*this).*shortvals[i].p = atoi(tag);
	    return (TRUE);
	}
    return (FALSE);
}

fxBool
FaxRequest::isTimeCmd(const fxStr& cmd, const fxStr& tag)
{
    for (int i = N(timevals)-1; i >= 0; i--)
	if (timevals[i].name == cmd) {
	    (*this).*timevals[i].p = atoi(tag);
	    return (TRUE);
	}
    return (FALSE);
}

fxBool
FaxRequest::checkDocument(const char* pathname)
{
    /*
     * XXX Scan full pathname to avoid security holes (e.g. foo/../../..)
     */
    if (pathname[0] == '.' || pathname[0] == '/') {
	error("Invalid document file \"%s\" (not in same directory)", pathname);
	return (FALSE);
    }
    int fd = open(pathname, 0);
    if (fd == -1) {
	error("Can not access document file \"%s\": %%m", pathname);
	return (FALSE);
    }
    close(fd);
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
		dirnum = (u_short) atoi(file);
		file.remove(0,l+1);
	    } else				// none present, zero
		dirnum = 0;
	    if (fileOK = (!docvals[i].check || checkDocument(file)))
		appendItem(file, dirnum, docvals[i].op);
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

void
FaxRequest::appendItem(const fxStr& file, u_short dirnum, FaxSendOp op)
{
    files.append(file);
    dirnums.append(dirnum);
    ops.append(op);
}

void
FaxRequest::insertItem(u_int ix, const fxStr& file, u_short dirnum, FaxSendOp op)
{
    files.insert(file, ix);
    dirnums.insert(dirnum, ix);
    ops.insert(op, ix);
}

void
FaxRequest::removeItems(u_int ix, u_int n)
{
    for (u_int i = 0; i < n; i++)
	if (ops[i] != send_poll)
	    ::unlink((char*) files[ix + i]);
    files.remove(ix, n);
    dirnums.remove(ix, n);
    ops.remove(ix, n);
}

#include <stdarg.h>

void
FaxRequest::error(const char* fmt, ...)
{
    char buf[2200];		// NB: big enough for line[2048]
    sprintf(buf, "%s: line %u: ", (char*) qfile, lineno);
    va_list ap;
    va_start(ap, fmt);
    vsprintf(strchr(buf,'\0'), fmt, ap);
    va_end(ap);
    syslog(LOG_ERR, buf);
}

fxIMPLEMENT_PrimArray(FaxSendOpArray, FaxSendOp);
fxIMPLEMENT_PrimArray(DirnumArray, u_short);
