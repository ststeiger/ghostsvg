/* ============================================================================	*/
/*	CGSPreferences.cp						written by Bernd Heller in 2000		*/
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


#include "MacGSViewConstants.h"
#include "CGSPreferences.h"

#include <UReanimator.h>
#include <LFileTypeList.h>
#include <UModalDialogs.h>
#include <LCheckBox.h>
#include <LPopupButton.h>
#include <LRadioButton.h>
#include <LEditText.h>
#include <LStaticText.h>
#include <LWindow.h>
#include <LString.h>
#include <LMultiPanelView.h>
#include <LPopupGroupBox.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


const unsigned char *PrefsFileName	= LStr255(kSTRListGeneral, kPrefsFileNameStr);
const int kCGSPreferencesVersion = 1003;	// currently implemented prefs struct version



// ---------------------------------------------------------------------------
//	€ CGSPreferences
// ---------------------------------------------------------------------------
//	Constructor

CGSPreferences::CGSPreferences()
	: LPreferencesFile(PrefsFileName)
{
	mPrintRecordH = (THPrint) ::NewHandle(sizeof(TPrint));
	ThrowIfMemFail_(mPrintRecordH);
	
	Load();
}



// ---------------------------------------------------------------------------
//	€ ~CGSPreferences
// ---------------------------------------------------------------------------
//	Destructor

CGSPreferences::~CGSPreferences()
{
	Save();
	
	if (mPrintRecordH)
		::DisposeHandle((Handle) mPrintRecordH);
}



// ---------------------------------------------------------------------------
//	€ GetDefaultPageSize											[static]
// ---------------------------------------------------------------------------

void
CGSPreferences::GetDefaultPageSize(float& outWidth, float& outHeight)
{
	if (mPrefs.useStandardPageSize) {
		CUtilities::GetStandardPageSize(mPrefs.standardPageSize, outWidth, outHeight);
	} else {
		outWidth  = mPrefs.specialPageSizeWidth;
		outHeight = mPrefs.specialPageSizeHeight;
	}
}



// ---------------------------------------------------------------------------
//	€ Load
// ---------------------------------------------------------------------------

SInt16
CGSPreferences::Load()
{
	OpenOrCreateResourceFork(fsRdWrPerm, LFileTypeList::GetProcessSignature(),
							 'pref', smSystemScript );
	
	Handle prefsHandle = ::Get1Resource('pref', 128);
	
	if (prefsHandle && (::GetHandleSize(prefsHandle) == sizeof(PrefsStruct))
			&& ((PrefsStruct *) *prefsHandle)->version == kCGSPreferencesVersion) {
		// copy the preferences struct
		mPrefs = *((PrefsStruct *) *prefsHandle);
		::ReleaseResource(prefsHandle);
		
		// copy print record
		**mPrintRecordH = mPrefs.printSettings.printRecord;
		::PrValidate(mPrintRecordH);
	} else {
		SetDefaultValues();
	}
	
	CloseResourceFork();
	
	return 0;
}



// ---------------------------------------------------------------------------
//	€ Save
// ---------------------------------------------------------------------------

