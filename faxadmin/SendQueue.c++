#ident $Header: /usr/people/sam/flexkit/fax/faxadmin/RCS/SendQueue.c++,v 1.7 91/05/28 22:18:32 sam Exp $

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
#include "SendQ.h"
#include "SendQList.h"
#include "SendQInfo.h"
#include "ConfirmDialog.h"
#include "config.h"

#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <osfcn.h>

static SendQ*
findQEntry(SendQPtrArray& q, const fxStr& file)
{
    for (u_int i = 0; i < q.length(); i++) {
	SendQ* req = q[i];
	if (req->qfile == file)
	    return (req);
    }
    return (0);
}

void
faxAdmin::scanSendQueue()
{
    sendQTimer.stopTimer();
    fxStr dirName(queueDir | "/" | FAX_SENDDIR);
    DIR* dir = opendir(dirName);
    if (dir != NULL) {
	struct stat sb;
	if (fstat(dirfd(dir), &sb) >= 0 && sb.st_mtime > sendQModTime) {
	    SendQPtrArray select;
	    sendQueue->getSelections(select);
	    sendQueue->removeAll(FALSE);
	    struct direct* dp;
	    while (dp = readdir(dir)) {
		if (dp->d_name[0] != 'q')
		    continue;
		fxStr entry(dirName | "/" | dp->d_name);
		struct stat sb;
		if (stat(entry, &sb) < 0 || (sb.st_mode & S_IFMT) != S_IFREG)
		    continue;
		int fd = ::open(entry, O_RDONLY);
		if (fd > 0) {
		    SendQ* req = findQEntry(sendq, dp->d_name);
		    if (!req) {
			req = new SendQ(dp->d_name);
			sendq.append(req);
		    }
		    if (!req->readQFile(fd, this)) {
			notifyUser(this, "Could not read send-q file \"%s\"",
			    (char*) entry);
			sendq.remove(sendq.find(req));
			delete req;
		    } else
			sendQueue->add(req, FALSE);
		    ::close(fd);
		}
	    }
	    sendQueue->sort(FALSE);
	    for (u_int i = 0; i < select.length(); i++)
		sendQueue->selectItem(select[i], FALSE, FALSE);
	    sendQueue->update();
	    sendQModTime = sb.st_mtime;
	}
	closedir(dir);
    } else
	notifyUser(this, "Can not access send queue directory \"%s\"",
	    (char*) dirName);
    handleSendQSelection();
    sendQTimer.startTimer();
}

void
faxAdmin::sendNow()
{
    if (!fifo) {
	notifyUser(this, "No server is currently running.");
	return;
    }
    SendQPtrArray select;
    sendQueue->getSelections(select);
    beginBusy(uiWindow);
    for (u_int i = 0; i < select.length(); i++) {
	SendQ* req = select[i];
	/*
	 * To force the job to be processed immediately, we
	 * tack an extra line onto the qfile that forces the
	 * tts to 0 (overriding any previous value), and then
	 * jab the server forcing it to rescan the job's q
	 * file and process the result (which will look as
	 * though it should be sent now).
	 */
	fxStr jobName(fxStr(FAX_SENDDIR) | "/" | req->qfile);
	FILE* fp = fopen(queueDir | "/" | jobName, "a+");
	if (!fp) {
	    notifyUser(this, "Unable to access qfile for job %d", req->jobid);
	    continue;
	}
	fprintf(fp, "tts:0\n");				// update qfile
	fclose(fp);
	fprintf(fifo, "JT%s 0", (char*) jobName);	// notify server
	flushAndCheck();
    }
    scanSendQueue();
    endBusy(uiWindow);
}

void
faxAdmin::sendMoveToTop()
{
    notifyUser(this, "Sorry, can't rearrange the queue yet!");
}

void
faxAdmin::sendMoveToBottom()
{
    notifyUser(this, "Sorry, can't rearrange the queue yet!");
}

