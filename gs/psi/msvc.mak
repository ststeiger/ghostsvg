#  Copyright (C) 2001-2010 Artifex Software, Inc.
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
# $Id: msvc32.mak 12087 2011-02-01 11:57:26Z robin $
# makefile for 32-bit Microsoft Visual C++, Windows NT or Windows 95 platform.
#
# All configurable options are surrounded by !ifndef/!endif to allow 
# preconfiguration from within another makefile.
#
# Optimization /O2 seems OK with MSVC++ 4.1, but not with 5.0.
# Created 1997-01-24 by Russell Lang from MSVC++ 2.0 makefile.
# Enhanced 97-05-15 by JD
# Common code factored out 1997-05-22 by L. Peter Deutsch.
# Made pre-configurable by JD 6/4/98
# Revised to use subdirectories 1998-11-13 by lpd.

# Note: If you are planning to make self-extracting executables,
# see winint.mak to find out about third-party software you will need.

# ------------------------------- Options ------------------------------- #

###### This section is the only part of the file you should need to edit.

# ------ Generic options ------ #

# Define the directory for the final executable, and the
# source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

!if "$(DEBUG)"=="1"
!ifdef WIN64
DEFAULT_OBJ_DIR=.\debugobj64
!else
DEFAULT_OBJ_DIR=.\debugobj
!endif
!else
!if "$(DEBUGSYM)"=="1"
!ifdef WIN64
DEFAULT_OBJ_DIR=.\profobj64
!else
DEFAULT_OBJ_DIR=.\profobj
!endif
!else
!ifdef WIN64
DEFAULT_OBJ_DIR=.\obj64
!else
DEFAULT_OBJ_DIR=.\obj
!endif
!endif
!endif

# Note that 32-bit and 64-bit binaries reside in a common directory
# since the names are unique
!ifndef BINDIR
!if "$(DEBUG)"=="1"
BINDIR=.\debugbin
!else
!if "$(DEBUGSYM)"=="1"
BINDIR=.\profbin
!else
BINDIR=.\bin
!endif
!endif
!endif
!ifndef GLSRCDIR
GLSRCDIR=.\base
!endif
!ifndef GLGENDIR
GLGENDIR=$(DEFAULT_OBJ_DIR)
!endif
!ifndef GLOBJDIR
GLOBJDIR=$(DEFAULT_OBJ_DIR)
!endif
!ifndef PSSRCDIR
PSSRCDIR=.\psi
!endif
!ifndef PSLIBDIR
PSLIBDIR=.\lib
!endif
!ifndef PSRESDIR
PSRESDIR=.\Resource
!endif
!ifndef PSGENDIR
PSGENDIR=$(DEFAULT_OBJ_DIR)
!endif
!ifndef PSOBJDIR
PSOBJDIR=$(DEFAULT_OBJ_DIR)
!endif
!ifndef SBRDIR
SBRDIR=$(DEFAULT_OBJ_DIR)
!endif

# Define the root directory for Ghostscript installation.

!ifndef AROOTDIR
AROOTDIR=c:/gs
!endif
!ifndef GSROOTDIR
GSROOTDIR=$(AROOTDIR)/gs$(GS_DOT_VERSION)
!endif

# Define the directory that will hold documentation at runtime.

!ifndef GS_DOCDIR
GS_DOCDIR=$(GSROOTDIR)/doc
!endif

# Define the default directory/ies for the runtime initialization, resource and
# font files.  Separate multiple directories with ';'.
# Use / to indicate directories, not \.
# MSVC will not allow \'s here because it sees '\;' CPP-style as an
# illegal escape.

!ifndef GS_LIB_DEFAULT
GS_LIB_DEFAULT=$(GSROOTDIR)/Resource/Init;$(GSROOTDIR)/lib;$(GSROOTDIR)/Resource/Font;$(AROOTDIR)/fonts
!endif

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems, but may be convenient sometimes.

!ifndef SEARCH_HERE_FIRST
SEARCH_HERE_FIRST=0
!endif

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

!ifndef GS_INIT
GS_INIT=gs_init.ps
!endif

# Choose generic configuration options.

# Setting DEBUG=1 includes debugging features in the build:
# 1. It defines the C preprocessor symbol DEBUG. The latter includes
#    tracing and self-validation code fragments into compilation.
#    Particularly it enables the -Z and -T switches in Ghostscript.
# 2. It compiles code fragments for C stack overflow checks.
# Code produced with this option is somewhat larger and runs 
# somewhat slower.

!ifndef DEBUG
DEBUG=0
!endif

# Setting TDEBUG=1 disables code optimization in the C compiler and
# includes symbol table information for the debugger.
# Code is substantially larger and slower.

# NOTE: The MSVC++ 5.0 compiler produces incorrect output code with TDEBUG=0.
# Also MSVC 6 must be service pack >= 3 to prevent INTERNAL COMPILER ERROR

# Default to 0 anyway since the execution times are so much better.
!ifndef TDEBUG
TDEBUG=0
!endif

# Setting DEBUGSYM=1 is only useful with TDEBUG=0.
# This option is for advanced developers. It includes symbol table
# information for the debugger in an optimized (release) build.
# NOTE: The debugging information generated for the optimized code may be
# significantly misleading. For general MSVC users we recommend TDEBUG=1.

!ifndef DEBUGSYM
DEBUGSYM=0
!endif


