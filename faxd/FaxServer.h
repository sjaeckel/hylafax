#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxServer.h,v 1.22 91/09/23 13:45:40 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FaxServer_
#define	_FaxServer_

#include "Application.h"
#include "Str.h"
#include "StrArray.h"
#include "FaxMachineInfo.h"
#include "FaxModem.h"
#include "FaxTrace.h"
#include "Exec.h"

class FaxRequest;
class RegExArray;

class FaxServer : public fxApplication {
private:
    class ModemListener : public fxSelectHandler {
    protected:
	fxOutputChannel*	ringChannel;
    public:
	ModemListener(int f);
	~ModemListener();
	const char* className() const;
	void handleRead();
    };

// generic modem-related stuff
    int		modemFd;		// open modem file
    fxStr	modemType;		// modem type identifier
    fxStr	modemDevice;		// name of device to open
    fxStr	modemCSI;		// encoded phone number
    FaxModem*	modem;			// modem driver
// server configuration
    u_short	ringsBeforeAnswer;	// # rings to wait
    u_short	waitTimeForCarrier;	// timeout waiting for carrier
    u_short	commaPauseTime;		// for "," in dial string
    SpeakerVolume speakerVolume;	// volume control
    fxBool	toneDialing;		// tone/pulse dialing
    fxBool	useDialPrefix;		// if true, prefix to dial out
    fxBool	okToReceive2D;		// alright to receive 2d-encoded fax
    fxBool	qualifyTSI;		// if true, no recv w/o acceptable tsi
    int		recvFileMode;		// protection mode for received files
    int		tracingLevel;		// protocol tracing level
// phone number-related state
    fxStr	dialPrefix;		// prefix str for external dialing
    fxStr	longDistancePrefix;	// prefix str for long distance dialing
    fxStr	internationalPrefix;	// prefix str for international dialing
    fxStr	myAreaCode;		// local area code
    fxStr	myCountryCode;		// local country code
    fxStr	FAXNumber;		// phone number
// group 3 protocol-related state
    u_int	clientDIS;		// received client DIS
    u_int	clientDCS;		// client's DCS
    FaxMachineInfo clientInfo;		// remote machine info
// buffered i/o stuff
    short	rcvCC;			// # bytes pending in rcvBuf
    short	rcvNext;		// next available byte in rcvBuf
    char	rcvBuf[1024];		// receive buffering
// for fax reception ...
    ModemListener* rcvHandler;		// XXX modem select handler
    fxBool	timeout;		// timeout during data reception
    fxBool	okToRecv;		// ok to accept stuff for this session
    fxStr	recvTSI;		// sender's TSI
    time_t	lastPatModTime;		// last mod time of patterns file
    RegExArray*	tsiPats;		// acceptable recv tsi patterns
// send+receive stats
    u_int	npages;			// # pages sent/received

    fxOutputChannel* sendCompleteChannel;
    fxOutputChannel* jobCompleteChannel;
    fxOutputChannel* jobRecvdChannel;
    fxOutputChannel* recvCompleteChannel;
    fxOutputChannel* sendStatusChannel;
    fxOutputChannel* traceChannel;

    friend class FaxModem;

// FAX transmission protocol support
    fxBool	sendClientCapabilitiesOK();
    fxBool	sendFaxPhaseB(const fxStr& file, fxStr& emsg, fxBool lastDoc);
    fxBool	sendSetupDCS(TIFF*, u_int& dcs, fxStr& emsg);
// FAX reception support
    void	recvComplete(const char* qfile, time_t recvTime);
    void	updateTSIPatterns();
    RegExArray*	readTSIPatterns(FILE*);
// modem capability negotiation & co.
    static int pageLengthCodes[4];
    static int pageWidthCodes[4];
    static int minScanlineTimeCodes[8][2];

    fxBool	setupModem(const char* type);
    int		modemDIS();
// modem i/o support
    int		getModemLine(char buf[], int timer = 0);
    fxBool	getTimedModemLine(char buf[], int timer);
    int		getModemChar(int timer = 0);
    void	flushModemInput();
    fxBool	putModem(void* data, int n, int timer = 0);
    void	putModemLine(const char* cp);
    void	startTimeout(int seconds);
    void	stopTimeout(const char* whichdir);
    void	modemFlushInput();
// modem line control
    fxBool	sendBreak(fxBool pause);
    fxBool	setBaudRate(int rate, fxBool enableFlow = TRUE);
    fxBool	setInputFlowControl(fxBool enable, fxBool flush = TRUE);
public:
    FaxServer(const fxStr& deviceName);
    ~FaxServer();

    const char* className() const;

    void open();
    void close();
    virtual void initialize(int argc, char** argv);

    fxBool openSucceeded() const;		// server is ready
    fxStr canonicalizePhoneNumber(const fxStr& number);
    fxStr localizePhoneNumber(const fxStr& canon);

    void restoreState(const fxStr& filename);
    void restoreStateItem(const char* buf);

    void setModemNumber(const fxStr& number);	// fax phone number
    const fxStr& getModemNumber();

    void setTracing(int level);			// protocol tracing
    int getTracing();
    void setToneDialing(fxBool on);		// default dialing technique
    fxBool getToneDialing();
    void setModemSpeakerVolume(SpeakerVolume);	// speaker volume
    SpeakerVolume getModemSpeakerVolume();
    void setRingsBeforeAnswer(int rings);	// rings to wait before answer
    int getRingsBeforeAnswer();
    void setCommaPauseTime(int seconds);	// delay for "," in phone number
    int getCommaPauseTime();
    void setWaitTimeForCarrier(int seconds);	// time to wait for carrier
    int getWaitTimeForCarrier();
    void setRecvFileMode(int mode);		// protection mode of recvd fax
    int getRecvFileMode();
    void setOkToReceive2D(fxBool yes);		// alright to receive 2d stuff
    fxBool getOkToReceive2D();
    void setQualifyTSI(fxBool yes);		// qualify tsi before recv
    fxBool getQualifyTSI();

// client-server send interface
    void sendFax(FaxRequest*);
    fxStr sendFax(const fxStr& number, const fxStr& canonicalNumber,
		const fxStrArray& files);

// client-server recv interface
    void recvFax(fxBool answerThePhone = FALSE);
// FAX receiving protocol support (used by modem classes)
    void recvDCS(u_int dcs, u_int xinfo);
    fxBool recvCheckTSI(const fxStr& tsi);
    void recvSetupPage(TIFF* tif, long group3opts, int fillOrder);

// general trace interface (public for modem classes)
    void traceStatus(int kind, const char* fmt ...);
};

typedef void (*sig_type)(int ...);
#ifndef CYPRESS_XGL
extern "C" sig_type signal(const int, const sig_type);
#endif

inline fxBool streq(const char* a, const char* b)
    { return (strcmp(a,b) == 0); }
inline fxBool streq(const char* a, const char* b, int n)
    { return (strncmp(a,b,n) == 0); }
#endif /* _FaxServer_ */