void
faxAdmin::sendDelete()
{
    if (confirmRequest(this, "Delete selected facsimile, confirm?")) {
	SendQPtrArray select;
	sendQueue->getSelections(select);
	beginBusy(uiWindow);
	fxStr sendDir(FAX_SENDDIR);
	for (u_int i = 0; i < select.length(); i++) {
	    SendQ* req = select[i];
	    if (removeJob(req))
		sendq.remove(sendq.find(req));
	}
	scanSendQueue();
	endBusy(uiWindow);
    }
}

#include "flock.h"

fxBool
faxAdmin::removeJob(SendQ *job)
{
    fxStr jobName(fxStr(FAX_SENDDIR) | "/" | job->qfile);
    FILE* fp = fopen(queueDir | "/" | jobName, "r");
    if (fp == NULL) {
	notifyUser(this, "Cannot open Job %d.", job->jobid);
	return (FALSE);
    }
    fxBool isLocked =
        (flock(fileno(fp), LOCK_EX|LOCK_NB) < 0 && errno == EWOULDBLOCK);
    if (!isLocked) {
	// re-read q file in case conversion caused file
	// names to change since we first read stuff in
	(void) job->readQFile(dup(fileno(fp)), this);

	for (u_int i = 0; i < job->files.length(); i++)
	    if (unlink(queueDir | "/" | job->files[i]) < 0)
		notifyUser(this, "Unlink \"%s\" failed.",
		    (char*) job->files[i]);
	char cmd[1024];
	if (job->sendjob.length()) {
	    sprintf(cmd, "%s %s > /dev/null 2>&1\n",
		FAX_ATRM, (char*) job->sendjob);
	    (void) system(cmd);
	}
	if (job->killjob.length()) {
	    sprintf(cmd, "%s %s > /dev/null 2>&1\n",
		FAX_ATRM, (char*) job->killjob);
	    (void) system(cmd);
	}
	if (unlink(queueDir | "/" | jobName) < 0)
	    notifyUser(this, "Unlink \"%s\" failed.", (char*) jobName);
	if (fifo)
	    fprintf(fifo, "R%s", (char*) jobName), fflush(fifo);
    } else
	notifyUser(this, "Job %d is locked.", job->jobid);
    (void) fclose(fp);
    return (!isLocked);
}

void
faxAdmin::sendShowInfo()
{
#ifdef notdef
    SendQ* req = sendQueue->getSelection();
    assert(req);
    fxDialogWindow* w = new SendQInfo(req);
    w->start(TRUE);
    remove(w);
#endif
}

#include "Menu.h"

fxMenu*
faxAdmin::setupSendQMenu()
{
    sendQmultiChannel	= addOutput("sendQ:multiSelection", fxDT_int);
    sendQsingleChannel	= addOutput("sendQ:singleSelection", fxDT_int);
    sendQnoChannel	= addOutput("sendQ:noSelection", fxDT_int);
    sendQueue->connect("newSelection", this, "::sendQSelection");

    fxMenuStack* stack = new fxMenuStack(vertical);
//	sendQItem(stack, "Show Info",		"::sendShowInfo",	FALSE);
	sendQItem(stack, "Send Now",		"::sendNow",		TRUE);
	sendQItem(stack, "Move to Top",		"::sendMoveToTop",	TRUE);
	sendQItem(stack, "Move to Bottom",	"::sendMoveToBottom",	TRUE);
	sendQItem(stack, "Delete",		"::sendDelete",		TRUE);
    return (new fxMenuBar->add(new fxMenuItem("Send Queue", stack)));
}

void
faxAdmin::handleSendQSelection()
{
    u_int count = sendQueue->getSelectionCount();
    if (okToUpdate && count > 0)
	sendInt(count == 1 ? sendQsingleChannel : sendQmultiChannel, 0);
    else
	sendInt(sendQnoChannel, 0);
}

void
faxAdmin::sendQItem(fxMenuStack* stack, const char* item, const char* wire, fxBool multi)
{
    fxMenuItem* mi = new fxMenuItem(item);
    if (multi) {
	connect("sendQ:multiSelection", mi, "enable");
	connect("sendQ:singleSelection", mi, "enable");
    } else {
	connect("sendQ:multiSelection", mi, "disable");
	connect("sendQ:singleSelection", mi, "enable");
    }
    connect("sendQ:noSelection", mi, "disable");
    stack->add(mi, this, wire);
}
