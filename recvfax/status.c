#ident	$Header: /usr/people/sam/flexkit/fax/recvfax/RCS/status.c,v 1.2 91/05/31 12:05:01 sam Exp $

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
#include "tiffio.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static	void getConfig(char* fileName, config* configp, config* deflt);

void
sendServerStatus()
{
    DIR* dirp;
    struct dirent* dentp;
    config deflt;

    if (!(dirp = opendir("."))) {
	syslog(LOG_ERR, "%s: opendir: %m", SPOOLDIR);
	sendError("Problem accessing spool directory");
	if (debug)
	    syslog(LOG_DEBUG, "EXIT");
	exit(-1);
    }
    getConfig(FAX_CONFIG, &deflt, 0);

    while ((dentp = readdir(dirp)) != 0) {
	char configName[1024];
	int fifo;

	if (strncmp(dentp->d_name, "FIFO.", 5) == 0 &&
	  (fifo = open(dentp->d_name, O_WRONLY|O_NDELAY)) > -1) {
	    config configuration;
	    char* cp;

	    (void) close(fifo);
	    cp = strchr(dentp->d_name, '.') + 1;
	    sprintf(configName, "%s.%s", FAX_CONFIG, cp);
	    getConfig(configName, &configuration, &deflt);
	    if (debug)
		syslog(LOG_DEBUG, "send \"server:%s\"",
		    configuration.faxNumber);
	    sendClient("server", "%s", configuration.faxNumber);
	}
    }
    (void) closedir(dirp);
}


void
sendClientJobStatus(Job* job, char* jobname)
{
    sendClient("jobStatus", "%s:%s:%d:%s", jobname,
	job->sender, job->tts, job->number);
}

void
sendClientJobLocked(Job* job, char* jobname)
{
    sendClient("jobLocked", "%s:%s:0:%s", jobname,
	job->sender, job->number);
}

void
sendAllStatus()
{
    Job* job;

    if (!jobList)
	jobList = readJobs();
    for(job = jobList; job; job = job->next) {
	char *jobname = job->qfile+strlen(FAX_SENDDIR)+2;
	if (job->flags & JOB_SENT)
	    continue;
	job->flags |= JOB_SENT;
	if (job->flags & JOB_LOCKED)
	    sendClientJobLocked(job, jobname);
	else
	    sendClientJobStatus(job, jobname);
    }
}

void
sendJobStatus(char* onwhat)
{
    Job* job;

    if (!jobList)
	jobList = readJobs();
    for(job = jobList; job; job = job->next) {
	char *jobname = job->qfile+strlen(FAX_SENDDIR)+2;
	if ((job->flags & JOB_SENT) || strcmp(jobname, onwhat) != 0)
	    continue;
	job->flags |= JOB_SENT;
	if (job->flags & JOB_LOCKED)
	    sendClientJobLocked(job, jobname);
	else
	    sendClientJobStatus(job, jobname);
    }
}

void
sendUserStatus(char* onwhat)
{
    Job* job;

    if (!jobList)
	jobList = readJobs();
    for(job = jobList; job; job = job->next) {
	char *jobname = job->qfile+strlen(FAX_SENDDIR)+2;
	if (job->flags & JOB_SENT)
	    continue;
	if (job->flags & JOB_LOCKED) {
	    job->flags |= JOB_SENT;
	    sendClientJobLocked(job, jobname);
	} else if (strcmp(job->sender, onwhat) == 0) {
	    job->flags |= JOB_SENT;
	    sendClientJobStatus(job, jobname);
	}
    }
}

static int
isFAXImage(TIFF* tif)
{
    u_short w;
    if (TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &w) && w != 1)
	return (0);
    if (TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &w) && w != 1)
	return (0);
    if (!TIFFGetField(tif, TIFFTAG_COMPRESSION, &w) ||
      (w != COMPRESSION_CCITTFAX3 && w != COMPRESSION_CCITTFAX4))
	return (0);
    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &w) ||
      (w != PHOTOMETRIC_MINISWHITE && w != PHOTOMETRIC_MINISBLACK))
	return (0);
    return (1);
}

static int
readQFile(int fd, char* qfile, int beingReceived, struct stat* sb)
{
    int ok = 0;
    TIFF* tif = TIFFFdOpen(fd, qfile, "r");
    if (tif) {
	ok = isFAXImage(tif);
	if (ok) {
	    u_long pageWidth, pageLength;
	    char* cp;
	    char sender[80];
	    float resolution = 98;
	    int npages;

	    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &pageWidth);
	    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &pageLength);
	    if (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &resolution)) {
		u_short resunit = RESUNIT_NONE;
		TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
		if (resunit == RESUNIT_CENTIMETER)
		    resolution *= 25.4;
	    } else
		resolution = 98;
	    if (!TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &cp))
		cp = "<unknown>";
	    strncpy(sender, cp, sizeof (sender));
	    sender[sizeof (sender)-1] = '\0';
	    npages = 0;
	    do {
		npages++;
	    } while (TIFFReadDirectory(tif));
	    sendClient("recvJob", "%d:%lu:%lu:%3.1f:%d:%d:%s",
		beingReceived, pageWidth, pageLength, resolution,
		npages, sb->st_mtime, sender);
	}
	TIFFClose(tif);
    }
    return (ok);
}

void
sendRecvStatus()
{
    DIR* dir = opendir(FAX_RECVDIR);
    if (dir != NULL) {
	struct dirent* dp;
	while (dp = readdir(dir)) {
	    char entry[1024];
	    struct stat sb;
	    int fd;

	    if (strncmp(dp->d_name, "fax", 3) != 0)
		continue;
	    sprintf(entry, "%s/%s", FAX_RECVDIR, dp->d_name);
	    if (stat(entry, &sb) < 0 || (sb.st_mode & S_IFMT) != S_IFREG)
		continue;
	    fd = open(entry, O_RDONLY);
	    if (fd > 0) {
		int beingReceived =
		   (flock(fd, LOCK_EX|LOCK_NB) < 0 && errno == EWOULDBLOCK);
		if (!readQFile(fd, entry, beingReceived, &sb) && !beingReceived)
		    sendError("Could not read recvq file \"%s\"", dp->d_name);
		close(fd);
	    }
	}
	closedir(dir);
    } else
	sendError("Can not access receive queue directory \"%s\"", FAX_RECVDIR);
}

static void
getConfig(char* fileName, config* configp, config* deflt)
{
    char* cp;
    FILE* configFile;
    char configLine[1024];

    if (!configp)
	return;
    if (deflt) {
	strcpy(configp->faxNumber, deflt->faxNumber);
    } else {
	configp->faxNumber[0] = '\0';
    }
    if (!fileName || !(configFile = fopen(fileName, "r")))
	return;
    while (fgets(configLine, sizeof (configLine)-1, configFile)) {
	if (! (cp = strchr(configLine, '#')))
	    cp = strchr(configLine, '\n');
	if (cp)
	    *cp = '\0';
	if ((cp = strchr(configLine, ':')) != 0)
	    for (*cp++ = '\0'; isspace(*cp); cp++)
		;
	else
	    continue;
	if (strcasecmp(configLine, "FAXNumber") == 0) {
	    strcpy(configp->faxNumber, cp);
	    break;
	}
    }
    (void) fclose(configFile);
}
