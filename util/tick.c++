#ident $Header: /d/sam/flexkit/fax/util/RCS/tick.c++,v 1.3 91/05/23 12:50:26 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "OrderedGlobal.h"
#include "Application.h"
#include "Timer.h"
class foo : public fxApplication {
    fxTimer t;
public:
    foo();
    virtual const char *className() const;
    void initialize(int argc, char** argv);
    void open();
    void tick();
} t;
static void s0(foo* o) { o->tick(); }

foo::foo()
{
    addInput("tick", fxDT_void, this, (fxStubFunc) s0);
    t.setTimerDuration(1);
    t.connect("tick", this, "tick");
}

const char* foo::className() const { return ("foo"); }

void
foo::initialize(int argc, char** argv)
{
}

void
foo::open()
{
    fxApplication::open();
    t.startTimer();
}

void
foo::tick()
{
    printf("tick\n");
    t.startTimer();
}
