/*	$Header: /usr/people/sam/fax/./recvfax/RCS/submit.c,v 1.38 1995/04/08 21:43:14 sam Rel $ */
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

#include <fcntl.h>
#include <sys/file.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

static	char qfile[1024];
static	FILE* qfd = NULL;
static	u_int jobseqnum = 0;

#define	MAXSEQNUM	32000		/* safely fits in 16 bits */
#define	NEXTSEQNUM(x)	((x) % MAXSEQNUM)

static	int getJobNumber();
static	int getGroupNumber();
static	int getDocumentNumbers(u_int);
static	void coverProtocol(int isLZW, int version, u_int seqnum);
static	void renameData(u_int seqnum);
static	void setupData(u_int seqnum);
static	void cleanupJob();
static	u_long cvtTimeOrDie(const char* spec, struct tm* ref, const char* what);
static	void setupLZW();
static	long decodeLZW(FILE*, FILE*);

void
submitJob(const char* modem, char* otag)
{
    u_long tts = 0;
    static u_int docseqnum = (u_int) -1;
    static u_int groupseqnum = (u_int) -1;

    (void) otag;
    jobseqnum = getJobNumber();
    if (groupseqnum == (u_int) -1)
	groupseqnum = getGroupNumber();
    sprintf(qfile, "%s/q%d", FAX_SENDDIR, jobseqnum);
    qfd = fopen(qfile, "w");
    if (qfd == NULL) {
	syslog(LOG_ERR, "%s: Can not create qfile: %m", qfile);
	sendError("Can not create qfile \"%s\".", qfile);
	cleanupJob();
    }
    flock(fileno(qfd), LOCK_EX);
    for (;;) {
	if (!getCommandLine())
	    cleanupJob();
	if (isCmd("end") || isCmd(".")) {
	    if (docseqnum == (u_int) -1) {
		docseqnum = getDocumentNumbers(nDataFiles);
		renameData(docseqnum);
	    }
	    setupData(jobseqnum);
	    break;
	}
	if (isCmd("sendAt")) {
	    tts = cvtTimeOrDie(tag, &now, "time-to-send");
	} else if (isCmd("killtime")) {
	    struct tm* tm;
	    if (tts) {		/* NB: assumes sendAt precedes killtime */
		time_t t = (time_t) tts;
		tm = localtime(&t);
	    } else
		tm = &now;
	    fprintf(qfd, "%s:%lu\n", line, cvtTimeOrDie(tag, tm, "kill-time"));
	} else if (isCmd("cover")) {
	    coverProtocol(0, atoi(tag), jobseqnum);
	} else if (isCmd("zcover")) {
	    coverProtocol(1, atoi(tag), jobseqnum);
	} else
	    fprintf(qfd, "%s:%s\n", line, tag);	/* XXX check info */
    }
    fprintf(qfd, "modem:%s\n", modem);
    fprintf(qfd, "jobid:%u\n", jobseqnum);
    fprintf(qfd, "groupid:%u\n", groupseqnum);
    fprintf(qfd, "client:%s\n", getClientIdentity());
    fprintf(qfd, "tts:%lu\n", tts);
    fclose(qfd), qfd = NULL;
    if (!notifyServer(modem, "S%s", qfile))
	sendError("Warning, no server appears to be running.");
    sendClient("job", version > 1 ? "%u:%u" : "%u", jobseqnum, groupseqnum);
}

static u_long
cvtTimeOrDie(const char* spec, struct tm* ref, const char* what)
{
    u_long when;

    if (!cvtTime(spec, ref, &when, what)) {
	cleanupJob();
	/*NOTREACHED*/
    }
    return (when);
}

