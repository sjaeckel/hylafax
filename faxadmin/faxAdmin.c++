#ident $Header: /usr/people/sam/flexkit/fax/faxadmin/RCS/faxAdmin.c++,v 1.12 91/05/28 22:18:35 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "faxAdmin.h"
#include "FaxTrace.h"
#include "ConfirmDialog.h"
#include "SendQList.h"
#include "RecvQList.h"
#include "config.h"

#include <osfcn.h>
#include <sys/fcntl.h>
#include <pwd.h>
#include <signal.h>

#include "Parts.h"
#include "ViewStack.h"
#include "Button.h"
#include "Label.h"
#include "Gap.h"
#include "FieldEditor.h"
#include "Font.h"
#include "Palette.h"
#include "Window.h"
#include "StyleGuide.h"
#include "Valuator.h"
#include "StatusLabel.h"
#include "Menu.h"
#include "EventHandler.h"
#include "RegEx.h"
#include "DialogBox.h"
#include "Menu.h"
#include "StrScrollList.h"

const fxStr faxAdmin::fifoName	= FAX_FIFO;
const fxStr faxAdmin::configName= FAX_CONFIG;

static void s0(faxAdmin* o)		{ o->close(); }
static void s1(faxAdmin* o)		{ o->answerPhone(); }
static void s2(faxAdmin* o, fxBool b)
    { b ? o->startServer() : o->stopServer(); }
static void s3(faxAdmin* o)		{ o->readConfiguration(); }
static void s4(faxAdmin* o)		{ o->writeConfiguration(); }
static void s5(faxAdmin* o)		{ o->scanSendQueue(); }
static void s6(faxAdmin* o)		{ o->scanReceiveQueue(); }
static void s7(faxAdmin* o)		{ o->help(); }

static void s20(faxAdmin* o, float v)	{ o->setRingsReadout((int) v); }

static void s31(faxAdmin* o, char* cp)	{ o->setDialingPrefix(cp); }
static void s32(faxAdmin* o, int v)	{ o->setUseDialingPrefix(v); }
static void s33(faxAdmin* o, char* cp)	{ o->setFaxNumber(cp); }
static void s34(faxAdmin* o, char* cp)	{ o->setVoiceNumber(cp); }
static void s36(faxAdmin* o, int v)	{ o->setSpeakerVolume(v); }
static void s37(faxAdmin* o, int v)	{ o->setToneDialing(v); }
static void s38(faxAdmin* o, char* cp)	{ o->setTagLineFormat(cp); }
static void s39(faxAdmin* o, char* cp)	{ o->setTagLineFont(cp); }
static void s40(faxAdmin* o, int v)	{ o->setTagLineAtTop(v); }
static void s41(faxAdmin* o)		{ o->setRings(); }
static void s42(faxAdmin* o, int v)	{ o->setQualifyTSI(v); }

static void s50(faxAdmin* o)		{ o->traceServer(); }
static void s51(faxAdmin* o)		{ o->traceProtocol(); }
static void s52(faxAdmin* o)		{ o->traceModemOps(); }
static void s53(faxAdmin* o)		{ o->traceModemCom(); }
static void s54(faxAdmin* o)		{ o->traceTimeouts(); }

static void s60(faxAdmin* o)		{ o->recvView(); }
static void s61(faxAdmin* o)		{ o->recvDeliver(); }
static void s62(faxAdmin* o)		{ o->recvDelete(); }
static void s63(faxAdmin* o)		{ o->recvPrint(); }
static void s64(faxAdmin* o)		{ o->recvForward(); }
static void s69(faxAdmin* o)		{ o->handleRecvQSelection(); }

static void s70(faxAdmin* o)		{ o->sendMoveToTop(); }
static void s71(faxAdmin* o)		{ o->sendMoveToBottom(); }
static void s72(faxAdmin* o)		{ o->sendDelete(); }
static void s73(faxAdmin* o)		{ o->sendShowInfo(); }
static void s74(faxAdmin* o)		{ o->sendNow(); }
static void s79(faxAdmin* o)		{ o->handleSendQSelection(); }

fxAPPINIT(faxAdmin,0);

