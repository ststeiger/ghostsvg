

/* Copyright (C) 1999-2000, Bernd Heller.  All rights reserved.
  
  This file is part of MacGSView.
  
  This program is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the MacGSView Free Public Licence 
  (the "Licence") for full details.
  
  Every copy of MacGSView must include a copy of the Licence, normally in a 
  plain ASCII text file named LICENCE.  The Licence grants you the right 
  to copy, modify and redistribute MacGSView, but only under certain conditions 
  described in the Licence.  Among other things, the Licence requires that 
  the copyright notice and this notice be preserved on all copies.
*/


#ifndef _H_MacGSViewConstants
#define _H_MacGSViewConstants
#pragma once



/* Print Dialog Extension */

#define	kPrintDialogDITL				5000

#define kPrDlgRenderResText				 1
#define kPrDlgRenderResEditText			 2
#define kPrDlgRenderDpiText				 3
#define kPrDlgRenderColText				 4
#define kPrDlgRenderColPopup			 5
#define	kPrDlgRenderGroupCheckBox		 6

#define kPrDlgFontUseTTCheckBox			 7
#define	kPrDlgFontGroup					 8

#define kPrDlgHalftoneSpotText			 9
#define kPrDlgHalftoneSpotPopup			10
#define	kPrDlgHalftoneFreqText			11
#define	kPrDlgHalftoneFreqEditText		12
#define	kPrDlgHalftoneLpiText			13
#define	kPrDlgHalftoneAngleText			14
#define	kPrDlgHalftoneAngleEditText		15
#define	kPrDlgHalftoneDegreeText		16
#define	kPrDlgHalftoneGroupCheckBox		17


/* Convert NavDialog Extension */
#define kConvertDialogDITL				6000
#define kConvertDialogFormatPopup		1

/* SaveAsWorkaround NavDialog Extension */
#define kSaveAsDialogDITL				7000
#define kSaveAsDialogFormatPopup		1


#include <PP_Types.h>




/*-----------------------------------------------*/



// Command constants

// Menus
const ResIDT	MENU_File				= 129;
const ResIDT	MENU_Edit				= 130;
const ResIDT	MENU_View				= 131;
const ResIDT	MENU_Window				= 132;



/*-----------------------------------------------*/



// File Menu
const MessageT	cmd_AddEPSPreview	= 'AdPv';
const MessageT	cmd_ConvertFiles	= 'Cvrt';

// View Menu
const MessageT	cmd_FirstPage		= 1101;
const MessageT	cmd_PreviousPage	= 1102;
const MessageT	cmd_NextPage		= 1103;
const MessageT	cmd_LastPage		= 1104;
const MessageT	cmd_GoToPage		= 1105;
const MessageT	cmd_FitPage			= 1106;
const MessageT	cmd_FitWidth		= 1107;
const MessageT	cmd_ZoomTo			= 1108;
const MessageT	cmd_RerenderPage	= 1109;
const MessageT	cmd_AbortRendering	= 1110;
const MessageT	cmd_ZoomUp			= 'ZmUp';
const MessageT	cmd_ZoomDown		= 'ZmDn';

const MessageT	cmd_AbortExport		= 2000;



/*-----------------------------------------------*/



// Dialog IDs
const ResIDT	PPob_AboutBoxDialog				=  130;
const ResIDT	PPob_DocumentWindow				=  128;
const ResIDT	PPob_GotoPageDialog				=  150;
const ResIDT	PPob_ZoomToDialog				=  160;




/*-----------------------------------------------*/



// Window Buttons
const PaneIDT	kWindowPageButton	=	'PgBt';
const PaneIDT	kWindowZoomButton	=	'ZmBt';
const MessageT	cmd_PageButton		=	'PgBt';
const MessageT	cmd_ZoomButton		=	'ZmBt';

const SInt32	zoom25Value			=	1;
const SInt32	zoom50Value			=	2;
const SInt32	zoom75Value			=	3;
const SInt32	zoom100Value		=	4;
const SInt32	zoom125Value		=	5;
const SInt32	zoom150Value		=	6;
const SInt32	zoom200Value		=	7;
const SInt32	zoom400Value		=	8;
const SInt32	zoom800Value		=	9;
/* 10 is separator */
const SInt32	zoomFitPage			=	11;
const SInt32	zoomFitWidth		=	12;
const SInt32	zoomToValue			=	13;