static void
coverProtocol(int isLZW, int cc, u_int jobseqnum)
{
    char template[1024];
    FILE* fd;

    sprintf(template, "%s/doc%u.cover", FAX_DOCDIR, jobseqnum);
    fd = fopen(template, "w");
    if (fd == NULL) {
	syslog(LOG_ERR, "%s: Could not create cover sheet file: %m",
	    template);
	sendError("Could not create cover sheet file \"%s\".", template);
	cleanupJob();
    }
    if (isLZW) {
	long total;
	/*
	 * Cover sheet comes across as LZW-compressed data;
	 * the version id is the count of the decompressed
	 * data bytes to expect (sigh, what a hack!)
	 */
	setupLZW();
	total = decodeLZW(stdin, fd);
	if (total != cc) {
	    protocolBotch("not enough data received: expected %u, got %u.",
		cc, total);
	    cleanupJob();
	}
	if (debug)
	    syslog(LOG_DEBUG, "%s: %ld-byte compressed %s",
		template, cc, "PostScript document");
    } else {
	/*
	 * Old-style, data comes across as
	 * ascii terminated by "..".
	 */
	while (getCommandLine() && !isCmd(".."))
	    fprintf(fd, "%s\n", tag);
    }
    if (fflush(fd) != 0) {
	extern int errno;
	sendAndLogError("Error writing cover sheet data: %s.",
	    strerror(errno));
	cleanupJob();
    }
    fclose(fd);
    fprintf(qfd, "postscript:0:%s\n", template);
}

static void
setupData(u_int jobseqnum)
{
    int i;

    for (i = 0; i < nDataFiles; i++) {
	char doc[1024];
	sprintf(doc, "%s.%u", dataFiles[i], jobseqnum);
	if (link(dataFiles[i], doc) < 0) {
	    syslog(LOG_ERR, "Can not link document \"%s\": %m", doc);
	    sendError("Problem setting up document files.");
	    while (--i >= 0) {
		sprintf(doc, "%s.%u", dataFiles[i], jobseqnum);
		unlink(doc);
	    }
	    cleanupJob();
	    /*NOTREACHED*/
	}
	fprintf(qfd, "%s:0:%s\n"
	    , fileTypes[i] == TYPE_TIFF ? "tiff"
	    : fileTypes[i] == TYPE_POSTSCRIPT ? "postscript"
	    : "data"
	    , doc);
    }
    for (i = 0; i < nPollIDs; i++)
	fprintf(qfd, "poll:%s\n", pollIDs[i]);
}

static void
renameData(u_int seqnum)
{
    u_int i;

    for (i = 0; i < nDataFiles; i++) {
	char doc[1024];
	sprintf(doc, "%s/doc%u", FAX_DOCDIR, seqnum);
	if (rename(dataFiles[i], doc) < 0) {
	    syslog(LOG_ERR, "Can not rename document \"%s\": %m", dataFiles[i]);
	    sendError("Problem renaming document files.");
	    cleanupJob();
	    /*NOTREACHED*/
	}
	free(dataFiles[i]);
	dataFiles[i] = strdup(doc);
	seqnum = NEXTSEQNUM(seqnum+1);
    }
}

static const char*
fileDesc(int type)
{
    return type == TYPE_TIFF ? "TIFF image"
	 : type == TYPE_POSTSCRIPT ? "PostScript document"
	 : "opaque data"
	 ;
}

#ifdef MIN
#undef MIN
#endif
#define	MIN(a,b)	((a)<(b)?(a):(b))

static void
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
	sendError("Could not create data temp file.");
	cleanupJob();
    }
    newDataFile(template, type);
    total = 0;
    while (cc > 0) {
	char buf[4096];
	int n = MIN(cc, sizeof (buf));
	if (fread(buf, n, 1, stdin) != 1) {
	    protocolBotch("not enough data received: %u of %u bytes.",
		total, total+cc);
	    cleanupJob();
	}
	if (write(dfd, buf, n) != n) {
	    extern int errno;
	    sendAndLogError("Error writing data file: %s.", strerror(errno));
	    cleanupJob();
	}
	cc -= n;
	total += n;
    }
    close(dfd);
    if (debug)
	syslog(LOG_DEBUG, "%s: %ld-byte %s", template, total, fileDesc(type));
}

#define MAXCODE(n)	((1L<<(n))-1)
#define	BITS_MIN	9		/* start with 9 bits */
#define	BITS_MAX	13		/* max of 13 bit strings */
#define	CSIZE		(MAXCODE(BITS_MAX)+1)

/* predefined codes */
#define	CODE_CLEAR	0		/* code to clear string table */
#define	CODE_EOI	1		/* end-of-information code */
#define CODE_FIRST	256		/* first free code entry */
#define	CODE_MAX	MAXCODE(BITS_MAX)

typedef u_short hcode_t;		/* codes fit in 16 bits */
typedef struct code_ent {
    struct code_ent* next;
    u_short	length;			/* string len, including this token */
    u_char	value;			/* data value */
    u_char	firstchar;		/* first token of string */
} code_t;
static	code_t* codetab = NULL;		/* LZW code table */

