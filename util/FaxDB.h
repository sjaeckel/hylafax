#ident $Header: /usr/people/sam/flexkit/fax/util/RCS/FaxDB.h,v 1.4 91/06/04 15:43:38 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FaxDB_
#define	_FaxDB_

#include "Dictionary.h"
#include "Ptr.h"

fxDECLARE_StrKeyDictionary(FaxValueDict, fxStr);

class FaxDBRecord : public fxObj {
public:
    fxDECLARE_Ptr(FaxDBRecord);
protected:
    FaxDBRecordPtr	parent;		// parent in hierarchy
    FaxValueDict	dict;		// key-value map

    static fxStr nullStr;

    friend class FaxDB;
public:
    FaxDBRecord();
    FaxDBRecord(FaxDBRecord* other);
    ~FaxDBRecord();

    const char* className() const;

    const fxStr& find(const fxStr& key);
    void set(const fxStr& key, const fxStr& value);
};

fxDECLARE_StrKeyDictionary(FaxInfoDict, FaxDBRecordPtr);

class FaxDB : public fxObj {
protected:
    fxStr	filename;
    int		lineno;			// for parsing
    FaxInfoDict dict;			// name->record map

    void parseDatabase(FILE*, FaxDBRecord* parent);
    fxBool getToken(FILE*, fxStr& token);
public:
    FaxDB(const fxStr& filename);
    ~FaxDB();

    FaxDB* dup() { referenceCount++; return this; }
    const char* className() const;

    static fxStr nameKey;
    static fxStr numberKey;
    static fxStr locationKey;
    static fxStr phoneKey;
    static fxStr companyKey;

    FaxDBRecord* find(const fxStr& pat, fxStr* name = 0);
    FaxDBRecord* operator[](const fxStr& name);
    const fxStr& getFilename();
    FaxInfoDict& getDict();
    void add(const fxStr& key, FaxDBRecord*);
};
#endif /* _FaxDB_ */
