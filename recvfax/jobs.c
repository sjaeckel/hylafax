/*	$Header: /usr/people/sam/fax/./recvfax/RCS/jobs.c,v 1.31 1995/04/08 21:43:08 sam Rel $ */
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
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>

#define	BADID	0x10000			/* impossible value */

static int
readJob(Job* job)
{
    char line[1024];
    char* cp;
    char* tag;
    FILE* fp;

    fp = fopen((char*) job->qfile, "r");
    if (fp == NULL)
	return (0);
    if (flock(fileno(fp), LOCK_SH|LOCK_NB) < 0 && errno == EWOULDBLOCK)
	job->flags |= JOB_LOCKED;
    job->jobid = BADID;
    job->groupid = BADID;
    while (fgets(line, sizeof (line) - 1, fp)) {
	if (line[0] == '#')
	    continue;
	if (cp = strchr(line, '\n'))
	    *cp = '\0';
	tag = strchr(line, ':');
	if (!tag || !*tag)
	    continue;
	*tag++ = '\0';
	while (isspace(*tag))
	    tag++;
	if (isCmd("tts")) {
	    job->tts = atoi(tag);
	} else if (isCmd("jobid")) {
	    job->jobid = atoi(tag);
	} else if (isCmd("groupid")) {
	    job->groupid = atoi(tag);
	} else if (isCmd("killtime")) {
	    job->killtime = strdup(tag);
	} else if (isCmd("number")) {
	    job->number = strdup(tag);
	} else if (isCmd("external")) {
	    job->external = strdup(tag);
	} else if (isCmd("sender")) {
	    job->sender = strdup(tag);
	} else if (isCmd("mailaddr")) {
	    job->mailaddr = strdup(tag);
	} else if (isCmd("status")) {
	    job->status = strdup(tag);
	} else if (isCmd("modem")) {
	    job->modem = strdup(tag);
	}
    }
    if (job->status == NULL)
	job->status = strdup("");
    if (job->modem == NULL)
	job->modem = strdup(MODEM_ANY);
    if (job->external == NULL && job->number != NULL)
	job->external = strdup(job->number);
    if ((job->flags & JOB_LOCKED) == 0)
	(void) flock(fileno(fp), LOCK_UN);
    fclose(fp);
    /* NB: number and sender are uniformly assumed to exist */
    return (job->jobid != BADID && job->groupid != BADID &&
	job->number != NULL && job->sender != NULL);
}

static Job*
newJob(const char* qfile)
{
    Job* job = (Job*) malloc(sizeof(Job));
    memset((char*) job, 0, sizeof (*job));
    job->qfile = malloc(sizeof (FAX_SENDDIR)-1 + strlen(qfile) + 2);
    sprintf(job->qfile, "%s/%s", FAX_SENDDIR, qfile);
    return (job);
}

static void
freeJob(Job* job)
{
    if (job->qfile)
	free(job->qfile);
    if (job->killtime)
	free(job->killtime);
    if (job->sender)
	free(job->sender);
    if (job->mailaddr)
	free(job->mailaddr);
    if (job->number)
	free(job->number);
    if (job->external)
	free(job->external);
    if (job->status)
	free(job->status);
    if (job->modem)
	free(job->modem);
    free(job);
}

Job*
readJobs()
{
    DIR* dirp;
    struct dirent* dentp;
    Job* jobs = 0;
    Job* jobp = 0;

    if (!(dirp = opendir(FAX_SENDDIR))) {
	syslog(LOG_ERR, "%s: opendir: %m", FAX_SENDDIR);
	sendError("Problem accessing send directory.");
	done(-1, "EXIT");
    }
    for (dentp = readdir(dirp); dentp; dentp = readdir(dirp)) {
	if (dentp->d_name[0] != 'q')
	    continue;
	jobp = newJob(dentp->d_name);
	if (jobp) {
	    if (readJob(jobp)) {
		jobp->next = jobs;
		jobs = jobp;
	    } else
		freeJob(jobp);
	}
    }
    closedir(dirp);
    return jobs;
}

int
modemMatch(const char* a, const char* b)
{
    return strcmp(a, MODEM_ANY) == 0 || strcmp(b, MODEM_ANY) == 0 ||
	strcmp(a, b) == 0;
}

