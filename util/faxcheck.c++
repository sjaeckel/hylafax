#ident $Header: /d/sam/flexkit/fax/util/RCS/faxcheck.c++,v 1.2 91/05/23 12:50:07 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include <stdio.h>
#include <setjmp.h>
#include "Types.h"

jmp_buf	recvEOF;
int	rawzeros;
FILE*	fd;
int	shbit;
int	shdata;
int	row;

#include <stdarg.h>

void
traceStatus(const char* va_alist ...)
#define	fmt va_alist
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
#undef fmt

const int EOL = 0x001;			// end-of-line code (11 0's + 1)

void
recvCode(int& len, int& code)
{
    code = 0;
    len = 0;
    do {
	if ((shbit & 0xff) == 0) {
	    shdata = getc(fd);
	    if (shdata == EOF)
		longjmp(recvEOF, 1);
	    shbit = 0x01; 
	}
	code <<= 1;
	if (shdata & shbit)
	    code |= 1;
	shbit <<= 1;
	len++;
    } while (code <= 0);
}

void
recvPageData()
{
    int	badfaxrows = 0;			// # of rows w/ errors
    int badfaxrun = 0;			// current run of bad rows
    int	maxbadfaxrun = 0;		// longest bad run
    fxBool seenRTC = FALSE;		// if true, saw RTC
    int eols = 0;			// count of consecutive EOL codes
    int row = 0;
    if (setjmp(recvEOF) == 0) {
    top:
	int bit = 0x80;
	int data = 0;
	int len;
	int code;
	fxBool emptyLine = TRUE;
	while (!seenRTC) {
	    recvCode(len, code);
	    if (len >= 12 && code == EOL) {
		/*
		 * Found an EOL, flush the current scanline
		 * and check for RTC (6 consecutive EOL codes).
		 */
		if (!emptyLine) {
		    row++;
		    eols = 0;
		} else
		    seenRTC = (++eols == 6);		// XXX RTC is 6
		if (eols > 0)
		    traceStatus("row %d, got EOL, eols %d", row, eols);
		if (badfaxrun > maxbadfaxrun)
			maxbadfaxrun = badfaxrun;
		badfaxrun = 0;
		goto top;
	    }
	    if (len > 13) {
		/*
		 * Encountered a bogus code word; skip to the EOL
		 * and regenerate the previous line.  Wouldn't it
		 * be nice if we could ask for a retransmit of a
		 * single scanline?
		 */
		traceStatus("Bad code word 0x%x, len %d, row %d",
		    code, len, row);
		badfaxrows++;
		badfaxrun++;
		// skip to EOL
		do
		    recvCode(len, code);
		while (len < 12 || code != EOL);
		// regenerate previous row
		goto top;
	    }
	    // shift code into scanline buffer
	    for (u_int mask = 1<<(len-1); mask; mask >>= 1) {
		if (code & mask)
		    data |= bit;
		if ((bit >>= 1) == 0) {
		    data = 0;
		    bit = 0x80;
		}
	    }
	    emptyLine = FALSE;
	}
    } else
	traceStatus("Premature EOF, row %d", row);
}

int
main(int argc, char** argv)
{
    if (argc > 1) {
	fd = fopen(argv[1], "r");
	if (fd == NULL) {
	    fprintf(stderr, "%s: Can't open.\n");
	    exit(-1);
	}
    } else {
	fprintf(stderr, "usage: %s file.fax\n", argv[0]);
	exit(-2);
    }
    recvPageData();
}
