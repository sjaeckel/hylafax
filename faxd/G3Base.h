/*	$Header: /usr/people/sam/fax/faxd/RCS/G3Base.h,v 1.1 1994/05/27 23:16:51 sam Exp $ */
/*
 * Copyright (c) 1994 Sam Leffler
 * Copyright (c) 1994 Silicon Graphics, Inc.
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
#ifndef _G3Base_
#define _G3Base_
/*
 * Group 3 Facsimile Decoder/Encoder Common Support.
 */
#include "Types.h"

struct G3Base {
    short	data;		// current input/output byte
    short	bit;		// current bit in input/output byte
    fxBool	is2D;		// whether or not data is 2d-encoded
    enum { G3_1D, G3_2D } tag;	// 1d/2d decoded row type
    const u_char* bitmap;	// bit reversal table

    static const u_char zeroruns[256];
    static const u_char oneruns[256];

    static int findspan(const u_char**, int, int, const u_char*);
    static int finddiff(const u_char*, int, int);
    static void fillspan(u_char* cp, int x, int count);

    void setup(const u_char*, fxBool is2d);
};
#endif /* _G3Base_ */
