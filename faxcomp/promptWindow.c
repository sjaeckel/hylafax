#include "Str.h"
#include "ViewStack.h"
#include "FieldEditor.h"
#include "Button.h"
#include "Font.h"
#include "Palette.h"
#include "Gap.h"
#include "Parts.h"
#include "Label.h"
#include "StyleGuide.h"
#include "StrScrollList.h"

#include "ConfirmDialog.h"
#include "FaxDB.h"
#include "promptWindow.h"

#include <sys/file.h>

fxStr promptWindow::locKey("Location");
fxStr promptWindow::compKey("Company");
fxStr promptWindow::phoneKey("Voice-Number");

static void s1(promptWindow* o)         { o->faxSend(); }
static void s2(promptWindow* o)         { o->faxCancel(); }

static void s10(promptWindow* o)         { o->setFax(); }
static void s11(promptWindow* o)         { o->setDestName(); }
static void s12(promptWindow* o)         { o->setLocation(); }
static void s13(promptWindow* o)         { o->setCompany(); }
static void s14(promptWindow* o)         { o->setPhone(); }
static void s15(promptWindow* o)         { o->setComments(); }

promptWindow::promptWindow()
{
    addInput("::faxSend",	fxDT_void,	this,	(fxStubFunc) s1);
    addInput("::faxCancel",	fxDT_void,	this,	(fxStubFunc) s2);
    addInput("::setFax",	fxDT_void,	this,	(fxStubFunc) s10);
    addInput("::setDestName",	fxDT_void,	this,	(fxStubFunc) s11);
    addInput("::setLocation",	fxDT_void,	this,	(fxStubFunc) s12);
    addInput("::setCompany",	fxDT_void,	this,	(fxStubFunc) s13);
    addInput("::setPhone",	fxDT_void,	this,	(fxStubFunc) s14);
    addInput("::setComments",	fxDT_void,	this,	(fxStubFunc) s15);

    sendChannel  = addOutput("send",   fxDT_void);
    cancelChannel  = addOutput("cancel",   fxDT_void);

    db = 0;
    curRec = 0;

    setupFax();
    setName("faxcomp");
    setTitle("FlexFAX Composer");
} 

promptWindow::~promptWindow() 
{
}

const char* promptWindow::className() const { return "promptWindow"; }

void
promptWindow::open()
{
    fxWindow::open();
    passFocus(nameEdit);
}

void
promptWindow::faxSend()
{
 // Use the person's name as the key into the fax db
    fxStr dest = nameEdit->getValue();
    if (dest.length() == 0) {		// try number instead
        dest = destEdit->getValue();
        if (dest.length() == 0) {
	    notifyUser(getApplication(), "No destination specified!");
            return;
        }
    } else if (db)
	updateRecord();
    sendVoid(sendChannel);
}

void
promptWindow::faxCancel()
{
    sendVoid(cancelChannel);
}

void
promptWindow::updateRecord()
{
    FaxDBRecord* r = db->find(nameEdit->getValue());
    if (r) {
	if (destEdit->getValue() != r->find(FaxDB::numberKey))
	    r->set(FaxDB::numberKey, destEdit->getValue());
	if (locEdit->getValue() != r->find(FaxDB::locationKey))
	    r->set(FaxDB::locationKey, locEdit->getValue());
	if (compEdit->getValue() != r->find(FaxDB::companyKey))
	    r->set(FaxDB::companyKey, compEdit->getValue());
	if (phoneEdit->getValue() != r->find(FaxDB::phoneKey))
	    r->set(FaxDB::phoneKey, phoneEdit->getValue());
	// XXX no way to update a record w/o rewriting file
	// XXX (which we don't do 'cuz it'd ruin structure)
    } else {
	r = new FaxDBRecord;
	r->set(FaxDB::numberKey, destEdit->getValue());
	r->set(FaxDB::locationKey, locEdit->getValue());
	r->set(FaxDB::companyKey, compEdit->getValue());
	r->set(FaxDB::phoneKey, phoneEdit->getValue());
	db->add(nameEdit->getValue(), r);
	appendToFile();
    }
}

extern "C" int flock(int, int);

