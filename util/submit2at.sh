#!/bin/sh
# $Revision: 1.5 $
# $Date: 91/05/23 12:50:23 $
#
# FlexFAX Facsimile System
#
# Copyright (c) 1991 by Sam Leffler.
# All rights reserved.
#
# This file is provided for unrestricted use provided that this
# legend is included on all tape media and as a part of the
# software program in whole or part.  Users may copy, modify or
# distribute this file at will.
#
(/usr/bin/at $* 2>&1) | /usr/bin/awk '/^job/ {printf $2; exit}'
