/*	$Header: /usr/people/sam/fax/./faxd/RCS/faxGettyApp.h,v 1.13 1995/04/08 21:31:16 sam Rel $ */
/*
 * Copyright (c) 1990-1995 Sam Leffler
 * Copyright (c) 1991-1995 Silicon Graphics, Inc.
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
#ifndef _faxGettyApp_
#define	_faxGettyApp_
/*
 * HylaFAX Modem Handler.
 */
#include "faxApp.h"
#include "FaxServer.h"

class UUCPLock;
class Getty;

class faxGettyApp : public FaxServer, public faxApp {
public:
    struct stringtag {
	const char*	 name;
	fxStr faxGettyApp::* p;
	const char*	 def;		// NULL is shorthand for ""
    };
    struct numbertag {
	const char*	 name;
	u_int faxGettyApp::*p;
	u_int		 def;
    };
private:
// runtime state
    fxBool	debug;			// enable optional debugging info
    fxStr	serverPID;		// pid of this process
    int		devfifo;		// fifo device interface
					// XXX
    time_t	jobStart;		// starting time for job
    time_t	fileStart;		// starting time for file/poll

    UUCPLock*	modemLock;		// UUCP interlock

    u_short	ringsBeforeAnswer;	// # rings to wait
    fxStr	qualifyCID;		// if set, no answer w/o acceptable cid
    time_t	lastCIDModTime;		// last mod time of CID patterns file
    RegExArray*	cidPats;		// recv cid patterns
    fxBoolArray* acceptCID;		// accept/reject matched cid
    fxStr	gettyArgs;		// getty arguments
    fxStr	vgettyArgs;		// voice getty arguments
    fxBool	adaptiveAnswer;		// answer as data if fax answer fails
    u_int	answerBias;		// rotor bias applied after good calls
    u_short	answerRotor;		// rotor into possible selections
    u_short	answerRotorSize;	// rotor table size
    AnswerType	answerRotary[3];	// rotary selection of answer types
    fxStr	notifyCmd;
    fxStr	faxRcvdCmd;

    static faxGettyApp* _instance;

    static const stringtag strings[];
    static const numbertag numbers[];

    static const fxStr fifoName;
    static const fxStr recvDir;

// configuration support
    void	setupConfig();
    void	resetConfig();
    fxBool	setConfigItem(const char* tag, const char* value);
    void	setAnswerRotary(const fxStr& value);
// modem handling
    fxBool	isModemLocked();
    fxBool	lockModem();
    void	unlockModem();
    fxBool	setupModem();
    void	discardModem(fxBool dropDTR);
// inbound call handling
    fxBool	isCIDOk(const fxStr& cid);
    fxBool	processCall(CallType ctype, fxStr& emsg);
    void	runGetty(const char* what,
		    Getty* (*newgetty)(const fxStr&, const fxStr&),
		    const char* args,
		    fxBool keepLock);
    void	setRingsBeforeAnswer(int rings);
    void	answerPhone(AnswerType, fxBool force);
// miscellaneous stuff
    void	sendQueuer(const fxStr& msg0);
    void	recordRecv(const FaxRecvInfo& ri);
    void	account(const char* cmd, const struct FaxAcctInfo&);
// FIFO-related stuff
    void	openFIFOs();
    void	closeFIFOs();
    void	FIFOMessage(const char* mesage);
// Dispatcher hooks
    int		inputReady(int);
// notification interfaces used by FaxServer
    void	notifyModemReady();
    void	notifyDocumentSent(FaxRequest&, u_int fileIndex);
    void	notifyPollRecvd(FaxRequest&, const FaxRecvInfo&);
    void	notifyPollDone(FaxRequest&, u_int pollIndex);
    void	notifyRecvDone(const FaxRecvInfo& req);
public:
    faxGettyApp(const fxStr& device, const fxStr& devID);
    ~faxGettyApp();

    static faxGettyApp& instance();

    void	initialize(int argc, char** argv);
    void	open();
    void	close();
};
#endif
