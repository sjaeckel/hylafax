/*	$Header: /usr/people/sam/fax/faxd/RCS/G3Encoder.h,v 1.3 1994/06/04 00:12:26 sam Exp $ */
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
#ifndef _G3Encoder_
#define _G3Encoder_
/*
 * Group 3 Facsimile Encoder Support.
 */
#include "G3Base.h"

class fxStackBuffer;
struct tableentry;

class G3Encoder : private G3Base {
private:
    fxStackBuffer& buf;

    void	putBits(u_int bits, u_int length);
    void	putcode(const tableentry& te);
    void	putspan(int span, const tableentry* tab);
    void	flushBits();
public:
    G3Encoder(fxStackBuffer&);
    virtual ~G3Encoder();

    void	setupEncoder(u_int fillOrder, fxBool is2d);
    void	encode(const void* raster, u_int w, u_int h);
};
#endif /* _G3Encoder_ */
