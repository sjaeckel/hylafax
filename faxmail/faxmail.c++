#ident $Header: /d/sam/flexkit/fax/faxmail/RCS/faxmail.c++,v 1.6 91/05/23 12:30:00 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "OrderedGlobal.h"
#include "Application.h"
#include "Str.h"
#include "StackBuffer.h"
#include "config.h"

#include <time.h>
#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define PPI 72.0		    /* points per inch */
#define BUFFLEN 1024		    /* 1K byte buffers */
#define HIGHWATER (3 * BUFFLEN / 4)

static	float p_w =  8.0;	    /* max paper width */
static	float p_h = 10.5;	    /* max papr height */
static	float pointsize = 14.0;	    /* default pntsize */

class faxMailApp : public fxApplication {
private:
    fxStr	appName;		// for error messages
    long	oline;
    long	pagefactor;
    fxStr	italicFont;
    fxStr	boldFont;
    fxStr	bodyFont;
    fxStr	bodyFontFile;
    char	buffer[BUFFLEN];
    char	outbuf[BUFFLEN];
    int		buf_mark;
    int		buf_length;
    int		chars_per_line;
    int		lines_per_page;
    int		lm_points;
    fxBool	inHeaders;
    fxBool	lastHeaderShown;
    fxBool	doBodyFont;

    void formatMail();
    int copyline(char* buffer, char* outbuf, int& index);
    void flushHeader(const char* buf, int x);
    void flushText(const char* buf, int x);
    fxBool okToShowHeader(const char* tag);
    void lookForBodyFont(const char* fromLine);
    void loadBodyFont(const char* fileName, const char* fontName);

    void usage();
    void printError(const char* va_alist ...);
public:
    faxMailApp();
    ~faxMailApp();

    void initialize(int argc, char** argv);
    void open();
    virtual const char *className() const;
};

fxAPPINIT(faxMailApp,0);

faxMailApp::faxMailApp()
{
    pagefactor = 1;
    boldFont = "Helvetica-Bold";
    italicFont = "Helvetica-Oblique";
    bodyFont = "Courier-Bold";
    inHeaders = TRUE;
    doBodyFont = TRUE;
}

faxMailApp::~faxMailApp()
{
}

const char* faxMailApp::className() const { return "faxMailApp"; }

void
faxMailApp::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');

    while ((c = getopt(argc, argv, "f:F:p:1234n")) != -1)
	switch (c) {
	case 'f':			// font for text body
	    bodyFont = optarg;
	    break;
	case 'F':			// special body font
	    bodyFontFile = optarg;
	    doBodyFont = FALSE;
	    break;
	case 'n':			// no special body font
	    doBodyFont = FALSE;
	    break;
	case 'p':			// point size
	    pointsize = atof(optarg);
	    break;
	case '1': case '2': case '3': case '4':
	    pagefactor = c - '0';
	    break;
	case '?':
	    usage();
	}
    lm_points = 24;
    lines_per_page = (int)((p_h - 0.5) * 72.0 / pointsize);
    chars_per_line = (int)((p_w - 0.5) * 72.0 / (pointsize * 0.625));
    chars_per_line = 10000;
}

static char* prolog =
"/inch {72 mul} def\n"
"/newline {"
    "/vpos vpos %5.2f sub def\n"
    "%d vpos moveto\n"
"} def\n"
"/outline {\n"
    ".1 setlinewidth\n"
    "0.8 setgray\n"
    "gsave\n"
    ".1 inch -.1 inch translate\n"
    "newpath\n"
    "0 0 moveto\n"
    "0 11 inch lineto\n"
    "8.5 inch 11 inch lineto\n"
    "8.5 inch 0 lineto\n"
    "closepath\n"
    "fill\n"
    "grestore\n"
    "1.0 setgray\n"
    "newpath\n"
    "0 0 moveto\n"
    "0 11 inch lineto\n"
    "8.5 inch 11 inch lineto\n"
    "8.5 inch 0 lineto\n"
    "closepath\n"
    "fill\n"
    "0.0 setgray\n"
    "newpath\n"
    "0 0 moveto\n"
    "0 11 inch lineto\n"
    "8.5 inch 11 inch lineto\n"
    "8.5 inch 0 lineto\n"
    "closepath\n"
    "stroke\n"
"} def\n"
"/ishow {ifont setfont show} def\n"
"/bshow {bfont setfont show} def\n"
"/rshow {bodyfont setfont show} def\n"
;