faxAdmin::faxAdmin() :
    device("/dev/ttym2"),
    queueDir(FAX_SPOOLDIR)
{
    addInput("close",			fxDT_void,	this, (fxStubFunc) s0);
    addInput("help",			fxDT_void,	this, (fxStubFunc) s7);
    addInput("::answerPhone",		fxDT_void,	this, (fxStubFunc) s1);
    addInput("::controlServer",		fxDT_int,	this, (fxStubFunc) s2);
    addInput("::readConfiguration",	fxDT_void,	this, (fxStubFunc) s3);
    addInput("::writeConfiguration",	fxDT_void,	this, (fxStubFunc) s4);
    addInput("::scanSendQueue",		fxDT_void,	this, (fxStubFunc) s5);
    addInput("::scanRecvQueue",		fxDT_void,	this, (fxStubFunc) s6);

    addInput("::setRingsReadout",	fxDT_float,	this, (fxStubFunc) s20);

    addInput("::setDialingPrefix",	fxDT_CharPtr,	this, (fxStubFunc) s31);
    addInput("::setUseDialingPrefix",	fxDT_int,	this, (fxStubFunc) s32);
    addInput("::setFaxNumber",		fxDT_CharPtr,	this, (fxStubFunc) s33);
    addInput("::setVoiceNumber",	fxDT_CharPtr,	this, (fxStubFunc) s34);
    addInput("::setSpeakerVolume",	fxDT_int,	this, (fxStubFunc) s36);
    addInput("::setToneDialing",	fxDT_int,	this, (fxStubFunc) s37);
    addInput("::setTagLineFormat",	fxDT_CharPtr,	this, (fxStubFunc) s38);
    addInput("::setTagLineFont",	fxDT_CharPtr,	this, (fxStubFunc) s39);
    addInput("::setTagLineAtTop",	fxDT_int,	this, (fxStubFunc) s40);
    addInput("::setRings",		fxDT_void,	this, (fxStubFunc) s41);
    addInput("::setQualifyTSI",		fxDT_int,	this, (fxStubFunc) s42);

    addInput("::traceServer",		fxDT_void,	this, (fxStubFunc) s50);
    addInput("::traceProtocol",		fxDT_void,	this, (fxStubFunc) s51);
    addInput("::traceModemOps",		fxDT_void,	this, (fxStubFunc) s52);
    addInput("::traceModemCom",		fxDT_void,	this, (fxStubFunc) s53);
    addInput("::traceTimeouts",		fxDT_void,	this, (fxStubFunc) s54);

    addInput("::recvView",		fxDT_void,	this, (fxStubFunc) s60);
    addInput("::recvDeliver",		fxDT_void,	this, (fxStubFunc) s61);
    addInput("::recvDelete",		fxDT_void,	this, (fxStubFunc) s62);
    addInput("::recvPrint",		fxDT_void,	this, (fxStubFunc) s63);
    addInput("::recvForward",		fxDT_void,	this, (fxStubFunc) s64);
    addInput("::recvQSelection",	fxDT_void,	this, (fxStubFunc) s69);

    addInput("::sendMoveToTop",		fxDT_void,	this, (fxStubFunc) s70);
    addInput("::sendMoveToBottom",	fxDT_void,	this, (fxStubFunc) s71);
    addInput("::sendDelete",		fxDT_void,	this, (fxStubFunc) s72);
    addInput("::sendShowInfo",		fxDT_void,	this, (fxStubFunc) s73);
    addInput("::sendNow",		fxDT_void,	this, (fxStubFunc) s74);
    addInput("::sendQSelection",	fxDT_void,	this, (fxStubFunc) s79);

    fifo = NULL;
    isDirty = FALSE;
    okToUpdate = FALSE;
    speakerVolume = FaxModem::QUIET;
    usePrefix = FALSE;
    toneDialing = TRUE;
    qualifyTSI = FALSE;
    rings = -1;
    pauseTime = -1;
    waitTime = -1;
    protocolTracing = 0;
    recvFileMode = 0600;
    tagLineAtTop = TRUE;
    startX = 150;
    startY = 350;
    printValue = fxStr(FAX_BINDIR) | "/" | FAX_PRINTFAX;

    sendQTimer.connect("tick", this, "::scanSendQueue");
    sendQTimer.setTimerDuration(5);
    sendQModTime = 0;
    recvQTimer.connect("tick", this, "::scanRecvQueue");
    recvQTimer.setTimerDuration(5);
    recvQModTime = 0;
}

