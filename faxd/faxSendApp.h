/*	$Header: /usr/people/sam/fax/./faxd/RCS/faxSendApp.h,v 1.9 1995/04/08 21:31:24 sam Rel $ */
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
#ifndef _faxSendApp_
#define	_faxSendApp_
/*
 * HylaFAX Send Job App.
 */
#include "faxApp.h"
#include "FaxServer.h"

class UUCPLock;

class faxSendApp : public FaxServer, public faxApp {
public:
    struct stringtag {
	const char*	 name;
	fxStr faxSendApp::* p;
	const char*	 def;		// NULL is shorthand for ""
    };
private:
// runtime state
    fxBool	ready;			// modem ready for use
    time_t	fileStart;		// starting time for file/poll
    UUCPLock*	modemLock;		// uucp lockfile handle
    fxStr	pollRcvdCmd;

    static faxSendApp* _instance;

    static const stringtag strings[];

// configuration support
    void	setupConfig();
    void	resetConfig();
    fxBool	setConfigItem(const char* tag, const char* value);
// modem handling
    fxBool	lockModem();
    void	unlockModem();
// miscellaneous stuff
    void	recordRecv(const FaxRecvInfo& ri);
    void	account(const char* cmd, const struct FaxAcctInfo&);
// notification interfaces used by FaxServer
    void	notifyModemReady();
    void	notifyDocumentSent(FaxRequest&, u_int fileIndex);
    void	notifyPollRecvd(FaxRequest&, const FaxRecvInfo&);
    void	notifyPollDone(FaxRequest&, u_int pollIndex);
    void	notifyRecvDone(const FaxRecvInfo& req);
public:
    faxSendApp(const fxStr& device, const fxStr& devID);
    ~faxSendApp();

    static faxSendApp& instance();

    void	initialize(int argc, char** argv);
    void	open();
    void	close();

    FaxSendStatus send(const char* filename);

    fxBool	isReady() const;
};
inline fxBool faxSendApp::isReady() const 	{ return ready; }
#endif