void
faxMailApp::open()
{
    printf("%%!PS-Adobe-2.0 EPSF-2.0\n");
    printf("%%%%Creator: faxmail\n");
    printf("%%%%Title: E-Mail\n");
    time_t t = time(0);
    printf("%%%%CreationDate: %s", ctime(&t));
    printf("%%%%Origin: 0 0\n");
    printf("%%%%BoundingBox: 0 0 %d %d\n", 11*72, (int)(8.5*72));
    printf("%%%%EndComments\n");
    printf(prolog, 11.2 / 10.8 * pointsize, lm_points);
    if (bodyFontFile != "") {
	bodyFont = bodyFontFile;
	int l;
	while ((l = bodyFont.next(0, '/')) != bodyFont.length())
	    bodyFont.remove(0, l);
	bodyFont.resize(bodyFont.next(0, '.'));
	loadBodyFont(bodyFontFile, bodyFont);
    } else
	printf("/bodyfont /%s findfont %.1f scalefont def\n",
	    (char*) bodyFont, pointsize);
    printf("/ifont /%s findfont %.1f scalefont def\n",
	(char*) italicFont, pointsize);
    printf("/bfont /%s findfont %.1f scalefont def\n",
	(char*) boldFont, pointsize);
    formatMail();
    printf("%%%%Trailer\n");
    exit(0);
}

void
faxMailApp::formatMail()
{
    long pagenumber = 1;

    for (;;) {
	printf("%%%%Page: %d %d\n", pagenumber, pagenumber);
	printf("/vpos 756 def\n");		/* top of page */
	pagenumber++;
	for (long col = 0; col < pagefactor; col++)
	    for (long row = 0; row < pagefactor; row++) {
		printf("gsave\n");
		printf("%g inch %g inch translate\n",
		    .225 + col * p_w / (float) pagefactor,
		    .25 + (pagefactor - row - 1) * p_h / (float) pagefactor);
		printf("%g %g scale\n",
 			(1.0 / pagefactor) / 1.125,	/* 1.17, 1.08 */
			(1.0 / pagefactor) / 1.1);
		if (pagefactor > 1) 
		    printf("outline\n");

		oline = 0;
		while (oline < lines_per_page)	{
		    if (buf_mark > buf_length) {
			fprintf(stderr, "mark %d, length %d?\n",
			    buf_mark, buf_length);
			*(char *)0 = 0;
		    }

		    /*
		     * First check: if starting position too far right,
		     * or too few characters in the buffer, copy to
		     * left and read some more
		     */
		    if (buf_mark > HIGHWATER || buf_mark == buf_length) {
			buf_length -= buf_mark;
			strncpy(buffer, buffer + buf_mark, buf_length);
			buf_mark = 0;
			buf_length += fread(buffer+buf_length, 1,
			    BUFFLEN-buf_length, stdin);
			if (buf_length == 0) {		// EOF
			    printf("grestore\n");
			    printf("showpage\n");
			    return;
			}
		    }
	
		    /* now filter the line into 'show'-able form */
		    long dx = copyline(buffer, outbuf, buf_mark);
		    if (dx == 0)
			dx = lm_points;
		    if (inHeaders) {
			if (strlen(outbuf) > 0)
			    flushHeader(outbuf, (int) dx);
		    } else {
			if (strlen(outbuf) > 0)
			    flushText(outbuf, (int) dx);
			else {
			    printf("newline\n");
			    oline++;
			}
		    }
		}
		printf("grestore\n");
	    }
	printf("showpage\n");
    }
}

/*
 * Copyline gives back the next string argument to 'show' in outbuf,
 * and returns:
 * 0 iff the output line should be followed by a vertical repositioning
 * dx > 0 iff the output line should be shown, and FOLLOWED by a moveto dx
 */
int
faxMailApp::copyline(char* buffer, char* outbuf, int& index)
{
    static int tab, index_0, padding;
    int numchars;

    for (;;) {
	if (padding + index - index_0 >= chars_per_line) {
	    /* force a newline */
	    index_0 = index;
	    return (*outbuf = tab = padding = 0);
	}
	switch (buffer[index]) {
	case '\f':
	    index++;
	    index_0 = index;
	    oline = lines_per_page;
	    return (*outbuf = tab = padding = 0);
	case '\t':
	    numchars = index++ - index_0;
	    tab = numchars + 8 - numchars % 8;
	    while (buffer[index] == '\t') {
		index++;
		tab += 8;
	    }
	    numchars = index - index_0;
	    padding += (tab - numchars);
	    *outbuf = 0;
	    return (tab * pointsize * 0.625);
	case '\\':
	case '(':
	case ')':
	    *outbuf++ = '\\';
	    *outbuf++ = buffer[index++];
	    break;
	case '\n':
	    if (index_0 == index)
		inHeaders = FALSE;
	    index++;
	    index_0 = index;
	    return (*outbuf = tab = padding = 0);
	default:
	    *outbuf++ = buffer[index++];
	    break;
	}
    }
}

