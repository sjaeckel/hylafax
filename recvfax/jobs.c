#ident	$Header: /usr/people/sam/flexkit/fax/recvfax/RCS/jobs.c,v 1.3 91/05/31 12:51:38 sam Exp $

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
#include <fcntl.h>
#include <dirent.h>
#include <malloc.h>
#include <pwd.h>

static Job*
readJob(Job* job)
{
    char line[1024];
    char* cp;
    char* tag;
    FILE* fp;

    job->flags = 0;
    fp = fopen((char*) job->qfile, "r");
    if (fp == NULL)
	return NULL;
    if (flock(fileno(fp), LOCK_SH|LOCK_NB) < 0)
	job->flags |= JOB_LOCKED;
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
	} else if (isCmd("killtime")) {
	    job->killtime = strdup(tag);
	} else if (isCmd("number")) {
	    job->number = strdup(tag);
	} else if (isCmd("sender")) {
	    job->sender = strdup(tag);
	} else if (isCmd("mailaddr")) {
	    job->mailaddr = strdup(tag);
	}
    }
    if ((job->flags & JOB_LOCKED) == 0)
	(void) flock(fileno(fp), LOCK_UN);
    fclose(fp);
    return job;
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
	sendError("Problem accessing send directory");
	if (debug)
	    syslog(LOG_DEBUG, "EXIT");
	exit(-1);
    }
    (void) flock(dirp->dd_fd, LOCK_SH);
    for (dentp = readdir(dirp); dentp; dentp = readdir(dirp)) {
	if (dentp->d_name[0] != 'q')
	    continue;
	jobp = malloc(sizeof(Job));
	jobp->qfile = malloc(strlen(FAX_SENDDIR) + strlen(dentp->d_name) + 2);
	sprintf(jobp->qfile, "%s/%s", FAX_SENDDIR, dentp->d_name);
	if (readJob(jobp)) {
	    jobp->next = jobs;
	    jobs = jobp;
	} else {
	    free(jobp);
	}
    }
    (void) flock(dirp->dd_fd, LOCK_UN);
    closedir(dirp);
    return jobs;
}

void
applyToJob(char* tag, int fifo, char* op, jobFunc* f)
{
    Job** job;
    struct passwd* pwd = NULL;
    char buf[1024];
    char* cp;
    char* requestor;
    char* arg;

    if (!jobList)
	jobList = readJobs();

    if ((requestor = strchr(tag, ':')) == 0) {
	syslog(LOG_ERR,
	    "protocol botch, expecting requestor name on %s command", op);
	sendError("Protocol botch, no requestor name");
	if (debug)
	    syslog(LOG_DEBUG, "EXIT");
	exit(1);
    }
    *requestor++ = '\0';
    arg = strchr(requestor, ':');
    if (arg)
	*arg++ = '\0';

    for(job = &jobList; *job; job = &((*job)->next)) {
	char *jobname = (*job)->qfile+strlen(FAX_SENDDIR)+2;
	if (strcmp(jobname, tag) == 0)
	    break;
    }
    if (!*job) {
	if (debug)
	    syslog(LOG_DEBUG, "cannot %s %s: not queued", op, tag);
	sendClient("notQueued", "%s", tag);
	return;			/* nothing to do */
    }
    if ((*job)->flags & JOB_LOCKED) {
	if (debug)
	    syslog(LOG_DEBUG, "cannot %s %s: locked", op, tag);
	sendClient("jobLocked", "%s", tag);
	return;			/* nothing to do */
    }
    if (strcmp(requestor, (*job)->sender) == 0) {
	/* alter job parameters */
	if (debug)
	    syslog(LOG_DEBUG, "%s request by owner for %s", op, tag);
	(*f)(*job, fifo, tag, arg);
	if ((*job)->flags & JOB_INVALID)
	    *job = (*job)->next;
	return;			/* nothing to do */
    }

    buf[0] = '\0';
    pwd = getpwuid(getuid());
    if (!pwd) {
	syslog(LOG_ERR, "getpwuid failed for uid %d: %m", getuid());
	pwd = getpwuid(geteuid());
    }
    if (!pwd) {
	syslog(LOG_ERR, "getpwuid failed for effective uid %d: %m", geteuid());
	sendClient("sOwner", "%s", tag);
    } else {
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
    }
    if (debug) {
	if (*buf)
	     syslog(LOG_DEBUG, "fax user: \"%s\"", buf);
	else
	     syslog(LOG_DEBUG, "name of fax user unknown");
    }
    if (strcmp(requestor, buf) == 0) {
	(*f)(*job, fifo, tag, arg);
	if ((*job)->flags & JOB_INVALID)
	    *job = (*job)->next;
    } else
	sendClient("jobOwner", "%s", tag);
}
