#ident $Header: /usr/people/sam/flexkit/fax/faxview/RCS/FAXViewer.c++,v 1.14 91/08/04 13:06:42 sam Exp $

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
#include "Window.h"
#include "ViewStack.h"
#include "Parts.h"
#include "Gap.h"
#include "Palette.h"
#include "Menu.h"
#include "Overlayer.h"
#include "EventHandler.h"
#include "Button.h"
#include "DialogBox.h"
#include "Cursor.h"
#include "ImageView.h"
#include "Bitmap.h"

#include "tiffio.h"
#include <getopt.h>
#include <osfcn.h>

const int ORIENT_NONE		= 0x0;
const int ORIENT_FLIP		= 0x1;
const int ORIENT_REVERSE	= 0x2;
const int ORIENT_OPS		= 0x3;

struct Page {
    fxMenuItem*	menu;
    fxBool	print;
    fxBool	read;
    int		orient;
};

class FAXViewer : public fxVisualApplication {
private:
    TIFF*	tif;
    ImageView*	pageView;
    fxBitmap*	page;
    fxStr	filename;
    fxStr	printCmd;
    fxMenu*	pageMenu;
    fxMenuItem*	nextPage;
    fxMenuItem*	prevPage;
    fxMenuItem*	linearItem;
    fxMenuItem*	expItem;
    fxMenuItem*	exp50Item;
    fxMenuItem*	exp60Item;
    fxMenuItem*	exp70Item;
    fxMenuItem*	curContrast;
    fxMenuItem*	orientItems[4];
    const fxCursor* prevCursor;
    short	curPageNum;
    short	maxPageNum;
    short	minPageNum;
    int		curDirnum;
    int		maxDirnum;
    fxBool	small;
    fxBool	removeFile;
    Page*	pages;
    fxWindow*	win;

    void usage(const char* appName);
    void uudecode(FILE* in, fxStr& filename);
    void selectPage(u_short);
    fxMenu* setupMenu();
    fxMenu* setupContrastMenu();
    fxMenu* setupOrientMenu();
    void setupPageMenu();
    fxMenuItem* checkItem(fxMenuStack*, const char* tag, const char* wire);
    void updateOrient(int o);

    void setContrast(fxMenuItem*, Contrast);

    void beginSlowOperation();
    void endSlowOperation();

    void doWarningDialog(char* msg);
    void cantOpenFile();
    void cantAccessDirectory();
    void warnAboutOrientation();
    void printCmdProblem();
public:
    FAXViewer();
    ~FAXViewer();

    void initialize(int argc, char** argv);
    void open();
    void close();

    void gotoDirectory(int dirnum);
    void gotoNextPage()	{ gotoDirectory(fxmin(curDirnum+1, maxDirnum)); }
    void gotoPrevPage()	{ gotoDirectory(fxmax(curDirnum-1, 0)); }
    void select(fxInputEvent&) { gotoNextPage(); }
    void adjust(fxInputEvent&)	{ gotoPrevPage(); }

    void setLinearContrast();
    void setExpContrast();
    void setExp50Contrast();
    void setExp60Contrast();
    void setExp70Contrast();
    void setOrientation(int orient);
    void printPage();
    void printAll();
    void print();
};

fxAPPINIT(FAXViewer, 0);

static void s0(FAXViewer* o)		{ o->close(); }
static void s1(FAXViewer* o, int v)	{ o->gotoDirectory(v); }
static void s3(FAXViewer* o)		{ o->printPage(); }
static void s4(FAXViewer* o)		{ o->printAll(); }
static void s5(FAXViewer* o)		{ o->print(); }
static void s7(FAXViewer* o)		{ o->gotoNextPage(); }
static void s8(FAXViewer* o)		{ o->gotoPrevPage(); }

static void s10(FAXViewer* o) { o->setOrientation(ORIENT_FLIP); }
static void s11(FAXViewer* o) { o->setOrientation(ORIENT_REVERSE); }
static void s12(FAXViewer* o) { o->setOrientation(ORIENT_FLIP|ORIENT_REVERSE); }

static void s20(FAXViewer* o) { o->setLinearContrast(); }
static void s21(FAXViewer* o) { o->setExpContrast(); }
static void s22(FAXViewer* o) { o->setExp50Contrast(); }
static void s23(FAXViewer* o) { o->setExp60Contrast(); }
static void s24(FAXViewer* o) { o->setExp70Contrast(); }

fxSTUBINPUTEVENT(FAXViewer,select);
fxSTUBINPUTEVENT(FAXViewer,adjust);

static const char* table = "\
<selectdown>:	select()\n\
<adjustdown>:	adjust()\n\
";

