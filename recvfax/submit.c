#ident	$Header: /usr/people/sam/flexkit/fax/recvfax/RCS/submit.c,v 1.4 91/06/04 20:11:35 sam Exp $

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

#include <fcntl.h>
#include <sys/file.h>
#include <dirent.h>
#include <malloc.h>

static	int getSequenceNumber();
static	void coverProtocol(int version, int seqnum);
static	void setupDataFiles(int seqnum);
static	void cleanupJob();

void
submitJob(char* otag, int fifo)
{
    int sendLater = 0;

    sprintf(qfile+1, "%s/q%d", FAX_SENDDIR, seqnum = getSequenceNumber());
    qfd = fopen(qfile+1, "w");
    if (qfd == NULL) {
	syslog(LOG_ERR, "%s: Can not create qfile: %m", qfile+1);
	sendError("Can not create qfile \"%s\"", qfile+1);
	cleanupJob();
    }
    flock(fileno(qfd), LOCK_EX);
    for (;;) {
	if (!getCommandLine())
	    cleanupJob();
	if (isCmd("end") || isCmd(".")) {
	    setupDataFiles(seqnum);
	    break;
	}
	if (isCmd("sendAt")) {
	    char buf[1024];
	    FILE *atFile;
	    sendLater = 1;
	    sprintf(buf, "/bin/echo %s %s | %s %s",
		FAX_SUBMIT, qfile+1, FAX_ATCMD, tag);
	    if (debug)
		syslog(LOG_DEBUG, "popen(%s, \"r\")", buf);
	    atFile = popen((char*) buf, "r");
	    if (atFile) {
		(void) fgets(buf, sizeof(buf)-1, atFile);
		pclose(atFile);
	    }
	    fprintf(qfd, "sendjob:%s\n", buf);
	    fprintf(qfd, "%s:%s\n", line, tag);
	    syslog(LOG_INFO, "%s: at job \"%s\" submitted for \"%s\"",
		qfile+1, buf, tag);
	} else if (isCmd("cover")) {
	    coverProtocol(atoi(tag), seqnum);
	} else
	    fprintf(qfd, "%s:%s\n", line, tag);	/* XXX check info */
    }
    if (!sendLater) {
	fprintf(qfd, "tts:0\n");
	fclose(qfd), qfd = NULL;
	if (fifo >= 0) {
	    int len;
	    qfile[0] = 'S';			/* send command */
	    len = strlen(qfile);
	    if (write(fifo, qfile, len+1) != len+1) {
		syslog(LOG_ERR, "fifo write failed for send (%m)");
		sendError("Warning, no server appears to be running");
	    } else if (debug)
		syslog(LOG_DEBUG, "wrote \"%s\" to FIFO", qfile);
	}
    } else {
	fclose(qfd), qfd = NULL;
    }
    if (debug)
	syslog(LOG_DEBUG, "send \"job:%d\"", seqnum);
    sendClient("job", "%d", seqnum);
}

static void
coverProtocol(int version, int seqnum)
{
    char template[1024];
    FILE* fd;

    sprintf(template, "%s/doc%d.cover", FAX_DOCDIR, seqnum);
    fd = fopen(template, "w");
    if (fd == NULL) {
	syslog(LOG_ERR, "%s: Could not create cover sheet file: %m",
	    template);
	sendError("Could not create cover sheet file \"%s\"", template);
	cleanupJob();
    }
    while (getCommandLine()) {
	if (isCmd("..")) {
	    fprintf(qfd, "postscript: %s\n", template);
	    break;
	}
	fprintf(fd, "%s\n", tag);
    }
    fclose(fd);
}

static void
setupDataFiles(int seqnum)
{
    int i;

    for (i = 0; i < nDataFiles; i++) {
	char doc[1024];
	sprintf(doc, "%s/doc%d.%d", FAX_DOCDIR, seqnum, i);
	if (link(dataFiles[i], doc) < 0) {
	    syslog(LOG_ERR, "Can not link document \"%s\": %m", doc);
	    sendError("Problem setting up document files");
	    while (--i >= 0) {
		sprintf(doc, "%s/doc%d.%d", FAX_DOCDIR, seqnum, i);
		unlink(doc);
	    }
	    cleanupJob();
	    /*NOTREACHED*/
	}
	fprintf(qfd, "%s:%s\n",
	    fileTypes[i] == TYPE_TIFF ? "tiff" : "postscript", doc);
    }
}