void
promptWindow::appendToFile()
{
    FILE* fp = fopen(db->getFilename(), "a");
    if (!fp) {
	notifyUser(getApplication(), "Could not update fax database!");
	return;
    }
    flock(fileno(fp), LOCK_EX);
    fprintf(fp, "\n[\n");
    fprintf(fp, "Name:\"%s\"\n", (char*) nameEdit->getValue());
    fprintf(fp, "FAX-Number:\"%s\"\n", (char*) destEdit->getValue());
    fprintf(fp, "Company:\"%s\"\n", (char*) compEdit->getValue());
    fprintf(fp, "Location:\"%s\"\n", (char*) locEdit->getValue());
    fprintf(fp, "Voice-Number:\"%s\"\n", (char*) phoneEdit->getValue());
    fprintf(fp, "]\n");
    flock(fileno(fp), LOCK_UN);
    fclose(fp);
}

void
promptWindow::setupFax()
{
    fxViewStack* vs = new fxViewStack(vertical, fixedSize, fixedSize, 8,8,4);
    vs->setColor(fx_uiGray);
    vs->add(setupPhoneBook());
    vs->add(setupControls());
    add(vs);
}

fxViewStack*
promptWindow::setupPhoneBook()
{
    fxViewStack* vs = new fxViewStack(vertical, fixedSize, fixedSize,0,0,4);

    fxFont* cbF = fxGetFont("Helvetica-Bold", 11);

    fxLabel* faxLabel = new fxLabel("Fax Number: ",cbF);
    fxLabel* nameLabel = new fxLabel("Name: ",cbF);
    fxLabel* compLabel = new fxLabel("Company: ",cbF);
    fxLabel* locLabel = new fxLabel("Location:",cbF);
    fxLabel* phoneLabel = new fxLabel("Phone: ",cbF);
    fxLabel* commLabel = new fxLabel("Comments: ",cbF);
    fxLabel* qualLabel = new fxLabel("Quality: ",cbF);
    fxLabel* noteLabel = new fxLabel("Notify: ",cbF);
    u_int w = maxViewWidth(
	faxLabel, nameLabel, compLabel, locLabel,
	phoneLabel, commLabel, qualLabel, noteLabel,
	NULL);

    nameEdit = setupFieldEditor(vs, nameLabel, w, fixedSize, 3);
    nameEdit->connect("acceptValue", this, "::setDestName");
    nameEdit->setLengthLimit(40);
    compEdit = setupFieldEditor(vs, compLabel, w, fixedSize, 3);
    compEdit->connect("acceptValue", this, "::setCompany");
    compEdit->setLengthLimit(80);
    locEdit = setupFieldEditor(vs, locLabel, w, fixedSize, 3);
    locEdit->connect("acceptValue", this, "::setLocation");
    locEdit->setLengthLimit(80);
    destEdit = setupFieldEditor(vs,  faxLabel, w, fixedSize, 2);
    destEdit->connect("acceptValue", this, "::setFax");
    destEdit->setLengthLimit(40);
    destEdit->setPattern("^[ ]*[+]*[ ]*[0-9][0-9\. -]*$");
    phoneEdit = setupFieldEditor(vs, phoneLabel, w, fixedSize, 2);
    phoneEdit->connect("acceptValue", this, "::setPhone");
    destEdit->setPattern("^[ ]*[+]*[ ]*[0-9][0-9\. -]*$");
    phoneEdit->setLengthLimit(40);
    cmntEdit = setupFieldEditor(vs, commLabel, w, fixedSize, 3);
    cmntEdit->connect("acceptValue", this, "::setComments");
    cmntEdit->setLengthLimit(80);

    vs->add(setupNotify(noteLabel, w), alignLeft);
    vs->add(setupChoice(resChoice, qualLabel, w,
	"Regular",	98,
	"Fine",		196,
	"Super",	400,
	0), alignLeft);
    resChoice.setCurrentButton(0);	// default is 98 lpi
    return (vs);
}

fxViewStack*
promptWindow::setupNotify(fxLabel* l, u_int w)
{
    fxViewStack* hs = setupRightAdjustedLabel(fixedSize, l, w);
    notifyDone = fxMakeCheckButton("when sent");
    notifyDone->setState(FALSE);
    hs->add(notifyDone);
    notifyQue = fxMakeCheckButton("when re-queued");
    notifyQue->setState(FALSE);
    hs->add(notifyQue);
    return (hs);
}

fxViewStack*
promptWindow::setupControls()
{
    fxViewStack* hs = new fxViewStack(horizontal, variableSize, fixedSize,4,0,12);
    coverButton = fxMakeCheckButton("Cover Page");
    coverButton->setState(1);
    hs->add(coverButton);
    hs->add(new fxGap(horizontal, variableSize));
    sendButton = fxMakePushButton("Send");
    sendButton->connect("click", this, "::faxSend");
    sendButton->enable(FALSE);
    hs->add(sendButton);
    fxButton* canButton = fxMakePushButton("Cancel");
    canButton->connect("click", this, "::faxCancel");
    hs->add(canButton);
    return (hs);
}

