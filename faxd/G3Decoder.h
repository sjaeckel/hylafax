/*	$Header: /usr/people/sam/fax/./faxd/RCS/G3Decoder.h,v 1.12 1995/04/08 21:30:36 sam Rel $ */
/*
 * Copyright (c) 1994-1995 Sam Leffler
 * Copyright (c) 1994-1995 Silicon Graphics, Inc.
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
#ifndef _G3Decoder_
#define _G3Decoder_
/*
 * Group 3 Facsimile Decoder Support.
 */
#include "G3Base.h"
extern "C" {
#include <setjmp.h>
}

class fxStackBuffer;

class G3Decoder : private G3Base {
private:
    short	data;		// current input/output byte
    short	bit;		// current bit in input/output byte
    short	bytePending;	// pending byte on recv
    short	prevByte;	// previous decoded byte
    u_char*	refline;	// reference line for 2d decoding
    fxStackBuffer* recvBuf;	// raw input for current row recvd

    fxBool	decode1DRow(u_char*, u_int);
    fxBool	decode2DRow(u_char*, u_int);
    int		nextBit();
    void	ungetBit();
    int		nextByte();
    int		decodeRun(const u_short fsm[][256]);
    int		decodeUncompCode();
    void	skipToEOL(int len);
protected:
    G3Decoder();

    void	raiseEOF();
    void	raiseRTC();

    void	setPendingByte(u_char);
    virtual int decodeNextByte() = 0;

    void	setRefLine(u_char*);
    u_char*	getRefLine();

    virtual void invalidCode(const char* type, int x);
    virtual void prematureEOL(const char* type, int x);
    virtual void badPixelCount(const char* type, int x);
    virtual void badDecodingState(const char* type, int x);
public:
    // XXX these should be private; see below for why they're public
    sigjmp_buf	jmpEOF;		// non-local goto on EOF
    sigjmp_buf	jmpRTC;		// non-local goto on RTC

    virtual ~G3Decoder();

    void	setupDecoder(u_int, fxBool is2D);

    void	decode(void* raster, u_int w, u_int h);
    void	skip(u_int h);

    void	skipLeader();
    fxBool	decodeRow(void* scanline, u_int w);
    void	skipRow();

    fxBool	isLastRow1D();
    fxBool	isNextRow1D();
    fxBool	isByteAligned();

    void	setRecvBuf(fxStackBuffer&);
    fxStackBuffer* getRecvBuf();
    void	flushRecvBuf();
};

/*
 * NB: These should be inline public methods but because we
 *     cannot depend on the compiler actually doing the inline
 *     we use #defines instead--if the sigsetjmp is done in
 *     the context of an out-of-line routine, then the saved
 *     frame pointer, pc, etc. will be wrong.
 */
#define	EOFraised()		(sigsetjmp(jmpEOF, 0) != 0)
#define	RTCraised()		(sigsetjmp(jmpRTC, 0) != 0)

inline void G3Decoder::setRecvBuf(fxStackBuffer& b){ recvBuf = &b; }
inline fxStackBuffer* G3Decoder::getRecvBuf()	{ return recvBuf; }
inline void G3Decoder::setRefLine(u_char* b)	{ refline = b; }
inline u_char* G3Decoder::getRefLine()		{ return refline; }
inline fxBool G3Decoder::isLastRow1D()		{ return tag == G3_1D; }
#endif /* _G3Decoder_ */
