#!/usr/bin/awk -f
################################################################################
# awkiawki - wikiwiki clone written in (n|g|m)awk
# $Id$
################################################################################
# Copyright 2008 Artifex Software, Inc.
# See the file `COPYING' for copyright notice.
################################################################################

# create an awki page from a plain list of links

BEGIN {
    if (head) print head
}

{
     sub(/.*/, "* [&]")
     sub(/\.awki/, "")
     gsub(/_/, " ")
     print
}

END {
    if (foot) print foot
}