faxAdmin::~faxAdmin()
{
}

#include <getopt.h>

void
faxAdmin::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;
    fxBool runReadOnly = FALSE;

    appName = argv[0];
    u_int l = appName.length();
    appName = appName.tokenR(l, '/');
    while ((c = getopt(argc, argv, "m:q:x:y:r")) != -1)
	switch (c) {
	case 'm':
	    device = optarg;
	    break;
	case 'q':
	    queueDir = optarg;
	    break;
	case 'r':
	    runReadOnly = TRUE;
	    break;
	case 'x':
	    startX = atoi(optarg);
	    break;
	case 'y':
	    startY = atoi(optarg);
	    break;
	case '?':
	    fprintf(stderr, "usage: %s"
	       " [-m modem-device]"
	       " [-q queue-directory]"
	       " [-x xpos] [-y ypos]"
	       " [-r]"
	       "\n", (char*) appName);
	    exit(-1);
	}
    int uid = getuid();
    if (uid == 0) {
	struct passwd* pwd = getpwnam(FAX_USER);
	if (!pwd)
	    fxFatal("No fax user \"%s\" defined on your system!\n"
		"This software is not installed properly!");
	(void) setuid(pwd->pw_uid);
	(void) setgid(pwd->pw_gid);
	okToUpdate = !runReadOnly;
    } else if (!runReadOnly) {
	struct passwd* pwd = getpwuid(uid);
	if (!pwd)
	    fxFatal("Can not figure out the identity of uid %d", uid);
	if (strcmp(pwd->pw_name, FAX_USER) == 0)
	    okToUpdate = TRUE;
    }
    if (okToUpdate)
	startY -= 50;
}

void
faxAdmin::open()
{
    if (!opened) {
	fxVisualApplication::open();

	u_int l = device.length();
	const fxStr& dev = device.tokenR(l, '/');
	restoreState(queueDir | "/" | configName | "." | dev);

	setupInterface();

	scanSendQueue();
	scanReceiveQueue();

	// flush pending events related to startup
	while (fx_theEventHandler->getNumberOfPendingEvents())
	    fx_theEventHandler->distributeEvents();
	isDirty = FALSE;

	int fd =
	    ::open(queueDir | "/" | fifoName | "." | dev, O_WRONLY|O_NDELAY);
	if (fd >= 0)
	    fifo = fdopen(fd, "w");
	setServerStatus(fifo != NULL);
// XXX include file bogosity
	(void) signal(SIGPIPE, (void (*)(int ... )) SIG_IGN);	// for FIFO writes w/o server
    }
}

void
faxAdmin::close()
{
    if (isDirty && !confirmRequest(this,
      "The configuration changed, but has not been saved, still quit?"))
	return;
    if (fifo)
	fclose(fifo);
    fxVisualApplication::close();
}

