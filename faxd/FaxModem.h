#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxModem.h,v 1.7 91/09/23 13:45:37 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FAXMODEM_
#define	_FAXMODEM_

#include "Types.h"
#include "tiffio.h"

class FaxServer;
class fxStr;

class FaxModem {
public:
    enum ModemType {
	UNKNOWN,
	EV958,		// old Everex 4800 baud
	EV968,		// new Everex 9600 baud
	ABATON,		// generic for EV958 or EV968
	CLASS1,		// aka EIA/TIA 578
	CLASS2,		// aka EIA/TIA 592, aka SP-2388
    };
    static FaxModem* getModemByName(const char* name, FaxServer&);
    static FaxModem* getModemByType(ModemType type, FaxServer&);

    enum CallStatus {
	OK,		// phone answered & carrier received
	BUSY,		// destination phone busy
	NOCARRIER,	// no carrier from remote
	NODIALTONE,	// no local dialtone (phone not plugged in?)
	ERROR,		// error in dial command
	FAILURE,	// other problem (e.g. modem turned off)
    };
    static const char* callStatus[6];

    enum SpeakerVolume {
	OFF,
	QUIET,
	LOW,
	MEDIUM,
	HIGH,
    };
protected:
    FaxServer&	server;
    ModemType	type;

    static const char* signallingNames[4];

    FaxModem(FaxServer& s, ModemType t);
// miscellaneous
    void	resetPages();
    void	countPage();
// modem i/o support
    int		getModemLine(char buf[], int timer = 0);
    fxBool	getTimedModemLine(char buf[], int timer);
// support write to modem w/ timeout
    void	beginTimedTransfer();
    void	endTimedTransfer();
    fxBool	wasTimeout();
    void	flushModemInput();
    fxBool	putModem(void* data, int n, int timer = 0);
    void	putModemLine(const char* cp);
    int		getModemChar(int timer = 0);
    void	startTimeout(int seconds);
    void	stopTimeout(const char* whichdir);
// host-modem protocol parsing support
    fxBool	atCmd(char cmd, char arg, fxBool waitForOK = TRUE);
    fxBool	atCmd(const fxStr& cmd, fxBool waitForOK = TRUE);
    int		fromHex(char*, int);
    fxStr	toHex(int, int ndigits);
// modem line control
    fxBool	selectBaudRate();
    fxBool	sendBreak(fxBool pause);
    fxBool	setBaudRate(int rate, fxBool enableFlow = TRUE);
    fxBool	setInputFlowControl(fxBool enable, fxBool flush = TRUE);
public:
    virtual ~FaxModem();

    virtual ModemType getType() const;
    virtual const char* getName() const = 0;

    virtual fxBool sync();
    virtual fxBool reset();
    virtual fxBool abort();
    virtual void hangup();
    virtual fxBool waitForRings(int n);

    virtual void setCommaPauseTime(int secs);
    virtual void setWaitTimeForCarrier(int secs);
    virtual void setSpeakerVolume(SpeakerVolume l);
    virtual void setEcho(fxBool on);
    virtual void setLID(const fxStr& number) = 0;

    virtual int selectSignallingRate(u_int t30rate) = 0;
    virtual u_int getBestSignallingRate() const = 0;
    static const char* getSignallingRateName(u_int) const;

    virtual u_int getBestScanlineTime() const;

    virtual CallStatus dial(const fxStr& number) = 0;
    virtual void sendBegin();
    virtual fxBool getPrologue(u_int& dis, u_int& xinfo, u_int& nfs) = 0;
    virtual void sendSetupPhaseB();
    virtual fxBool sendPhaseB(TIFF*, u_int dcs, fxStr& emsg, fxBool lastDoc) = 0;
    virtual void sendEnd();

    virtual fxBool recvBegin(u_int dis) = 0;
    virtual TIFF* recvPhaseB(fxBool okToRecv) = 0;
    virtual void recvEnd();
};
#endif /* _FAXMODEM_ */
