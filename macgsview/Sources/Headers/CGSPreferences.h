/* ============================================================================	*/
/*	CGSPreferences.h						written by Bernd Heller in 2000		*/
/* ----------------------------------------------------------------------------	*/
/*	This objects handles everything for the preferences (Dialogs,Files)			*/
/* ============================================================================	*/


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


#ifndef _H_CGSPreferences
#define _H_CGSPreferences
#pragma once

#include <LView.h>
#include <LWindow.h>
#include <LCommander.h>
#include <LPreferencesFile.h>


#include "CUtilities.h"

#include <Printing.h>


class CGSPreferences;




typedef enum { kBlackWhite=1, k16Grays=2, k256Grays=3,
			   k256Colors=4, kMillionColors=5, kMaximumColors = 7 } ColorDepth;

typedef enum { kLineSpot=1, kDotSpot=2, kEllipseSpot=3 } SpotType;


typedef struct {
	short			resolution;
	ColorDepth		colorDepth;
	bool			useExternalFonts;
} RenderPrefs;


typedef struct {
	SpotType		spotType;
	short			frequency;
	short			angle;
} HalftonePrefs;


typedef struct {
	TPrint			printRecord;
	bool			useRenderSettings;
	bool			useHalftoneSettings;
	RenderPrefs		renderSettings;
	HalftonePrefs	halftoneSettings;
} PrintPrefs;


typedef struct {
	int				version;
	
	PrintPrefs		printSettings;
	
	ColorDepth		colorDepth;
	
	Rect			defaultWindowSize;
	bool			defaultWindowZoomed;
	
	bool			antiAliasing;
	bool			drawBuffered;
	SInt16			defaultZoomMenuItem;
	
	bool			useExternalFonts;
	
	bool			useStandardPageSize;
	PaperSize		standardPageSize;
	float			specialPageSizeWidth;	// width in points (1/72 inch)
	float			specialPageSizeHeight;	// height in points (1/72 inch)
	PageSizeUnit	specialPageSizeUnit;
	bool			alwaysUseDefaultPageSize;
	
	bool			showErrorDialog;
	bool			showDSCDialog;
} PrefsStruct;


class CGSPreferences : public LPreferencesFile, LCommander
{
	protected:
	
	enum GSPrefsPanelMode {GSPrefsPanelSetup, GSPrefsPanelAccept, GSPrefsPanelCancel,
						   GSPrefsPanelMessage};
	typedef void (CGSPreferences::* PanelHandler)(GSPrefsPanelMode inMode, LView* inView,
												  MessageT inMsg);
	
	typedef struct {
		SInt16			panelNumber;
		ResIDT			panelID;
		LView			*panel;
		PanelHandler	handler;
	} GSPrefsPanel;
	
	
	public:
							CGSPreferences();
							~CGSPreferences();
		
		SInt16				DoPrefsDialog();
		SInt16				Load();
		SInt16				Save();
		void				SetDefaultValues();
		
		PrintPrefs*			GetPrintSettings()			{ return &(mPrefs.printSettings); }
		THPrint				GetDefaultPrintRecord();
		
		ColorDepth			GetColorDepth()				{ return mPrefs.colorDepth; }
		
		bool				GetAntiAliasing()			{ return mPrefs.antiAliasing; }
		bool				GetDrawBuffered()			{ return mPrefs.drawBuffered; }
		SInt16				GetDefaultZoomMenuItem()	{ return mPrefs.defaultZoomMenuItem; }
		
		bool				GetUseExternalFonts()		{ return mPrefs.useExternalFonts; }
		
		bool				GetUseAlwaysDefaultPageSize()	{ return mPrefs.alwaysUseDefaultPageSize; }
		void				GetDefaultPageSize(float& outWidth, float& outHeight);
		
		bool				GetShowErrorDialog()		{ return mPrefs.showErrorDialog; }
		bool				GetShowDSCDialog()			{ return mPrefs.showDSCDialog; }
		
		void				SetWindowSize(Rect& inBounds, bool inZoomed)
								{ mPrefs.defaultWindowSize = inBounds;
								  mPrefs.defaultWindowZoomed = inZoomed;
								  Save();
								}
		void				GetWindowSize(Rect& outBounds, bool& outZoomed)
								{ outBounds = mPrefs.defaultWindowSize;
								  outZoomed = mPrefs.defaultWindowZoomed;
								}
		
	protected:
		PrefsStruct			mPrefs;
		THPrint				mPrintRecordH;
		
		// dialog stuff
		short				mNumPanels;
		GSPrefsPanel		mPanelList[50];
		void				AddPanel(LWindow *inDialog, ResIDT inPanel, PanelHandler inHandler,
									 ConstStringPtr inMenuName);
		virtual void		InstallPanels(LWindow *inDialog);
		
		virtual void		HandleDisplayPanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg);
		virtual void		HandleFontsPanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg);
		virtual void		HandlePageSizePanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg);
		virtual void		HandleOtherPanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg);
		
		
};


#endif /*_H_CGSPreferences*/