void
faxAdmin::setupInterface()
{
    fxFont* fnt = fxGetFont("Helvetica-Bold", 11);
    fxLabel* ringsLabel = new fxLabel("Rings Before Answer:", fnt);
    fxLabel* speakerLabel = new fxLabel("Modem Speaker:", fnt);
    fxLabel* faxLabel = new fxLabel("FAX Phone Number:", fnt);
    fxLabel* voiceLabel = new fxLabel("Voice Phone Number:", fnt);
    fxLabel* statusLabel = new fxLabel("Server Status:", fnt);
    fxLabel* qualifyLabel = new fxLabel("Qualify TSI:", fnt);
    fxLabel* deviceLabel = new fxLabel("Device:", fnt);
    fxLabel* queueDirLabel = new fxLabel("Spooling Directory:", fnt);

    u_int w = maxViewWidth(
	deviceLabel, queueDirLabel, statusLabel, faxLabel,
	speakerLabel, ringsLabel, voiceLabel, qualifyLabel,
	0);

    fxViewStack* panel = new
	fxViewStack(vertical, variableSize, fixedSize, 16, 12, 10);
    fxGLColor c(fx_uiGray);
    c.darken(.10);
    panel->setColor(c);

    setupLabeledValue(panel, deviceLabel, w, device);
    setupLabeledValue(panel, queueDirLabel, w, queueDir);
    setupPhoneNumbers(panel, faxLabel, voiceLabel, w);
    setupServerControls(panel, statusLabel, w);
    setupQualifyTSI(panel, qualifyLabel, w);
    setupSpeakerVolume(panel, speakerLabel, w);
    setupAnswerRings(panel, ringsLabel, w);

    panel->add(new fxGap(vertical, fixedSize, 4));
    { fxViewStack* hs = setupRightAdjustedLabel(variableSize,
	    new fxLabel("Send Queue:", fnt), w);
	sendQueue = new SendQList(variableSize, fixedSize);
	(void) fxMakeList(sendQueue, new SendQListTitle(sendQueue), hs,
			variableSize, fixedSize, 0, 100);
	sendQueue->setPopup(setupSendQMenu());
	panel->add(hs, alignLeft);
    }
    panel->add(new fxGap(vertical, fixedSize, 4));
    { fxViewStack* hs = setupRightAdjustedLabel(variableSize,
	    new fxLabel("Receive Queue:", fnt), w);
	recvQueue = new RecvQList(variableSize, fixedSize);
	(void) fxMakeList(recvQueue, new RecvQListTitle(recvQueue), hs,
			variableSize, fixedSize, 0, 100);
	recvQueue->setPopup(setupRecvQMenu());
	panel->add(hs, alignLeft);
    }
    panel->add(new fxGap(vertical, fixedSize, 4));

    panel->add(new fxGap(vertical, fixedSize, 8));
    { fxViewStack* hs = new fxViewStack(horizontal, variableSize, fixedSize,0,0,6);
      fxButton* b;
      if (okToUpdate) {
	  b = fxMakePushButton("Save");
	      b->connect("click", this, "::writeConfiguration");
	      hs->add(b);
	  b = fxMakePushButton("Restore");
	      b->connect("click", this, "::readeConfiguration");
	      hs->add(b);
      }
      hs->add(new fxGap(horizontal, variableSize));
      b = fxMakePushButton("Quit");
	  b->connect("click", this, "close");
	  hs->add(b);
      b = fxMakePushButton("Help");
	  b->connect("click", this, "help");
	  hs->add(b);
      panel->add(hs);
    }
    panel->setPopup(setupMenu());

    uiWindow = new fxWindow;
    uiWindow->add(panel);
    uiWindow->setDisableQuit(TRUE);
    uiWindow->setDisableQuit(TRUE);
    uiWindow->setName("faxadmin");
    uiWindow->setTitle("FlexFAX Manager");
    uiWindow->setInitialSize(600, uiWindow->getMinimumHeight());
    uiWindow->setInitialOrigin(startX, startY);
    add(uiWindow);

    uiWindow->open();
}

fxMenu*
faxAdmin::setupMenu()
{
    fxMenuStack* stack = new fxMenuStack(vertical);
    stack->add(new fxMenuItem("Dialing Info..."));
    stack->add(new fxMenuItem("TagLine Info..."));
    stack->add(new fxMenuItem("Tracing", setupTraceMenu()));
    stack->scoreMark(3);
#ifdef notdef
    fxMenuItem* mi;
    stack->add(mi = new fxMenuItem("Save Config"),this, "::writeConfiguration");
    if (!okToUpdate)
	mi->disableByClick(0);
    stack->add(new fxMenuItem("Restore Config"),this,"::readConfiguration");
#else
    stack->add(new fxMenuItem("Quit"), this, "close");
#endif
    return (new fxMenuBar->add(new fxMenuItem("FlexFAX", stack)));
}

