#ident $Header: /usr/people/sam/flexkit/fax/faxadmin/RCS/SendQ.c++,v 1.4 91/05/23 12:13:34 sam Exp $

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
#include "SendQ.h"
#include "ConfirmDialog.h"

SendQ::SendQ(const char* qf) : qfile(qf)
{
    jobid = atoi(qf+1);
    tts = 0;
    notify = no_notice;
}

SendQ::~SendQ()
{
}

#define	isCmd(cmd)	(strcasecmp(line, cmd) == 0)

// Return True for success, False for failure
fxBool
SendQ::readQFile(int fd, fxVisualApplication* app)
{
    FILE* fp = fdopen(fd, "r+w");
    if (fp == NULL) {
	notifyUser(app, "%s: Can not open.", (char*) qfile);
	return FALSE;
    }
    files.resize(0);
    formats.resize(0);
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
	    notifyUser(app, "%s: Malformed line \"%s\"", (char*) qfile, line);
	    fclose(fp);
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
	} else if (isCmd("npages")) {	// # of pages
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
		notifyUser(app, "job %s: Invalid notify value \"%s\"\n",
		    (char*) qfile, tag);
	    }
	} else if (isCmd("postscript") || isCmd("tiff")) {// document file
	    fxBool format = isCmd("postscript");
	    // XXX scan full pathname to avoid security holes
	    if (tag[0] == '.' || tag[0] == '/')
		notifyUser(app,
		    "%s: Invalid data file \"%s\"; not in same directory",
		    (char*) qfile, tag);
	    files.append(tag);
	    formats.append(format);
	} else
	    notifyUser(app, "%s: Ignoring unknown command line \"%s: %s\"",
		(char*) qfile, line, tag);
    }
    fclose(fp);
    return TRUE;
}

fxIMPLEMENT_PtrArray(SendQPtrArray, SendQ*);

int
SendQPtrArray::compareElements(void* a, void* b) const
{
    const SendQ* qa = *(SendQ**)a;
    const SendQ* qb = *(SendQ**)b;
    int diff = qb->tts - qa->tts;
    return (diff ? diff : qb->jobid - qa->jobid);
}