# We can compile for a 32-bit or 64-bit target
# WIN32 and WIN64 are mutually exclusive.  WIN32 is the default.
!if !defined(WIN32) && (!defined(Win64) || !defined(WIN64))
WIN32=0
!endif

# We can build either 32-bit or 64-bit target on a 64-bit platform
# but the location of the binaries differs. Would be nice if the
# detection of the platform could be automatic.
#!ifndef BUILD_SYSTEM
BUILD_SYSTEM=32
#!endif

# Define the name of the executable file.

!ifndef GS
!ifdef WIN64
GS=gswin64
!else
GS=gswin32
!endif
!endif
!ifndef GSCONSOLE
GSCONSOLE=$(GS)c
!endif
!ifndef GSDLL
!ifdef WIN64
GSDLL=gsdll64
!else
GSDLL=gsdll32
!endif
!endif

# To build two small executables and a large DLL use MAKEDLL=1
# To build two large executables use MAKEDLL=0

!ifndef MAKEDLL
MAKEDLL=1
!endif

# Define the directory where the FreeType2 library sources are stored.
# See freetype.mak for more information.

!ifdef UFST_BRIDGE
!if "$(UFST_BRIDGE)"=="1"
FT_BRIDGE=0
!endif
!endif

!ifndef FT_BRIDGE
FT_BRIDGE=1
!endif

!ifndef FTSRCDIR
FTSRCDIR=freetype
!endif
!ifndef FT_CFLAGS
FT_CFLAGS=-I$(FTSRCDIR)\include
!endif

!ifdef BITSTREAM_BRIDGE
FT_BRIDGE=0
!endif

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

!ifndef JSRCDIR
JSRCDIR=jpeg
!endif

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See png.mak for more information.

!ifndef PNGSRCDIR
PNGSRCDIR=libpng
!endif

!ifndef TIFFSRCDIR
TIFFSRCDIR=tiff$(D)
TIFFCONFIG_SUFFIX=.vc
TIFFPLATFORM=win32
!endif

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

!ifndef ZSRCDIR
ZSRCDIR=zlib
!endif

# Define which jbig2 library to use
!ifndef JBIG2_LIB
JBIG2_LIB=jbig2dec
!endif

!if "$(JBIG2_LIB)" == "luratech" || "$(JBIG2_LIB)" == "ldf_jb2"
# Set defaults for using the Luratech JB2 implementation
!ifndef JBIG2SRCDIR
# CSDK source code location
JBIG2SRCDIR=ldf_jb2
!endif
!ifndef JBIG2_CFLAGS
# required compiler flags
JBIG2_CFLAGS=-DUSE_LDF_JB2 -DWIN32
!endif
!else
# Use jbig2dec by default. See jbig2.mak for more information.
!ifndef JBIG2SRCDIR
# location of included jbig2dec library source
JBIG2SRCDIR=jbig2dec
!endif
!endif

# Alternatively, you can build a separate DLL
# and define SHARE_JBIG2=1 in src/winlib.mak

# Define which jpeg2k library to use
!ifndef JPX_LIB
JPX_LIB=jasper
!endif

# Alternatively, you can build a separate DLL
# and define SHARE_JPX=1 in src/winlib.mak

# Define the directory where the lcms source is stored.
# See lcms.mak for more information
!ifndef LCMSSRCDIR
LCMSSRCDIR=lcms
!endif

# Define the directory where the lcms2 source is stored.
# See lcms2.mak for more information
!ifndef LCMS2SRCDIR
LCMS2SRCDIR=lcms2
!endif

# Define the directory where the ijs source is stored,
# and the process forking method to use for the server.
# See ijs.mak for more information.

!ifndef IJSSRCDIR
SHARE_IJS=0
IJS_NAME=
IJSSRCDIR=ijs
IJSEXECTYPE=win
!endif

# Define the directory where the imdi library source is stored.
# See devs.mak for more information

!ifndef IMDISRCDIR
IMDISRCDIR=imdi
!endif

# Define the directory where the CUPS library sources are stored,

!ifndef LCUPSSRCDIR
SHARE_LCUPS=0
LCUPS_NAME=
LCUPSSRCDIR=cups
LCUPSBUILDTYPE=win
CUPS_CC=$(CC) $(CFLAGS) -DWIN32 
!endif

!ifndef LCUPSISRCDIR
SHARE_LCUPSI=0
LCUPSI_NAME=
LCUPSISRCDIR=cups
CUPS_CC=$(CC) $(CFLAGS) -DWIN32 -DHAVE_BOOLEAN
!endif


# Define any other compilation flags.

# support XCFLAGS for parity with the unix makefiles
!ifndef XCFLAGS
XCFLAGS=
!endif

!ifndef CFLAGS
CFLAGS=
!endif

CFLAGS=$(CFLAGS) $(XCFLAGS)

# 1 --> Use 64 bits for gx_color_index.  This is required only for
# non standard devices or DeviceN process color model devices.
USE_LARGE_COLOR_INDEX=1

!if $(USE_LARGE_COLOR_INDEX) == 1
# Definitions to force gx_color_index to 64 bits
LARGEST_UINTEGER_TYPE=unsigned __int64
GX_COLOR_INDEX_TYPE=$(LARGEST_UINTEGER_TYPE)

CFLAGS=$(CFLAGS) /DGX_COLOR_INDEX_TYPE="$(GX_COLOR_INDEX_TYPE)"
!endif