SInt16
CGSPreferences::Save()
{
	OSErr	err = noErr;
	
	// enter print record
	mPrefs.printSettings.printRecord = **mPrintRecordH;
	
	OpenOrCreateResourceFork(fsRdWrPerm, LFileTypeList::GetProcessSignature(),
							 'pref', smSystemScript );
	
	Handle prefsHandle = ::Get1Resource('pref', 128);
	
	if (prefsHandle) {
		::SetHandleSize(prefsHandle, sizeof(PrefsStruct));
		if ((err = ::MemError()) == noErr) {
			// copy the prefs struct into the handle and tell the rsrc manager
			*((PrefsStruct *) *prefsHandle) = mPrefs;
			::ChangedResource(prefsHandle);
		}
	} else {
		// create a new resource
		if ((prefsHandle = ::NewHandle(sizeof(PrefsStruct)) )) {
			// copy the prefs struct into the handle and make it into a resource
			*((PrefsStruct *) *prefsHandle) = mPrefs;
			::AddResource(prefsHandle, 'pref', 128, "\pPrefsStruct");
			if ((err = ::ResError()) != noErr)
				::DisposeHandle(prefsHandle);
		} else {
			err = ::MemError(); // NewHandle failed
		}
	}
	
	CloseResourceFork();
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ GetDefaultPrintRecord
// ---------------------------------------------------------------------------
//	Returns the applications default printer settings

THPrint
CGSPreferences::GetDefaultPrintRecord()
{
	return mPrintRecordH;
}



// ---------------------------------------------------------------------------
//	€ SetDefaultValues
// ---------------------------------------------------------------------------

void
CGSPreferences::SetDefaultValues()
{
	// preferences struct version
	mPrefs.version					= kCGSPreferencesVersion;
	
	// window
	::SetRect(&mPrefs.defaultWindowSize, 0, 0, 400, 300);
	mPrefs.defaultWindowZoomed = true;
	
	// print preferences
	mPrefs.printSettings.useRenderSettings					= false;
	mPrefs.printSettings.useHalftoneSettings				= false;
	mPrefs.printSettings.renderSettings.resolution			= -1;
	mPrefs.printSettings.renderSettings.colorDepth			= kMillionColors;
	mPrefs.printSettings.renderSettings.useExternalFonts	= false;
	mPrefs.printSettings.halftoneSettings.spotType			= kDotSpot;
	mPrefs.printSettings.halftoneSettings.frequency			= -1;
	mPrefs.printSettings.halftoneSettings.angle				= -1;
	
	// render settings
	mPrefs.colorDepth				= kMaximumColors;
	
	// display settings
	mPrefs.antiAliasing				= false;
	mPrefs.drawBuffered				= true;
	mPrefs.defaultZoomMenuItem		= zoomFitWidth;
	
	mPrefs.graphicsTransparency		= 1;
	mPrefs.textTransparency			= 4;
	
	// fonts settings
	mPrefs.useExternalFonts			= false;
	
	// page size settings
	mPrefs.useStandardPageSize		= true;
	mPrefs.standardPageSize			= kA4;
	mPrefs.specialPageSizeWidth		= 0;
	mPrefs.specialPageSizeHeight	= 0;
	mPrefs.specialPageSizeUnit		= kMillimeters;
	mPrefs.alwaysUseDefaultPageSize	= false;
	
	// other settings
	mPrefs.showErrorDialog			= true;
	mPrefs.showDSCDialog			= true;
	
	// set default print record
	::PrOpen();
	if (::PrError() == noErr)
		::PrintDefault(mPrintRecordH);
	::PrClose();
}



#pragma mark -
#pragma mark === Dialog Handling ===



// ---------------------------------------------------------------------------
//	€ DoPrefsDialog
// ---------------------------------------------------------------------------

SInt16
CGSPreferences::DoPrefsDialog()
{
	StDialogHandler		theHandler(PPob_PrefsDialog, this);
	LWindow*			theDialog = theHandler.GetDialog();
	LMultiPanelView*	panelView = dynamic_cast<LMultiPanelView*>
									(theDialog->FindPaneByID(kPrefsMultiPanelView));
	
	// let the device install its panels
	mNumPanels = 0;
	InstallPanels(theDialog);
	
	// make the multi-panel view listen to its superview
	// have to do it here explicitly because adding menu items causes wrong switch messages
	LBroadcaster	*panelViewSuper = dynamic_cast<LBroadcaster*>(panelView->GetSuperView());
	panelViewSuper->AddListener(panelView);
	
	// we want to listen to messages from the panels
	for (short i=0; i<mNumPanels; i++)
		UReanimator::LinkListenerToControls(&theHandler, mPanelList[i].panel, mPanelList[i].panelID);
	
	// let all panel handler initialize their controls
	for (short i=0; i<mNumPanels; i++)
		(this->*(mPanelList[i].handler))(GSPrefsPanelSetup, mPanelList[i].panel, 0);
	
	// show the dialog
	panelView->SwitchToPanel(1);
	theDialog->Show();
	
	while (true) {
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK) {
			for (short i=0; i<mNumPanels; i++)
				(this->*(mPanelList[i].handler))(GSPrefsPanelAccept, mPanelList[i].panel, hitMessage);
			Save();
			return 0;
		} else if (hitMessage == msg_Cancel) {
			for (short i=0; i<mNumPanels; i++)
				(this->*(mPanelList[i].handler))(GSPrefsPanelCancel, mPanelList[i].panel, hitMessage);
			return -1;
		} else {
			short	i = panelView->GetCurrentIndex() - 1;
			(this->*(mPanelList[i].handler))(GSPrefsPanelMessage, mPanelList[i].panel, hitMessage);
		}
	}
}