FAXViewer::FAXViewer()
{
    fxADDINPUTEVENT(FAXViewer,select);
    fxADDINPUTEVENT(FAXViewer,adjust);
    addInput("close",		fxDT_void,	this, (fxStubFunc) s0);
    addInput("gotoPage",	fxDT_int,	this, (fxStubFunc) s1);
    addInput("printPage",	fxDT_void,	this, (fxStubFunc) s3);
    addInput("printAll",	fxDT_void,	this, (fxStubFunc) s4);
    addInput("print",		fxDT_void,	this, (fxStubFunc) s5);
    addInput("::nextPage",	fxDT_void,	this, (fxStubFunc) s7);
    addInput("::prevPage",	fxDT_void,	this, (fxStubFunc) s8);

    addInput("flipPage",	fxDT_void,	this, (fxStubFunc) s10);
    addInput("reversePage",	fxDT_void,	this, (fxStubFunc) s11);
    addInput("flip&reversePage",fxDT_void,	this, (fxStubFunc) s12);

    addInput("setLinearContrast",fxDT_void,	this, (fxStubFunc) s20);
    addInput("setExpContrast",	fxDT_void,	this, (fxStubFunc) s21);
    addInput("setExp50Contrast",fxDT_void,	this, (fxStubFunc) s22);
    addInput("setExp60Contrast",fxDT_void,	this, (fxStubFunc) s23);
    addInput("setExp70Contrast",fxDT_void,	this, (fxStubFunc) s24);

    tif = 0;
    pageView = 0;
    curContrast = 0;
    small = FALSE;
    curDirnum = -1;
    maxDirnum = -1;
    curPageNum = -1;
    removeFile = FALSE;
    pages = new Page;
    pages[0].print = TRUE;
}

FAXViewer::~FAXViewer()
{
}

static fxBool
isFAXImage(TIFF* tif)
{
    u_short w;
    if (TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &w) && w != 1)
	return (FALSE);
    if (TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &w) && w != 1)
	return (FALSE);
    if (!TIFFGetField(tif, TIFFTAG_COMPRESSION, &w) ||
      (w != COMPRESSION_CCITTFAX3 && w != COMPRESSION_CCITTFAX4))
	return (FALSE);
    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &w) ||
      (w != PHOTOMETRIC_MINISWHITE && w != PHOTOMETRIC_MINISBLACK))
	return (FALSE);
    return (TRUE);
}

void
FAXViewer::usage(const char* appName)
{
    fprintf(stderr, "usage: %s [-b] [-s] [-u] [file.tif]\n", appName);
    exit(-1);
}

void
FAXViewer::initialize(int argc, char** argv)
{
    extern int optind;
    extern char* optarg;
    int c;
    fxBool decode = FALSE;

    while ((c = getopt(argc, argv, ":bsu")) != -1)
	switch (c) {
	case 'b':
	    small = FALSE;
	    break;
	case 's':
	    small = TRUE;
	    break;
	case 'u':
	    decode = TRUE;
	    break;
	case '?':
	    usage(argv[0]);
	    /*NOTREACHED*/
	}
    if (optind == argc) {		// decode standard input
	if (decode) {
	    uudecode(stdin, filename);
	    removeFile = TRUE;		// remove temp file on exit
	} else
	    usage(argv[0]);
    } else
	filename = argv[optind];
    tif = TIFFOpen(filename, "r");
    if (!tif || !isFAXImage(tif)) {
	fprintf(stderr, "%s: Not a FAX image.\n", (char*) filename);
	exit(-2);
    }
    setupPageMenu();
}

void
FAXViewer::setupPageMenu()
{
    // construct page-number <-> directory map
    int dirnum = 0;
    maxPageNum = -1;
    minPageNum = 10000;
    pageMenu = new fxMenuStack(vertical);
    do {
	short pn, total;
	if (!TIFFGetField(tif, TIFFTAG_PAGENUMBER, &pn, &total))
	    pn = dirnum;			// makeup page number
	char buf[32];
	sprintf(buf, "%-3d", pn+1);
	fxMenuItem* mi = new fxMenuItem(buf);
	mi->setGizmo(fxMenuItem::radioButton);
	pageMenu->add(mi, this, "gotoPage");
	mi->setClickValue(dirnum++);
	if (pn > maxDirnum) {
	    maxDirnum = pn;
	    pages = (Page*)realloc(pages, (maxDirnum+1) * sizeof (Page));
	}
	pages[pn].print = TRUE;
	pages[pn].read = FALSE;
	pages[pn].menu = mi;
	pages[pn].orient = 0;
	if (pn < minPageNum)
	    minPageNum = pn;
	if (pn > maxPageNum)
	    maxPageNum = pn;
    } while (TIFFReadDirectory(tif));
}

