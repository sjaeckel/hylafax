#ident $Header: /usr/people/sam/flexkit/fax/faxadmin/RCS/faxAdmin.h,v 1.14 91/05/28 22:18:38 sam Exp $
#ifndef _faxAdmin_
#define	_faxAdmin_

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "VisualApplication.h"
#include "StrArray.h"
#include "MultiChoice.h"
#include "FaxModem.h"
#include "Timer.h"
#include "BusyCursor.h"
#include "RecvQ.h"
#include "SendQ.h"
#include <stdarg.h>
#include <time.h>

class fxButton;
class fxFieldEditor;
class SendQList;
class RecvQList;
class fxViewStack;
class fxLabel;
class fxMenu;
class fxMenuStack;
class fxMenuItem;
class fxValuator;
class StatusLabel;
class fxView;
class fxFont;
class fxWindow;

class faxAdmin : public fxVisualApplication, public BusyCursor {
protected:
    fxStr		appName;
    FILE*		fifo;			// connection to server
    fxBool		isDirty;		// true if something modified
    fxBool		okToUpdate;		// if true, allright to update
    fxStr		queueDir;		// spooling directory
    fxStr		device;			// modem tty file
    StatusLabel*	statusReadout;		// server status
    fxStr		faxNumber;		// phone number for fax modem
    fxStr		prefix;			// external dialing prefix
    fxStr		areaCode;		// area code for voice&fax
    fxStr		countryCode;		// country code for voice&fax
    fxStr		internationalPrefix;	// international dialing prefix
    fxStr		longDistancePrefix;	// long distance dialing prefix
    fxStr		voiceNumber;		// phone number for voice line
    fxBool		usePrefix;		// if true, use prefix
    SpeakerVolume	speakerVolume;		// speaker loudness level
    ModemType		modemType;		// kind of modem
    fxBool		toneDialing;		// tone/pulse dialing
    fxBool		qualifyTSI;		// qualify TSI before recv
    int			rings;			// rings before answer
    int			pauseTime;		// pause time for ","
    int			waitTime;		// time to wait for carrier
    int			protocolTracing;	// protocol tracing level
    int			recvFileMode;		// recv file protection mode
    fxStr		tagLineFormat;		// tagline format string
    fxStr		tagLineFont;		// tagline font name
    fxBool		tagLineAtTop;		// tagline position
    RecvQPtrArray	recvq;
    SendQPtrArray	sendq;
    fxStr		printValue;		// current print dialog value
    fxStr		deliverValue;		// current deliver dialog value
    fxStr		forwardValue;		// current forward dialog value

    fxMultiChoice	serverStateChoice;	// start/stop server
    fxMultiChoice	speakerVolumeChoice;	// speak volume when on
    fxMultiChoice	qualifyTSIChoice;	// qualify TSI of receiver
    fxValuator*		ringsSlider;		// rings before answer
    StatusLabel*	ringsReadout;		// numeric readout for slider
    fxFieldEditor*	faxNumberEditor;	// fax machine number editor
    fxFieldEditor*	voiceNumberEditor;	// voice number editor
    fxButton*		answerButton;		// answer telephone button
    fxWindow*		uiWindow;		// for changing cursor
    u_short		startX, startY;		// starting position of window

    SendQList*		sendQueue;
    time_t		sendQModTime;
    fxTimer		sendQTimer;
    fxOutputChannel*	sendQsingleChannel;
    fxOutputChannel*	sendQmultiChannel;
    fxOutputChannel*	sendQnoChannel;

    RecvQList*		recvQueue;
    time_t		recvQModTime;
    fxTimer		recvQTimer;
    fxOutputChannel*	recvQsingleChannel;
    fxOutputChannel*	recvQmultiChannel;
    fxOutputChannel*	recvQnoChannel;

    fxMenuItem*		traceServerItem;
    fxMenuItem*		traceProtoItem;
    fxMenuItem*		traceModemOpsItem;
    fxMenuItem*		traceModemComItem;
    fxMenuItem*		traceTimeoutsItem;

    static const fxStr fifoName;
    static const fxStr configName;

