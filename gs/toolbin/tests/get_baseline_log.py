#!/usr/bin/env python
# -*- Mode: python -*-

#    Copyright (C) 2001 Artifex Software Inc.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the
#terms
# of the license contained in the file LICENSE in this distribution.
#
# For more information about licensing, please refer to
# http://www.ghostscript.com/licensing/. For information on
# commercial licensing, go to http://www.artifex.com/licensing/ or
# contact Artifex Software, Inc., 101 Lucas Valley Road #110,
# San Rafael, CA  94903, U.S.A., +1(415)492-9861.

# $Id: get_baselines,v 1.3 2004/05/02 19:23:01 ray Exp $

#
# get_baseline_log <date>
#
# This script gets the baseline log entries since DATE.  DATE should
# be provided in ctime format (eg: 'Thu Jun 12 12:17:28 2003')

import sys
import time
import string

import gsconf

def usage():
    print "usage: get_baselines <date>"
    print "date must be in ctime format."
    sys.exit(1)

if len(sys.argv) == 2:
    try:
        from_date = time.mktime(time.strptime(sys.argv[1]))
    except:
        print "ERROR: Could not parse date."
        sys.exit(1)
else:
    usage()

name=gsconf.baseline_log
try:
    baseline_log = open(name,'r')
except:
    print "ERROR: cannot open baseline log",name
    sys.exit(1)

changes = {}
for line in baseline_log.readlines():
    line = line[:-1]
    if len(line) == 0:
        continue
    tokens = string.split(line)
    time_tokens=tokens[:5]
    time_tokens=string.join(time_tokens)
    try:
        tm = time.mktime(time.strptime(time_tokens))
    except ValueError:
        print "bad",tokens
        changes[string.join(tokens[5:])] = 1
        continue
    
    if from_date <= tm:
        changes[string.join(tokens[5:])] = 1

if len(changes) > 0:
    keylist=changes.keys()
    for k in keylist:
        print k
else:
    print "No baseline updates."

   