# -W3 generates too much noise.
!ifndef WARNOPT
WARNOPT=-W2
!endif

#
# Do not edit the next group of lines.

#!include $(COMMONDIR)\msvcdefs.mak
#!include $(COMMONDIR)\pcdefs.mak
#!include $(COMMONDIR)\generic.mak
!include $(GLSRCDIR)\version.mak
# The following is a hack to get around the special treatment of \ at
# the end of a line.
NUL=
DD=$(GLGENDIR)\$(NUL)
GLD=$(GLGENDIR)\$(NUL)
PSD=$(PSGENDIR)\$(NUL)

!ifdef SBR
SBRFLAGS=/FR$(SBRDIR)\$(NUL)
!endif

# ------ Platform-specific options ------ #

# Define which major version of MSVC is being used
# (currently, 4, 5, 6, 7, and 8 are supported).
# Define the minor version of MSVC, currently only
# used for Microsoft Visual Studio .NET 2003 (7.1)

#MSVC_VERSION=6
#MSVC_MINOR_VERSION=0

# Make a guess at the version of MSVC in use
# This will not work if service packs change the version numbers.

!if defined(_NMAKE_VER) && !defined(MSVC_VERSION)
!if "$(_NMAKE_VER)" == "162"
MSVC_VERSION=5
!endif
!if "$(_NMAKE_VER)" == "6.00.8168.0"
MSVC_VERSION=6
!endif
!if "$(_NMAKE_VER)" == "7.00.9466"
MSVC_VERSION=7
!endif
!if "$(_NMAKE_VER)" == "7.00.9955"
MSVC_VERSION=7
!endif
!if "$(_NMAKE_VER)" == "7.10.3077"
MSVC_VERSION=7
MSVC_MINOR_VERSION=1
!endif
!if "$(_NMAKE_VER)" == "8.00.40607.16"
MSVC_VERSION=8
!endif
!if "$(_NMAKE_VER)" == "8.00.50727.42"
MSVC_VERSION=8
!endif
!if "$(_NMAKE_VER)" == "8.00.50727.762"
MSVC_VERSION=8
!endif
!if "$(_NMAKE_VER)" == "9.00.21022.08"
MSVC_VERSION=9
!endif
!if "$(_NMAKE_VER)" == "9.00.30729.01"
MSVC_VERSION=9
!endif
!if "$(_NMAKE_VER)" == "10.00.30319.01"
MSVC_VERSION=10
!endif
!endif

!ifndef MSVC_VERSION
MSVC_VERSION=6
!endif
!ifndef MSVC_MINOR_VERSION
MSVC_MINOR_VERSION=0
!endif

# Define the drive, directory, and compiler name for the Microsoft C files.
# COMPDIR contains the compiler and linker (normally \msdev\bin).
# MSINCDIR contains the include files (normally \msdev\include).
# LIBDIR contains the library files (normally \msdev\lib).
# COMP is the full C compiler path name (normally \msdev\bin\cl).
# COMPCPP is the full C++ compiler path name (normally \msdev\bin\cl).
# COMPAUX is the compiler name for DOS utilities (normally \msdev\bin\cl).
# RCOMP is the resource compiler name (normallly \msdev\bin\rc).
# LINK is the full linker path name (normally \msdev\bin\link).
# Note that when MSINCDIR and LIBDIR are used, they always get a '\' appended,
#   so if you want to use the current directory, use an explicit '.'.

!if $(MSVC_VERSION) == 4
! ifndef DEVSTUDIO
DEVSTUDIO=c:\msdev
! endif
COMPBASE=$(DEVSTUDIO)
SHAREDBASE=$(DEVSTUDIO)
!endif

!if $(MSVC_VERSION) == 5
! ifndef DEVSTUDIO
DEVSTUDIO=C:\Program Files\Devstudio
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\SharedIDE
!endif
!endif

!if $(MSVC_VERSION) == 6
! ifndef DEVSTUDIO
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC98
SHAREDBASE=$(DEVSTUDIO)\Common\MSDev98
!endif
!endif

!if $(MSVC_VERSION) == 7
! ifndef DEVSTUDIO
!if $(MSVC_MINOR_VERSION) == 0
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio .NET
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio .NET 2003
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\Vc7
SHAREDBASE=$(DEVSTUDIO)\Vc7
!endif
!endif

!if $(MSVC_VERSION) == 8
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 8
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 8
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\AMD64"
!endif
!endif
!endif

!if $(MSVC_VERSION) == 9
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 9.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 9.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
RCDIR=$(MSSDK)\bin
! else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v6.0A\bin
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\AMD64"
!endif
!endif
!endif

!if $(MSVC_VERSION) == 10
! ifndef DEVSTUDIO
!ifdef WIN64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 10.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 10.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
RCDIR=$(MSSDK)\bin
! else
!ifdef WIN64
RCDIR=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\bin
!else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v7.0A\bin
!endif
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\AMD64"   
!endif
!endif
!endif

# Some environments don't want to specify the path names for the tools at all.
# Typical definitions for such an environment would be:
#   MSINCDIR= LIBDIR= COMP=cl COMPAUX=cl RCOMP=rc LINK=link
# COMPDIR, LINKDIR, and RCDIR are irrelevant, since they are only used to
# define COMP, LINK, and RCOMP respectively, but we allow them to be
# overridden anyway for completeness.
!ifndef COMPDIR
!if "$(COMPBASE)"==""
COMPDIR=
!else
!ifdef WIN64
COMPDIR=$(COMPDIR64)
!else
COMPDIR=$(COMPBASE)\bin
!endif
!endif
!endif

