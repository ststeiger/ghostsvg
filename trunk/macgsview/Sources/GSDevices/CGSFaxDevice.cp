/* ============================================================================	*/
/*	CGSFaxDevice.cp							written by Bernd Heller in 2000		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "tiffg3", "tiffg32d", "tiffg4"						*/
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


#include <LRadioButton.h>
#include <LPopupButton.h>
#include <LStaticText.h>

#include "CGSFaxDevice.h"


// ---------------------------------------------------------------------------------
//	€ CGSFaxDevice
// ---------------------------------------------------------------------------------
//	Constructor

CGSFaxDevice::CGSFaxDevice(CGSDocument* inDocument)
		: CGSDevice(inDocument)
{
	mFileType		= 'TIFF';
	mFileCreator	= 'ogle';
}



// ---------------------------------------------------------------------------
//	€ InstallPanels
// ---------------------------------------------------------------------------

void
CGSFaxDevice::InstallPanels(LWindow *inDialog)
{
	inherited::InstallPanels(inDialog);
	
	AddPanel(inDialog, PPob_ExportFaxTypePanel, (PanelHandler) &HandleFaxTypePanel,
				LStr255(kSTRListExportPanelNames, kExportPanelFaxTypeStr));
}



// ---------------------------------------------------------------------------
//	€ HandleRenderingPanel
// ---------------------------------------------------------------------------

void
CGSFaxDevice::HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
	static LPopupButton		*colorMenu;
	static LStaticText		*colorText;
	
	inherited::HandleRenderingPanel(inMode, inView, inMsg);
	
	if (inMode == GSExportPanelSetup) {
		colorMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportRenderingColorDepth));
		colorText = dynamic_cast<LStaticText*> (inView->FindPaneByID(kExportRenderingColorDepthText));
		
		colorMenu->Hide();
		colorText->Hide();
	}
}



// ---------------------------------------------------------------------------
//	€ HandleFaxTypePanel
// ---------------------------------------------------------------------------

void
CGSFaxDevice::HandleFaxTypePanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
#pragma unused(inMsg)
	
	static LRadioButton		*g3Button, *g32dButton, *g4Button;
	
	static short			lastType = 1;
	
	if (inMode == GSExportPanelSetup) {
		g3Button = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportFaxTypeGroup31D));
		g32dButton = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportFaxTypeGroup32D));
		g4Button = dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportFaxTypeGroup4));
		
		if (lastType == 1) {
			g3Button->SetValue(Button_On);
		} else if (lastType == 2) {
			g32dButton->SetValue(Button_On);
		} else if (lastType == 3) {
			g4Button->SetValue(Button_On);
		}
	} else if (inMode == GSExportPanelAccept) {
		if (g3Button->GetValue()) {
			lastType = 1;
			sprintf(mDeviceName, "tiffg3");
		} else if (g32dButton->GetValue()) {
			lastType = 2;
			sprintf(mDeviceName, "tiffg32d");
		} else if (g4Button->GetValue()) {
			lastType = 3;
			sprintf(mDeviceName, "tiffg4");
		}
	}
}