inline int DEC(char c) { return ((c - ' ') & 077); }

extern "C" {
    int mkstemp(char*);
}

void
FAXViewer::uudecode(FILE* in, fxStr& temp)
{
    char buf[1024];
    do {
	if (!fgets(buf, sizeof (buf), in)) {
	    fprintf(stderr, "Missing \"begin\" line\n");
	    exit(-1);
	}
    } while (strncmp(buf, "begin ", 6));
    temp = "/tmp/faxvXXXXXX";
    int fd = mkstemp(temp);
    if (fd == -1) {
	fprintf(stderr, "Could not create temp file \"%s\"\n", (char*)temp);
	exit(-2);
    }
    FILE* out = fdopen(fd, "w");
    for (;;) {
	if (!fgets(buf, sizeof (buf), in)) {
	    fprintf(stderr, "Warning, improperly formatted data\n");
	    break;
	}
	char* cp = buf;
	int n = DEC(*cp);
	if (n <= 0)
	    break;
	int c;
	for (cp++; n >= 3; cp += 4, n -= 3) {
	    c = (DEC(cp[0])<<2) | (DEC(cp[1])>>4); putc(c, out);
	    c = (DEC(cp[1])<<4) | (DEC(cp[2])>>2); putc(c, out);
	    c = (DEC(cp[2])<<6) |  DEC(cp[3]);	   putc(c, out);
	}
	if (n >= 1)
	    c = (DEC(cp[0])<<2) | (DEC(cp[1])>>4), putc(c, out);
	if (n >= 2)
	    c = (DEC(cp[1])<<4) | (DEC(cp[2])>>2), putc(c, out);
	if (n >= 3)
	    c = (DEC(cp[2])<<6) |  DEC(cp[3]),	   putc(c, out);
    }
    while (fgets(buf, sizeof (buf), in))	// flush input
	;
    fclose(out);
}

void
FAXViewer::selectPage(u_short pn)
{
    if (curPageNum != -1)
	pages[curPageNum].menu->setState(0);
    pages[pn].menu->setState(1);
    if (pn == minPageNum)
	prevPage->disableByClick(0);
    else
	prevPage->enableByClick(0);
    if (pn == maxPageNum)
	nextPage->disableByClick(0);
    else
	nextPage->enableByClick(0);
    curPageNum = pn;
}

void
FAXViewer::gotoDirectory(int dirnum)
{
    if (dirnum == curDirnum)
	return;
    if (!TIFFSetDirectory(tif, dirnum)) {
	cantAccessDirectory();
	return;
    }
    curDirnum = dirnum;
    u_short pn, total;
    if (!TIFFGetField(tif, TIFFTAG_PAGENUMBER, &pn, &total))
	pn = dirnum;			// makeup page number
    selectPage(pn);
    beginSlowOperation();
    u_short photometric = PHOTOMETRIC_MINISWHITE;
    (void) TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    pageView->setPhotometric(photometric, FALSE);
    float yres;
    if (!TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)) {
	TIFFWarning(TIFFFileName(tif), "No y-resolution, assuming 98 lpi");
	yres = 98;					/* XXX */
    }
    u_short unit;
    TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &unit);
    if (unit == RESUNIT_CENTIMETER)
	yres *= 25.4;
    u_long w, h;
    (void) TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    (void) TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    if (!page || page->r.w != w || page->r.h != h) {
	if (page)
	    delete page;
	page = new fxBitmap((u_int) w, (u_int) h);
    }
    u_short orient;
    if (!TIFFGetField(tif, TIFFTAG_ORIENTATION, &orient))
	orient = ORIENTATION_TOPLEFT;
    u_long y;
    switch (orient) {
    case ORIENTATION_BOTRIGHT:
    case ORIENTATION_RIGHTBOT:	/* XXX */
    case ORIENTATION_LEFTBOT:	/* XXX */
//	    warnAboutOrientation();
	orient = ORIENTATION_BOTLEFT;
	/* fall thru... */
    case ORIENTATION_BOTLEFT:
	y = 0;
	break;
    case ORIENTATION_TOPRIGHT:
    case ORIENTATION_RIGHTTOP:	/* XXX */
    case ORIENTATION_LEFTTOP:	/* XXX */
    default:
//	    warnAboutOrientation();
	orient = ORIENTATION_TOPLEFT;
	/* fall thru... */
    case ORIENTATION_TOPLEFT:
	y = h-1;
	break;
    }
    int scanline = TIFFScanlineSize(tif);
    for (int row = 0; h-- > 0; row++) {
	(void) TIFFReadScanline(tif, (u_char*) page->addr(0,y), row, 0);
	y += (orient == ORIENTATION_TOPLEFT ? -1 : 1);
    }
    pageView->setImage(*page, yres, FALSE);
    if (!pages[pn].read) {
	switch (orient) {
	case ORIENTATION_BOTRIGHT:
	case ORIENTATION_RIGHTBOT:
	    pages[pn].orient = ORIENT_FLIP|ORIENT_REVERSE;
	    break;
	case ORIENTATION_LEFTBOT:
	case ORIENTATION_BOTLEFT:
	    pages[pn].orient = ORIENT_FLIP;
	    break;
	case ORIENTATION_TOPRIGHT:
	case ORIENTATION_RIGHTTOP:
	    pages[pn].orient = ORIENT_REVERSE;
	    break;
	}
	pages[pn].read = TRUE;
    }
    if (pages[pn].orient & ORIENT_FLIP)
	pageView->flip(FALSE);
    if (pages[pn].orient & ORIENT_REVERSE)
	pageView->reverse(FALSE);
    pageView->update();
    endSlowOperation();
    updateOrient(pages[pn].orient);
}