// Window Views
const PaneIDT	kWindowGSView		=	'gsvw';
const PaneIDT	kWindowGSToolbar	=	'gstb';

const SInt16	kWindowGSPanelHeight  = 36;
const SInt16	kWindowGSScrollerSize = 15;

// Tool Panel in Windows
const PaneIDT	kToolbarChasingArrows	=	'arrw';
const PaneIDT	kToolbarStatusText		=	'stat';
const PaneIDT	kToolbarProgressBar		=	'pgbr';
const PaneIDT	kToolbarStopButton		=	'stop';




/*-----------------------------------------------*/



// Cursors
const ResIDT	curs_HandOpen		=	128;
const ResIDT	curs_HandClosed		=	129;



/*-----------------------------------------------*/

// About Box
const PaneIDT	kAboutBoxGSVersion	=	'GSTx';


/*-----------------------------------------------*/


// Goto Page Dialog
const PaneIDT	kGotoMaxPages		=	'PgMx';



/*-----------------------------------------------*/



// Export Options Dialog
const ResIDT	PPob_ExportDialog					=	500;
const ResIDT	PPob_ExportPageRangePanel			=	510;
const ResIDT	PPob_ExportPageSizePanel			=	520;
const ResIDT	PPob_ExportRenderingPanel			=	530;
const ResIDT	PPob_ExportHalftonePanel			=	540;

const PaneIDT	kExportPopupGroupBox				=	'PUVw';
const PaneIDT	kExportMultiPanelView				=	'MPVW';

const PaneIDT	kExportPageRangeCurrentPage			=	'CrPg';
const PaneIDT	kExportPageRangeAllPages			=	'AlPg';
const PaneIDT	kExportPageRangePageRange			=	'PgRg';
const PaneIDT	kExportPageRangeFirstPage			=	'FtPg';
const PaneIDT	kExportPageRangeLastPage			=	'LtPg';
const PaneIDT	kExportPageRangeToText				=	'ToTx';

const PaneIDT	kExportRenderingResolution			=	'Rsln';
const PaneIDT	kExportRenderingColorDepth			=	'ClDp';
const PaneIDT	kExportRenderingColorDepthText		=	'ClTx';

const PaneIDT	kExportPageSizeDocumentFormat		=	'PgDc';
const PaneIDT	kExportPageSizeStandardFormat		=	'PgSt';
const PaneIDT	kExportPageSizeSpecialFormat		=	'PgSp';
const PaneIDT	kExportPageSizeFormatPopup			=	'StMn';
const PaneIDT	kExportPageSizeUnitPopup			=	'SpMn';
const PaneIDT	kExportPageSizeWidthEdit			=	'SpWd';
const PaneIDT	kExportPageSizeWidthText			=	'WdTx';
const PaneIDT	kExportPageSizeHeightEdit			=	'SpHt';
const PaneIDT	kExportPageSizeHeightText			=	'HtTx';

const PaneIDT	kExportHalftoneCheckbox				=	'PSHT';
const PaneIDT	kExportHalftoneSpotType				=	'SpTy';
const PaneIDT	kExportHalftoneFrequency			=	'Freq';
const PaneIDT	kExportHalftoneAngle				=	'Angl';

const MessageT	msg_SelectPageRange					=	'SlPR';

const MessageT	msg_FormatRadioButtonClick			=	'FmBt';
const MessageT	msg_ChangeUnit						=	'ChUn';

// JPEG Device Export Panel
const ResIDT	PPob_ExportJPEGQualityPanel			=	600;
const PaneIDT	kExportJPEGQuality					=	'QySc';

// Fax Device Export Panel
const ResIDT	PPob_ExportFaxTypePanel				=	610;
const PaneIDT	kExportFaxTypeGroup31D				=	'Fg3 ';
const PaneIDT	kExportFaxTypeGroup32D				=	'Fg32';
const PaneIDT	kExportFaxTypeGroup4				=	'Fg4 ';


/*-----------------------------------------------*/



