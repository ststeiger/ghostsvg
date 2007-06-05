#  Copyright (C) 2001-2006 Artifex Software, Inc.
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
# Makefile for compiling the Wadalab free Kanji font into the executable.
# This does not yet include rules for creating the wmin*.c files.

ccfonts_ps=gs_kanji gs_ccfnt

ccfonts1_=wmin.$(OBJ) wminr21.$(OBJ) wminr22.$(OBJ) wminr23.$(OBJ) \
 wminr28.$(OBJ)
ccfonts1=wmin wminr21 wminr22 wminr23 wminr28

ccfonts2_=wminr24.$(OBJ) wminr25.$(OBJ)
ccfonts2=wminr24 wminr25

ccfonts3_=wminr30.$(OBJ) wminr31.$(OBJ) wminr32.$(OBJ) wminr33.$(OBJ) \
 wminr34.$(OBJ) wminr35.$(OBJ) wminr36.$(OBJ) wminr37.$(OBJ) \
 wminr38.$(OBJ) wminr39.$(OBJ) wminr3A.$(OBJ) wminr3B.$(OBJ) \
 wminr3C.$(OBJ) wminr3D.$(OBJ) wminr3E.$(OBJ) wminr3F.$(OBJ)
ccfonts3=wminr30 wminr31 wminr32 wminr33 wminr34 wminr35 wminr36 wminr37 \
 wminr38 wminr39 wminr3A wminr3B wminr3C wminr3D wminr3E wminr3F

ccfonts4_=wminr40.$(OBJ) wminr41.$(OBJ) wminr42.$(OBJ) wminr43.$(OBJ) \
 wminr44.$(OBJ) wminr45.$(OBJ) wminr46.$(OBJ) wminr47.$(OBJ) \
 wminr48.$(OBJ) wminr49.$(OBJ) wminr4A.$(OBJ) wminr4B.$(OBJ) \
 wminr4C.$(OBJ) wminr4D.$(OBJ) wminr4E.$(OBJ) wminr4F.$(OBJ)
ccfonts4=wminr40 wminr41 wminr42 wminr43 wminr44 wminr45 wminr46 wminr47 \
 wminr48 wminr49 wminr4A wminr4B wminr4C wminr4D wminr4E wminr4F

ccfonts5_=wminr50.$(OBJ) wminr51.$(OBJ) wminr52.$(OBJ) wminr53.$(OBJ) \
 wminr54.$(OBJ) wminr55.$(OBJ) wminr56.$(OBJ) wminr57.$(OBJ) \
 wminr58.$(OBJ) wminr59.$(OBJ) wminr5A.$(OBJ) wminr5B.$(OBJ) \
 wminr5C.$(OBJ) wminr5D.$(OBJ) wminr5E.$(OBJ) wminr5F.$(OBJ)
ccfonts5=wminr50 wminr51 wminr52 wminr53 wminr54 wminr55 wminr56 wminr57 \
 wminr58 wminr59 wminr5A wminr5B wminr5C wminr5D wminr5E wminr5F

ccfonts6_=wminr60.$(OBJ) wminr61.$(OBJ) wminr62.$(OBJ) wminr63.$(OBJ) \
 wminr64.$(OBJ) wminr65.$(OBJ) wminr66.$(OBJ) wminr67.$(OBJ) \
 wminr68.$(OBJ) wminr69.$(OBJ) wminr6A.$(OBJ) wminr6B.$(OBJ) \
 wminr6C.$(OBJ) wminr6D.$(OBJ) wminr6E.$(OBJ) wminr6F.$(OBJ)
ccfonts6=wminr60 wminr61 wminr62 wminr63 wminr64 wminr65 wminr66 wminr67 \
 wminr68 wminr69 wminr6A wminr6B wminr6C wminr6D wminr6E wminr6F

ccfonts7_=wminr70.$(OBJ) wminr71.$(OBJ) wminr72.$(OBJ) wminr73.$(OBJ) \
 wminr74.$(OBJ)
ccfonts7=wminr70 wminr71 wminr72 wminr73 wminr74
