/* $Header: /usr/people/sam/tiff/contrib/fax2ps/RCS/fax2ps.c,v 1.33.2.1 1995/03/16 18:56:30 sam Exp $" */

/*
 * Copyright (c) 1991, 1992, 1993, 1994 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"

float	defxres = 204.;		/* default x resolution (pixels/inch) */
float	defyres = 98.;		/* default y resolution (lines/inch) */
float	pageWidth = 8.5;	/* image page width (inches) */
float	pageHeight = 11.0;	/* image page length (inches) */
int	scaleToPage = FALSE;	/* if true, scale raster to page dimensions */

static	void do_font_insert(void);

static void
setupPass(TIFF* tif, int pass)
{
    uint32* stripbytecount;

    fax.pass = pass;
    fax.row = 0;
    TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripbytecount);
    fax.cc = stripbytecount[0];
    fax.bp = fax.buf;
    FaxPreDecode(tif);
}

static	int totalPages = 0;

void
printTIF(TIFF* tif, int pageNumber)
{
    uint32 w, h;
    uint16 fill, unit, photometric, compression;
    float xres, yres;
    uint32 g3opts;
    uint32* stripbytecount;

    TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripbytecount);
    fax.cc = stripbytecount[0];
    fax.buf = (u_char*) malloc(fax.cc);
    TIFFReadRawStrip(tif, 0, fax.buf, fax.cc);
    if (TIFFGetField(tif,TIFFTAG_FILLORDER, &fill) && fill != FILLORDER_MSB2LSB)
	TIFFReverseBits(fax.buf, fax.cc);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    if (!TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres)) {
	TIFFWarning(TIFFFileName(tif),
	    "No x-resolution, assuming %g dpi", defxres);
	xres = defxres;
    }
    if (!TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)) {
	TIFFWarning(TIFFFileName(tif),
	    "No y-resolution, assuming %g lpi", defyres);
	yres = defyres;					/* XXX */
    }
    if (TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &unit) &&
      unit == RESUNIT_CENTIMETER) {
	xres *= 25.4;
	yres *= 25.4;
    }
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    /*
     * Calculate the scanline/tile widths.
     */
    if (isTiled(tif)) {
        fax.b.rowbytes = TIFFTileRowSize(tif);
	TIFFGetField(tif, TIFFTAG_TILEWIDTH, &fax.b.rowpixels);
    } else {
        fax.b.rowbytes = TIFFScanlineSize(tif);
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &fax.b.rowpixels);
    }
    if (TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS, &g3opts))
	fax.is2d = (g3opts & GROUP3OPT_2DENCODING) != 0;
    else
	fax.is2d = 0;
    fax.scanline = (u_char*) malloc(2*fax.b.rowbytes);
    fax.b.refline = fax.scanline + fax.b.rowbytes;
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
    if (compression == COMPRESSION_CCITTFAX4)
	fax.options = FAX3_NOEOL;

    printf("%%%%Page: \"%d\" %d\n", pageNumber, pageNumber);
    printf("gsave\n");
    if (scaleToPage) {
	float yscale = pageHeight / (h/yres);
	float xscale = pageWidth / (w/xres);
	printf("0 %d translate\n", (int)(yscale*(h/yres)*72.));
	printf("%g %g scale\n", (72.*xscale)/xres, -(72.*yscale)/yres);
    } else {
	printf("0 %d translate\n", (int)(72.*h/yres));
	printf("%g %g scale\n", 72./xres, -72./yres);
    }
    printf("0 setgray\n");
    setupPass(tif, 3);
    while (fax.cc > 0) {
	if (compression == COMPRESSION_CCITTFAX4)
	    Fax4DecodeRow(tif, w);
	else
	    Fax3DecodeRow(tif, w);
	print_scan_row(w, fax.scanline);
    }
    printf("p\n");
    printf("grestore\n");
    free((char*) fax.scanline);
    free((char*) fax.buf);
    totalPages++;
}

#define	GetPageNumber(tif) \
TIFFGetField(tif, TIFFTAG_PAGENUMBER, &pn, &ptotal)

int
findPage(TIFF* tif, int pageNumber)
{
    uint16 pn = (uint16) -1;
    uint16 ptotal = (uint16) -1;
    if (GetPageNumber(tif)) {
	while (pn != pageNumber && TIFFReadDirectory(tif) && GetPageNumber(tif))
	    ;
	return (pn == pageNumber);
    } else
	return (TIFFSetDirectory(tif, pageNumber-1));
}

void
fax2ps(TIFF* tif, int npages, int* pages, char* filename)
{
    if (npages > 0) {
	uint16 pn, ptotal;
	int i;

	if (!GetPageNumber(tif))
	    fprintf(stderr, "%s: No page numbers, counting directories.\n",
		filename);
	for (i = 0; i < npages; i++) {
	    if (findPage(tif, pages[i]))
		printTIF(tif, pages[i]);
	    else
		fprintf(stderr, "%s: No page number %d\n", filename, pages[i]);
	}
    } else {
	int pageNumber = 1;
	do
	    printTIF(tif, pageNumber++);
	while (TIFFReadDirectory(tif));
    }
}

#undef GetPageNumber

static int
pcompar(void* va, void* vb)
{
    int* pa = (int*) va;
    int* pb = (int*) vb;
    return (*pa - *pb);
}

extern	double atof();