fxMenu*
faxAdmin::setupTraceMenu()
{
    fxMenuStack* stack = new fxMenuStack(vertical);
    traceServerItem =
	checkItem(stack, "Server Operation",	"::traceServer");
	traceServerItem->setState(protocolTracing & FAXTRACE_SERVER);
    traceProtoItem =
	checkItem(stack, "FAX Protocol",	"::traceProtocol");
	traceProtoItem->setState(protocolTracing & FAXTRACE_PROTOCOL);
    traceModemOpsItem =
	checkItem(stack, "Modem Operations",	"::traceModemOps");
	traceModemOpsItem->setState(protocolTracing & FAXTRACE_MODEMOPS);
    traceModemComItem =
	checkItem(stack, "Modem Communication",	"::traceModemCom");
	traceModemComItem->setState(protocolTracing & FAXTRACE_MODEMCOM);
    traceTimeoutsItem =
	checkItem(stack, "Timeouts",		"::traceTimeouts");
	traceTimeoutsItem->setState(protocolTracing & FAXTRACE_TIMEOUTS);
    return (stack);
}

fxMenuItem*
faxAdmin::checkItem(fxMenuStack* stack, const char* tag, const char* wire)
{
    fxMenuItem* mi = new fxMenuItem(tag, 0, fxMenuItem::checkMark);
    mi->setState(0);
    if (!okToUpdate) {
	mi->disableByClick(0);
	stack->add(mi);
    } else
	stack->add(mi, this, wire);
    return (mi);
}

void
faxAdmin::setupPhoneNumbers(fxViewStack* parent, fxLabel* fl, fxLabel* vl, u_int w)
{
    if (okToUpdate) {
	faxNumberEditor = setupLabeledEditor(parent, fl, w, faxNumber);
	faxNumberEditor->connect("value", this, "::setFaxNumber");
    } else
	setupLabeledValue(parent, fl, w, faxNumber);
    if (okToUpdate) {
	voiceNumberEditor = setupLabeledEditor(parent, vl, w, voiceNumber);
	voiceNumberEditor->connect("value", this, "::setVoiceNumber");
    } else
	setupLabeledValue(parent, vl, w, voiceNumber);
}

void
faxAdmin::setupServerControls(fxViewStack* parent, fxLabel* l, u_int w)
{
    fxViewStack* hs = setupRightAdjustedLabel(fixedSize, l, w);
    if (okToUpdate) {
	fxButton* b = fxMakeToggleButton("Stop");;
	    serverStateChoice.add(b);
	hs->add(b);
	b = fxMakeToggleButton("Start");
	    serverStateChoice.add(b);
	hs->add(b);
	serverStateChoice.connect("pick", this, "::controlServer");
    }
    statusReadout = new StatusLabel;
    statusReadout->setFillColor(parent->getFillColor());
    fxFont* f = statusReadout->getFont();
    statusReadout->layout(f->getWidth("InActive"), f->getMaxHeight("InActive"));
    hs->add(statusReadout, alignCenter);
    if (okToUpdate) {
	answerButton = fxMakePushButton("Answer Phone");
	    answerButton->connect("click", this, "::answerPhone");
	hs->add(answerButton);
    }
    parent->add(hs, alignLeft);
}

void
faxAdmin::setupQualifyTSI(fxViewStack* parent, fxLabel* l, u_int w)
{
    fxViewStack* hs = setupRightAdjustedLabel(fixedSize, l, w);
    if (okToUpdate) {
	parent->add(setupChoice(qualifyTSIChoice, l, w,
	    "No",	FALSE,
	    "Yes",	TRUE,
	    0), alignLeft);
	qualifyTSIChoice.setCurrentButton(qualifyTSI);
	qualifyTSIChoice.connect("pick", this, "::setQualifyTSI");
    } else
	setupLabeledValue(parent, l, w, qualifyTSI ? "Yes" : "No");
}

