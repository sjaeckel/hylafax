#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxRequest.c++,v 1.12 91/05/23 12:25:49 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <ctype.h>
#include <unistd.h>
#include "SLinearPattern.h"
#include "FaxRequest.h"
#include "config.h"
extern "C" {
#include <syslog.h>
void syslog(int, const char* ...);
}
#ifndef CYPRESS_XGL
extern "C" int ftruncate(const long, const long);
#endif

FaxRequest::FaxRequest(const fxStr& qf) : qfile(qf)
{
    tts = 0;
    fp = NULL;
    status = FALSE;
    npages = 0;
    notify = no_notice;
}

FaxRequest::~FaxRequest()
{
    if (fp)
	fclose(fp);
}

#define	isCmd(cmd)	(strcasecmp(line, cmd) == 0)

// Return True for success, False for failure
fxBool
FaxRequest::readQFile(int fd)
{
    if (fd > -1)
	fp = fdopen(fd, "r+w");
    else
	fp = fopen((char*) qfile, "r+w");
    if (fp == NULL) {
	syslog(LOG_ERR, "%s: open: %m", (char*) qfile);
	return FALSE;
    }
    char line[1024];
    char* cp;
    // XXX maybe use object transcription!!!
    while (fgets(line, sizeof (line) - 1, fp)) {
	if (line[0] == '#')
	    continue;
	if (cp = strchr(line, '\n'))
	    *cp = '\0';
	char* tag = strchr(line, ':');
	if (!tag) {
	    syslog(LOG_ERR, "%s: Malformed line \"%s\"", (char*) qfile, line);
	    return FALSE;
	}
	*tag++ = '\0';
	while (isspace(*tag))
	    tag++;
	if (isCmd("tts")) {		// time to send
	    tts = atoi(tag);
	    if (tts == 0)
		tts = time(0);		// distinguish "now" from unset
	} else if (isCmd("killtime")) {	// time to kill job
	    killtime = tag;
	} else if (isCmd("killjob")) {	// at job to kill fax job
	    killjob = tag;
	} else if (isCmd("sendjob")) {	// at job to send fax job
	    sendjob = tag;
	} else if (isCmd("number")) {	// phone number
	    number = tag;
	} else if (isCmd("sender")) {	// sender's name
	    sender = tag;
	} else if (isCmd("mailaddr")) {	// return mail address
	    mailaddr = tag;
	} else if (isCmd("resolution")) {// vertical resolution in dpi
	    resolution = atof(tag);
	} else if (isCmd("npages")) {	// number of pages
	    npages = atoi(tag);
	} else if (isCmd("pagewidth")) {// page width in pixels
	    pagewidth = atoi(tag);
	} else if (isCmd("pagelength")) {// page length in mm
	    pagelength = atof(tag);
	} else if (isCmd("notify")) {	// email notification
	    if (strcmp(tag, "when done") == 0) {
		notify = when_done;
	    } else if (strcmp(tag, "when requeued") == 0) {
		notify = when_requeued;
	    } else if (strcmp(tag, "none") == 0) {
		notify = no_notice;
	    } else {
		syslog(LOG_ERR, "job %s: Invalid notify value \"%s\"\n",
		    (char*) qfile, tag);
	    }
	} else if (isCmd("postscript") || isCmd("tiff")) {// document file
	    fxBool format = isCmd("postscript");
	    // XXX scan full pathname to avoid security holes
	    if (tag[0] == '.' || tag[0] == '/') {
		syslog(LOG_ERR,
		    "%s: Invalid data file \"%s\"; not in same directory",
		    (char*) qfile, tag);
		return FALSE;
	    }
	    if (access(tag, 4) != 0) {
		syslog(LOG_ERR, "%s: Can not access data file \"%s\"",
		    (char*) qfile, tag);
		return FALSE;
	    }
	    files.append(tag);
	    formats.append(format);
	} else
	    syslog(LOG_INFO, "%s: ignoring unknown command line \"%s: %s\"",
		(char*) qfile, line, tag);
    }
    if (tts == 0) {
	syslog(LOG_INFO, "%s: not ready to send yet", (char*) qfile);
	return FALSE;
    }
    if (number == "" || sender == "" || mailaddr == "") {
	syslog(LOG_ERR, "%s: Malformed job description, "
	    "missing number|sender|mailaddr", (char*) qfile);
	return FALSE;
    }
    if (files.length() > 0)
	return TRUE;
    syslog(LOG_ERR, "%s: No files to send (number \"%s\")",
	(char*) qfile, (char*) number);
    return FALSE;
}

void
FaxRequest::writeQFile()
{
    rewind(fp);
    fprintf(fp, "tts:%d\n", tts);
    fprintf(fp, "number:%s\n", (char*) number);
    fprintf(fp, "sender:%s\n", (char*) sender);
    fprintf(fp, "resolution:%g\n", resolution);
    fprintf(fp, "npages:%d\n", npages);
    fprintf(fp, "pagewidth:%d\n", pagewidth);
    fprintf(fp, "pagelength:%g\n", pagelength);
    fprintf(fp, "mailaddr:%s\n", (char*) mailaddr);
    if (killjob.length() == 0) {
	if (killtime.length() == 0)
	    killtime = "now + 1 day";
	fxStr atCmd = "/bin/echo ";
	fxSLinearPattern pat = FAX_QFILEPREF;
	int qfileTail = pat.findEnd(qfile);
	atCmd = atCmd
	    | FAX_RM
	    | " "
	    | qfile.extract(qfileTail, qfile.length()-qfileTail)
	    | " | "
	    | FAX_ATCMD
	    | " "
	    | killtime;
	FILE* atFile = popen(atCmd, "r");
	if (atFile) {
	    char buf[80];
	    if (fgets(buf, sizeof (buf)-1, atFile) == 0) {
		syslog(LOG_ERR, "read from \"%s\" failed: %m", (char*) atCmd);
		pclose(atFile);
	    } else {
		killjob = buf;
		pclose(atFile);
		fprintf(fp, "killjob:%s\n", (char*) killjob);
		syslog(LOG_INFO, "%s: submitted at job \"%s\" to kill request",
		    (char*) qfile, (char*) killjob);
	    }
	} else {
	    syslog(LOG_ERR, "%s: could not submit kill job to at: %m",
		(char*) qfile);
	}
    } else
	fprintf(fp, "killjob:%s\n", (char*) killjob);
    fprintf(fp, "killtime:%s\n", (char*) killtime);
    if (notify == when_requeued)
	fprintf(fp, "notify:when requeued\n");
    else if (notify == when_done)
	fprintf(fp, "notify:when done\n");
    for (u_int i = 0, n = files.length(); i < n; i++)
	fprintf(fp, "%s:%s\n", formats[i] ? "postscript" : "tiff",
	    (char*) files[i]);
    if (ftruncate(fileno(fp), ftell(fp)) < 0)
	syslog(LOG_ERR, "%s: truncate failed: %m", (char*) qfile);
}