// ---------------------------------------------------------------------------
//	€ InstallPanels
// ---------------------------------------------------------------------------
// is called from DoPrefsDialog(), this is the place to add panels

void
CGSPreferences::InstallPanels(LWindow *inDialog)
{
	AddPanel(inDialog, PPob_PrefsDisplayPanel, &HandleDisplayPanel,
				LStr255(kSTRListPrefsPanelNames, kExportPanelPageRangeStr));
	AddPanel(inDialog, PPob_PrefsFontsPanel, &HandleFontsPanel,
				LStr255(kSTRListPrefsPanelNames, kExportPanelPageSizeStr));
	AddPanel(inDialog, PPob_PrefsPageSizePanel, &HandlePageSizePanel,
				LStr255(kSTRListPrefsPanelNames, kExportPanelRenderingStr));
	AddPanel(inDialog, PPob_PrefsOtherPanel, &HandleOtherPanel,
				LStr255(kSTRListPrefsPanelNames, kExportPanelHalftonesStr));
}



// ---------------------------------------------------------------------------
//	€ AddPanel
// ---------------------------------------------------------------------------

void
CGSPreferences::AddPanel(LWindow *inDialog, ResIDT inPanel, PanelHandler inHandler,
						 ConstStringPtr inMenuName)
{
	LPopupGroupBox		*panelMenu = dynamic_cast<LPopupGroupBox*>
										(inDialog->FindPaneByID(kPrefsPopupGroupBox));
	LMultiPanelView		*panelView = dynamic_cast<LMultiPanelView*>
										(inDialog->FindPaneByID(kPrefsMultiPanelView));
	
	panelMenu->AppendMenu(inMenuName);
	panelView->AddPanel(inPanel, 0, panelView->GetPanelCount()+1);	// add at the end
	
	mPanelList[mNumPanels].panelNumber = panelView->GetPanelCount();
	mPanelList[mNumPanels].panelID = inPanel;
	mPanelList[mNumPanels].handler = inHandler;
	mPanelList[mNumPanels].panel = panelView->CreatePanel(mPanelList[mNumPanels].panelNumber);
	mPanelList[mNumPanels].panel->Hide();
	mNumPanels++;
}



// ---------------------------------------------------------------------------
//	€ HandleDisplayPanel
// ---------------------------------------------------------------------------

void
CGSPreferences::HandleDisplayPanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg)
{
#pragma unused(inMsg)
	
	static LPopupButton		*colDepthMenu, *defaultZoomMenu;
	static LCheckBox		*antiAliasBox, *bufferedBox;
	static LRadioButton		*graphicsAlphaBits1, *graphicsAlphaBits2, *graphicsAlphaBits4,
							*textAlphaBits1, *textAlphaBits2, *textAlphaBits4;
	
	if (inMode == GSPrefsPanelSetup) {
		colDepthMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kPrefsDisplayColorDepth));
		antiAliasBox = dynamic_cast<LCheckBox*> (inView->FindPaneByID(kPrefsDisplayAntiAliasing));
		bufferedBox = dynamic_cast<LCheckBox*> (inView->FindPaneByID(kPrefsDisplayDrawBuffered));
		defaultZoomMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kPrefsDisplayDefaultZoom));
		graphicsAlphaBits1 = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsDisplayGraphicsAlphaBits1));
		graphicsAlphaBits2 = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsDisplayGraphicsAlphaBits2));
		graphicsAlphaBits4 = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsDisplayGraphicsAlphaBits4));
		textAlphaBits1 = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsDisplayTextAlphaBits1));
		textAlphaBits2 = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsDisplayTextAlphaBits2));
		textAlphaBits4 = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsDisplayTextAlphaBits4));
		
		// remove last menu item from zoom menu (other zoom)
		defaultZoomMenu->DeleteMenuItem(zoomToValue);
		
		colDepthMenu->SetCurrentMenuItem(mPrefs.colorDepth);
		defaultZoomMenu->SetCurrentMenuItem(mPrefs.defaultZoomMenuItem);
		antiAliasBox->SetValue(mPrefs.antiAliasing);
		bufferedBox->SetValue(mPrefs.drawBuffered);
		
		graphicsAlphaBits1->SetValue(false);
		graphicsAlphaBits2->SetValue(false);
		graphicsAlphaBits4->SetValue(false);
		switch(mPrefs.graphicsTransparency) {
			case 1: graphicsAlphaBits1->SetValue(true); break;
			case 2: graphicsAlphaBits2->SetValue(true); break;
			case 4: graphicsAlphaBits4->SetValue(true); break;
		}
		
		textAlphaBits1->SetValue(false);
		textAlphaBits2->SetValue(false);
		textAlphaBits4->SetValue(false);
		switch(mPrefs.textTransparency) {
			case 1: textAlphaBits1->SetValue(true); break;
			case 2: textAlphaBits2->SetValue(true); break;
			case 4: textAlphaBits4->SetValue(true); break;
		}
	} else if (inMode == GSPrefsPanelAccept) {
		mPrefs.colorDepth = (ColorDepth) colDepthMenu->GetCurrentMenuItem();
		mPrefs.defaultZoomMenuItem = defaultZoomMenu->GetCurrentMenuItem();
		mPrefs.antiAliasing = antiAliasBox->GetValue();
		mPrefs.drawBuffered = bufferedBox->GetValue();
		
		if (graphicsAlphaBits1->GetValue())			mPrefs.graphicsTransparency = 1;
		else if (graphicsAlphaBits2->GetValue())	mPrefs.graphicsTransparency = 2;
		else if (graphicsAlphaBits4->GetValue())	mPrefs.graphicsTransparency = 4;
		
		if (textAlphaBits1->GetValue())			mPrefs.textTransparency = 1;
		else if (textAlphaBits2->GetValue())	mPrefs.textTransparency = 2;
		else if (textAlphaBits4->GetValue())	mPrefs.textTransparency = 4;
	}
}



