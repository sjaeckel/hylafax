/*	$Id: faxconfig.c,v 1.5 1996/08/26 19:15:08 sam Rel $ */
/*
 * Copyright (c) 1995-1996 Sam Leffler
 * Copyright (c) 1995-1996 Silicon Graphics, Inc.
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
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"
#include "port.h"

static void
fatal(char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (isatty(fileno(stderr))) {
	vfprintf(stderr, fmt, ap);
	putc('\n', stderr);
    } else
	vsyslog(LOG_ERR, fmt, ap);
    va_end(ap);
    exit(-1);
}

extern	int cvtFacility(const char*, int*);

void
main(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;
    char* spooldir = FAX_SPOOLDIR;
    char* devid = NULL;
    char fifoname[80];
    const char* usage = "[-q queue-dir] [-m devid]";
    char* cp;
    int facility = LOG_DAEMON;

    (void) cvtFacility(LOG_FAX, &facility);
    openlog(argv[0], LOG_PID|LOG_ODELAY, facility);
    while ((c = getopt(argc, argv, "m:q:")) != -1)
	switch (c) {
	case 'q':
	    spooldir = optarg;
	    break;
	case 'm':
	    devid = optarg;
	    break;
	case '?':
	    fatal("Bad option `%c'.\nusage: %s %s [param value ...]",
		c, argv[0], usage);
	    /*NOTREACHED*/
	}
    if (devid != NULL) {
	if (devid[0] == FAX_FIFO[0])
	    strcpy(fifoname, devid);
	else
	    sprintf(fifoname, "%s.%.*s", FAX_FIFO,
		sizeof (fifoname) - sizeof (FAX_FIFO), devid);
    } else
	strcpy(fifoname, FAX_FIFO);
    for (cp = fifoname; cp = strchr(cp, '/'); *cp++ = '_')
	;
    if (chdir(spooldir) < 0)
	fatal("%s: chdir: %s", spooldir, strerror(errno));
    if (optind < argc) {
	int fifo = open(fifoname, O_WRONLY|O_NDELAY);
	if (fifo < 0)
	    fatal("%s: open: %s", fifoname, strerror(errno));
	do {
	    int quote;
	    char *cmd;

	    if (argc - optind < 2)
		fatal("Missing value for \"%s\" parameter.\n", argv[optind]);
	    cp = argv[optind+1];
	    if (*cp != '"') {
		for (; *cp && !isspace(*cp); cp++) 
		    ;
		quote = (*cp != '\0');
	    } else
		quote = 1;
	    cmd = malloc(strlen(argv[optind])+strlen(argv[optind+1])+10);
	    if (quote)
		sprintf(cmd, "C%s:\"%s\"", argv[optind], argv[optind+1]);
	    else
		sprintf(cmd, "C%s:%s", argv[optind], argv[optind+1]);
	    if (write(fifo, cmd, strlen(cmd)) != strlen(cmd))
		fatal("%s: FIFO write failed for command (%s)",
		    argv[0], strerror(errno));
	    free(cmd);
	} while ((optind += 2) < argc);
	(void) close(fifo);
    }
    exit(0);
}
