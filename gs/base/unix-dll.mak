#  Copyright (C) 2001-2007 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# $Id$
# Partial makefile for Unix shared library target

# Useful make commands:
#  make so		make ghostscript as a shared object
#  make sodebug		make debug ghostscript as a shared object
#  make soinstall	install shared object ghostscript
#  make soclean		remove build files
#
# If you want to test the executable without installing:
#  export LD_LIBRARY_PATH=/insert-path-here/sobin
#  export GS_LIB=/insert-path-here/lib

# Location for building shared object
SOOBJRELDIR=../soobj
SOBINRELDIR=../sobin

# ------------------- Ghostscript shared object --------------------------- #

# Shared object names

# simple loader (no support for display device)
GSSOC_XENAME=$(GS)c$(XE)
GSSOC_XE=$(BINDIR)/$(GSSOC_XENAME)
GSSOC=$(BINDIR)/$(SOBINRELDIR)/$(GSSOC_XENAME)

# loader suporting display device using Gtk+
GSSOX_XENAME=$(GS)x$(XE)
GSSOX_XE=$(BINDIR)/$(GSSOX_XENAME)
GSSOX=$(BINDIR)/$(SOBINRELDIR)/$(GSSOX_XENAME)

# shared library
GS_SONAME_BASE=lib$(GS)

# GNU/Linux
GS_SOEXT=so
GS_SONAME=$(GS_SONAME_BASE).$(GS_SOEXT)
GS_SONAME_MAJOR=$(GS_SONAME).$(GS_VERSION_MAJOR)
GS_SONAME_MAJOR_MINOR=$(GS_SONAME).$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR)
#LDFLAGS_SO=-shared -Wl,-soname=$(GS_SONAME_MAJOR)

# NOTE: the value of LD_SET_DT_SONAME for, for example, Solaris ld, must contain the
# trailing space to separation it from the value of the option. For GNU ld and
# similar linkers it must containt the trailing "=" 
LDFLAGS_SO=-shared -Wl,$(LD_SET_DT_SONAME)$(LDFLAGS_SO_PREFIX)$(GS_SONAME_MAJOR)


# MacOS X
#GS_SOEXT=dylib
#GS_SONAME=$(GS_SONAME_BASE).$(GS_SOEXT)
#GS_SONAME_MAJOR=$(GS_SONAME_BASE).$(GS_VERSION_MAJOR).$(GS_SOEXT)
#GS_SONAME_MAJOR_MINOR=$(GS_SONAME_BASE).$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR).$(GS_SOEXT)
#LDFLAGS_SO=-dynamiclib -flat_namespace
#LDFLAGS_SO=-dynamiclib -install_name $(GS_SONAME_MAJOR_MINOR)
#LDFLAGS_SO=-dynamiclib -install_name $(FRAMEWORK_NAME)

GS_SO=$(BINDIR)/$(GS_SONAME)
GS_SO_MAJOR=$(BINDIR)/$(GS_SONAME_MAJOR) 
GS_SO_MAJOR_MINOR=$(BINDIR)/$(GS_SONAME_MAJOR_MINOR)

# Shared object is built by redefining GS_XE in a recursive make.

# Create symbolic links to the Ghostscript interpreter library

$(GS_SO): $(GS_SO_MAJOR)
	$(RM_) $(GS_SO)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO)

$(GS_SO_MAJOR): $(GS_SO_MAJOR_MINOR)
	$(RM_) $(GS_SO_MAJOR)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO_MAJOR)

# Build the small Ghostscript loaders, with Gtk+ and without
$(GSSOC_XE): $(GS_SO) $(PSSRC)$(SOC_LOADER)
	$(GLCC) -g -o $(GSSOC_XE) $(PSSRC)dxmainc.c \
	-L$(BINDIR) -l$(GS)

$(GSSOX_XE): $(GS_SO) $(PSSRC)$(SOC_LOADER)
	$(GLCC) -g $(SOC_CFLAGS) -o $(GSSOX_XE) $(PSSRC)$(SOC_LOADER) \
	-L$(BINDIR) -l$(GS) $(SOC_LIBS)

# ------------------------- Recursive make targets ------------------------- #

SODEFS=\
 GS_XE=$(BINDIR)/$(SOBINRELDIR)/$(GS_SONAME_MAJOR_MINOR)\
 STDIO_IMPLEMENTATION=c\
 DISPLAY_DEV=$(DD)$(SOOBJRELDIR)/display.dev\
 BINDIR=$(BINDIR)/$(SOBINRELDIR)\
 GLGENDIR=$(GLGENDIR)/$(SOOBJRELDIR)\
 GLOBJDIR=$(GLOBJDIR)/$(SOOBJRELDIR)\
 PSGENDIR=$(PSGENDIR)/$(SOOBJRELDIR)\
 PSOBJDIR=$(PSOBJDIR)/$(SOOBJRELDIR)


