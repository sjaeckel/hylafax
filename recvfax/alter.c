#ident	$Header: /usr/people/sam/flexkit/fax/recvfax/RCS/alter.c,v 1.1 91/05/31 12:52:03 sam Exp $

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
#include <stdarg.h>

/*
 * Setup a job's description file for alteration.
 */
static FILE*
setupAlteration(Job* job, char* jobname)
{
    FILE* fp = fopen(job->qfile, "a+");
    if (fp == NULL) {
	syslog(LOG_ERR, "alter: cannot open %s (%m)", job->qfile);
	sendClient("openFailed", "%s", jobname);
	return (fp);
    }
    if (flock(fileno(fp), LOCK_EX|LOCK_NB) < 0) {
	syslog(LOG_INFO, "%s locked during alteration (%m)", job->qfile);
	sendClient("jobLocked", "%s", jobname);
	fclose(fp);
	return (NULL);
    }
    return (fp);
}

/*
 * Notify server of job parameter alteration.
 */
static void
notifyServer(int fifo, char* va_alist, ...)
#define	fmt va_alist
{
    if (fifo != -1) {
	char buf[1024];
	int len;
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	len = strlen(buf);
	/* XXX this will only reach one server */
	if (write(fifo, buf, len+1) != len+1)
	    syslog(LOG_ERR, "fifo write failed for alter (%m)");
    }
}
#undef fmt

static void
reallyAlterJobTTS(Job* job, int fifo, char* jobname, char* tts)
{
    if (tts) {
	FILE* fp = setupAlteration(job, jobname);
	if (fp) {
	    fprintf(fp, "tts:%s\n", tts);
	    (void) flock(fileno(fp), LOCK_UN);
	    (void) fclose(fp);
	    /* XXX at job for delayed submissions! */
	    notifyServer(fifo, "JT%s %s", job->qfile, tts);
	    syslog(LOG_INFO, "ALTER %s TTS %s completed", job->qfile, tts);
	    sendClient("altered", "%s", jobname);
	}
    } else {
	syslog(LOG_ERR, "protocol botch, expecting tts specification");
	sendError("Protocol botch, no time-to-send specification");
    }
}

static void
reallyAlterJobNotification(Job* job, int fifo, char* jobname, char* note)
{
    if (note) {
	FILE* fp = setupAlteration(job, jobname);
	if (fp) {
	    fprintf(fp, "notify:%s\n", note);
	    (void) flock(fileno(fp), LOCK_UN);
	    (void) fclose(fp);
	    /* NB: no server notification */
	    syslog(LOG_INFO, "ALTER %s NOTIFY %s completed", job->qfile, note);
	    sendClient("altered", "%s", jobname);
	}
    } else {
	syslog(LOG_ERR, "protocol botch, expecting notification specification");
	sendError("Protocol botch, no notification specification");
    }
}

void
alterJobTTS(char* tag, int fifo)
{
    applyToJob(tag, fifo, "alter", reallyAlterJobTTS);
}

void
alterJobNotification(char* tag, int fifo)
{
    applyToJob(tag, fifo, "alter", reallyAlterJobNotification);
}