!ifndef LINKDIR
!if "$(COMPBASE)"==""
LINKDIR=
!else
!ifdef WIN64
LINKDIR=$(COMPDIR64)
!else
LINKDIR=$(COMPBASE)\bin
!endif
!endif
!endif

!ifndef RCDIR
!if "$(SHAREDBASE)"==""
RCDIR=
!else
RCDIR=$(SHAREDBASE)\bin
!endif
!endif

!ifndef MSINCDIR
!if "$(COMPBASE)"==""
MSINCDIR=
!else
MSINCDIR=$(COMPBASE)\include
!endif
!endif

!ifndef LIBDIR
!if "$(COMPBASE)"==""
LIBDIR=
!else
!ifdef WIN64
LIBDIR=$(COMPBASE)\lib\amd64
!else
LIBDIR=$(COMPBASE)\lib
!endif
!endif
!endif

!ifndef COMP
!if "$(COMPDIR)"==""
COMP=cl
!else
COMP="$(COMPDIR)\cl"
!endif
!endif
!ifndef COMPCPP
COMPCPP=$(COMP)
!endif
!ifndef COMPAUX
!ifdef WIN64
COMPAUX=$(COMP)
!else
COMPAUX=$(COMP)
!endif
!endif

!ifndef RCOMP
!if "$(RCDIR)"==""
RCOMP=rc
!else
RCOMP="$(RCDIR)\rc"
!endif
!endif

!ifndef LINK
!if "$(LINKDIR)"==""
LINK=link
!else
LINK="$(LINKDIR)\link"
!endif
!endif

# nmake does not have a form of .BEFORE or .FIRST which can be used
# to specify actions before anything else is done.  If LIB and INCLUDE
# are not defined then we want to define them before we link or
# compile.  Here is a kludge which allows us to to do what we want.
# nmake does evaluate preprocessor directives when they are encountered.
# So the desired set statements are put into dummy preprocessor
# directives.
!ifndef INCLUDE
!if "$(MSINCDIR)"!=""
!if [set INCLUDE=$(MSINCDIR)]==0
!endif
!endif
!endif
!ifndef LIB
!if "$(LIBDIR)"!=""
!if [set LIB=$(LIBDIR)]==0
!endif
!endif
!endif

!ifndef LINKLIBPATH
LINKLIBPATH=
!endif

# Define the processor architecture. (i386, ppc, alpha)

!ifndef CPU_FAMILY
CPU_FAMILY=i386
#CPU_FAMILY=ppc
#CPU_FAMILY=alpha  # not supported yet - we need someone to tweak
!endif

# Define the processor (CPU) type. Allowable values depend on the family:
#   i386: 386, 486, 586
#   ppc: 601, 604, 620
#   alpha: not currently used.

!ifndef CPU_TYPE
CPU_TYPE=486
#CPU_TYPE=601
!endif

# Define special features of CPUs

# We'll assume that if you have an x86 machine, you've got a modern
# enough one to have SSE2 instructions. If you don't, then predefine
# DONT_HAVE_SSE2 when calling this makefile
!if "$(CPU_FAMILY)" == "i386"
!ifndef DONT_HAVE_SSE2
!ifndef HAVE_SSE2
!message **************************************************************
!message * Assuming that target has SSE2 instructions available. If   *
!message * this is NOT the case, define DONT_HAVE_SSE2 when building. *
!message **************************************************************
!endif
HAVE_SSE2=1
CFLAGS=$(CFLAGS) /DHAVE_SSE2
!endif
!endif

# Define the .dev module that implements thread and synchronization
# primitives for this platform.  Don't change this unless you really know
# what you're doing.

!ifndef SYNC
SYNC=winsync
!endif

# Luratech jp2 flags depend on the compiler version
#
!if "$(JPX_LIB)" == "luratech" || "$(JPX_LIB)" == "lwf_jp2"
# Set defaults for using the Luratech JP2 implementation
!ifndef JPXSRCDIR
# CSDK source code location
JPXSRCDIR=lwf_jp2
!endif
!ifndef JPX_CFLAGS
# required compiler flags
JPX_CFLAGS=-DUSE_LWF_JP2 -DWIN32 -DNO_ASSEMBLY
!endif
!else
# Use jasper by default. See jasper.mak for more information.
!ifndef JPXSRCDIR
JPXSRCDIR=jasper
!endif
!endif

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

!ifndef FEATURE_DEVS
FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)dpsnext.dev $(PSD)ttfont.dev $(PSD)epsf.dev $(PSD)mshandle.dev $(PSD)msprinter.dev $(PSD)mspoll.dev $(GLD)pipe.dev $(PSD)fapi.dev $(PSD)jbig2.dev $(PSD)jpx.dev $(PSD)winutf8.dev
!endif

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

!ifndef COMPILE_INITS
COMPILE_INITS=1
!endif

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=file
!endif

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.

!ifndef BAND_LIST_COMPRESSOR
BAND_LIST_COMPRESSOR=zlib
!endif

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfxfd.c for more details.

!ifndef FILE_IMPLEMENTATION
FILE_IMPLEMENTATION=stdio
!endif

# Choose the implementation of stdio: '' for file I/O and 'c' for callouts
# See gs.mak and ziodevs.c/ziodevsc.c for more details.

