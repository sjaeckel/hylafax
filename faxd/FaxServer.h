/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxServer.h,v 1.135 1995/04/08 21:30:30 sam Rel $ */
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
#ifndef _FaxServer_
#define	_FaxServer_
/*
 * Fax Modem and Protocol Server.
 */
#include "ModemServer.h"

class FaxRecvInfo;
class FaxRecvInfoArray;
class FaxAcctInfo;

/*
 * This class defines the ``server process'' that manages the fax
 * modem and implements the necessary protocol above the FaxModem
 * driver interface.  When the server is multi-threaded, this class
 * embodies a separate thread.
 */
class FaxServer : public ModemServer {
private:
    FaxModem*	modem;			// modem driver
// group 3 protocol-related state
    Class2Params clientCapabilities;	// received client capabilities
    Class2Params clientParams;		// current session parameters
// for fax reception ...
    fxBool	okToRecv;		// ok to accept stuff for this session
    fxStr	recvTSI;		// sender's TSI
    fxStr	hostname;		// host on which fax is received
    u_int	recvPages;		// count of received pages
    time_t	recvStart;		// starting time for document receive
// send+receive stats
    u_int	npages;			// # pages sent/received

    friend class FaxModem;

// FAX transmission protocol support
    void	sendFax(FaxRequest& fax, FaxMachineInfo&, const fxStr& number);
    fxBool	sendClientCapabilitiesOK(FaxMachineInfo&, u_int nsf, fxStr&);
    fxBool	sendFaxPhaseB(FaxRequest&, faxRequest&, FaxMachineInfo&);
    void	sendPoll(FaxRequest& fax, fxBool remoteHasDoc);
    FaxSendStatus sendSetupParams(TIFF*,
		    Class2Params&, const FaxMachineInfo&, fxStr&);
    FaxSendStatus sendSetupParams1(TIFF*,
		    Class2Params&, const FaxMachineInfo&, fxStr&);
    void	sendFailed(FaxRequest& fax,
		    FaxSendStatus, const char* notice, u_int tts = 0);
// FAX reception support
    TIFF*	setupForRecv(const char* op, FaxRecvInfo&, FaxRecvInfoArray&);
    fxBool	recvDocuments(const char* op, TIFF*,
		    FaxRecvInfo&, FaxRecvInfoArray&, fxStr& emsg);
    fxBool	recvFaxPhaseD(TIFF* tif, int& ppm, fxStr& emsg);
    void	recvComplete(FaxRecvInfo&, time_t, const fxStr& emsg);
    fxBool	pollFaxPhaseB(const char* cig, FaxRecvInfoArray&, fxStr& emsg);

// FAX receiving protocol support (used by modem classes)
    void	recvDCS(const Class2Params&);
    void	recvNSF(u_int);
    fxBool	recvCheckTSI(const fxStr& tsi);
    void	recvSetupPage(TIFF* tif, long group3opts, int fillOrder);
protected:
    FaxServer(const fxStr& deviceName, const fxStr& devID);

    fxBool	setupModem();
    ClassModem*	deduceModem();
    void	discardModem(fxBool dropDTR);
    fxStr	getModemCapabilities() const;

    void	readConfig(const fxStr& filename);
    void	setLocalIdentifier(const fxStr& lid);

    void	sendFax(FaxRequest&, FaxMachineInfo&, FaxAcctInfo&);
    fxBool	recvFax();

// notification interfaces implemented in derived class
    virtual void notifyDocumentSent(FaxRequest&, u_int fileIndex) = 0;
    virtual void notifyPollRecvd(FaxRequest&, const FaxRecvInfo&) = 0;
    virtual void notifyPollDone(FaxRequest&, u_int pollIndex) = 0;
    virtual void notifyRecvDone(const FaxRecvInfo& req) = 0;
public:
    virtual ~FaxServer();

    void initialize(int argc, char** argv);

    const fxStr& getLocalIdentifier() const;

    fxBool modemSupports2D() const;
    fxBool modemSupportsEOLPadding() const;
    fxBool modemSupportsVRes(float res) const;
    fxBool modemSupportsPageWidth(u_int w) const;
    fxBool modemSupportsPageLength(u_int l) const;
    fxBool modemSupportsPolling() const;
};
#endif /* _FaxServer_ */