static int
checkUser(const char* requestor, struct passwd* pwd)
{
    char buf[1024];
    char* cp;

    buf[0] = '\0';
    if (pwd->pw_gecos) {
	if (pwd->pw_gecos[0] == '&') {
	    strcpy(buf, pwd->pw_name);
	    strcat(buf, pwd->pw_gecos+1);
	    if (islower(buf[0]))
		buf[0] = toupper(buf[0]);
	} else
	    strcpy(buf, pwd->pw_gecos);
	if ((cp = strchr(buf,',')) != 0)
	    *cp = '\0';
    } else
	strcpy(buf, pwd->pw_name);
    if (debug) {
	if (*buf)
	     syslog(LOG_DEBUG, "%s user: \"%s\"", pwd->pw_name, buf);
	else
	     syslog(LOG_DEBUG, "name of %s user unknown", pwd->pw_name);
    }
    return (strcmp(requestor, buf) == 0);
}

static u_int
setupJobOp(char* tag, const char* op, char** preq, char** parg)
{
    if (!jobList)
	jobList = readJobs();

    if ((*preq = strchr(tag, ':')) == 0) {
	protocolBotch("no requestor name for \"%s\" command.", op);
	done(1, "EXIT");
    }
    *(*preq)++ = '\0';
    *parg = strchr(*preq, ':');
    if (*parg)
	*(*parg)++ = '\0';
    return ((u_int) atoi(tag));
}

static int
isAdmin(const char* requestor)
{
    static int checked = 0;
    static int isadmin = 0;

    if (!checked) {
	struct passwd* pwd = getpwuid(getuid());

	if (!pwd) {
	    syslog(LOG_ERR, "getpwuid failed for uid %d: %m", getuid());
	    pwd = getpwuid(geteuid());
	}
	if (!pwd) {
	    syslog(LOG_ERR, "getpwuid failed for effective uid %d: %m",
		geteuid());
	    isadmin = 0;
	}
	isadmin = checkUser(requestor, pwd);
	if (!isadmin) {					/* not fax user */
	    pwd = getpwnam("root");
	    if (!pwd) {
		syslog(LOG_ERR, "getpwnam failed for \"root\": %m");
		isadmin = 0;
	    } else
		isadmin = checkUser(requestor, pwd);	/* root user */
	}
	checked = 1;
    }
    return (isadmin);
}

void
applyToJob(const char* modem, char* tag, const char* op, jobFunc* f)
{
    Job** jpp;
    Job* job;
    char* requestor;
    char* arg;
    u_int jobid;

    jobid = setupJobOp(tag, op, &requestor, &arg);
    for (jpp = &jobList; job = *jpp; jpp = &job->next)
	if (modemMatch(modem, job->modem) && job->jobid == jobid)
	    break;
    if (job) {
	/*
	 * Validate requestor is permitted to do op to the
	 * requested job.  We permit the person that submitted
	 * the job, the fax user, and root.  Using the GECOS
	 * field in doing the comparison is a crock, but not
	 * something to change now--leave it for a protocol
	 * redesign.
	 */
	if (strcmp(requestor, job->sender) == 0 || isAdmin(requestor)) {
	    if (debug)
		syslog(LOG_DEBUG, "%s request by %s for %s",
		    op, requestor, tag);
	    (*f)(job, tag, arg);
	    if (job->flags & JOB_INVALID)
		*jpp = job->next;
	} else
	    sendClient("jobOwner", "%s", tag);
    } else
	sendClient("notQueued", "%s", tag);
}

void
applyToJobGroup(const char* modem, char* tag, const char* op, jobFunc* f)
{
    Job** jpp;
    Job* job;
    char* requestor;
    char* arg;
    u_int jobsDone = 0;
    u_int jobsSkipped = 0;
    u_int groupid;
    char jobname[32];

    groupid = setupJobOp(tag, op, &requestor, &arg);

    jpp = &jobList;
    while (job = *jpp) {
	if (modemMatch(modem, job->modem) && job->groupid == groupid) {
	    /*
	     * Validate requestor is permitted to do op to the
	     * requested job.  We permit the person that submitted
	     * the job, the fax user, and root.  Using the GECOS
	     * field in doing the comparison is a crock, but not
	     * something to change now--leave it for a protocol
	     * redesign.
	     */
	    if (strcmp(requestor, job->sender) && !isAdmin(requestor)) {
		jobsSkipped++;
		continue;
	    }
	    if (debug)
		syslog(LOG_DEBUG, "%s request by %s for %s",
		    op, requestor, tag);
	    sprintf(jobname, "%u", job->jobid);
	    (*f)(job, jobname, arg);
	    jobsDone++;
	    if (job->flags & JOB_INVALID) {		/* remove job */
		*jpp = job->next;
		continue;
	    }
	}
	jpp = &job->next;
    }
    if (jobsDone == 0) {
	if (jobsSkipped > 0)
	    sendClient("jobOwner", "%s", tag);
	else
	    sendClient("notQueued", "%s", tag);
    }
}