!ifndef STDIO_IMPLEMENTATION
STDIO_IMPLEMENTATION=c
!endif

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and contrib.mak for the list of available devices.

!ifndef DEVICE_DEVS
DEVICE_DEVS=$(DD)display.dev $(DD)mswindll.dev $(DD)mswinpr2.dev
DEVICE_DEVS2=$(DD)epson.dev $(DD)eps9high.dev $(DD)eps9mid.dev $(DD)epsonc.dev $(DD)ibmpro.dev
DEVICE_DEVS3=$(DD)deskjet.dev $(DD)djet500.dev $(DD)laserjet.dev $(DD)ljetplus.dev $(DD)ljet2p.dev
DEVICE_DEVS4=$(DD)cdeskjet.dev $(DD)cdjcolor.dev $(DD)cdjmono.dev $(DD)cdj550.dev
DEVICE_DEVS5=$(DD)uniprint.dev $(DD)djet500c.dev $(DD)declj250.dev $(DD)lj250.dev $(DD)ijs.dev
DEVICE_DEVS6=$(DD)st800.dev $(DD)stcolor.dev $(DD)bj10e.dev $(DD)bj200.dev
DEVICE_DEVS7=$(DD)t4693d2.dev $(DD)t4693d4.dev $(DD)t4693d8.dev $(DD)tek4696.dev
DEVICE_DEVS8=$(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcx16.dev $(DD)pcx256.dev $(DD)pcx24b.dev $(DD)pcxcmyk.dev
DEVICE_DEVS9=$(DD)pbm.dev $(DD)pbmraw.dev $(DD)pgm.dev $(DD)pgmraw.dev $(DD)pgnm.dev $(DD)pgnmraw.dev $(DD)pkmraw.dev
DEVICE_DEVS10=$(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev $(DD)tifflzw.dev $(DD)tiffpack.dev
DEVICE_DEVS11=$(DD)bmpmono.dev $(DD)bmpgray.dev $(DD)bmp16.dev $(DD)bmp256.dev $(DD)bmp16m.dev $(DD)tiff12nc.dev $(DD)tiff24nc.dev $(DD)tiff48nc.dev $(DD)tiffgray.dev $(DD)tiff32nc.dev $(DD)tiff64nc.dev $(DD)tiffsep.dev $(DD)tiffsep1.dev $(DD)tiffscaled.dev
DEVICE_DEVS12=$(DD)psmono.dev $(DD)bit.dev $(DD)bitrgb.dev $(DD)bitcmyk.dev
DEVICE_DEVS13=$(DD)pngmono.dev $(DD)pnggray.dev $(DD)png16.dev $(DD)png256.dev $(DD)png16m.dev $(DD)pngalpha.dev
DEVICE_DEVS14=$(DD)jpeg.dev $(DD)jpeggray.dev $(DD)jpegcmyk.dev
DEVICE_DEVS15=$(DD)pdfwrite.dev $(DD)pswrite.dev $(DD)ps2write.dev $(DD)epswrite.dev $(DD)txtwrite.dev $(DD)pxlmono.dev $(DD)pxlcolor.dev $(DD)svgwrite.dev
DEVICE_DEVS16=$(DD)bbox.dev $(DD)cups.dev
# Overflow for DEVS3,4,5,6,9
DEVICE_DEVS17=$(DD)ljet3.dev $(DD)ljet3d.dev $(DD)ljet4.dev $(DD)ljet4d.dev
DEVICE_DEVS18=$(DD)pj.dev $(DD)pjxl.dev $(DD)pjxl300.dev $(DD)jetp3852.dev $(DD)r4081.dev
DEVICE_DEVS19=$(DD)lbp8.dev $(DD)m8510.dev $(DD)necp6.dev $(DD)bjc600.dev $(DD)bjc800.dev
DEVICE_DEVS20=$(DD)pnm.dev $(DD)pnmraw.dev $(DD)ppm.dev $(DD)ppmraw.dev $(DD)pamcmyk32.dev $(DD)pamcmyk4.dev
DEVICE_DEVS21= $(DD)spotcmyk.dev $(DD)devicen.dev $(DD)bmpsep1.dev $(DD)bmpsep8.dev $(DD)bmp16m.dev $(DD)bmp32b.dev $(DD)psdcmyk.dev $(DD)psdrgb.dev
!endif

# FAPI compilation options :
UFST_CFLAGS=-DMSVC

BITSTREAM_CFLAGS=

# ---------------------------- End of options ---------------------------- #

# Define the name of the makefile -- used in dependencies.

MAKEFILE=$(PSSRCDIR)\msvc32.mak
TOP_MAKEFILES=$(MAKEFILE) $(GLSRCDIR)\msvccmd.mak $(GLSRCDIR)\msvctail.mak $(GLSRCDIR)\winlib.mak $(PSSRCDIR)\winint.mak

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=$(GLGENDIR)\lib.rsp\
 $(GLOBJDIR)\*.exp $(GLOBJDIR)\*.ilk $(GLOBJDIR)\*.pdb $(GLOBJDIR)\*.lib\
 $(BINDIR)\*.exp $(BINDIR)\*.ilk $(BINDIR)\*.pdb $(BINDIR)\*.lib obj.pdb\
 obj.idb $(GLOBJDIR)\gs.pch $(SBRDIR)\*.sbr $(GLOBJDIR)\cups\*.h

!ifdef BSCFILE
BEGINFILES2=$(BEGINFILES2) $(BSCFILE)
!endif

!include $(GLSRCDIR)\msvccmd.mak
# psromfs.mak must precede lib.mak
!include $(PSSRCDIR)\psromfs.mak
!include $(GLSRCDIR)\winlib.mak
!include $(GLSRCDIR)\msvctail.mak
!include $(PSSRCDIR)\winint.mak

# ----------------------------- Main program ------------------------------ #

GSCONSOLE_XE=$(BINDIR)\$(GSCONSOLE).exe
GSDLL_DLL=$(BINDIR)\$(GSDLL).dll
GSDLL_OBJS=$(PSOBJ)gsdll.$(OBJ) $(GLOBJ)gp_msdll.$(OBJ)

!if $(TDEBUG) != 0
$(PSGEN)lib.rsp: $(TOP_MAKEFILES)
	echo /NODEFAULTLIB:LIBC.lib > $(PSGEN)lib.rsp
	echo /NODEFAULTLIB:LIBCMT.lib >> $(PSGEN)lib.rsp
	echo LIBCMTD.lib >> $(PSGEN)lib.rsp
!else
$(PSGEN)lib.rsp: $(TOP_MAKEFILES)
	echo /NODEFAULTLIB:LIBC.lib > $(PSGEN)lib.rsp
	echo /NODEFAULTLIB:LIBCMTD.lib >> $(PSGEN)lib.rsp
	echo LIBCMT.lib >> $(PSGEN)lib.rsp
!endif


!if $(MAKEDLL)
# The graphical small EXE loader
$(GS_XE): $(GSDLL_DLL)  $(DWOBJ) $(GSCONSOLE_XE) $(SETUP_XE) $(UNINSTALL_XE)
	echo /SUBSYSTEM:WINDOWS > $(PSGEN)gswin.rsp
!ifdef WIN64
	echo /DEF:$(PSSRCDIR)\dwmain64.def /OUT:$(GS_XE) >> $(PSGEN)gswin.rsp
!else
	echo /DEF:$(PSSRCDIR)\dwmain32.def /OUT:$(GS_XE) >> $(PSGEN)gswin.rsp
!endif
	$(LINK) $(LCT) @$(PSGEN)gswin.rsp $(DWOBJ) $(LINKLIBPATH) @$(LIBCTR) $(GS_OBJ).res
	del $(PSGEN)gswin.rsp

# The console mode small EXE loader
$(GSCONSOLE_XE): $(OBJC) $(GS_OBJ).res $(PSSRCDIR)\dw64c.def $(PSSRCDIR)\dw32c.def
	echo /SUBSYSTEM:CONSOLE > $(PSGEN)gswin.rsp
!ifdef WIN64
	echo  /DEF:$(PSSRCDIR)\dw64c.def /OUT:$(GSCONSOLE_XE) >> $(PSGEN)gswin.rsp
!else
	echo  /DEF:$(PSSRCDIR)\dw32c.def /OUT:$(GSCONSOLE_XE) >> $(PSGEN)gswin.rsp
!endif
	$(LINK) $(LCT) @$(PSGEN)gswin.rsp $(OBJC) $(LINKLIBPATH) @$(LIBCTR) $(GS_OBJ).res
	del $(PSGEN)gswin.rsp

# The big DLL
$(GSDLL_DLL): $(GS_ALL) $(DEVS_ALL) $(GSDLL_OBJS) $(GSDLL_OBJ).res $(PSGEN)lib.rsp $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ)
	echo /DLL /DEF:$(PSSRCDIR)\$(GSDLL).def /OUT:$(GSDLL_DLL) > $(PSGEN)gswin.rsp
	$(LINK) $(LCT) @$(PSGEN)gswin.rsp $(GSDLL_OBJS) @$(ld_tr) $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) @$(PSGEN)lib.rsp $(LINKLIBPATH) @$(LIBCTR) $(GSDLL_OBJ).res
	del $(PSGEN)gswin.rsp

