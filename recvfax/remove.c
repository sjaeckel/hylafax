#ident	$Header: /usr/people/sam/flexkit/fax/recvfax/RCS/remove.c,v 1.2 91/05/31 12:04:59 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "defs.h"

#include <sys/file.h>

static void
reallyRemoveJob(Job* job, int fifo, char* jobname, char* arg)
{
    char line[1024];		/* call it line to use isCmd() on it */
    char* cp;
    char* tag;
    int fd;
    FILE* fp;

    fp = fopen((char*) job->qfile, "r+w");
    if (fp == NULL) {
	syslog(LOG_ERR, "remove: cannot open %s (%m)", job->qfile);
	sendClient("openFailed", "%s", jobname);
	return;
    }
    fd = fileno(fp);
    if (flock(fd, LOCK_EX|LOCK_NB) < 0) {
	syslog(LOG_INFO, "%s locked during removal (%m)", job->qfile);
	sendClient("jobLocked", "%s", jobname);
	fclose(fp);
	return;
    }
    while (fgets(line, sizeof (line) - 1, fp)) {
	if (line[0] == '#')
	    continue;
	if (cp = strchr(line, '\n'))
	    *cp = '\0';
	tag = strchr(line, ':');
	if (tag)
	    *tag++ = '\0';
	while (isspace(*tag))
	    tag++;
	if (isCmd("tiff") || isCmd("postscript")) {
	    if (unlink(tag) < 0) {
		syslog(LOG_ERR, "remove: unlink %s failed (%m)", tag);
		sendClient("docUnlinkFailed", "%s", jobname);
	    }
	} else if (isCmd("sendjob") || isCmd("killjob")) {
	    char buf[1024];
	    sprintf(buf, "%s %s > /dev/null 2>&1\n", FAX_ATRM, tag);
	    system(buf);
	    syslog(LOG_INFO, "remove at job \"%s\"", tag);
	}
    }
    if (unlink(job->qfile) < 0) {
	syslog(LOG_ERR, "remove: unlink %s failed (%m)", job->qfile);
	sendClient("unlinkFailed", "%s", jobname);
    } else {
	syslog(LOG_INFO, "REMOVE %s completed", job->qfile);
	sendClient("removed", "%s", jobname);
    }
    job->flags |= JOB_INVALID;
    (void) flock(fileno(fp), LOCK_UN);
    (void) fclose(fp);

    /* tell server about removal */
    /* XXX this will only reach one server */
    if (fifo != -1) {
	int len;
	sprintf(line, "R%s", job->qfile);
	len = strlen(line);
	if (write(fifo, line, len+1) != len+1)
	    syslog(LOG_ERR, "fifo write failed for remove (%m)");
    }
}

void
removeJob(char* tag, int fifo)
{
    applyToJob(tag, fifo, "remove", reallyRemoveJob);
}
