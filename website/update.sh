#!/bin/sh
#
# update.sh
#
# ghostscript website (and mirror) update script
# takes care of updating the working copy of the website
# from cvs. generally you pipe an alias subscribed to gs-cvs
# to this script.

# for now, just update whenever we're called.

# this is the setting for ghostscript.com for reference
# we use the root passed as an argument instead
HTMLROOT=/home/www/ghostscript.com/html

CVSROOT=:pserver:anonymous@cvs.ghostscript.com:/cvs/ghostscript
CVS='cvs -z3 -q'

if [ x$1 != 'x-d' -o x$2 = 'x' ]; then
  echo "Usage: $0 -d <path_to_html>"
  echo "  This script will update the ghostscript website installed"
  echo "  at <path_to_html> from cvs. The path option is required"
  echo "  because this script is usually invoked remotely."
  exit 1
fi

# grab the html root from the cmdline
HTMLROOT=$2

if [ ! -d $HTMLROOT ]; then
  echo "$HTMLROOT is not a directory"
  exit 2
fi

# gratuitous login
# $CVS -d $CVSROOT login

# update the main site
cd $HTMLROOT
$CVS update -Pd

# update the documentation
if [ -d doc/cvs ]; then
  cd doc/cvs
  $CVS update -Pd
fi
