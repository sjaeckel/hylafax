/*	$Header: /usr/people/sam/fax/./recvfax/RCS/alter.c,v 1.20 1995/04/08 21:43:05 sam Rel $ */
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
#include <stdarg.h>

/*
 * Setup a job's description file for alteration.
 */
static FILE*
setupAlteration(Job* job, const char* jobname)
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

static int
reallyAlterJob(Job* job, const char* jobname, char* tag, const char* item)
{
    if (item) {
	FILE* fp = setupAlteration(job, jobname);
	if (fp) {
	    fprintf(fp, "%s:%s\n", tag, item);
	    (void) flock(fileno(fp), LOCK_UN);
	    (void) fclose(fp);
	    syslog(LOG_INFO, "ALTER %s %s %s completed", job->qfile, tag, item);
	    sendClient("altered", "%s", jobname);
	    return (1);
	}
    } else
	protocolBotch("no %s specification.", tag);
    return (0);
}

static void
cvtTimeToAscii(const char* spec, char* buf, const char* what)
{
    u_long when;

    if (!cvtTime(spec, &now, &when, what)) {
	done(1, "EXIT");
	/*NOTREACHED*/
    }
    sprintf(buf, "%lu", when);
}

#define	DEFINE_Alter(param)						\
void alterJob##param(const char* modem, char* tag)			\
    { applyToJob(modem, tag, "alter", reallyAlterJob##param); }		\
void alterJobGroup##param(const char* modem, char* tag)			\
    { applyToJobGroup(modem, tag, "alter", reallyAlterJob##param); }

static void
reallyAlterJobTTS(Job* job, const char* jobname, const char* spec)
{
    char tts[20];
    cvtTimeToAscii(spec, tts, "time-to-send");
    if (reallyAlterJob(job, jobname, "tts", tts))
	notifyServer(job->modem, "JT%s %s", job->qfile, tts);
}
DEFINE_Alter(TTS)

static void
reallyAlterJobKillTime(Job* job, const char* jobname, const char* spec)
{
    char killtime[20];
    cvtTimeToAscii(spec, killtime, "kill-time");
    if (reallyAlterJob(job, jobname, "killtime", killtime))
	notifyServer(job->modem, "JK%s %s", job->qfile, killtime);
}
DEFINE_Alter(KillTime)

static void
reallyAlterJobModem(Job* job, const char* jobname, const char* device)
{
    if (reallyAlterJob(job, jobname, "modem", device))
	notifyServer(job->modem, "JM%s %s", job->qfile, device);
}
DEFINE_Alter(Modem)

static void
reallyAlterJobPriority(Job* job, const char* jobname, const char* priority)
{
    if (reallyAlterJob(job, jobname, "priority", priority))
	notifyServer(job->modem, "JP%s %s", job->qfile, priority);
}
DEFINE_Alter(Priority)

static void
reallyAlterJobMaxDials(Job* job, const char* jobname, const char* max)
{
    (void) reallyAlterJob(job, jobname, "maxdials", max);
}
DEFINE_Alter(MaxDials)

static void
reallyAlterJobNotification(Job* job, const char* jobname, const char* note)
{
    (void) reallyAlterJob(job, jobname, "notify", note);
}
DEFINE_Alter(Notification)