void
faxMailApp::flushHeader(const char* buf, int x)
{
    int len = strlen(buf);
    /*
     * Collect field name in a canonical format.
     * We carefully include the colon (':') so
     * that it gets emboldened below.  If the line
     * begins with whitespace, then it's the
     * continuation of a previous header.
     */ 
    if (!isspace(buf[0])) { 
	fxStackBuffer field;
	for (const char* cp = buf; cp < buf+len;) {
	    char c = *cp++;
	    if (c == ':' && cp < buf+len && *cp != '\0') {
		field.set('\0');
		if (!okToShowHeader(field)) {
		    lastHeaderShown = FALSE;
		    return;
		} else {
		    if (strcmp(field, "from") == 0 && doBodyFont)
			lookForBodyFont(cp);
		    field.put(c);
		    break;
		}
	    }
	    field.put(isupper(c) ? tolower(c) : c);
	}
	/*
	 * Embolden field name and italicize field value.
	 */
	if (field.getLength() > 0) {
	    printf("%d vpos moveto\n", x);
	    printf("(%.*s) bshow\n", field.getLength(), buf);
	    printf("(%.*s) ishow\n", len - field.getLength(), cp);
	    printf("newline\n");
	    oline++;
	    lastHeaderShown = TRUE;
	}
    } else if (lastHeaderShown) {
	printf("%d vpos moveto\n", x);
	printf("(%.*s) ishow\n", len, buf);
	printf("newline\n");
	oline++;
    }
}

void
faxMailApp::flushText(const char* outbuf, int x)
{
    printf("%d vpos moveto\n", x);
    printf("(%s) rshow\n", outbuf);
    printf("newline\n");
    oline++;
}

static char* headers[] = {
    "subject",
    "from",
    "date",
    "cc",
    "summary",
    "keywords",
    "to",
};
#define	NHEADERS	(sizeof (headers) / sizeof (headers[0]))

fxBool
faxMailApp::okToShowHeader(const char* tag)
{
    for (u_int i = 0; i < NHEADERS; i++)
	if (strcmp(headers[i], tag) == 0)
	    return (TRUE);
    return (FALSE);
}

#include <pwd.h>
#include <osfcn.h>
#include <sys/fcntl.h>

void
faxMailApp::lookForBodyFont(const char* sender)
{
    for (const char* cp = sender; isspace(*cp); cp++)
	;
    fxStr name(cp);
    int l = name.next(0, '<');
    if (l != name.length()) {	// Joe Schmo <joe@foobar>
	name.remove(0, l);
	name.resize(name.next(0, '>'));
    } else			// joe@foobar (Joe Schmo)
	name.resize(name.next(0, ' '));
    if (name.length() == 0)
	return;
    // remove @host
    name.resize(name.next(0, '@'));
    // now strip leading host!host!...
    while ((l = name.next(0, '!')) != name.length())
	name.remove(0, l+1);
    // got user account name, look for font in home directory!
    struct passwd* pwd = getpwnam(name);
    if (pwd)
	loadBodyFont(fxStr(pwd->pw_dir) | "/" | ".faxfont.ps", name);
}

void
faxMailApp::loadBodyFont(const char* fileName, const char* fontName)
{
    int fd = ::open(fileName, O_RDONLY);
    if (fd >= 0) {
	char buf[16*1024];
	int n;
	while ((n = read(fd, buf, sizeof (buf))) > 0)
	    fwrite(buf, n, 1, stdout);
	::close(fd);
	printf("/bodyfont /%s findfont %.1f scalefont def\n",
	    fontName, pointsize);
    }
}

void
faxMailApp::usage()
{
    fxFatal("usage: %s"
	" [-p pointsize]"
	" [-1234]"
	, (char*) appName);
}

void
faxMailApp::printError(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, va_alist);
    fprintf(stderr, "%s: ", (char*) appName);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
}
#undef fmt