void
faxAdmin::setupSpeakerVolume(fxViewStack* parent, fxLabel* l, u_int w)
{
    if (okToUpdate) {
	parent->add(setupChoice(speakerVolumeChoice, l, w,
	    "Off",	FaxModem::OFF,
	    "Quiet",	FaxModem::QUIET,
	    "Low",	FaxModem::LOW,
	    "Medium",	FaxModem::MEDIUM,
	    "High",	FaxModem::HIGH,
	    0), alignLeft);
	    speakerVolumeChoice.setCurrentButton(speakerVolume);
	    speakerVolumeChoice.connect("pick", this, "::setSpeakerVolume");
    } else
	setupLabeledValue(parent, l, w, 
	    speakerVolume == FaxModem::OFF ?	"Off"	:
	    speakerVolume == FaxModem::QUIET ?	"Quiet"	:
	    speakerVolume == FaxModem::LOW ?	"Low"	:
	    speakerVolume == FaxModem::MEDIUM ?"Medium":
	    speakerVolume == FaxModem::HIGH ?	"High"	:
						"???");
}

void
faxAdmin::setupAnswerRings(fxViewStack* parent, fxLabel* l, u_int w)
{
    if (okToUpdate) {
	ringsSlider = setupSlider(parent, l, w,
	    0, 7,
	    ringsReadout,	"::setRingsReadout");
	    ringsSlider->setValue(rings);
	    setRingsReadout(rings);
	ringsSlider->connect("endDrag", this, "::setRings");
    } else {
	char buf[80];
	sprintf(buf, rings == 0 ? "don't answer" :
		     rings == 1 ? "1 ring" :
				  "%d rings",
		rings);
	setupLabeledValue(parent, l, w, buf);
    }
}

void
faxAdmin::setServerStatus(fxBool active)
{
    if (active) {
	statusReadout->setTextColor(fx_lightOlive);
	statusReadout->setValue("Running");
    } else {
	statusReadout->setTextColor(fx_red);
	statusReadout->setValue("InActive");
    }
    if (okToUpdate) {
	serverStateChoice.setCurrentButton(active);
	answerButton->enable(active);
    }
}

u_int
faxAdmin::maxViewWidth(fxView* va_alist, ...)
#define	view va_alist
{
    va_list ap;
    va_start(ap, view);
    u_int w = 0;
    do {
	w = fxmax(w, (u_int) view->getMinimumWidth());
    } while (view = va_arg(ap, fxView*));
    va_end(ap);
    return (w);
}
#undef l

fxViewStack*
faxAdmin::setupRightAdjustedLabel(fxLayoutConstraint xc, fxLabel* l, u_int w)
{
    fxViewStack* hs = new fxViewStack(horizontal, xc, fixedSize, 0, 0, 4);
    hs->add(new fxGap(horizontal, fixedSize, w - l->getMinimumWidth()));
    hs->add(l, alignCenter);
    return (hs);
}

void
faxAdmin::setupLabeledValue(fxViewStack* parent,
    fxLabel* l, u_int w, const fxStr& value)
{
    fxViewStack* hs = setupRightAdjustedLabel(fixedSize, l, w);
	fxLabel* v = new fxLabel(value);
	v->setColor(fx_blue);
	hs->add(v);
    parent->add(hs, alignLeft);
}

fxFieldEditor*
faxAdmin::setupLabeledEditor(fxViewStack* parent,
    fxLabel* l, u_int w, const fxStr& value)
{
    fxFieldEditor* editor = setupFieldEditor(parent, l, w, fixedSize);
	editor->setValue(value);
    return (editor);
}

fxFieldEditor*
faxAdmin::setupFieldEditor(fxViewStack* parent, fxLabel* l, u_int w, fxLayoutConstraint fc)
{
    fxGLColor& face = fx_theStyleGuide->fieldEditorPageColor;
    fxGLColor topLeft = fx_lightGray;
    fxGLColor botRight;
    botRight.blend(face, fx_uiCharcoal);

    fxViewStack* hs = setupRightAdjustedLabel(fc, l, w);
	fxViewStack* frame = fxMakeFrame(horizontal, fc, fixedSize);
	    fxViewStack* tile = fxMakeTile(horizontal, fc, fixedSize,
	            face, topLeft, botRight, 1, FALSE, FALSE);
	    fxFieldEditor* editor = new fxFieldEditor(fc, 2, 2);
	tile->add(editor);
	frame->add(tile);
	hs->add(frame);
    parent->add(hs, alignLeft);
    return (editor);
}

