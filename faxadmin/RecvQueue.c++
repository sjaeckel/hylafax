#ident $Header: /usr/people/sam/flexkit/fax/faxadmin/RCS/RecvQueue.c++,v 1.8 91/06/11 15:23:05 sam Exp $

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
#include "StrDialog.h"
#include "RecvQList.h"
#include "config.h"

#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <osfcn.h>
#include <errno.h>

#include "flock.h"

static RecvQ*
findQEntry(RecvQPtrArray& q, const fxStr& file)
{
    for (u_int i = 0; i < q.length(); i++) {
	RecvQ* req = q[i];
	if (req->qfile == file)
	    return (req);
    }
    return (0);
}

void
faxAdmin::scanReceiveQueue()
{
    recvQTimer.stopTimer();
    fxStr dirName(queueDir | "/" | FAX_RECVDIR);
    DIR* dir = opendir(dirName);
    if (dir != NULL) {
	struct stat dsb;
	if (fstat(dirfd(dir), &dsb) >= 0 && dsb.st_mtime > recvQModTime) {
	    RecvQPtrArray select;
	    recvQueue->getSelections(select);
	    recvQueue->removeAll(FALSE);
	    struct direct* dp;
	    while (dp = readdir(dir)) {
		if (strncmp(dp->d_name, "fax", 3) != 0)
		    continue;
		fxStr entry(dirName | "/" | dp->d_name);
		struct stat sb;
		if (stat(entry, &sb) < 0 || (sb.st_mode & S_IFMT) != S_IFREG)
		    continue;
		int fd = ::open(entry, O_RDONLY);
		if (fd > 0) {
		    // if this is a file being received,
		    // reset the mod time so that we'll rescan
		    // the queue again soon
		    fxBool beingReceived =
		       (flock(fd, LOCK_EX|LOCK_NB) < 0 && errno == EWOULDBLOCK);
		    if (beingReceived)
			dsb.st_mtime = recvQModTime;
		    RecvQ* req = findQEntry(recvq, entry);
		    if (!req) {
			req = new RecvQ(entry);
			recvq.append(req);
		    }
		    if (!req->readQFile(fd, this)) {
			if (!beingReceived)
			    notifyUser(this, "Could not read recvq file \"%s\"",
				dp->d_name);
			recvq.remove(recvq.find(req));
			delete req;
		    } else
			recvQueue->add(req, FALSE);
		    ::close(fd);
		}
	    }
	    recvQueue->sort(FALSE);
	    for (u_int i = 0; i < select.length(); i++)
		recvQueue->selectItem(select[i], FALSE, FALSE);
	    recvQueue->update();
	    recvQModTime = dsb.st_mtime;
	}
	closedir(dir);
    } else
	notifyUser(this, "Can not access receive queue directory \"%s\"",
	    (char*) dirName);
    handleRecvQSelection();
    recvQTimer.startTimer();
}


void
faxAdmin::recvView()
{
    RecvQ* req = recvQueue->getSelection();
    char cmd[1024];
    sprintf(cmd, "%s/%s %s&", FAX_BINDIR, FAX_VIEWER, (char*) req->qfile);
    if (system(cmd) != 0)
	notifyUser(this, "Could not launch viewer (%s)", FAX_VIEWER);
}

void
faxAdmin::recvDeliver()
{
    StrDialog* box = MakeQuestionDialog(this, "User:", TRUE);
    box->setValue(deliverValue);
    box->setTitle("Deliver by Mail");
    box->start(TRUE);
    if (!box->wasCancelled()) {
	RecvQPtrArray select;
	recvQueue->getSelections(select);
	deliverValue = box->getValue();
	beginBusy(uiWindow);
	for (u_int i = 0; i < select.length(); i++) {
	    RecvQ* req = select[i];
	    char cmd[2048];
	    sprintf(cmd, "%s/%s %s %s",
		FAX_SPOOLDIR, FAX_DELIVERCMD,
		(char*) req->qfile,
		(char*) deliverValue);
	    if (system(cmd) != 0)
		notifyUser(this, "There was a problem delivering\n%s.",
		   (char*) req->qfile);
	}
	endBusy(uiWindow);
    }
    remove(box);
}