!else
# The big graphical EXE
$(GS_XE): $(GSCONSOLE_XE) $(GS_ALL) $(DEVS_ALL) $(GSDLL_OBJS) $(DWOBJNO) $(GSDLL_OBJ).res $(PSSRCDIR)\dwmain32.def\
		$(PSSRCDIR)\dwmain64.def $(PSGEN)lib.rsp $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ)
	copy $(ld_tr) $(PSGEN)gswin.tr
	echo $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) >> $(PSGEN)gswin.tr
	echo $(PSOBJ)dwnodll.obj >> $(PSGEN)gswin.tr
	echo $(GLOBJ)dwimg.obj >> $(PSGEN)gswin.tr
	echo $(PSOBJ)dwmain.obj >> $(PSGEN)gswin.tr
	echo $(GLOBJ)dwtext.obj >> $(PSGEN)gswin.tr
	echo $(GLOBJ)dwreg.obj >> $(PSGEN)gswin.tr
!ifdef WIN64
	echo /DEF:$(PSSRCDIR)\dwmain64.def /OUT:$(GS_XE) > $(PSGEN)gswin.rsp
!else
	echo /DEF:$(PSSRCDIR)\dwmain32.def /OUT:$(GS_XE) > $(PSGEN)gswin.rsp
!endif
	$(LINK) $(LCT) @$(PSGEN)gswin.rsp $(GLOBJ)gsdll @$(PSGEN)gswin.tr $(LINKLIBPATH) @$(LIBCTR) @$(PSGEN)lib.rsp $(GSDLL_OBJ).res $(DWTRACE)
	del $(PSGEN)gswin.tr
	del $(PSGEN)gswin.rsp

