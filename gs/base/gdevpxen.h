/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Enumerated attribute value definitions for PCL XL */

#ifndef gdevpxen_INCLUDED
#  define gdevpxen_INCLUDED

typedef enum {
    eClockWise = 0,
    eCounterClockWise,
    pxeArcDirection_next
} pxeArcDirection_t;

typedef enum {
    eNoSubstitution = 0,
    eVerticalSubstitution,
    pxeCharSubModeArray_next
} pxeCharSubModeArray_t;

typedef enum {
    eNonZeroWinding = 0,
    eEvenOdd,
    pxeClipMode_next,
    pxeFillMode_next = pxeClipMode_next		/* see pxeFillMode_t below */
} pxeClipMode_t;

typedef enum {
    eInterior = 0,
    eExterior,
    pxeClipRegion_next
} pxeClipRegion_t;

typedef enum {
    e1Bit = 0,
    e4Bit,
    e8Bit,
    pxeColorDepth_next
} pxeColorDepth_t;

typedef enum {
    eCRGB = 5,			/* Note: for this enumeration, 0 is not a valid value */
    pxeColorimetricColorSpace_next
} pxeColorimetricColorSpace_t;	/* 2.0 */

typedef enum {
    eDirectPixel = 0,
    eIndexedPixel,
    pxeColorMapping_next
} pxeColorMapping_t;

typedef enum {
    eNoColorSpace = 0,		/* Note: for this enumeration, 0 is not a valid value */
    eGray,
    eRGB,
    eSRGB = 6,		/* 2.0, Note: HP's value is 6 not the expected 3 */
    pxeColorSpace_next
} pxeColorSpace_t;

typedef enum {
    eNoCompression = 0,
    eRLECompression,
    eJPEGCompression,		/* 2.0 */
    eDeltaRowCompression,       /* 2.1 */
    pxeCompressMode_next
} pxeCompressMode_t;

typedef enum {
    eBinaryHighByteFirst = 0,
    eBinaryLowByteFirst,
    pxeDataOrg_next		/* is this DataOrg or DataOrganization? */
} pxeDataOrg_t;

typedef enum {
    eDefault = 0,		/* bad choice of name! */
    pxeDataSource_next
} pxeDataSource_t;

typedef enum {
    eUByte = 0,
    eSByte,
    eUInt16,
    eSInt16,
    pxeDataType_next
} pxeDataType_t;

typedef enum {
    eDownloaded = -1,		/* Not a real value, indicates a downloaded matrix */
    eDeviceBest = 0,
    pxeDitherMatrix_next
} pxeDitherMatrix_t;

typedef enum {
    eDuplexHorizontalBinding = 0,
    eDuplexVerticalBinding,
    pxeDuplexPageMode_next
} pxeDuplexPageMode_t;

typedef enum {
    eFrontMediaSide = 0,
    eBackMediaSide,
    pxeDuplexPageSide_next
} pxeDuplexPageSide_t;

typedef enum {
    /* Several pieces of code know that this is a bit mask. */
    eNoReporting = 0,
    eBackChannel,
    eErrorPage,
    eBackChAndErrPage,
    eNWBackChannel,		/* 2.0 */
    eNWErrorPage,		/* 2.0 */
    eNWBackChAndErrPage,	/* 2.0 */
    pxeErrorReport_next
} pxeErrorReport_t;

typedef pxeClipMode_t pxeFillMode_t;

typedef enum {
    eButtCap = 0,
    eRoundCap,
    eSquareCap,
    eTriangleCap,
    pxeLineCap_next
} pxeLineCap_t;

#define pxeLineCap_to_library\
  { gs_cap_butt, gs_cap_round, gs_cap_square, gs_cap_triangle }

typedef enum {
    eMiterJoin = 0,
    eRoundJoin,
    eBevelJoin,
    eNoJoin,
    pxeLineJoin_next
} pxeLineJoin_t;

#define pxeLineJoin_to_library\
  { gs_join_miter, gs_join_round, gs_join_bevel, gs_join_none }

typedef enum {
    eInch = 0,
    eMillimeter,
    eTenthsOfAMillimeter,
    pxeMeasure_next
} pxeMeasure_t;

#define pxeMeasure_to_points { 72.0, 72.0 / 25.4, 72.0 / 254.0 }

typedef enum {
    eDefaultDestination = 0,
    eFaceDownBin,		/* 2.0 */
    eFaceUpBin,			/* 2.0 */
    eJobOffsetBin,		/* 2.0 */
    pxeMediaDestination_next
} pxeMediaDestination_t;

typedef enum {
    eLetterPaper = 0,
    eLegalPaper,
    eA4Paper,
    eExecPaper,
    eLedgerPaper,
    eA3Paper,
    eCOM10Envelope,
    eMonarchEnvelope,
    eC5Envelope,
    eDLEnvelope,
    eJB4Paper,
    eJB5Paper,
    eB5Envelope,
    eB5Paper,                   /* 2.1 */
    eJPostcard,
    eJDoublePostcard,
    eA5Paper,
    eA6Paper,			/* 2.0 */
    eJB6Paper,			/* 2.0 */
    eJIS8K,                      /* 2.1 */
    eJIS16K,                     /* 2.1 */
    eJISExec,                    /* 2.1 */
    eDefaultPaperSize = 96,     /* 2.1 */
    pxeMediaSize_next
} pxeMediaSize_t;

