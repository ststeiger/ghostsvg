#!/bin/sh
#
# update.sh
#
# ghostscript website (and mirror) update script
# takes care of updating the working copy of the website
# from cvs. generally you pipe an alias subscribed to gs-cvs
# to this script.

# for now, just update whenever we're called.

INSTALL_PATH=/home/www/ghostscript.com/html
CVSROOT=:pserver:anonymous@cvs.ghostscript.sourceforge.net:/cvsroot/ghostscript
CVS='cvs -z3 -q'

# gratuitous login
# $CVS -d $CVSROOT login

# update the main site
cd $INSTALL_PATH
$CVS update -Pd

# update the documentation
cd doc/cvs
$CVS update -Pd
