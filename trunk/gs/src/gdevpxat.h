/* Copyright (C) 1997, 1998, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/*$RCSfile$ $Revision$ */
/* Attribute ID definitions for PCL XL */

#ifndef gdevpxat_INCLUDED
#  define gdevpxat_INCLUDED

typedef enum {

    pxaPaletteDepth = 2,
    pxaColorSpace,
    pxaNullBrush,
    pxaNullPen,
    pxaPaletteData,

    pxaPatternSelectID = 8,
    pxaGrayLevel,
    pxaLightness,		/* 2.0 */
    pxaRGBColor,
    pxaPatternOrigin,
    pxaNewDestinationSize,
    pxaPrimaryArray,		/* 2.0 */
    pxaPrimaryDepth,		/* 2.0 */
    pxaSaturation,		/* 2.0 */
    pxaColorimetricColorSpace,	/* 2.0 */
    pxaXYChromaticities,	/* 2.0 */
    pxaWhiteReferencePoint,	/* 2.0 */
    pxaCRGBMinMax,		/* 2.0 */
    pxaGammaGain,		/* 2.0 */

    pxaDeviceMatrix = 33,
    pxaDitherMatrixDataType,
    pxaDitherOrigin,
    pxaMediaDestination,
    pxaMediaSize,
    pxaMediaSource,
    pxaMediaType,
    pxaOrientation,
    pxaPageAngle,
    pxaPageOrigin,
    pxaPageScale,
    pxaROP3,
    pxaTxMode,

    pxaCustomMediaSize = 47,
    pxaCustomMediaSizeUnits,
    pxaPageCopies,
    pxaDitherMatrixSize,
    pxaDitherMatrixDepth,
    pxaSimplexPageMode,
    pxaDuplexPageMode,
    pxaDuplexPageSide,

    pxaArcDirection = 65,
    pxaBoundingBox,
    pxaDashOffset,
    pxaEllipseDimension,
    pxaEndPoint,
    pxaFillMode,
    pxaLineCapStyle,
    pxaLineJoinStyle,
    pxaMiterLength,
    pxaLineDashStyle,
    pxaPenWidth,
    pxaPoint,
    pxaNumberOfPoints,
    pxaSolidLine,
    pxaStartPoint,
    pxaPointType,
    pxaControlPoint1,
    pxaControlPoint2,
    pxaClipRegion,
    pxaClipMode,

    pxaColorDepth = 98,
    pxaBlockHeight,
    pxaColorMapping,
    pxaCompressMode,
    pxaDestinationBox,
    pxaDestinationSize,
    pxaPatternPersistence,
    pxaPatternDefineID,

    pxaSourceHeight = 107,
    pxaSourceWidth,
    pxaStartLine,
    pxaPadBytesMultiple,	/* 2.0 */
    pxaBlockByteLength,		/* 2.0 */

    pxaNumberOfScanLines = 115,

    pxaCommentData = 129,
    pxaDataOrg,

    pxaMeasure = 134,

    pxaSourceType = 136,
    pxaUnitsPerMeasure,

    pxaStreamName = 139,
    pxaStreamDataLength,

    pxaErrorReport = 143,

    pxaCharAngle = 161,
    pxaCharCode,
    pxaCharDataSize,
    pxaCharScale,
    pxaCharShear,
    pxaCharSize,
    pxaFontHeaderLength,
    pxaFontName,
    pxaFontFormat,
    pxaSymbolSet,
    pxaTextData,
    pxaCharSubModeArray,
    pxaWritingMode,		/* 2.0 */

    pxaXSpacingData = 175,
    pxaYSpacingData,
    pxaCharBoldValue,

    px_attribute_next

} px_attribute_t;

#endif /* gdevpxat_INCLUDED */