// Preferences Dialog
const ResIDT	PPob_PrefsDialog					=	400;
const ResIDT	PPob_PrefsDisplayPanel				=	410;
const ResIDT	PPob_PrefsFontsPanel				=	420;
const ResIDT	PPob_PrefsPageSizePanel				=	430;
const ResIDT	PPob_PrefsOtherPanel				=	440;

const PaneIDT	kPrefsPopupGroupBox					=	'PUVw';
const PaneIDT	kPrefsMultiPanelView				=	'MPVW';

const PaneIDT	kPrefsDisplayColorDepth				=	'ClDp';
const PaneIDT	kPrefsDisplayDefaultZoom			=	'DfZm';
const PaneIDT	kPrefsDisplayAntiAliasing			=	'AnAl';
const PaneIDT	kPrefsDisplayDrawBuffered			=	'DrBf';

const PaneIDT	kPrefsDisplayUseExternalFonts		=	'UsXF';

const PaneIDT	kPrefsPageSizeStandardFormat		=	'PgSt';
const PaneIDT	kPrefsPageSizeFormatPopup			=	'StMn';
const PaneIDT	kPrefsPageSizeSpecialFormat			=	'PgSp';
const PaneIDT	kPrefsPageSizeUnitPopup				=	'SpMn';
const PaneIDT	kPrefsPageSizeWidthEdit				=	'SpWd';
const PaneIDT	kPrefsPageSizeWidthText				=	'WdTx';
const PaneIDT	kPrefsPageSizeHeightEdit			=	'SpHt';
const PaneIDT	kPrefsPageSizeHeightText			=	'HtTx';
const PaneIDT	kPrefsPageSizeOverrideDoc			=	'AlPg';

const PaneIDT	kPrefsOtherReportErrors				=	'ErrD';
const PaneIDT	kPrefsOtherDSCWarning				=	'DSCD';



/*-----------------------------------------------*/



// Conversion Progress Dialog
const ResIDT	PPob_ConversionProgressDialog		=	700;
const PaneIDT	kConvertDialogFileCountText			=	'CvFC';
const PaneIDT	kConvertDialogFileText				=	'CvTx';
const PaneIDT	kConvertDialogFileProgressBar		=	'CvFP';
const PaneIDT	kConvertDialogPageProgressBar		=	'CvPP';




/*-----------------------------------------------*/
/*-----------------------------------------------*/



// constants for custom NavServices popup menu items
const OSType	kCGSSaveFormatAI				=	'AI  ';
const OSType	kCGSSaveFormatASCII				=	'TEXT';
const OSType	kCGSSaveFormatBMP				=	'BMP ';
const OSType	kCGSSaveFormatEPSF				=	'EPSF';
const OSType	kCGSSaveFormatFax				=	'Fax ';
const OSType	kCGSSaveFormatJPEG				=	'JPEG';
const OSType	kCGSSaveFormatPxM				=	'PxM ';
const OSType	kCGSSaveFormatPCX				=	'PCX ';
const OSType	kCGSSaveFormatPDF				=	'PDF ';
const OSType	kCGSSaveFormatPICT				=	'PICT';
const OSType	kCGSSaveFormatPNG				=	'PNG ';
const OSType	kCGSSaveFormatPS				=	'PS  ';
const OSType	kCGSSaveFormatTIFF				=	'TIFF';

const OSType	kCGSShowFormatPS				=	'PS  ';
const OSType	kCGSShowFormatEPSF				=	'EPSF';
const OSType	kCGSShowFormatPDF				=	'PDF ';
const OSType	kCGSShowFormatTEXT				=	'TEXT';



/*-----------------------------------------------*/
/*-----------------------------------------------*/



// string lists

// general string list
const ResIDT	kSTRListGeneral					=	250;
const SInt16	kApplicationNameStr				=	  1;
const SInt16	kDSCWarningStr					=	  2;
const SInt16	kPostScriptErrorStr				=	  3;
const SInt16	kPrefsFileNameStr				=	  4;
const SInt16	kLibNotFoundStr					=	  5;
const SInt16	kNotEnoughMemErrorStr			=	  6;
const SInt16	kUnexpectedErrorStr				=	  7;
const SInt16	kRerenderPageStr				=	  8;
const SInt16	kRerenderDocumentStr			=	  9;
const SInt16	kAbortStr						=	 10;
const SInt16	kContinueStr					=	 11;