void
FAXViewer::open()
{
    if (opened)
	return;
    fxViewStack* top = new fxViewStack(vertical, fixedSize, fixedSize, 4, 4);
    top->setColor(fx_lightGray);
    top->add(pageView = new ImageView(small));
    top->setPopup(setupMenu());

    setPreTranslationTable(table);

    win = new fxWindow();
    win->add(top);
    win->setName("faxview");
    add(win);
    win->open();
    fxVisualApplication::open();

    // flush pending redraw events so window is cleared quickly
    while (fx_theEventHandler->getNumberOfPendingEvents() > 0)
	fx_theEventHandler->distributeEvents();

    setExpContrast();
    gotoDirectory(0);
}

extern "C" void _exit(int);

void
FAXViewer::close()
{
    fxVisualApplication::close();
    if (removeFile)
	(void) unlink(filename);
    _exit(0);		// XXX avoid event handling bug
}

fxMenu*
FAXViewer::setupMenu()
{
    fxMenuStack* stack = new fxMenuStack(vertical);
    stack->add(nextPage = new fxMenuItem("Next Page"), this, "::nextPage");
    stack->add(prevPage = new fxMenuItem("Prev Page"), this, "::prevPage");
    stack->add(new fxMenuItem("Goto Page", pageMenu));
    stack->scoreMark(3);
    stack->add(new fxMenuItem("Contrast", setupContrastMenu()));
    stack->add(new fxMenuItem("Orientation", setupOrientMenu()));
    stack->add(new fxMenuItem("Print", fxMakeMenu(
	"Current Page%w"	"|"
	"Selected Pages...%w"	"|"
	"All Pages%w",
	this, "printPage",
	this, "printSelected",
	this, "printAll"
    )));
    stack->scoreMark(6);
    stack->add(new fxMenuItem("Quit"), this, "close");
    return (new fxMenuBar->add(new fxMenuItem("FAXView", stack)));
}

fxMenu*
FAXViewer::setupContrastMenu()
{
    fxMenuStack* stack = new fxMenuStack(vertical);
    linearItem	= checkItem(stack, "Linear",		"setLinearContrast");
    expItem	= checkItem(stack, "Exponential",	"setExpContrast");
    exp50Item	= checkItem(stack, "50% Exp/Black",	"setExp50Contrast");
    exp60Item	= checkItem(stack, "60% Exp/Black",	"setExp60Contrast");
    exp70Item	= checkItem(stack, "70% Exp/Black",	"setExp70Contrast");
    return (stack);
}

fxMenu*
FAXViewer::setupOrientMenu()
{
    fxMenuStack* stack = new fxMenuStack(vertical);
    orientItems[ORIENT_FLIP] =
	checkItem(stack , "Flip",	"flipPage");
    orientItems[ORIENT_REVERSE] =
	checkItem(stack , "Reverse",	"reversePage");
    orientItems[ORIENT_FLIP|ORIENT_REVERSE] =
	checkItem(stack , "Flip&Reverse","flip&reversePage");
    return (stack);
}

fxMenuItem*
FAXViewer::checkItem(fxMenuStack* stack, const char* tag, const char* wire)
{
    fxMenuItem* mi = new fxMenuItem(tag, 0, fxMenuItem::checkMark);
    mi->setState(0);
    stack->add(mi, this, wire);
    return (mi);
}