static void
setupLZW()
{
    int code;

    if (codetab != NULL)
	return;
    codetab = (code_t*) malloc(CSIZE * sizeof (code_t));
    assert(codetab != NULL);
    for (code = CODE_FIRST-1; code > CODE_EOI; code--) {
	codetab[code].value = code;
	codetab[code].firstchar = code;
	codetab[code].length = 1;
	codetab[code].next = NULL;
    }
}

/*
 * Decode a "hunk of data".
 */
#define	GetNextCode(fin, code) {				\
    nextdata = (nextdata<<8) | getc(fin);			\
    if ((nextbits += 8) < nbits) {				\
	nextdata = (nextdata<<8) | getc(fin);			\
	nextbits += 8;						\
    }								\
    code = (hcode_t)((nextdata >> (nextbits-nbits))&nbitsmask);	\
    nextbits -= nbits;						\
}

static long
decodeLZW(FILE* fin, FILE* fout)
{
    u_int nbits = BITS_MIN;
    u_int nextbits = 0;
    u_long nextdata = 0;
    u_long nbitsmask = MAXCODE(BITS_MIN);
    code_t* freep = &codetab[CODE_FIRST];
    code_t* oldcodep = codetab-1;
    code_t* maxcodep = &codetab[nbitsmask-1];
    long total = 0;

    for (;;) {
	hcode_t code;
	code_t* codep;

	GetNextCode(fin, code);
	if (code == CODE_EOI)
	    return (total);
	if (code == CODE_CLEAR) {
	    freep = &codetab[CODE_FIRST];
	    nbits = BITS_MIN;
	    nbitsmask = MAXCODE(BITS_MIN);
	    maxcodep = &codetab[nbitsmask-1];
	    GetNextCode(fin, code);
	    if (code == CODE_EOI)
		return (total);
	    putc(code, fout);
	    total++;
	    oldcodep = &codetab[code];
	    continue;
	}
	codep = &codetab[code];
	/*
	 * Add the new entry to the code table.
	 */
	freep->next = oldcodep;
	freep->firstchar = freep->next->firstchar;
	freep->length = freep->next->length+1;
	freep->value = (codep < freep) ? codep->firstchar : freep->firstchar;
	if (++freep > maxcodep) {
	    nbits++;
	    if (nbits > BITS_MAX) {
		protocolBotch("LZW code length overflow %s",
		    "(invalid compressed data)");
		cleanupJob();
		/*NOTREACHED*/
	    }
	    nbitsmask = MAXCODE(nbits);
	    maxcodep = &codetab[nbitsmask-1];
	}
	oldcodep = codep;
	if (code >= CODE_FIRST) {
	    /*
	     * Code maps to a string, copy string
	     * value to output (written in reverse).
	     */
	    char buf[1024];
	    int len = codep->length;
	    char* tp = (len > sizeof (buf) ? (char*) malloc(len) : buf) + len;
	    do {
		*--tp = codep->value;
	    } while (codep = codep->next);
	    fwrite(tp, len, 1, fout);
	    total += len;
	    if (tp != buf)
		free(tp);
	} else {
	    putc(code, fout);
	    total++;
	}
    }
#ifdef notdef
    protocolBotch("not enough data received: out of data before EOI.");
    cleanupJob();
#endif
    /*NOTREACHED*/
}

static void
getLZWData(int type, long cc)
{
    int dfd;
    char template[80];
    FILE* fout;
    long total;

    if (cc <= 0)
	return;
    sprintf(template, "%s/sndXXXXXX", FAX_TMPDIR);
    dfd = mkstemp(template);
    if (dfd < 0) {
	syslog(LOG_ERR, "%s: Could not create data temp file: %m", template);
	sendError("Could not create data temp file.");
	cleanupJob();
    }
    newDataFile(template, type);
    fout = fdopen(dfd, "w");
    setupLZW();
    total = decodeLZW(stdin, fout);
    if (total != cc) {
	protocolBotch("not enough data received: expected %u, got %u.",
	    cc, total);
	cleanupJob();
    }
    if (fflush(fout) != 0) {
	extern int errno;
	sendAndLogError("Error writing data file: %s.", strerror(errno));
	cleanupJob();
    }
    fclose(fout);
    if (debug)
	syslog(LOG_DEBUG, "%s: %ld-byte compressed %s",
	    template, cc, fileDesc(type));
}

