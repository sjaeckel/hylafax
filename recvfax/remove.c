/*	$Header: /usr/people/sam/fax/./recvfax/RCS/remove.c,v 1.23 1995/04/08 21:43:11 sam Rel $ */
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
#include "defs.h"
#include <sys/file.h>

static void
reallyRemoveJob(const char* op, Job* job, const char* jobname)
{
    char line[1024];		/* call it line to use isCmd() on it */
    char* cp;
    char* tag;
    char* cmd;
    int fd;
    FILE* fp;

    fp = fopen((char*) job->qfile, "r+w");
    if (fp == NULL) {
	syslog(LOG_ERR, "%s: cannot open %s (%m)", op, job->qfile);
	sendClient("openFailed", "%s", jobname);
	return;
    }
    fd = fileno(fp);
    /*
     * First ask server to do removal.  If the job is being
     * processed, it will first be aborted.  Otherwise, do
     * the cleanup here.
     */
    cmd = (strcmp(op, "remove") == 0 ? "R" : "K");
    if (notifyServer(job->modem, "%s%s", cmd, job->qfile)) {
	sendClient("removed", "%s", jobname);
    } else if (flock(fd, LOCK_EX|LOCK_NB) == 0) {
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
	    if (cp = strchr(tag, ':'))	/* skip optional directory index */
		tag = cp+1;
	    if (isCmd("tiff") || isCmd("!tiff") ||
		isCmd("postscript") || isCmd("!postscript") ||
		isCmd("fax") || isCmd("data")) {
		if (unlink(tag) < 0) {
		    syslog(LOG_ERR, "%s: unlink %s failed (%m)", op, tag);
		    sendClient("docUnlinkFailed", "%s", jobname);
		}
	    }
	}
	if (unlink(job->qfile) < 0) {
	    syslog(LOG_ERR, "%s: unlink %s failed (%m)", op, job->qfile);
	    sendClient("unlinkFailed", "%s", jobname);
	} else {
	    syslog(LOG_INFO, "%s %s completed", 
		strcmp(op, "remove") == 0 ? "REMOVE" : "KILL",
		job->qfile);
	    sendClient("removed", "%s", jobname);
	}
    }
    job->flags |= JOB_INVALID;
    (void) fclose(fp);			/* implicit unlock */
}

#define	DEFINE_Op(op)						\
void op##Job(const char* modem, char* tag)			\
    { applyToJob(modem, tag, "##op##", do##op); }		\
void op##JobGroup(const char* modem, char* tag)			\
    { applyToJobGroup(modem, tag, "##op##", do##op); }

static void
doremove(Job* job, const char* jobname, const char* arg)
{
    (void) arg;
    reallyRemoveJob("remove", job, jobname);
}
DEFINE_Op(remove)

static void
dokill(Job* job, const char* jobname, const char* arg)
{
    (void) arg;
    reallyRemoveJob("kill", job, jobname);
}
DEFINE_Op(kill)
