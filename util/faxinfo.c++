/*	$Header: /usr/people/sam/fax/./util/RCS/faxinfo.c++,v 1.22 1995/04/08 21:44:51 sam Rel $ */
/*
 * Copyright (c) 1990-1995 Sam Leffler
 * Copyright (c) 1991-1995 Silicon Graphics, Inc.
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
#include "tiffio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "PageSize.h"

#include "port.h"

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

static void
sanitize(char* dst, const char* src, u_int maxlen)
{
    u_int i;
    for (i = 0; i < maxlen-1 && src[i] != '\0'; i++)
	dst[i] = (isascii(src[i]) && isprint(src[i]) ? src[i] : '?');
    dst[i] = '\0';
}

int
main(int argc, char** argv)
{
    char* cp;
    int npages, ok;
    TIFF* tif;
    float vres, w, h;
    long pageWidth, pageLength;
    char sender[80];
    char date[80];

    if (argc != 2) {
	fprintf(stderr, "usage: %s file.tif\n", argv[0]);
	return (-1);
    }
    printf("%s:\n", argv[1]);
    TIFFSetErrorHandler(NULL);
    TIFFSetWarningHandler(NULL);
    tif = TIFFOpen(argv[1], "r");
    if (tif == NULL) {
	printf("Could not open (%s).\n", strerror(errno));
	return (0);
    }
    ok = isFAXImage(tif);
    if (!ok) {
	printf("Does not look like a facsimile?\n");
	return (0);
    }
    if (TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &cp))
	sanitize(sender, cp, sizeof (sender));
    else
	strcpy(sender, "<unknown>");
    printf("%11s %s\n", "Sender:", sender);
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &pageWidth);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &pageLength);
    if (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &vres)) {
	short resunit = RESUNIT_NONE;
	TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
	if (resunit == RESUNIT_CENTIMETER)
	    vres *= 25.4;
    } else
	vres = 98;
    npages = 0;
    do {
	npages++;
    } while (TIFFReadDirectory(tif));
    printf("%11s %u\n", "Pages:", npages);
    if (vres == 98)
	printf("%11s Normal\n", "Quality:");
    else if (vres == 196)
	printf("%11s Fine\n", "Quality:");
    else
	printf("%11s %.2f lines/inch\n", "Quality:", vres);
    h = pageLength / (vres < 100 ? 3.85 : 7.7);
    w = (pageWidth / 204.) * 25.4;
    PageSizeInfo* info = PageSizeInfo::getPageSizeBySize(w, h);
    if (info)
	printf("%11s %s\n", "Page:", info->name());
    else
	printf("%11s %lu by %lu\n", "Page:", pageWidth, pageLength);
    delete info;
    if (!TIFFGetField(tif, TIFFTAG_DATETIME, &cp)) {
	struct stat sb;
	fstat(TIFFFileno(tif), &sb);
	strftime(date, sizeof (date),
	    "%Y:%m:%d %H:%M:%S %Z", localtime(&sb.st_mtime));
    } else
	sanitize(date, cp, sizeof (date));
    printf("%11s %s\n", "Received:", date);
    return (0);
}