fxValuator*
faxAdmin::setupSlider(fxViewStack* top,
    fxLabel* l, u_int w, float low, float high,
    StatusLabel*& sl, const char* slwire)
{
    fxViewStack* hs = setupRightAdjustedLabel(variableSize, l, w);
	fxValuator *v = fxMakeHorizontalSlider();
	    v->setLowerBound(low);
	    v->setUpperBound(high);
	    v->connect("value", this, slwire);
	    v->setMinimumSize(fx_theStyleGuide->scrollListWidth, v->getMinimumHeight());
	hs->add(fxMakeHorizontalValuator(v, 2, 3, TRUE, "%g"));
	hs->add(sl = new StatusLabel, alignBottom);
	sl->setFillColor(top->getFillColor());
    top->add(hs);
    return (v);
}

fxViewStack*
faxAdmin::makeChoice(fxMultiChoice& c,
    fxButton* (*buttonMaker)(const char* face, fxFont* f = 0),
    fxOrientation o,
    fxLabel* label, u_int w, va_list ap)
{
    char* face;

    fxViewStack* bs = new fxViewStack(o, fixedSize, fixedSize, 0, 0, 2);
    u_int maxw = 0;
    while (face = va_arg(ap, char*)) {
	fxButton* b = (*buttonMaker)(face);
	c.add(b);
	bs->add(b, o == vertical ? alignLeft : alignCenter);
	b->setClickValue(va_arg(ap, int));
	maxw = fxmax(maxw, (u_int) b->getMinimumWidth());
    }
    if (o == vertical) {
	// make all buttons the same width
	for (int i = c.elements()-1; i >= 0; i--) {
	    fxButton* b = c[i];
	    b->setMinimumSize(maxw, b->getMinimumHeight());
	}
    }
    if (label) {
	fxViewStack* hs = setupRightAdjustedLabel(fixedSize, label, w);
	hs->add(bs);
	return (hs);
    } else
	return (bs);
}

fxViewStack*
faxAdmin::setupChoice(fxMultiChoice& c, fxLabel* label, u_int va_alist, ...)
#define	w va_alist
{
    va_list ap;
    va_start(ap, w);
    fxViewStack* vs = makeChoice(c, fxMakeToggleButton, horizontal, label, w, ap);
    va_end(ap);
    return (vs);
}
#undef width

fxViewStack*
faxAdmin::setupRadioChoice(fxMultiChoice& c, fxLabel* label, u_int va_alist, ...)
#define	w va_alist
{
    va_list ap;
    va_start(ap, w);
    fxViewStack* vs = makeChoice(c, fxMakeRadioButton, vertical, label, w, ap);
    va_end(ap);
    return (vs);
}
#undef width

fxViewStack*
faxAdmin::setupCheckChoice(fxMultiChoice& c, fxLabel* label, u_int va_alist, ...)
#define	w va_alist
{
    va_list ap;
    va_start(ap, w);
    fxViewStack* vs = makeChoice(c, fxMakeCheckButton, horizontal, label, w, ap);
    va_end(ap);
    return (vs);
}
#undef width

fxViewStack*
faxAdmin::setupToggleChoice(fxMultiChoice& c, fxLabel* label, u_int va_alist, ...)
#define	w va_alist
{
    va_list ap;
    va_start(ap, w);
    fxViewStack* vs = makeChoice(c, fxMakeToggleButton, vertical, label, w, ap);
    va_end(ap);
    return (vs);
}
#undef width

void
faxAdmin::doWarningDialog(char* msg)
{
    fxDialogBox* box;
    box = fxMakeAlertDialog(this, fxMakeWarningDialogIcon(), msg, TRUE);
    box->setStartWithMouseInView(
	box->addGoAwayButton("OK", fxMakeAcceptButton("OK")));
    box->setTitle("Notifier");
    box->start(TRUE);
    remove(box);
}

#define	WarningBOX(name, message) \
    void faxAdmin::name() { doWarningDialog(message); }
WarningBOX(badPhoneNumber,	"Invalid phone number: "
	"number must be of the form \"areacode-number\" or "
	"\"countrycode-areacode-number\".  (White space or hyphens"
	"can be used for separation.)");
WarningBOX(badFont,		"Can not locate font with that name.");
#undef WarningBOX