# The big console mode EXE
$(GSCONSOLE_XE): $(GS_ALL) $(DEVS_ALL) $(GSDLL_OBJS) $(OBJCNO) $(GS_OBJ).res $(PSSRCDIR)\dw64c.def $(PSSRCDIR)\dw32c.def $(PSGEN)lib.rsp $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ)
	copy $(ld_tr) $(PSGEN)gswin.tr
	echo $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) >> $(PSGEN)gswin.tr
	echo $(PSOBJ)dwnodllc.obj >> $(PSGEN)gswin.tr
	echo $(GLOBJ)dwimg.obj >> $(PSGEN)gswin.tr
	echo $(PSOBJ)dwmainc.obj >> $(PSGEN)gswin.tr
	echo $(PSOBJ)dwreg.obj >> $(PSGEN)gswin.tr
	echo /SUBSYSTEM:CONSOLE > $(PSGEN)gswin.rsp
!ifdef WIN64
	echo /DEF:$(PSSRCDIR)\dw64c.def /OUT:$(GSCONSOLE_XE) >> $(PSGEN)gswin.rsp
!else
	echo /DEF:$(PSSRCDIR)\dw32c.def /OUT:$(GSCONSOLE_XE) >> $(PSGEN)gswin.rsp
!endif
	$(LINK) $(LCT) @$(PSGEN)gswin.rsp $(GLOBJ)gsdll @$(PSGEN)gswin.tr $(LINKLIBPATH) @$(LIBCTR) @$(PSGEN)lib.rsp $(GS_OBJ).res $(DWTRACE)
	del $(PSGEN)gswin.rsp
	del $(PSGEN)gswin.tr
!endif

# ---------------------- Setup and uninstall programs ---------------------- #

!if $(MAKEDLL)

$(SETUP_XE): $(PSOBJ)dwsetup.obj $(PSOBJ)dwinst.obj $(PSOBJ)dwsetup.res $(PSSRC)dwsetup.def $(PSSRC)dwsetup_x86.manifest $(PSSRC)dwsetup_x64.manifest $(LIBCTR)
	echo /DEF:$(PSSRC)dwsetup.def /OUT:$(SETUP_XE) > $(PSGEN)dwsetup.rsp
	echo $(PSOBJ)dwsetup.obj $(PSOBJ)dwinst.obj >> $(PSGEN)dwsetup.rsp
	copy $(LIBCTR) $(PSGEN)dwsetup.tr
	echo ole32.lib >> $(PSGEN)dwsetup.tr
	echo uuid.lib >> $(PSGEN)dwsetup.tr
	$(LINK) $(LCT) @$(PSGEN)dwsetup.rsp $(LINKLIBPATH) @$(PSGEN)dwsetup.tr $(PSOBJ)dwsetup.res
	del $(PSGEN)dwsetup.rsp
	del $(PSGEN)dwsetup.tr
!if $(MSVC_VERSION) >= 8
!ifdef WIN64
	mt -nologo -manifest $(PSSRC)dwsetup_x64.manifest -outputresource:$(SETUP_XE);#1
!else
	mt -nologo -manifest $(PSSRC)dwsetup_x86.manifest -outputresource:$(SETUP_XE);#1
!endif
!endif

$(UNINSTALL_XE): $(PSOBJ)dwuninst.obj $(PSOBJ)dwuninst.res $(PSSRC)dwuninst.def $(PSSRC)dwuninst_x86.manifest $(PSSRC)dwuninst_x64.manifest $(LIBCTR)
	echo /DEF:$(PSSRC)dwuninst.def /OUT:$(UNINSTALL_XE) > $(PSGEN)dwuninst.rsp
	echo $(PSOBJ)dwuninst.obj >> $(PSGEN)dwuninst.rsp
	copy $(LIBCTR) $(PSGEN)dwuninst.tr
	echo ole32.lib >> $(PSGEN)dwuninst.tr
	echo uuid.lib >> $(PSGEN)dwuninst.tr
	$(LINK) $(LCT) @$(PSGEN)dwuninst.rsp $(LINKLIBPATH) @$(PSGEN)dwuninst.tr $(PSOBJ)dwuninst.res
	del $(PSGEN)dwuninst.rsp
	del $(PSGEN)dwuninst.tr
!if $(MSVC_VERSION) >= 8
!ifdef WIN64
	mt -nologo -manifest $(PSSRC)dwuninst_x64.manifest -outputresource:$(UNINSTALL_XE);#1
!else
	mt -nologo -manifest $(PSSRC)dwuninst_x86.manifest -outputresource:$(UNINSTALL_XE);#1
!endif
!endif

!endif

$(MAKE_FILELIST_XE): $(PSSRC)mkfilelt.cpp
	$(CCAUX) /Fe$(MAKE_FILELIST_XE) $(PSSRC)mkfilelt.cpp $(CCAUX_TAIL)

# ---------------------- Debug targets ---------------------- #
# Simply set some definitions and call ourselves back         #

!ifdef WIN64
WINDEFS=WIN64= BUILD_SYSTEM="$(BUILD_SYSTEM)"
!else
WINDEFS=BUILD_SYSTEM="$(BUILD_SYSTEM)"
!endif

DEBUGDEFS=DEBUG=1 TDEBUG=1

