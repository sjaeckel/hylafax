/*	$Header: /usr/people/sam/fax/faxd/RCS/Class2.h,v 1.65 1994/06/06 22:54:36 sam Exp $ */
/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
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
#ifndef _CLASS2_
#define	_CLASS2_
/*
 * Base class for a Class 2 and Class 2.0 Modem Drivers.
 */
#include "FaxModem.h"
#include <stdarg.h>

class Class2Modem : public FaxModem {
protected:
    fxStr	classCmd;		// set class command
    fxStr	cqCmds;			// copy quality setup commands
    fxStr	tbcCmd;			// modem-host communication mode command
    fxStr	crCmd;			// enable receiving command
    fxStr	phctoCmd;		// set Phase C timeout command
    fxStr	bugCmd;			// enable HDLC tracing command
    fxStr	lidCmd;			// set local ID command
    fxStr	dccCmd;			// set configuration parameters command
    fxStr	dccQueryCmd;		// modem capabilities query command
    fxStr	disCmd;			// set session parameters command
    fxStr	cigCmd;			// set polling ID string command
    fxStr	splCmd;			// set polling request command
    fxStr	nrCmd;			// negotiation message reporting control
    fxStr	pieCmd;			// procedure interrupt enable control
    fxStr	borCmd;			// set bit order command
    fxStr	abortCmd;		// abort session command
    fxStr	ptsCmd;			// set page status command
    u_int	serviceType;		// modem service required
    u_int	modemCQ;		// copy quality capabilities mask

    Class2Params params;		// current params during send
    fxBool	xmitWaitForXON;		// if true, wait for XON when sending
    fxBool	hostDidCQ;		// if true, copy quality done on host
    fxBool	hasPolling;		// if true, modem does polled recv
    char	recvDataTrigger;	// char to send to start recv'ing data
    char	hangupCode[4];		// hangup reason (from modem)
    long	group3opts;		// for writing received TIFF

// modem setup stuff
    void setupDefault(fxStr&, const fxStr&, const char*);
    virtual fxBool setupModem();
    virtual fxBool setupModel(fxStr& model);
    virtual fxBool setupRevision(fxStr& rev);
    virtual fxBool setupDCC();
    virtual fxBool setupClass2Parameters();
// transmission support
    fxBool	dataTransfer();

    virtual fxBool sendPage(TIFF* tif) = 0;
    virtual fxBool pageDone(u_int ppm, u_int& ppr) = 0;
// reception support
    const AnswerMsg* findAnswer(const char*);
    fxBool	recvDCS(const char*);
    fxBool	recvPageData(TIFF*, fxStr& emsg);
    fxBool	recvPPM(TIFF*, int& ppr);
    fxBool	parseFPTS(TIFF*, const char* cp, int& ppr);
    void	abortPageRecv();
// miscellaneous
    enum {			// Class 2-specific AT responses
	AT_FHNG		= 100,	// remote hangup
	AT_FCON		= 101,	// fax connection status
	AT_FPOLL	= 102,	// document available for polling status
	AT_FDIS		= 103,	// DIS received status
	AT_FNSF		= 104,	// NSF received status
	AT_FCSI		= 105,	// CSI received status
	AT_FPTS		= 106,	// post-page status
	AT_FDCS		= 107,	// DCS received status
	AT_FNSS		= 108,	// NSS received status
	AT_FTSI		= 109,	// TSI received status
	AT_FET		= 110,	// post-page-response status
	AT_FVO		= 111,	// voice transition status
    };
    virtual ATResponse atResponse(char* buf, long ms = 30*1000) = 0;
    fxBool	waitFor(ATResponse wanted, long ms = 30*1000);
    fxStr	stripQuotes(const char*);
// hangup processing
    void	processHangup(const char*);
    fxBool	isNormalHangup();
    const char*	hangupCause(const char* code);
    void	tracePPR(const char* dir, u_int ppr);
    void	tracePPM(const char* dir, u_int ppm);
// class 2 command support routines
    fxBool	class2Cmd(const char* cmd, const Class2Params& p);
    fxBool	class2Cmd(const char* cmd);
    fxBool	class2Cmd(const char* cmd, int a0);
    fxBool	class2Cmd(const char* cmd, const char* s);
// parsing routines for capability&parameter strings
    fxBool	parseClass2Capabilities(const char* cap, Class2Params&);
    fxBool	parseRange(const char*, Class2Params&);
    const char* skipStatus(const char*);

    Class2Modem(FaxServer&, const ModemConfig&);
public:
    virtual ~Class2Modem();

// send support
    CallStatus	dial(const char* number, const Class2Params& dis, fxStr& emsg);
    CallStatus	dialResponse(fxStr& emsg);
    fxBool	getPrologue(Class2Params&, u_int& nsf, fxStr&, fxBool& hasDoc);
    FaxSendStatus sendPhaseB(TIFF* tif, Class2Params&, FaxMachineInfo&,
		    fxStr& pph, fxStr& emsg);
    void	sendAbort();

// receive support
    fxBool	recvBegin(fxStr& emsg);
    fxBool	recvPage(TIFF*, int& ppm, fxStr& emsg);
    fxBool	recvEnd(fxStr& emsg);
    void	recvAbort();

// polling support
    fxBool	requestToPoll();
    fxBool	pollBegin(const fxStr& pollID, fxStr& emsg);

// miscellaneous
    fxBool	dataService();			// establish data service
    fxBool	voiceService();			// establish voice service
    fxBool	reset(long ms);			// reset modem
    void	setLID(const fxStr& number);	// set local id string
    fxBool	supportsPolling() const;	// modem capability
};
#endif /* _CLASS2_ */