/*
 * Apply a macro (or procedure) to all known paper sizes.  The
 * arguments are: media size code, pjl paper name, resolution for
 * width/height, width, and height.  Name aliases are used for example
 * "jis b4" is the same as "jisb4".
 */

#define px_enumerate_media(m)\
  m(eDefaultPaperSize, "defaultpaper",  -1,   -1,   -1) \
  m(eLetterPaper,      "letter",       300, 2550, 3300) \
  m(eLegalPaper,       "legal",        300, 2550, 4200) \
  m(eA4Paper,          "a4",           300, 2480, 3507) \
  m(eExecPaper,        "executive",    300, 2175, 3150) \
  m(eLedgerPaper,      "ledger",       300, 3300, 5100) \
  m(eA3Paper,          "a3",           300, 3507, 4960) \
  m(eCOM10Envelope,    "com10",        300, 1237, 2850) \
  m(eMonarchEnvelope,  "monarch",      300, 1162, 2250) \
  m(eC5Envelope,       "c5",           300, 1913, 2704) \
  m(eDLEnvelope,       "dl",           300, 1299, 2598) \
  m(eJB4Paper,         "jisb4",        300, 3035, 4299) \
  m(eJB4Paper,         "jis b4",       300, 3035, 4299) \
  m(eJB5Paper,         "jisb5",        300, 2150, 3035) \
  m(eJB5Paper,         "jis b5",       300, 2150, 3035) \
  m(eB5Envelope,       "b5",           300, 2078, 2952) \
  m(eB5Paper,          "b5paper",      300, 2150, 3035) \
  m(eJPostcard,        "jpost",        300, 1181, 1748) \
  m(eJDoublePostcard,  "jpostd",       300, 2362, 1748) \
  m(eA5Paper,          "a5",           300, 1748, 2480) \
  m(eA6Paper,          "a6",           300, 1240, 1748) \
  m(eJB6Paper,         "jisb6",        300, 1512, 2150) \
  m(eJIS8K,            "jis8K",        300, 3154, 4606) \
  m(eJIS16K,           "jis16K",       300, 2303, 3154) \
  m(eJISExec,          "jisexec",      300, 2551, 3898)

typedef enum {
    eDefaultSource = 0,
    eAutoSelect,
    eManualFeed,
    eMultiPurposeTray,
    eUpperCassette,
    eLowerCassette,
    eEnvelopeTray,
    eThirdCassette,
    pxeMediaSource_next
} pxeMediaSource_t;

/**** MediaType is not documented. ****/
typedef enum {
    eDefaultType = 0,
    pxeMediaType_next
} pxeMediaType_t;

typedef enum {
    ePortraitOrientation = 0,
    eLandscapeOrientation,
    eReversePortrait,
    eReverseLandscape,
    eDefaultOrientation, /* 2.1 */
    pxeOrientation_next
} pxeOrientation_t;

typedef enum {
    eTempPattern = 0,
    ePagePattern,
    eSessionPattern,
    pxePatternPersistence_next
} pxePatternPersistence_t;

typedef enum {
    eSimplexFrontSide = 0,
    pxeSimplexPageMode_next
} pxeSimplexPageMode_t;

typedef enum {
    eOpaque = 0,
    eTransparent,
    pxeTxMode_next
} pxeTxMode_t;

typedef enum {
    eHorizontal = 0,
    eVertical,
    pxeWritingMode_next
} pxeWritingMode_t;		/* 2.0 */

/* the following 4 enumerations are new with XL 3.0 */

typedef enum {
    eDisableAH = 0,   /* the documentation uses a eDisable here and in
                         Trapping - add AH to avoid duplicate
                         identifier. */
    eEnableAH,
    pxeAdaptive_Halftoning_next
} pxeAdaptiveHalftone_t;

typedef enum {
    eHighLPI = 0,
    eMediumLPI,
    eLowLPI,
    pxeeHalftoneMethod_next
} pxeHalftoneMethod_t;

typedef enum {
    eDisableCT = 0,
    eMax,
    eNormal,
    eLight,
    pxeColorTrapping_next
} pxeColorTrapping_t;

typedef enum {
    eTonerBlack = 0,
    eProcessBlack,
    pxeNeutralAxis_next
} pxeNeutralAxis_t;

typedef enum {
    eNoTreatment = 0,
    eVivid,
    eScreenMatch,
    eText,
    eOutofGamut,
    eCIELabmatch1,
    eCIELabmatch2,
    pxeColorTreatment_next
} pxeColorTreatment;
    
#endif /* gdevpxen_INCLUDED */