void
promptWindow::passFocus(fxFieldEditor* editor)
{
    editor->getSharedEditor();
    editor->getKeyboard();
}

void promptWindow::setFaxDB(FaxDB* d)		{ db = d; }
void promptWindow::setCoverPage(fxBool b)	{ coverButton->setState(b); }
void promptWindow::setNotifyDone(fxBool b)	{ notifyDone->setState(b); }
void promptWindow::setNotifyRequeue(fxBool b)	{ notifyQue->setState(b); }

void
promptWindow::setResolution(float r)
{
    int b = resChoice.getCurrentButton();
    switch ((int) r) {
    case 98:	b = 0; break;
    case 196:	b = 1; break;
    case 400:	b = 2; break;
    }
    resChoice.setCurrentButton(b);
}

void
promptWindow::setFax()
{
    sendButton->enable(destEdit->getValue().length() > 0);
    passFocus(phoneEdit);
}
void promptWindow::setCompany()	{ passFocus(locEdit); }
void promptWindow::setLocation(){ passFocus(destEdit); }
void promptWindow::setPhone()	{ passFocus(cmntEdit); }
void promptWindow::setComments(){ passFocus(nameEdit); }

void
promptWindow::setDestName()
{
    const fxStr& s = nameEdit->getValue();
    if (s.length() > 0 && db) {
	fxStr name;
	FaxDBRecord* r = db->find(s, &name);
	if (r && r != curRec) {
	    nameEdit->setValue(name);
	    destEdit->setValue(r->find(FaxDB::numberKey));
	    locEdit->setValue(r->find(locKey));
	    compEdit->setValue(r->find(compKey));
	    phoneEdit->setValue(r->find(phoneKey));
	    sendButton->enable(TRUE);
	    curRec = r;
	}
    }
    passFocus(compEdit);
}

const fxStr& promptWindow::getFax()	{ return destEdit->getValue(); }
const fxStr& promptWindow::getDestName(){ return nameEdit->getValue(); }
const fxStr& promptWindow::getCompany()	{ return compEdit->getValue(); }
const fxStr& promptWindow::getLocation(){ return locEdit->getValue(); }
const fxStr& promptWindow::getPhone()	{ return phoneEdit->getValue(); }
const fxStr& promptWindow::getComments(){ return cmntEdit->getValue(); }

fxBool promptWindow::getNotifyDone()
    { return notifyDone->getCurrentState(); }
fxBool promptWindow::getNotifyRequeued()
    { return notifyQue->getCurrentState(); }
fxBool promptWindow::getCoverPage()
    { return coverButton->getCurrentState(); }
float promptWindow::getResolution()
    { return resChoice.getCurrentButton() ? 196. : 98.; }

u_int
promptWindow::maxViewWidth(fxView* va_alist, ...)
#define	view va_alist
{
    va_list ap;
    va_start(ap, view);
    u_int w = 0;
    do {
	w = fxmax(w, (u_int) view->getBounds().w);
    } while (view = va_arg(ap, fxView*));
    va_end(ap);
    return (w);
}
#undef l

fxViewStack*
promptWindow::setupRightAdjustedLabel(fxLayoutConstraint xc, fxLabel* l, u_int w)
{
    fxViewStack* hs = new fxViewStack(horizontal, xc, fixedSize, 0, 0, 4);
    hs->add(new fxGap(horizontal, fixedSize, w - l->getBounds().w));
    hs->add(l, alignCenter);
    return (hs);
}

fxFieldEditor*
promptWindow::setupFieldEditor(fxViewStack* parent, fxLabel* l, u_int w, fxLayoutConstraint fc, u_int fw)
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
		if ((fc != variableSize) && fw) {
		    fxBoundingBox bbox = editor->getBounds();
		    bbox.w = fw*fx_theStyleGuide->fieldEditorWidth;
		    editor->layout(bbox.w, bbox.h);
		}
	tile->add(editor);
	frame->add(tile);
	hs->add(frame);
    parent->add(hs, alignLeft);
    return (editor);
}

fxViewStack*
promptWindow::makeChoice(fxMultiChoice& c,
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
promptWindow::setupChoice(fxMultiChoice& c, fxLabel* label, u_int va_alist, ...)
#define	w va_alist
{
    va_list ap;
    va_start(ap, w);
    fxViewStack* vs = makeChoice(c, fxMakeToggleButton, horizontal, label, w, ap);
    va_end(ap);
    return (vs);
}
#undef width