void
getTIFFData(const char* modemname, char* tag)
{
    (void) modemname;
    getData(TYPE_TIFF, atol(tag));
}

void
getPostScriptData(const char* modemname, char* tag)
{
    (void) modemname;
    getData(TYPE_POSTSCRIPT, atol(tag));
}

void
getOpaqueData(const char* modemname, char* tag)
{
    (void) modemname;
    getData(TYPE_OPAQUE, atol(tag));
}

void
getZPostScriptData(const char* modemname, char* tag)
{
    (void) modemname;
    getLZWData(TYPE_POSTSCRIPT, atol(tag));
}

void
getZOpaqueData(const char* modemname, char* tag)
{
    (void) modemname;
    getLZWData(TYPE_OPAQUE, atol(tag));
}

void
getDataOldWay(const char* modemname, char* tag)
{
    long cc;
    int type;

    (void) modemname;
    if (isTag("tiff"))
	type = TYPE_TIFF;
    else if (isTag("postscript"))
	type = TYPE_POSTSCRIPT;
    else {
	sendAndLogError("Can not handle \"%s\"-type data", tag);
	cleanupJob();
	/*NOTREACHED*/
    }
    if (fread(&cc, sizeof (long), 1, stdin) != 1) {
	protocolBotch("no data byte count.");
	cleanupJob();
    }
    getData(type, cc);
}

void
newDataFile(char* filename, int type)
{
    if (++nDataFiles == 1) {
	dataFiles = (char**)malloc(sizeof (char*));
	fileTypes = (int*)malloc(sizeof (int));
    } else {
	dataFiles = (char**)realloc(dataFiles, nDataFiles * sizeof (char*));
	fileTypes = (int*)realloc(fileTypes, nDataFiles * sizeof (int));
    }
    dataFiles[nDataFiles-1] = strdup(filename);
    fileTypes[nDataFiles-1] = type;
}

void
newPollID(const char* modemname, char* pid)
{
    (void) modemname;
    if (++nPollIDs == 1)
	pollIDs = (char**)malloc(sizeof (char*));
    else
	pollIDs = (char**)realloc(pollIDs, nPollIDs * sizeof (char*));
    pollIDs[nPollIDs-1] = strdup(pid);
}

static void
cleanupJob()
{
    int i;

    for (i = 0; i < nDataFiles; i++)
	unlink(dataFiles[i]);
    { char template[1024];
      sprintf(template, "%s/doc%u.cover", FAX_DOCDIR, jobseqnum);
      unlink(template);
    }
    if (qfd)
	unlink(qfile);
    done(1, "EXIT");
}

static u_int
getSequenceNumber(const char* filename, u_int count, const char* kind)
{
    u_int seqnum;
    int fd;

    fd = open(filename, O_CREAT|O_RDWR, 0644);
    if (fd < 0) {
	syslog(LOG_ERR, "%s: open: %m", filename);
	sendError("Problem opening %s number file.", kind);
	done(-2, "EXIT");
    }
    flock(fd, LOCK_EX);
    seqnum = 1;
    if (read(fd, line, sizeof (line)) > 0)
	seqnum = atoi(line);
    if (seqnum < 1 || seqnum >= MAXSEQNUM) {
	syslog(LOG_WARNING, "Invalid %s number %u, resetting to 1.",
	    kind, seqnum);
	seqnum = 1;
    }
    sprintf(line, "%u", NEXTSEQNUM(seqnum+count));
    lseek(fd, 0, SEEK_SET);
    if (write(fd, line, strlen(line)) != strlen(line)) {
	sendAndLogError("Problem updating %s number file.", kind);
	done(-3, "EXIT");
    }
    close(fd);			/* implicit unlock */
    if (debug)
	syslog(LOG_DEBUG, "%s %d", kind, seqnum);
    return (seqnum);
}

static int
getJobNumber()
{
    return (getSequenceNumber(FAX_JOBSEQF, 1, "job sequence"));
}

static int
getGroupNumber()
{
    return (getSequenceNumber(FAX_GROUPSEQF, 1, "group sequence"));
}

static int
getDocumentNumbers(u_int count)
{
    return (getSequenceNumber(FAX_DOCSEQF, count, "document sequence"));
}