void
FAXViewer::printPage()
{
    u_short pn, total;
    if (!TIFFGetField(tif, TIFFTAG_PAGENUMBER, &pn, &total))
	pn = curDirnum;			// makeup page number
    char buf[1024];
    sprintf(buf, "fax2ps -p %d %s | lp -s", pn, (char*) filename);
    printCmd = buf;
#ifdef notdef
    dm->registerConfirmer("print", printCmd);
    dm->startDialog("print", TRUE);
#else
    print();
#endif
}

void
FAXViewer::printAll()
{
    printCmd = "fax2ps " | filename | " | lp -s";
#ifdef notdef
    dm->registerConfirmer("print", printCmd);
    dm->startDialog("print", TRUE);
#else
    print();
#endif
}

void
FAXViewer::print()
{
    beginSlowOperation();
    if (system(printCmd))
	printCmdProblem();
    endSlowOperation();
}

void
FAXViewer::setOrientation(int o)
{
    beginSlowOperation();
    if (o & ORIENT_FLIP)
	pageView->flip(FALSE);
    if (o & ORIENT_REVERSE)
	pageView->reverse(FALSE);
    endSlowOperation();
    pageView->update();
    updateOrient(pages[curPageNum].orient ^= (o & ORIENT_OPS));
}

void
FAXViewer::updateOrient(int o)
{
    switch (o) {
    case ORIENT_NONE:
	orientItems[ORIENT_FLIP]->setState(0);
	orientItems[ORIENT_REVERSE]->setState(0);
	orientItems[ORIENT_FLIP+ORIENT_REVERSE]->setState(0);
	break;
    case ORIENT_FLIP:
	orientItems[ORIENT_FLIP]->setState(1);
	orientItems[ORIENT_REVERSE]->setState(0);
	orientItems[ORIENT_FLIP+ORIENT_REVERSE]->setState(0);
	break;
    case ORIENT_REVERSE:
	orientItems[ORIENT_FLIP]->setState(0);
	orientItems[ORIENT_REVERSE]->setState(1);
	orientItems[ORIENT_FLIP+ORIENT_REVERSE]->setState(0);
	break;
    case ORIENT_FLIP+ORIENT_REVERSE:
	orientItems[ORIENT_FLIP]->setState(0);
	orientItems[ORIENT_REVERSE]->setState(0);
	orientItems[ORIENT_FLIP+ORIENT_REVERSE]->setState(1);
	break;
    }
}

void FAXViewer::setLinearContrast()
    { setContrast(linearItem, ImageView::LINEAR); }
void FAXViewer::setExpContrast()
    { setContrast(expItem, ImageView::EXP); }
void FAXViewer::setExp50Contrast()
    { setContrast(exp50Item, ImageView::EXP50); }
void FAXViewer::setExp60Contrast()
    { setContrast(exp60Item, ImageView::EXP60); }
void FAXViewer::setExp70Contrast()
    { setContrast(exp70Item, ImageView::EXP70); }

void
FAXViewer::setContrast(fxMenuItem* mi, Contrast c)
{
    if (curContrast)
	curContrast->setState(0);
    (curContrast = mi)->setState(1);
    pageView->setContrast(c);
}

void
FAXViewer::doWarningDialog(char* msg)
{
    fxDialogBox* box;
    box = fxMakeAlertDialog(this, fxMakeWarningDialogIcon(), msg, TRUE);
    box->setStartWithMouseInView(
	box->addGoAwayButton("OK", fxMakeAcceptButton("OK")));
    box->setTitle("Warning");
    box->start(TRUE);
    remove(box);
}

#define	WarningBOX(name, message) \
    void FAXViewer::name() { doWarningDialog(message); }
WarningBOX(cantOpenFile,	"Can not open file.")
WarningBOX(cantAccessDirectory, "Error accessing directory.");
WarningBOX(warnAboutOrientation,"Warning, using bottom-left orientation.");
WarningBOX(printCmdProblem,	"Error printing page(s).");
#undef WarningBOX

static u_short hourglass_bits[] = {
    0xffff, 0xffff, 0x4ff2, 0x23c4, 0x1188, 0x0890, 0x0520, 0x0240,
    0x03c0, 0x07e0, 0x0fb0, 0x1e78, 0x381c, 0x4002, 0x8001, 0xffff,
};
static fxCursor HourGlassCursor(16, 1, 8, 8, hourglass_bits);

void
FAXViewer::beginSlowOperation()
{
    prevCursor = &win->getCursor();
    win->setCursor(HourGlassCursor);
}

void
FAXViewer::endSlowOperation()
{
    win->setCursor(*prevCursor);
}

void TIFFWarning(const char*, const char*, ...) {}