    void setupInterface();
    fxMenu* setupMenu();
    void setupPhoneNumbers(fxViewStack*, fxLabel*, fxLabel*, u_int w);
    void setupServerControls(fxViewStack* parent, fxLabel* l, u_int w);
    void setupSpeakerVolume(fxViewStack* parent, fxLabel* l, u_int w);
    void setupAnswerRings(fxViewStack* parent, fxLabel* l, u_int w);
    void setupQualifyTSI(fxViewStack* parent, fxLabel* l, u_int w);

    fxViewStack* setupRightAdjustedLabel(fxLayoutConstraint, fxLabel*, u_int);
    void setupLabeledValue(fxViewStack* parent,
	fxLabel* l, u_int w, const fxStr& value);
    fxFieldEditor* setupLabeledEditor(fxViewStack* parent,
	fxLabel* l, u_int w, const fxStr& value);
    fxFieldEditor* setupFieldEditor(fxViewStack* parent,
	fxLabel* l, u_int w, fxLayoutConstraint fc = fixedSize);
    fxValuator*	setupSlider(fxViewStack* parent,
	fxLabel* l, u_int w, float low, float high,
	StatusLabel*& sl, const char* slwire);
    u_int maxViewWidth(fxView*, ...);

    fxMenu* setupTraceMenu();
    fxMenuItem* checkItem(fxMenuStack*, const char* tag, const char* wire);

    fxMenu* setupRecvQMenu();
    fxMenu* setupSendQMenu();
    void recvQItem(fxMenuStack*, const char* item, const char* wire, fxBool);
    void sendQItem(fxMenuStack*, const char* item, const char* wire, fxBool);

    fxViewStack* makeChoice(fxMultiChoice&,
	fxButton* (*buttonMaker)(const char*, fxFont* = 0),
	fxOrientation, fxLabel*, u_int w, va_list);
    fxViewStack* setupChoice(fxMultiChoice&, fxLabel*, u_int w, ...);
    fxViewStack* setupRadioChoice(fxMultiChoice&, fxLabel*, u_int w, ...);
    fxViewStack* setupCheckChoice(fxMultiChoice&, fxLabel*, u_int w, ...);
    fxViewStack* setupToggleChoice(fxMultiChoice&, fxLabel*, u_int w, ...);

    void restoreState(const char* file);
    void saveState(const char* file);
    void sendServer(const char* tag, const fxStr& value);
    void sendServer(const char* tag, int value);
    fxBool flushAndCheck();
    void setServerStatus(fxBool active);
    fxBool checkPhoneNumber(const fxStr& code, fxStr& canonicalResult);
    void setTracing(fxMenuItem*, int);

    ModemType getModemTypeFromName(const char*);
    const char* getNameFromModemType(const ModemType type);

    fxBool removeJob(SendQ *job);

    void doWarningDialog(char* msg);
    void badPhoneNumber();
    void badFont();
public:
    faxAdmin();
    ~faxAdmin();

    void open();
    void close();
    void initialize(int argc, char** argv);

    void help();

    void setFaxNumber(const fxStr& code);
    void setVoiceNumber(const fxStr& code);
    void setDialingPrefix(const fxStr& prefix);
    void setUseDialingPrefix(fxBool on);
    void setSpeakerVolume(int level);
    void setToneDialing(fxBool on);
    void setQualifyTSI(fxBool on);

    void setTagLineFormat(const fxStr& fmt);
    void setTagLineFont(const fxStr& font);
    void setTagLineAtTop(fxBool yes);

    void setRings();
    void setRingsReadout(int v);

    void traceServer();
    void traceProtocol();
    void traceModemOps();
    void traceModemCom();
    void traceTimeouts();

    void scanReceiveQueue();
    void recvView();
    void recvDeliver();
    void recvForward();
    void recvDelete();
    void recvPrint();
    void handleRecvQSelection();

    void scanSendQueue();
    void sendDelete();
    void sendMoveToTop();
    void sendMoveToBottom();
    void sendNow();
    void sendShowInfo();
    void handleSendQSelection();

    void answerPhone();
    void stopServer();
    void startServer();

    void readConfiguration();
    void writeConfiguration();
};
#endif /* _faxAdmin_ */