void
getDataOldWay(int type)
{
    long cc;

    if (fread(&cc, sizeof (long), 1, stdin) != 1) {
	syslog(LOG_ERR, "protocol botch, no data byte count");
	sendError("Protocol botch, no data byte count");
	cleanupJob();
    }
    getData(type, cc);
}

void
getData(int type, long cc)
{
    long total;
    int dfd;
    char template[80];

    if (cc <= 0)
	return;
    sprintf(template, "%s/sndXXXXXX", FAX_TMPDIR);
    dfd = mkstemp(template);
    if (dfd < 0) {
	syslog(LOG_ERR, "%s: Could not create data temp file: %m", template);
	sendError("Could not create data temp file");
	cleanupJob();
    }
    newDataFile(template, type);
    total = 0;
    while (cc > 0) {
	char buf[4096];
	int n = MIN(cc, sizeof (buf));
	if (fread(buf, n, 1, stdin) != 1) {
	    syslog(LOG_ERR,
		"protocol botch, not enough data (received %d of %d bytes)",
		total, total+cc);
	    sendError("Protocol botch, not enough data received");
	    cleanupJob();
	}
	if (write(dfd, buf, n) != n) {
	    syslog(LOG_ERR, "write error");
	    sendError("Write error");
	    cleanupJob();
	}
	cc -= n;
	total += n;
    }
    close(dfd);
    if (debug)
	syslog(LOG_DEBUG, "%s: %d-byte %s", template, total,
	    type == TYPE_TIFF ? "TIFF image" : "PostScript document");
}

void
cantHandleData()
{
    syslog(LOG_ERR, "Can not handle \"%s\"-type data", tag);
    sendError("Can not handle \"%s\"-type data", tag);
    cleanupJob();
}

void
newDataFile(char* filename, int type)
{
    int l;
    char* cp;

    if (++nDataFiles == 1) {
	dataFiles = (char**)malloc(sizeof (char*));
	fileTypes = (int*)malloc(sizeof (int));
    } else {
	dataFiles = (char**)realloc(dataFiles, nDataFiles * sizeof (char*));
	fileTypes = (int*)realloc(fileTypes, nDataFiles * sizeof (int));
    }
    l = strlen(filename)+1;
    cp = malloc(l);
    bcopy(filename, cp, l);
    dataFiles[nDataFiles-1] = cp;
    fileTypes[nDataFiles-1] = type;
}

static void
cleanupJob()
{
    int i;

    for (i = 0; i < nDataFiles; i++)
	unlink(dataFiles[i]);
    { char template[1024];
      sprintf(template, "%s/doc%d.cover", FAX_DOCDIR, seqnum);
      unlink(template);
    }
    if (qfd)
	unlink(qfile+1);
    if (debug)
	syslog(LOG_DEBUG, "EXIT");
    exit(1);
}

static int
getSequenceNumber()
{
    int seqnum;
    int fd;

    fd = open(FAX_SEQF, O_CREAT|O_RDWR, 0644);
    if (fd < 0) {
	syslog(LOG_ERR, "%s: open: %m", FAX_SEQF);
	sendError("Problem opening sequence number file");
	if (debug)
	    syslog(LOG_DEBUG, "EXIT");
	exit(-2);
    }
    flock(fd, LOCK_EX);
    seqnum = 1;
    if (read(fd, line, sizeof (line)) > 0)
	seqnum = atoi(line);
    sprintf(line, "%d", seqnum < 9999 ? seqnum+1 : 1);
    lseek(fd, 0, L_SET);
    if (write(fd, line, strlen(line)) != strlen(line)) {
	syslog(LOG_ERR, "%s: Can not update sequence number", FAX_SEQF);
	sendError("Problem updating sequence number file");
	if (debug)
	    syslog(LOG_DEBUG, "EXIT");
	exit(-3);
    }
    close(fd);			/* implicit unlock */
    if (debug)
	syslog(LOG_DEBUG, "seqnum %d", seqnum);
    return (seqnum);
}