void
faxAdmin::recvForward()
{
    RecvQPtrArray select;
    recvQueue->getSelections(select);
    fxStr cmd(FAX_BINDIR);
    cmd.append('/');
    cmd.append(FAX_COMPFAX);
    for (u_int i = 0; i < select.length(); i++)
	cmd = cmd | " " | select[i]->qfile;
    (void) system(cmd | "&");
}

void
faxAdmin::recvDelete()
{
    if (confirmRequest(this, "Delete selected facsimile, confirm?")) {
	RecvQPtrArray select;
	recvQueue->getSelections(select);
	beginBusy(uiWindow);
	for (u_int i = 0; i < select.length(); i++) {
	    RecvQ* req = select[i];
	    if (unlink(req->qfile) < 0)
		notifyUser(this,
		    "There was a problem deleting\n%s.", (char*) req->qfile);
	    else
		recvq.remove(recvq.find(req));
	}
	scanReceiveQueue();
	endBusy(uiWindow);
    }
}

void
faxAdmin::recvPrint()
{
    StrDialog* box = MakeQuestionDialog(this, "Print Command:", TRUE);
    box->setValue(printValue);
    box->setTitle("Print Received FAX");
    box->start(TRUE);
    if (!box->wasCancelled()) {
	RecvQPtrArray select;
	recvQueue->getSelections(select);
	printValue = box->getValue();
	beginBusy(uiWindow);
	for (u_int i = 0; i < select.length(); i++) {
	    RecvQ* req = select[i];
	    if (system("cat " | req->qfile | " | " | printValue) != 0)
		notifyUser(this,
		    "There was a problem printing\n"
		    "the facsimile with \"%s\".", (char*) printValue);
	}
	endBusy(uiWindow);
    }
    remove(box);
}

#include "Menu.h"

fxMenu*
faxAdmin::setupRecvQMenu()
{
    recvQmultiChannel	= addOutput("recvQ:multiSelection", fxDT_int);
    recvQsingleChannel	= addOutput("recvQ:singleSelection", fxDT_int);
    recvQnoChannel	= addOutput("recvQ:noSelection", fxDT_int);
    recvQueue->connect("newSelection", this, "::recvQSelection");

    fxMenuStack* stack = new fxMenuStack(vertical);
	recvQItem(stack, "View",    		"::recvView",	FALSE);
	recvQItem(stack, "Deliver by Mail",	"::recvDeliver",TRUE);
	recvQItem(stack, "Forward by FAX",	"::recvForward",TRUE);
	recvQItem(stack, "Delete",		"::recvDelete",	TRUE);
	recvQItem(stack, "Print",		"::recvPrint",	TRUE);
    return (new fxMenuBar->add(new fxMenuItem("Receive Queue", stack)));
}

void
faxAdmin::handleRecvQSelection()
{
    u_int count = recvQueue->getSelectionCount();
    if (okToUpdate && count > 0)
	sendInt(count == 1 ? recvQsingleChannel : recvQmultiChannel, 0);
    else
	sendInt(recvQnoChannel, 0);
}

void
faxAdmin::recvQItem(fxMenuStack* stack, const char* item, const char* wire, fxBool multi)
{
    fxMenuItem* mi = new fxMenuItem(item);
    if (multi) {
	connect("recvQ:multiSelection", mi, "enable");
	connect("recvQ:singleSelection", mi, "enable");
    } else {
	connect("recvQ:multiSelection", mi, "disable");
	connect("recvQ:singleSelection", mi, "enable");
    }
    connect("recvQ:noSelection", mi, "disable");
    stack->add(mi, this, wire);
}