debug:
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" FT_BRIDGE=$(FT_BRIDGE) $(DEBUGDEFS) $(WINDEFS)

debugclean:
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" FT_BRIDGE=$(FT_BRIDGE) $(DEBUGDEFS) $(WINDEFS) clean

debugbsc:
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" FT_BRIDGE=$(FT_BRIDGE) $(DEBUGDEFS) $(WINDEFS) bsc

# ---------------------- Profile targets ---------------------- #
# Simply set some definitions and call ourselves back         #

PROFILEDEFS=DEBUGSYM=1

profile:
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" FT_BRIDGE=$(FT_BRIDGE) $(PROFILEDEFS) $(WINDEFS)

profileclean:
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" FT_BRIDGE=$(FT_BRIDGE) $(PROFILEDEFS) $(WINDEFS) clean

profilebsc:
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" FT_BRIDGE=$(FT_BRIDGE) $(PROFILEDEFS) $(WINDEFS) bsc



# ---------------------- UFST targets ---------------------- #
# Simply set some definitions and call ourselves back        #

!ifndef UFST_ROOT
UFST_ROOT=C:\ufst
!endif

UFST_ROMFS_ARGS=-b \
   -P $(UFST_ROOT)/fontdata/mtfonts/pcl45/mt3/ -d fontdata/mtfonts/pcl45/mt3/ pcl___xj.fco plug__xi.fco wd____xh.fco \
   -P $(UFST_ROOT)/fontdata/mtfonts/pclps2/mt3/ -d fontdata/mtfonts/pclps2/mt3/ pclp2_xj.fco \
   -c -P $(PSSRCDIR)/../lib/ -d Resource/Init/ FAPIconfig-FCO

UFSTROMFONTDIR=\\\"%%%%%rom%%%%%fontdata/\\\"
UFSTDISCFONTDIR="$(UFST_ROOT)/fontdata"

UFSTBASEDEFS=UFST_BRIDGE=1 FT_BRIDGE=1 UFST_ROOT="$(UFST_ROOT)" UFST_ROMFS_ARGS="$(UFST_ROMFS_ARGS)" UFSTFONTDIR="$(UFSTFONTDIR)" UFSTROMFONTDIR="$(UFSTROMFONTDIR)"

!ifdef WIN64
UFSTDEBUGDEFS=BINDIR=.\ufstdebugbin GLGENDIR=.\ufstdebugobj64 GLOBJDIR=.\ufstdebugobj64 PSLIBDIR=.\lib PSGENDIR=.\ufstdebugobj64 PSOBJDIR=.\ufstdebugobj64 DEBUG=1 TDEBUG=1 SBRDIR=.\ufstdebugobj64
UFSTDEFS=BINDIR=.\ufstbin GLGENDIR=.\ufstobj64 GLOBJDIR=.\ufstobj64 PSLIBDIR=.\lib PSGENDIR=.\ufstobj64 PSOBJDIR=.\ufstobj64 SBRDIR=.\ufstobj64
!else
UFSTDEBUGDEFS=BINDIR=.\ufstdebugbin GLGENDIR=.\ufstdebugobj GLOBJDIR=.\ufstdebugobj PSLIBDIR=.\lib PSGENDIR=.\ufstdebugobj PSOBJDIR=.\ufstdebugobj DEBUG=1 TDEBUG=1 SBRDIR=.\ufstdebugobj
UFSTDEFS=BINDIR=.\ufstbin GLGENDIR=.\ufstobj GLOBJDIR=.\ufstobj PSLIBDIR=.\lib PSGENDIR=.\ufstobj PSOBJDIR=.\ufstobj SBRDIR=.\ufstobj
!endif

ufst-lib:
#	Could make this call a makefile in the ufst code? 
#	cd $(UFST_ROOT)\rts\lib
#	nmake -f makefile.artifex fco_lib.a if_lib.a psi_lib.a tt_lib.a

ufst-debug: ufst-lib
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" $(UFSTBASEDEFS) $(UFSTDEBUGDEFS) UFST_CFLAGS="$(UFST_CFLAGS)" $(WINDEFS)

ufst-debugclean: ufst-lib
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" $(UFSTBASEDEFS) $(UFSTDEBUGDEFS) UFST_CFLAGS="$(UFST_CFLAGS)" $(WINDEFS) clean

ufst-debugbsc: ufst-lib
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" $(UFSTBASEDEFS) $(UFSTDEBUGDEFS) UFST_CFLAGS="$(UFST_CFLAGS)" $(WINDEFS) bsc

ufst: ufst-lib
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" $(UFSTBASEDEFS) $(UFSTDEFS) UFST_CFLAGS="$(UFST_CFLAGS)" $(WINDEFS)

ufst-clean: ufst-lib
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" $(UFSTBASEDEFS) $(UFSTDEFS) UFST_CFLAGS="$(UFST_CFLAGS)" $(WINDEFS) clean

ufst-bsc: ufst-lib
	nmake -f $(MAKEFILE) DEVSTUDIO="$(DEVSTUDIO)" $(UFSTBASEDEFS) $(UFSTDEFS) UFST_CFLAGS="$(UFST_CFLAGS)" $(WINDEFS) bsc

# ---------------------- Browse information step ---------------------- #

bsc:
	bscmake /o $(SBRDIR)\ghostscript.bsc /v $(GLOBJDIR)\*.sbr

# end of makefile
