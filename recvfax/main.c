#ident	$Header: /usr/people/sam/flexkit/fax/recvfax/RCS/main.c,v 1.21 91/06/04 20:11:28 sam Exp $

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
#include <stdarg.h>

char*	SPOOLDIR = FAX_SPOOLDIR;

FILE*	qfd = NULL;
char	qfile[1024];
char**	dataFiles;
int*	fileTypes;
int	nDataFiles;
char	line[1024];		/* current input line */
char*	tag;			/* keyword: tag */
int	debug = 0;
Job*	jobList = 0;
int	seqnum = 0;

extern	char* getcwd(char* buf, int size);

main(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c, fifo;

    openlog(argv[0], LOG_PID, LOG_DAEMON);
    umask(077);
    while ((c = getopt(argc, argv, "q:d")) != -1)
	switch (c) {
	case 'q':
	    SPOOLDIR = optarg;
	    break;
	case 'd':
	    debug = 1;
	    syslog(LOG_DEBUG, "BEGIN");
	    break;
	case '?':
	    syslog(LOG_ERR,
		"Bad option `%c'; usage: faxd.recv [-q queue-dir] [-d]", c);
	    if (debug)
		syslog(LOG_DEBUG, "EXIT");
	    exit(-1);
	}
    if (chdir(SPOOLDIR) < 0) {
	syslog(LOG_ERR, "%s: chdir: %m", SPOOLDIR);
	sendError("Can not change to spooling directory");
	if (debug)
	    syslog(LOG_DEBUG, "EXIT");
	exit(-1);
    }
    if (debug) {
	char buf[82];
	syslog(LOG_DEBUG, "chdir to %s", getcwd(buf, 80));
    }
    fifo = open(FAX_FIFO, O_WRONLY|O_NDELAY);
    if (fifo < 0 && debug)
	syslog(LOG_ERR, "%s: open: %m", FAX_FIFO);
    while (getCommandLine() && !isCmd(".")) {
	if (isCmd("data")) {
	    checkPermission();
	    if (isTag("tiff"))
		getDataOldWay(TYPE_TIFF);
	    else if (isTag("postscript"))
		getDataOldWay(TYPE_POSTSCRIPT);
	    else 
		cantHandleData();
	} else if (isCmd("tiff")) {
	    checkPermission();
	    getData(TYPE_TIFF, atol(tag));
	} else if (isCmd("postscript")) {
	    checkPermission();
	    getData(TYPE_POSTSCRIPT, atol(tag));
	} else if (isCmd("begin")) {
	    checkPermission();
	    submitJob(tag, fifo);
	} else if (isCmd("serverStatus")) {
	    sendServerStatus();
	} else if (isCmd("allStatus")) {
	    sendAllStatus();
	} else if (isCmd("userStatus")) {
	    sendUserStatus(tag);
	} else if (isCmd("jobStatus")) {
	    sendJobStatus(tag);
	} else if (isCmd("recvStatus")) {
	    sendRecvStatus();
	} else if (isCmd("remove")) {
	    checkPermission();
	    removeJob(tag, fifo);
	} else if (isCmd("alterTTS")) {
	    checkPermission();
	    alterJobTTS(tag, fifo);
	} else if (isCmd("alterNotify")) {
	    checkPermission();
	    alterJobNotification(tag, fifo);
	} else {
	    syslog(LOG_ERR,
		"protocol botch, unrecognized command \"%s\"", line);
	    sendError("Protocol botch, unrecognized command \"%s\"", line);
	    if (debug)
		syslog(LOG_DEBUG, "EXIT");
	    exit(1);
	}
    }
    /* remove original files -- only links remain */
    { int i;
      for (i = 0; i <nDataFiles; i++)
	unlink(dataFiles[i]);
    }
    if (debug)
	syslog(LOG_DEBUG, "END");
    exit(0);
}

int
getCommandLine()
{
    char* cp;

    if (!fgets(line, sizeof (line) - 1, stdin)) {
	syslog(LOG_ERR, "protocol botch, unexpected EOF");
	sendError("Protocol botch, unexpected EOF");
	return (0);
    }
    cp = strchr(line, '\n');
    if (cp)
	*cp = '\0';
    if (debug)
	syslog(LOG_DEBUG, "line \"%s\"", line);
    if (strcmp(line, ".") && strcmp(line, "..")) {
	tag = strchr(line, ':');
	if (!tag) {
	    syslog(LOG_ERR, "protocol botch, line \"%s\"", line);
	    sendError("Protocol botch, malfored line \"%s\"", line);
	    return (0);
	}
	*tag++ = '\0';
	while (isspace(*tag))
	    tag++;
    }
    return (1);
}

void
sendClient(char* tag, char* va_alist, ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "%s:", tag);
    vfprintf(stdout, fmt, ap);
    fputc('\n', stdout);
    va_end(ap);
}
#undef fmt

void
sendError(char* va_alist, ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "error:");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, ".\n");
    va_end(ap);
}
#undef fmt