# Normal shared object
so: SODIRS
	@if test -z "$(MAKE)" -o -z "`$(MAKE) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) $(SODEFS) LDFLAGS='$(LDFLAGS)' CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(XCFLAGS)' prefix=$(prefix) \
                $(GLOBJDIR)/$(SOOBJRELDIR)/echogs$(XEAUX) $(GLOBJDIR)/$(SOOBJRELDIR)/genarch$(XEAUX)
	$(MAKE) $(SODEFS) LDFLAGS='$(LDFLAGS) $(LDFLAGS_SO)' CFLAGS='$(CFLAGS_STANDARD) $(CFLAGS_SO) $(GCFLAGS) $(XCFLAGS)' \
                prefix=$(prefix)
	$(MAKE) $(SODEFS) LDFLAGS='$(LDFLAGS)' CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(XCFLAGS)' prefix=$(prefix) $(GSSOC) $(GSSOX)

# Debug shared object
# Note that this is in the same directory as the normal shared
# object, so you will need to use 'make soclean', 'make sodebug'
sodebug: SODIRS
	@if test -z "$(MAKE)" -o -z "`$(MAKE) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) $(SODEFS) GENOPT='-DDEBUG' LDFLAGS='$(LDFLAGS)' CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(XCFLAGS)' prefix=$(prefix) \
                $(GLOBJDIR)/$(SOOBJRELDIR)/echogs$(XEAUX) $(GLOBJDIR)/$(SOOBJRELDIR)/genarch$(XEAUX)
	$(MAKE) $(SODEFS) GENOPT='-DDEBUG' LDFLAGS='$(LDFLAGS) $(LDFLAGS_SO)' CFLAGS='$(CFLAGS_STANDARD) $(CFLAGS_SO) $(GCFLAGS) $(XCFLAGS)' \
                prefix=$(prefix)
	$(MAKE) $(SODEFS) GENOPT='-DDEBUG' LDFLAGS='$(LDFLAGS)' CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(XCFLAGS)' prefix=$(prefix) $(GSSOC) $(GSSOX)


#	$(MAKE) $(SODEFS) GENOPT='-DDEBUG' CFLAGS='$(CFLAGS_DEBUG) $(CFLAGS_SO) $(GCFLAGS) $(XCFLAGS)' $(GSSOC) $(GSSOX)

install-so: so
	-mkdir -p $(DESTDIR)$(prefix)
	-mkdir -p $(DESTDIR)$(datadir)
	-mkdir -p $(DESTDIR)$(gsdir)
	-mkdir -p $(DESTDIR)$(gsdatadir)
	-mkdir -p $(DESTDIR)$(bindir)
	-mkdir -p $(DESTDIR)$(libdir)
	-mkdir -p $(DESTDIR)$(gsincludedir)
	$(INSTALL_PROGRAM) $(GSSOC) $(DESTDIR)$(bindir)/$(GSSOC_XENAME)
	$(INSTALL_PROGRAM) $(GSSOX) $(DESTDIR)$(bindir)/$(GSSOX_XENAME)
	$(INSTALL_PROGRAM) $(BINDIR)/$(SOBINRELDIR)/$(GS_SONAME_MAJOR_MINOR) $(DESTDIR)$(libdir)/$(GS_SONAME_MAJOR_MINOR)
	$(RM_) $(DESTDIR)$(libdir)/$(GS_SONAME)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(DESTDIR)$(libdir)/$(GS_SONAME)
	$(RM_) $(DESTDIR)$(libdir)/$(GS_SONAME_MAJOR)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(DESTDIR)$(libdir)/$(GS_SONAME_MAJOR)
	$(INSTALL_DATA) $(PSSRC)iapi.h $(DESTDIR)$(gsincludedir)iapi.h
	$(INSTALL_DATA) $(PSSRC)ierrors.h $(DESTDIR)$(gsincludedir)ierrors.h
	$(INSTALL_DATA) $(GLSRC)gdevdsp.h $(DESTDIR)$(gsincludedir)gdevdsp.h

soinstall: install-so install-scripts install-data $(INSTALL_SHARED) $(INSTALL_CONTRIB)

# Make the build directories
SODIRS: STDDIRS
	@if test ! -d $(BINDIR)/$(SOBINRELDIR); then mkdir -p $(BINDIR)/$(SOBINRELDIR); fi
	@if test ! -d $(GLGENDIR)/$(SOOBJRELDIR); then mkdir -p $(GLGENDIR)/$(SOOBJRELDIR); fi
	@if test ! -d $(GLOBJDIR)/$(SOOBJRELDIR); then mkdir -p $(GLOBJDIR)/$(SOOBJRELDIR); fi
	@if test ! -d $(PSGENDIR)/$(SOOBJRELDIR); then mkdir -p $(PSGENDIR)/$(SOOBJRELDIR); fi
	@if test ! -d $(PSOBJDIR)/$(SOOBJRELDIR); then mkdir -p $(PSOBJDIR)/$(SOOBJRELDIR); fi


soclean: SODIRS
	$(MAKE) $(SODEFS) clean
	$(RM_) $(BINDIR)/$(SOBINRELDIR)/$(GS_SONAME)
	$(RM_) $(BINDIR)/$(SOBINRELDIR)/$(GS_SONAME_MAJOR)
	$(RM_) $(GSSOC)
	$(RM_) $(GSSOX)
	@-rmdir $(BINDIR)/$(SOBINRELDIR) $(GLGENDIR)/$(SOOBJRELDIR) $(GLOBJDIR)/$(SOOBJRELDIR) $(PSGENDIR)/$(SOOBJRELDIR) $(PSOBJDIR)/$(SOOBJRELDIR) || true 

# End of unix-dll.mak
