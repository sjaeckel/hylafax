#ifndef _promptWindow_
#define _promptWindow_

#include "Window.h"
#include "MultiChoice.h"
#include <stdarg.h>

class FaxDB;
class FaxDBRecord;
class fxFieldEditor;
class fxButton;
class fxView;
class fxViewStack;
class fxLabel;
class fxFont;

class promptWindow : public fxWindow {
public:
    promptWindow();
    ~promptWindow();
    virtual const char *className() const;

    virtual void open();

    void faxSend();
    void faxCancel();

    const fxStr& getFax();
    const fxStr& getDestName();
    const fxStr& getCompany();
    const fxStr& getLocation();
    const fxStr& getPhone();
    const fxStr& getComments();
    fxBool getNotifyDone();
    fxBool getNotifyRequeued();
    float getResolution();
    fxBool getCoverPage();
    FaxDB* getFaxDB();

    void setFaxDB(FaxDB*);
    void setNotifyDone(fxBool);
    void setNotifyRequeue(fxBool);
    void setResolution(float);
    void setCoverPage(fxBool);

    void setFax();
    void setDestName();
    void setCompany();
    void setLocation();
    void setPhone();
    void setComments();
private:
    fxFieldEditor* cmntEdit; 
    fxFieldEditor* nameEdit;
    fxFieldEditor* compEdit;
    fxFieldEditor* locEdit;
    fxFieldEditor* phoneEdit;
    fxFieldEditor* destEdit;  

    fxButton* notifyDone;
    fxButton* notifyQue;
    fxButton* sendButton;
    fxButton* coverButton;
    fxMultiChoice resChoice;

    fxOutputChannel* sendChannel;
    fxOutputChannel* cancelChannel;

    FaxDB* db;
    FaxDBRecord* curRec;

    static fxStr locKey;
    static fxStr compKey;
    static fxStr phoneKey;

    void setupFax();
    fxViewStack* setupPhoneBook();
    fxViewStack* setupControls();
    fxViewStack* setupNotify(fxLabel* l, u_int w);

    u_int maxViewWidth(fxView* va_alist, ...);
    fxViewStack* setupRightAdjustedLabel(fxLayoutConstraint, fxLabel*, u_int);
    fxFieldEditor* setupFieldEditor(fxViewStack*,
		fxLabel*, u_int, fxLayoutConstraint, u_int = 0);

    fxViewStack* makeChoice(fxMultiChoice&,
	fxButton* (*buttonMaker)(const char*, fxFont* = 0),
	fxOrientation, fxLabel*, u_int w, va_list);
    fxViewStack* setupChoice(fxMultiChoice&, fxLabel*, u_int w, ...);

    void passFocus(fxFieldEditor* editor);
    void updateRecord();
    void appendToFile();
};
#endif
