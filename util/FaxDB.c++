#ident $Header: /usr/people/sam/flexkit/fax/util/RCS/FaxDB.c++,v 1.3 91/06/04 15:44:25 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "FaxDB.h"
#include "SLinearPattern.h"

FaxDBRecord::FaxDBRecord()
{
}

FaxDBRecord::FaxDBRecord(FaxDBRecord* other)
{
    parent = other;
}

FaxDBRecord::~FaxDBRecord()
{
}

const char* FaxDBRecord::className() const { return "FaxDBRecord"; }

fxStr FaxDBRecord::nullStr("");

const fxStr&
FaxDBRecord::find(const fxStr& key)
{
    fxStr* s = 0;
    FaxDBRecord* rec = this;
    for (; rec && !(s = rec->dict.find(key)); rec = rec->parent)
	;
    return (s ? *s : nullStr);
}

void FaxDBRecord::set(const fxStr& key, const fxStr& value)
    { dict[key] = value; }

fxIMPLEMENT_StrKeyObjValueDictionary(FaxValueDict, fxStr);

FaxDB::FaxDB(const fxStr& file) : filename(file)
{
    FILE* fd = fopen(file, "r");
    if (fd) {
	lineno = 0;
	parseDatabase(fd, 0);
	fclose(fd);
    }
}

FaxDB::~FaxDB()
{
}

const char* FaxDB::className() const { return "FaxDB"; }

fxStr FaxDB::nameKey("Name");
fxStr FaxDB::numberKey("FAX-Number");
fxStr FaxDB::companyKey("Company");
fxStr FaxDB::locationKey("Location");
fxStr FaxDB::phoneKey("Voice-Number");

FaxDBRecord*
FaxDB::find(const fxStr& s, fxStr* name)
{
    fxStr canon(s);
    canon.lower();
    fxSLinearPattern pat(canon);
    for (FaxInfoDictIter iter(dict); iter.notDone(); iter++) {
	fxStr t(iter.key());
	t.lower();
	if (pat.find(t) != -1) {
	    if (name)
		*name = iter.key();
	    return (iter.value());
	}
    }
    return (0);
}

FaxDBRecord* FaxDB::operator[](const fxStr& name)	{ return dict[name]; }
const fxStr& FaxDB::getFilename()			{ return filename; }
FaxInfoDict& FaxDB::getDict()				{ return dict; }
void FaxDB::add(const fxStr& key, FaxDBRecord* r)	{ dict[key] = r; }

void
FaxDB::parseDatabase(FILE* fd, FaxDBRecord* parent)
{
    FaxDBRecordPtr rec(new FaxDBRecord(parent));
    fxStr key;
    while (getToken(fd, key)) {
	if (key == "]") {
	    if (parent == 0)
		fprintf(stderr, "%s: line %d: Unmatched \"]\".\n",
		    (char*) filename, lineno);
	    break;
	}
	if (key == "[") {
	    parseDatabase(fd, rec);    		// recurse to form hierarchy
	    continue;
	}
	fxStr value;
	if (!getToken(fd, value))
	    break;
	if (value != ":") {
	    fprintf(stderr, "%s: line %d: Missing \":\" separator.\n",
		(char*) filename, lineno);
	    continue;
	}
	if (!getToken(fd, value))
	    break;
	rec->set(key, value);
	if (key == nameKey)			// XXX what about duplicates?
	    add(value, rec);
    }
}

#include "StackBuffer.h"
#include <ctype.h>

fxBool
FaxDB::getToken(FILE* fd, fxStr& token)
{
    int c;
top:
    if ((c = getc(fd)) == EOF)
	return (FALSE);
    while (isspace(c)) {
	if (c == '\n')
	    lineno++;
	c = getc(fd);
    }
    if (c == '#') {
	while ((c = getc(fd)) != EOF && c != '\n')
	    ;
	if (c == EOF)
	    return (FALSE);
	lineno++;
	goto top;
    }
    if (c == '[' || c == ']' || c == ':') {
	char buf[2];
	buf[0] = c;
	buf[1] = '\0';
	token = buf;
	return (TRUE);
    }
    fxStackBuffer buf;
    if (c == '"') {
	while ((c = getc(fd)) != EOF) {
	    if (c == '\\') {
		c = getc(fd);
		if (c == EOF) {
		    fprintf(stderr, "%s: Premature EOF.\n", (char*) filename);
		    return (FALSE);
		}
		// XXX handle standard escapes
		if (c == '\n')
		    lineno++;
	    } else {
		if (c == '"')
		    break;
		if (c == '\n')
		    lineno++;
	    }
	    buf.put(c);
	}
    } else {
	do
	    buf.put(c);
	while ((c = getc(fd)) != EOF && !isspace(c) &&
	  c != ':' && c != ']' && c != '[' && c != '#');
	if (c != EOF)
	    ungetc(c, fd);
    }
    buf.set('\0');
    token = (char*) buf;
    return (TRUE);
}

#ifdef notdef
void
FaxDB::write()
{
    fxStr temp(filename | "#");
    FILE* fp = fopen(temp, "w");
    if (fp) {
	write(fp);
	fclose(fp);
	::rename(temp, filename);
    }
}

void
FaxDB::write(FILE* fp)
{
    for (FaxInfoDictIter iter(dict); iter.notDone(); iter++) {
	fprintf(fp, "[ Name: \"%s\"\n", (char*) iter.key());
	FaxDBRecord* r = iter.value();
	const fxStr& 
	fprintf(fp, "]\n");
    }
}
#endif

fxIMPLEMENT_StrKeyPtrValueDictionary(FaxInfoDict, FaxDBRecord*);