// open dialog string list
const ResIDT	kSTRListOpenDialog				=	300;
const SInt16	kOpenDialogPSDocument			=	  1;
const SInt16	kOpenDialogEPSFDocument			=	  2;
const SInt16	kOpenDialogPDFDocument			=	  3;
const SInt16	kOpenDialogTextDocument			=	  4;

// save dialog string list
const ResIDT	kSTRListSaveDialog				=	310;
const SInt16	kSaveDialogTitle				=	  1;
const SInt16	kSaveDialogBMPDocument			=	  2;
const SInt16	kSaveDialogEPSFDocument			=	  3;
const SInt16	kSaveDialogJPEGDocument			=	  4;
const SInt16	kSaveDialogPxMDocument			=	  5;
const SInt16	kSaveDialogPDFDocument			=	  6;
const SInt16	kSaveDialogPICTDocument			=	  7;
const SInt16	kSaveDialogPSDocument			=	  8;
const SInt16	kSaveDialogTIFFDocument			=	  9;
const SInt16	kSaveDialogPCXDocument			=	 10;
const SInt16	kSaveDialogPNGDocument			=	 11;
const SInt16	kSaveDialogASCIIDocument		=	 12;
const SInt16	kSaveDialogAIDocument			=	 13;
const SInt16	kSaveDialogMessageStr			=	 14;
const SInt16	kSaveDialogFaxDocument			=	 15;

// page number string list
const ResIDT	kSTRListPageNumber				=	320;
const SInt16	kPageNumberPageStr				=	  1;
const SInt16	kPageNumberOfStr				=	  2;
const SInt16	kPageNumberNextPageStr			=	  3;

// toolbar status text string list
const ResIDT	kSTRListToolbar					=	330;
const SInt16	kToolbarScanningPSStr			=	  1;
const SInt16	kToolbarScanningPDFStr			=	  2;
const SInt16	kToolbarExportingStr			=	  3;
const SInt16	kToolbarAbortingStr				=	  4;
const SInt16	kToolbarExportingPageStr		=	  5;

// export colors string list
const ResIDT	kSTRListExport					=	340;
const SInt16	kExportOptionsStr				=	  1;
const SInt16	kExportBlackWhiteStr			=	  2;
const SInt16	kExportGrayscaleStr				=	  3;
const SInt16	kExportColorStr					=	  4;
const SInt16	kExport16GraysStr				=	  5;
const SInt16	kExport256GraysStr				=	  6;
const SInt16	kExport16ColorsStr				=	  7;
const SInt16	kExport256ColorsStr				=	  8;
const SInt16	kExportThousandsColorsStr		=	  9;
const SInt16	kExportMillionsColorsStr		=	 10;
const SInt16	kExportCMYKStr					=	 11;

// export panel names string list
const ResIDT	kSTRListExportPanelNames		=	350;
const SInt16	kExportPanelPageRangeStr		=	  1;
const SInt16	kExportPanelPageSizeStr			=	  2;
const SInt16	kExportPanelRenderingStr		=	  3;
const SInt16	kExportPanelHalftonesStr		=	  4;
const SInt16	kExportPanelJPEGQualityStr		=	  5;
const SInt16	kExportPanelFaxTypeStr			=	  6;

// prefs panel names string list
const ResIDT	kSTRListPrefsPanelNames			=	360;
const SInt16	kPrefsPanelDisplayStr			=	  1;
const SInt16	kPrefsPanelFontsStr				=	  2;
const SInt16	kPrefsPanelPageSizeStr			=	  3;
const SInt16	kPrefsPanelOtherStr				=	  4;

// convert dialog string list
const ResIDT	kSTRListConvertDialog			=	370;
const SInt16	kConvertDialogTitle				=	  1;
const SInt16	kConvertDialogMessageStr		=	  2;
const SInt16	kConvertDestDialogTitle			=	  3;
const SInt16	kConvertDestDialogMessageStr	=	  4;
const SInt16	kConvertDialogActionButtonStr	=	  5;



/*-----------------------------------------------*/
/*-----------------------------------------------*/



// Resource constants

const ResIDT	kSystemTooOldAlert				=	129;


#endif _H_MacGSViewConstants