// ---------------------------------------------------------------------------
//	€ HandleFontsPanel
// ---------------------------------------------------------------------------

void
CGSPreferences::HandleFontsPanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg)
{
#pragma unused(inMsg)
	
	static LCheckBox		*useXFontsBox;
	
	if (inMode == GSPrefsPanelSetup) {
		useXFontsBox = dynamic_cast<LCheckBox*> (inView->FindPaneByID(kPrefsDisplayUseExternalFonts));	
		useXFontsBox->SetValue(mPrefs.useExternalFonts);
	} else if (inMode == GSPrefsPanelAccept) {
		mPrefs.useExternalFonts = useXFontsBox->GetValue();
	}
}



// ---------------------------------------------------------------------------
//	€ HandlePageSizePanel
// ---------------------------------------------------------------------------

void
CGSPreferences::HandlePageSizePanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg)
{
	static char				valueStr[50];
	static long				len;
	
	static LRadioButton		*standardFormat, *specialFormat;
	static LPopupButton		*formatMenu, *unitMenu;
	static LEditText		*widthField, *heightField;
	static LStaticText		*widthText, *heightText;
	static LCheckBox		*overrideDoc;
	
	if (inMode == GSPrefsPanelSetup) {
			// get pointers to our controls (only once in a dialogs life)
		standardFormat	= dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsPageSizeStandardFormat));
		specialFormat = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kPrefsPageSizeSpecialFormat));
		formatMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kPrefsPageSizeFormatPopup));
		unitMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kPrefsPageSizeUnitPopup));
		widthField = dynamic_cast<LEditText*> (inView->FindPaneByID(kPrefsPageSizeWidthEdit));
		heightField = dynamic_cast<LEditText*> (inView->FindPaneByID(kPrefsPageSizeHeightEdit));
		widthText = dynamic_cast<LStaticText*> (inView->FindPaneByID(kPrefsPageSizeWidthText));
		heightText = dynamic_cast<LStaticText*> (inView->FindPaneByID(kPrefsPageSizeHeightText));
		overrideDoc = dynamic_cast<LCheckBox*> (inView->FindPaneByID(kPrefsPageSizeOverrideDoc));
		
			// initialize with default values
		formatMenu->SetCurrentMenuItem(mPrefs.standardPageSize);
		unitMenu->SetCurrentMenuItem(mPrefs.specialPageSizeUnit);
		
		if (!mPrefs.specialPageSizeWidth) {
			widthField->SetText("", 0);
		} else {
			len = sprintf(valueStr, "%.2f", CUtilities::ConvertUnit(kPoints, mPrefs.specialPageSizeUnit, mPrefs.specialPageSizeWidth));
			widthField->SetText(valueStr, len);
		}
		
		if (!mPrefs.specialPageSizeHeight) {
			heightField->SetText("", 0);
		} else {
			sprintf(valueStr, "%.2f", CUtilities::ConvertUnit(kPoints, mPrefs.specialPageSizeUnit, mPrefs.specialPageSizeHeight));
			heightField->SetText(valueStr, len);
		}
		
		if (mPrefs.useStandardPageSize) {
			standardFormat->SetValue(true);
			specialFormat->SetValue(false);
			unitMenu->Disable();
			widthField->Disable();
			widthText->Disable();
			heightField->Disable();
			heightText->Disable();
		} else {
			standardFormat->SetValue(false);
			specialFormat->SetValue(true);
			formatMenu->Disable();
		}
		
		overrideDoc->SetValue(mPrefs.alwaysUseDefaultPageSize);
		HandlePageSizePanel(GSPrefsPanelMessage, inView, msg_FormatRadioButtonClick);
	} else if (inMode == GSPrefsPanelAccept) {		// accept panel settings
		mPrefs.useStandardPageSize = standardFormat->GetValue();
		mPrefs.standardPageSize = (PaperSize) formatMenu->GetCurrentMenuItem();
		mPrefs.specialPageSizeUnit = (PageSizeUnit) unitMenu->GetCurrentMenuItem();
		mPrefs.alwaysUseDefaultPageSize = overrideDoc->GetValue();
	
		widthField->GetText(valueStr, 50, &len); valueStr[len] = 0;
		mPrefs.specialPageSizeWidth = CUtilities::ConvertUnit(mPrefs.specialPageSizeUnit, kPoints, atof(valueStr));
		
		heightField->GetText(valueStr, 50, &len); valueStr[len] = 0;
		mPrefs.specialPageSizeHeight = CUtilities::ConvertUnit(mPrefs.specialPageSizeUnit, kPoints, atof(valueStr));
		
		// handle empty page size editfields
		if (!mPrefs.useStandardPageSize) {
			if (mPrefs.specialPageSizeWidth <= 0 || mPrefs.specialPageSizeHeight <= 0)
				mPrefs.useStandardPageSize = true;
		}
	} else if (inMode == GSPrefsPanelMessage) {		// handle other messages
		if (inMsg == msg_FormatRadioButtonClick) {
			if (standardFormat->GetValue()) {
				formatMenu->Enable();
				unitMenu->Disable();
				heightField->Disable();
				widthField->Disable();
				heightText->Disable();
				widthText->Disable();
			} else if (specialFormat->GetValue()) {
				formatMenu->Disable();
				unitMenu->Enable();
				heightField->Enable();
				widthField->Enable();
				heightText->Enable();
				widthText->Enable();
			}
		} else if (inMsg == msg_ChangeUnit) {
			PageSizeUnit	currUnit = (PageSizeUnit) unitMenu->GetCurrentMenuItem();
			double			value;
			
			widthField->GetText(valueStr, 50, &len); valueStr[len] = 0;
			value = CUtilities::ConvertUnit(mPrefs.specialPageSizeUnit, currUnit, atof(valueStr));
			len = sprintf(valueStr, "%.2f", value);
			widthField->SetText(valueStr, len);
			
			heightField->GetText(valueStr, 50, &len); valueStr[len] = 0;
			value = CUtilities::ConvertUnit(mPrefs.specialPageSizeUnit, currUnit, atof(valueStr));
			len = sprintf(valueStr, "%.2f", value);
			heightField->SetText(valueStr, len);
			
			mPrefs.specialPageSizeUnit = currUnit;
		}
	}
}



// ---------------------------------------------------------------------------
//	€ HandleOtherPanel
// ---------------------------------------------------------------------------

void
CGSPreferences::HandleOtherPanel(GSPrefsPanelMode inMode, LView* inView, MessageT inMsg)
{
#pragma unused(inMsg)
	
	static LCheckBox		*errorDialogBox, *dscDialogBox;
	
	if (inMode == GSPrefsPanelSetup) {
		errorDialogBox = dynamic_cast<LCheckBox*> (inView->FindPaneByID(kPrefsOtherReportErrors));	
		dscDialogBox = dynamic_cast<LCheckBox*> (inView->FindPaneByID(kPrefsOtherDSCWarning));
		
		errorDialogBox->SetValue(mPrefs.showErrorDialog);
		dscDialogBox->SetValue(mPrefs.showDSCDialog);
	} else if (inMode == GSPrefsPanelAccept) {
		mPrefs.showErrorDialog = errorDialogBox->GetValue();
		mPrefs.showDSCDialog = dscDialogBox->GetValue();
	}
}






