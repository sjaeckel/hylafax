#ident $Header: /d/sam/flexkit/fax/faxd/RCS/Class2Tab.c++,v 1.2 91/05/23 12:25:15 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Class2.h"

#include "t.30.h"
#include "class2.h"

u_int Class2Modem::vrDISTab[2] = { 0, DIS_7MMVRES };
u_int Class2Modem::dfDISTab[2] = { 0, DIS_2DENCODE };

u_int Class2Modem::brDISTab[4] = {
    DISSIGRATE_V27FB<<12,
    DISSIGRATE_V27<<12,
    DISSIGRATE_V29<<12,
    DISSIGRATE_V27V29<<12
};
u_int Class2Modem::brDCSTab[4] = {
    DCSSIGRATE_2400V27,
    DCSSIGRATE_4800V27,
    DCSSIGRATE_7200V29,
    DCSSIGRATE_9600V29
};
u_int Class2Modem::DCSbrTab[4] = { 0, 1, 3, 2 };

u_int Class2Modem::wdDISTab[3] = {
    DISWIDTH_1728<<6,
    DISWIDTH_2048<<6,
    DISWIDTH_2432<<6
};
u_int Class2Modem::DCSwdTab[4] = { 0, 2, 1, 0 };

u_int Class2Modem::lnDISTab[3] = {
    DISLENGTH_A4<<4,
    DISLENGTH_A4B4<<4,
    DISLENGTH_UNLIMITED<<4
};
u_int Class2Modem::DCSlnTab[4] = { 0, 2, 1, 0 };

u_int Class2Modem::stDISTab[8] = {
    DISMINSCAN_0MS<<1,
    DISMINSCAN_5MS<<1,
    DISMINSCAN_10MS2<<1,
    DISMINSCAN_10MS<<1,
    DISMINSCAN_20MS2<<1,
    DISMINSCAN_20MS<<1,
    DISMINSCAN_40MS2<<1,
    DISMINSCAN_40MS<<1,
};
u_int Class2Modem::DCSstTab[8] = { 5, 7, 3, 0, 1, 0, 0, 0 };
