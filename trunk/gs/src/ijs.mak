#    Copyright (C) 2002 artofcode LLC.  All rights reserved.
# 
# This file is part of GNU Ghostscript.
# 
# GNU Ghostscript is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
# to anyone for the consequences of using it or for whether it serves any
# particular purpose or works at all, unless he says so in writing.  Refer
# to the GNU General Public License for full details.
# 
# Everyone is granted permission to copy, modify and redistribute GNU
# Ghostscript, but only under the conditions described in the GNU General
# Public License.  A copy of this license is supposed to have been given
# to you along with GNU Ghostscript so you can know your rights and
# responsibilities.  It should be in a file named COPYING.  Among other
# things, the copyright notice and this notice must be preserved on all
# copies.

# $Id$
# makefile for ijs client library code.
# Users of this makefile must define the following:
#	IJSSRCDIR - the icclib source directory
#	IJSEXECTYPE - which process control code to use
#		in spawning the server. currently
#		'unix' and 'win' are supported.
#	BINDIR - where to put the executible examples

# This partial makefile compiles the IJS client library for use in
# Ghostscript.

IJSSRC=$(IJSSRCDIR)$(D)
IJSGEN=$(IJSGENDIR)$(D)
IJSOBJ=$(IJSOBJDIR)$(D)
IJSO_=$(O_)$(IJSOBJ)

# We need I_, _I_, and _I because the OpenVMS compiler uses different
# syntax from other compilers.
# IJSI_ and IJSF_ are defined in gs.mak (why?)
# as are IJSGENDIR and IJSOBJDIR above.
IJS_INCL=$(I_)$(IJSI_)$(_I)
IJS_CCFLAGS=$(IJS_INCL) $(IJSF_) 
IJS_CC=$(CC_) $(IJS_CCFLAGS)

# Define the name of this makefile.
IJS_MAK=$(GLSRC)ijs.mak

ijs.clean : ijs.config-clean ijs.clean-not-config-clean

### WRONG.  MUST DELETE OBJ AND GEN FILES SELECTIVELY.
ijs.clean-not-config-clean :
#	echo $(IJSSRC) $(IJSGEN) $(IJSOBJ) $(IJSO_)
	$(EXP)$(ECHOGS_XE) $(IJSSRC) $(IJSGEN) $(IJSOBJ) $(IJSO_)
	$(RM_) $(IJSOBJ)*.$(OBJ)

ijs.config-clean :
	$(RMN_) $(IJSGEN)ijs*.dev

IJSDEP=$(AK)

ijslib_=$(IJSOBJ)ijs.$(OBJ) $(IJSOBJ)ijs_server.$(OBJ) \
    $(IJSOBJ)ijs_client.$(OBJ) $(IJSOBJ)ijs_exec_$(IJSEXECTYPE).$(OBJ)
$(IJSGEN)ijslib.dev : $(IJS_MAK) $(ECHOGS_XE) $(ijslib_)
	$(SETMOD) $(IJSGEN)ijslib $(ijslib_)

ijs_h=$(IJSSRC)ijs.h

ijs_client_h=$(IJSSRC)$(D)ijs_client.h
ijs_server_h=$(IJSSRC)$(D)ijs_server.h

$(IJSOBJ)ijs.$(OBJ) : $(IJSSRC)ijs.c $(IJSDEP) $(ijs_h)
#	echo $(IJS_CCFLAGS)
	$(EXP)$(ECHOGS_XE) $(IJS_CCFLAGS)
	$(IJS_CC) $(IJSO_)ijs.$(OBJ) $(C_) $(IJSSRC)ijs.c

$(IJSOBJ)ijs_client.$(OBJ) : $(IJSSRC)ijs_client.c \
    $(IJSDEP) $(ijs_h) $(ijs_client_h)
	$(IJS_CC) $(IJSO_)ijs_client.$(OBJ) $(C_) $(IJSSRC)ijs_client.c

$(IJSOBJ)ijs_server.$(OBJ) : $(IJSSRC)ijs_server.c \
    $(IJSDEP) $(ijs_h) $(ijs_server_h)
	$(IJS_CC) $(IJSO_)ijs_server.$(OBJ) $(C_) $(IJSSRC)ijs_server.c

$(IJSOBJ)ijs_exec_unix.$(OBJ) : $(IJSSRC)ijs_exec_unix.c \
    $(IJSDEP) $(ijs_h) $(ijs_client_h)
	$(IJS_CC) $(IJSO_)ijs_exec_unix.$(OBJ) $(C_) $(IJSSRC)ijs_exec_unix.c

$(IJSOBJ)ijs_exec_win.$(OBJ) : $(IJSSRC)ijs_exec_win.c \
    $(IJSDEP) $(ijs_h) $(ijs_client_h)
# This can't be compiled with /Za because it needs windows.h.
	$(CC_WX) $(IJS_CCFLAGS) $(IJSO_)ijs_exec_win.$(OBJ) $(C_) $(IJSSRC)ijs_exec_win.c


#
# rules for the example client/server implementation
# FIXME: linking not portable (or per policy!)

ijs_server_example_=$(BINDIR)$(D)ijs_server_example

ijs_client_example_=$(BINDIR)$(D)ijs_client_example


ijs_examples_=$(ijs_server_example_) $(ijs_client_example_)
$(IJSGEN)ijs_examples.dev : $(IJS_MAK) $(ECHOGS_XE) \
    $(ijs_examples_) $(ijslib_)
	$(SETMOD) $(IJSGEN)ijs_examples $(ijs_examples_)
	$(ADDMOD) $(IJSGEN)ijs_examples $(ijslib_)

$(IJSOBJ)ijs_client_example.$(OBJ) : $(IJSSRC)ijs_client_example.c \
    $(IJSDEP) $(ijs_h) $(ijs_client_h)
	$(IJS_CC) $(IJSO_)ijs_client_example.$(OBJ) $(C_) $(IJSSRC)ijs_client_example.c

$(BINDIR)$(D)ijs_client_example : $(IJSOBJ)ijs_client_example.$(OBJ) $(ijslib_)
	$(IJS_CC) -o bin/ijs_client_example $(IJSOBJ)ijs_client_example.$(OBJ) $(ijslib_)

$(IJSOBJ)ijs_server_example.$(OBJ) : $(IJSSRC)ijs_server_example.c \
    $(IJSDEP) $(ijs_h) $(ijs_server_h)
	$(IJS_CC) $(IJSO_)ijs_server_example.$(OBJ) $(C_) $(IJSSRC)ijs_server_example.c

$(BINDIR)$(D)ijs_server_example : $(IJSOBJ)ijs_server_example.$(OBJ) $(ijslib_)
	$(IJS_CC) -o bin/ijs_server_example $(IJSOBJ)ijs_server_example.$(OBJ) $(ijslib_)
