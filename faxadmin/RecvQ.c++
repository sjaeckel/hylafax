#ident $Header: /d/sam/flexkit/fax/faxadmin/RCS/RecvQ.c++,v 1.6 91/05/23 12:13:17 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "RecvQ.h"
#include "ConfirmDialog.h"
#include "tiffio.h"

#include <sys/stat.h>

RecvQ::RecvQ(const char* qf) : qfile(qf)
{
    pagewidth = 0;
    pagelength = 0;
    resolution = 0;
}

RecvQ::~RecvQ()
{
}

static fxBool
isFAXImage(TIFF* tif)
{
    u_short w;
    if (TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &w) && w != 1)
	return (FALSE);
    if (TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &w) && w != 1)
	return (FALSE);
    if (!TIFFGetField(tif, TIFFTAG_COMPRESSION, &w) ||
      (w != COMPRESSION_CCITTFAX3 && w != COMPRESSION_CCITTFAX4))
	return (FALSE);
    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &w) ||
      (w != PHOTOMETRIC_MINISWHITE && w != PHOTOMETRIC_MINISBLACK))
	return (FALSE);
    return (TRUE);
}

// Return True for success, False for failure
fxBool
RecvQ::readQFile(int fd, fxVisualApplication* app)
{
    TIFF* tif = TIFFFdOpen(fd, qfile, "r");
    if (tif == NULL)
	return FALSE;
    fxBool ok = isFAXImage(tif);
    if (ok) {
	u_long w;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
	pagewidth = w;
	u_long l;
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &l);
	pagelength = l;
	if (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &resolution)) {
	    u_short resunit = RESUNIT_NONE;
	    TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
	    if (resunit == RESUNIT_CENTIMETER)
		resolution *= 25.4;
	    if (resolution)
		pagelength /= resolution;
	}
	char* desc;
	if (TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &desc))
	    number = desc;
	else
	    number = "<unknown>";
	npages = 0;
	do {
	    npages++;
	} while (TIFFReadDirectory(tif));
	struct stat sb;
	fstat(TIFFFileno(tif), &sb);
	arrival = sb.st_mtime;
    }
    TIFFClose(tif);
    return ok;
}

fxIMPLEMENT_PtrArray(RecvQPtrArray, RecvQ*);

int
RecvQPtrArray::compareElements(void* a, void* b) const
{
    const RecvQ* qa = *(RecvQ**)a;
    const RecvQ* qb = *(RecvQ**)b;
    return (qb->arrival - qa->arrival);
}
