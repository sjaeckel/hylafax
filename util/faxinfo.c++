/*	$Id$ */
/*
 * Copyright (c) 1990-1996 Sam Leffler
 * Copyright (c) 1991-1996 Silicon Graphics, Inc.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "tiffio.h"

#include "PageSize.h"
#include "Class2Params.h"
#include "CallID.h"

#include "port.h"

extern	const char* fmtTime(time_t t);

static bool
isFAXImage(TIFF* tif)
{
    uint16 w;
    if (TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &w) && w != 1)
	return (false);
    if (TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &w) && w != 1)
	return (false);
    if (!TIFFGetField(tif, TIFFTAG_COMPRESSION, &w) ||
      (w != COMPRESSION_CCITTFAX3 && w != COMPRESSION_CCITTFAX4))
	return (false);
    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &w) ||
      (w != PHOTOMETRIC_MINISWHITE && w != PHOTOMETRIC_MINISBLACK))
	return (false);
    return (true);
}

static void
sanitize(fxStr& s)
{
    for(u_int i = 0; i < s.length(); i++) {
        if (!isascii(s[i]) || !isprint(s[i])) s[i] = '?';
    }
}

int
main(int argc, char** argv)
{
    bool showFilename = true;
    const char* appName = argv[0];

    if (argc > 2 && streq(argv[1], "-n")) {
	showFilename = false;
	argc--, argv++;
    }
    if (argc != 2) {
	fprintf(stderr, "usage: %s [-n] file.tif\n", appName);
	return (-1);
    }
    if (showFilename)
	printf("%s:\n", argv[1]);
    TIFFSetErrorHandler(NULL);
    TIFFSetWarningHandler(NULL);
    TIFF* tif = TIFFOpen(argv[1], "r");
    if (tif == NULL) {
	printf("Could not open %s; either not TIFF or corrupted.\n", argv[1]);
	return (0);
    }
    bool ok = isFAXImage(tif);
    if (!ok) {
	printf("Does not look like a facsimile?\n");
	return (0);
    }

    Class2Params params;
    uint32 v;
    float vres = 3.85;					// XXX default
    float hres = 8.03;
#ifdef TIFFTAG_FAXRECVPARAMS
    if (TIFFGetField(tif, TIFFTAG_FAXRECVPARAMS, &v)) {
	params.decode((u_int) v);			// page transfer params
	// inch & metric resolutions overlap and are distinguished by yres
	TIFFGetField(tif, TIFFTAG_YRESOLUTION, &vres);
	switch ((u_int) vres) {
	    case 100:
		params.vr = VR_200X100;
		break;
	    case 200:
		params.vr = VR_200X200;
		break;
	    case 400:
		params.vr = VR_200X400;
		break;
	    case 300:
		params.vr = VR_300X300;
		break;
	}
    } else {
#endif
    if (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &vres)) {
	uint16 resunit = RESUNIT_INCH;			// TIFF spec default
	TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
	if (resunit == RESUNIT_INCH)
	    vres /= 25.4;
	if (resunit == RESUNIT_NONE)
	    vres /= 720.0;				// postscript units ?
	if (TIFFGetField(tif, TIFFTAG_XRESOLUTION, &hres)) {
	    if (resunit == RESUNIT_INCH)
		hres /= 25.4;
	    if (resunit == RESUNIT_NONE)
		hres /= 720.0;				// postscript units ?
        }
    }
    params.setRes((u_int) hres, (u_int) vres);		// resolution
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &v);
    params.setPageWidthInPixels((u_int) v);		// page width
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &v);
    params.setPageLengthInMM((u_int)(v / vres));	// page length
#ifdef TIFFTAG_FAXRECVPARAMS
    }
#endif
    fxStr sender = "";
    CallID callid;
    char* cp;
    if (TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &cp)) {
	while (cp[0] != '\0' && cp[0] != '\n') {	// sender
	    sender.append(cp[0]);
	    cp++;
	}
	sanitize(sender);
	u_int i = 0;
	while (cp[0] == '\n') {
	    cp++;
	    callid.resize(i+1);
	    while (cp[0] != '\0' && cp[0] != '\n') {
		callid[i].append(cp[0]);
		cp++;
	    }
	    sanitize(callid[i]);
	    i++;
	}
    } else
	sender = "<unknown>";
    printf("%11s %s\n", "Sender:", (const char*) sender);
#ifdef TIFFTAG_FAXSUBADDRESS
    if (TIFFGetField(tif, TIFFTAG_FAXSUBADDRESS, &cp)) {
	fxStr subaddr(cp);
	sanitize(subaddr);
	printf("%11s %s\n", "SubAddr:", (const char*) subaddr);
    }
#endif
    fxStr date;
    if (TIFFGetField(tif, TIFFTAG_DATETIME, &cp)) {	// time received
	date = cp;
	sanitize(date);
    } else {
	struct stat sb;
	fstat(TIFFFileno(tif), &sb);
	char buf[80];
	strftime(buf, sizeof (buf),
	    "%Y:%m:%d %H:%M:%S %Z", localtime(&sb.st_mtime));
	date = buf;
    }
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &v);
    float h = v / (params.vr == VR_NORMAL ? 3.85 : 
	params.vr == VR_200X100 ? 3.94 : 
	params.vr == VR_FINE ? 7.7 :
	params.vr == VR_200X200 ? 7.87 : 
	params.vr == VR_R8 ? 15.4 : 
	params.vr == VR_200X400 ? 15.75 : 
	params.vr == VR_300X300 ? 11.81 : 15.4);
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &v);
    float w = v / (params.vr == VR_NORMAL ? 8.0 : 
	params.vr == VR_200X100 ? 8.00 : 
	params.vr == VR_FINE ? 8.00 :
	params.vr == VR_200X200 ? 8.00 : 
	params.vr == VR_R8 ? 8.00 : 
	params.vr == VR_200X400 ? 8.00 : 
	params.vr == VR_300X300 ? 12.01 : 16.01);
    time_t time = 0;
    u_int npages = 0;					// page count
    do {
	npages++;
#ifdef TIFFTAG_FAXRECVTIME
	if (TIFFGetField(tif, TIFFTAG_FAXRECVTIME, &v))
	    time += v;
#endif
    } while (TIFFReadDirectory(tif));
    TIFFClose(tif);

    printf("%11s %u\n", "Pages:", npages);
    if (params.vr == VR_NORMAL)
	printf("%11s Normal\n", "Quality:");
    else if (params.vr == VR_FINE)
	printf("%11s Fine\n", "Quality:");
    else if (params.vr == VR_R8)
	printf("%11s Superfine\n", "Quality:");
    else if (params.vr == VR_R16)
	printf("%11s Hyperfine\n", "Quality:");
    else
	printf("%11s %u lines/inch\n", "Quality:", params.verticalRes());
    PageSizeInfo* info = PageSizeInfo::getPageSizeBySize(w, h);
    if (info)
	printf("%11s %s\n", "Page:", info->name());
    else
	printf("%11s %u by %u\n", "Page:", params.pageWidth(), (u_int) h);
    delete info;
    printf("%11s %s\n", "Received:", (const char*) date);
    printf("%11s %s\n", "TimeToRecv:", time == 0 ? "<unknown>" : fmtTime(time));
    printf("%11s %s\n", "SignalRate:", params.bitRateName());
    printf("%11s %s\n", "DataFormat:", params.dataFormatName());
    printf("%11s %s\n", "ErrCorrect:", params.ec == EC_DISABLE ? "No" : "Yes");
    for (u_int i = 0; i < callid.size(); i++) {
	// formatting will mess up if i gets bigger than one digit
	printf("%9s%u: %s\n", "CallID", i+1, (const char*) callid.id(i));
    }
    return (0);
}