main(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c, pageNumber;
    int* pages = 0, npages = 0;
    int dowarnings = FALSE;	/* if 1, enable library warnings */
    long t;
    TIFF* tif;

    while ((c = getopt(argc, argv, "p:x:y:W:H:wS")) != -1)
	switch (c) {
	case 'H':		/* page height */
	    pageHeight = atof(optarg);
	    break;
	case 'S':		/* scale to page */
	    scaleToPage = TRUE;
	    break;
	case 'W':		/* page width */
	    pageWidth = atof(optarg);
	    break;
	case 'p':		/* print specific page */
	    pageNumber = atoi(optarg);
	    if (pageNumber < 1) {
		fprintf(stderr, "%s: Invalid page number (must be > 0).\n",
		    optarg);
		exit(-1);
	    }
	    if (pages)
		pages = (int*) realloc((char*) pages, (npages+1)*sizeof (int));
	    else
		pages = (int*) malloc(sizeof (int));
	    pages[npages++] = pageNumber;
	    break;
	case 'w':
	    dowarnings = TRUE;
	    break;
	case 'x':
	    defxres = atof(optarg);
	    break;
	case 'y':
	    defyres = atof(optarg);
	    break;
	case '?':
	    fprintf(stderr,
"usage: %s [-w] [-p pagenumber] [-x xres] [-y res] [-S] [-W pagewidth] [-H pageheight] [files]\n",
		argv[0]);
	    exit(-1);
	}
    if (npages > 0)
	qsort(pages, npages, sizeof (int), pcompar);
    if (!dowarnings)
	TIFFSetWarningHandler(0);
    printf("%%!PS-Adobe-3.0\n");
    printf("%%%%Creator: fax2ps\n");
#ifdef notdef
    printf("%%%%Title: %s\n", file);
#endif
    t = time(0);
    printf("%%%%CreationDate: %s", ctime(&t));
    printf("%%%%Origin: 0 0\n");
    printf("%%%%BoundingBox: 0 0 %u %u\n",
	(int)(pageHeight*72), (int)(pageWidth*72));	/* XXX */
    printf("%%%%Pages: (atend)\n");
    printf("%%%%EndComments\n");
    printf("%%%%BeginProlog\n");
    do_font_insert();
    printf("/d{bind def}def\n"); /* bind and def proc */
    printf("/m{0 exch moveto}d\n");
    printf("/s{show}d\n");
    printf("/p{showpage}d \n");	/* end page */
    printf("%%%%EndProlog\n");
    if (optind < argc) {
	do {
	    tif = TIFFOpen(argv[optind], "r");
	    if (tif) {
		fax2ps(tif, npages, pages, argv[optind]);
		TIFFClose(tif);
	    } else
		fprintf(stderr, "%s: Can not open, or not a TIFF file.\n",
		    argv[optind]);
	} while (++optind < argc);
    } else {
	int n, fd;
	char temp[1024], buf[16*1024];

	strcpy(temp, "/tmp/fax2psXXXXXX");
	fd = mkstemp(temp);
	if (fd == -1) {
	    fprintf(stderr, "Could not create temp file \"%s\"\n", temp);
	    exit(-2);
	}
	while ((n = read(fileno(stdin), buf, sizeof (buf))) > 0)
	    write(fd, buf, n);
	tif = TIFFOpen(temp, "r");
	unlink(temp);
	if (tif) {
	    fax2ps(tif, npages, pages, "<stdin>");
	    TIFFClose(tif);
	} else
	    fprintf(stderr, "%s: Can not open, or not a TIFF file.\n", temp);
	close(fd);
    }
    printf("%%%%Trailer\n");
    printf("%%%%Pages: %u\n", totalPages);
    printf("%%%%EOF\n");

    exit(0);
}

/* 
 * Create a special PostScript font for printing FAX documents.  By taking
 * advantage of the font-cacheing mechanism, a substantial speed-up in 
 * rendering time is realized. 
 */
static void
do_font_insert(void)
{
    puts("/newfont 10 dict def newfont begin /FontType 3 def /FontMatrix [1");
    puts("0 0 1 0 0] def /FontBBox [0 0 512 1] def /Encoding 256 array def");
    puts("0 1 31{Encoding exch /255 put}for 120 1 255{Encoding exch /255");
    puts("put}for Encoding 37 /255 put Encoding 40 /255 put Encoding 41 /255");
    puts("put Encoding 92 /255 put /count 0 def /ls{Encoding exch count 3");
    puts("string cvs cvn put /count count 1 add def}def 32 1 36{ls}for");
    puts("38 1 39{ls}for 42 1 91{ls}for 93 1 99{ls}for /count 100");
    puts("def 100 1 119{ls}for /CharDict 5 dict def CharDict begin /white");
    puts("{dup 255 eq{pop}{1 dict begin 100 sub neg 512 exch bitshift");
    puts("/cw exch def cw 0 0 0 cw 1 setcachedevice end}ifelse}def /black");
    puts("{dup 255 eq{pop}{1 dict begin 110 sub neg 512 exch bitshift");
    puts("/cw exch def cw 0 0 0 cw 1 setcachedevice 0 0 moveto cw 0 rlineto");
    puts("0 1 rlineto cw neg 0 rlineto closepath fill end}ifelse}def /numbuild");
    puts("{dup 255 eq{pop}{6 0 0 0 6 1 setcachedevice 0 1 5{0 moveto");
    puts("dup 32 and 32 eq{1 0 rlineto 0 1 rlineto -1 0 rlineto closepath");
    puts("fill newpath}if 1 bitshift}for pop}ifelse}def /.notdef {}");
    puts("def /255 {}def end /BuildChar{exch begin dup 110 ge{Encoding");
    puts("exch get 3 string cvs cvi CharDict /black get}{dup 100 ge {Encoding");
    puts("exch get 3 string cvs cvi CharDict /white get}{Encoding exch get");
    puts("3 string cvs cvi CharDict /numbuild get}ifelse}ifelse exec end");
    puts("}def end /Bitfont newfont definefont 1 scalefont setfont");
}
