/* ============================================================================	*/
/*	CGSJPEGDevice.cp						written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "jpeg", "jpeggray"									*/
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


#include <string.h>

#include "CGSSharedLib.h"
#include "CGSJPEGDevice.h"


#include <LSlider.h>


// ---------------------------------------------------------------------------------
//	€ CGSJPEGDevice
// ---------------------------------------------------------------------------------
//	Constructor

CGSJPEGDevice::CGSJPEGDevice(CGSDocument* inDocument)
		: CGSDevice(inDocument)
{
	mFileType		= 'JPEG';
	mFileCreator	= 'ogle';
}



// ---------------------------------------------------------------------------
//	€ InstallPanels
// ---------------------------------------------------------------------------

void
CGSJPEGDevice::InstallPanels(LWindow *inDialog)
{
	inherited::InstallPanels(inDialog);
	
	AddPanel(inDialog, PPob_ExportJPEGQualityPanel, (PanelHandler) &HandleJPEGQualityPanel,
				LStr255(kSTRListExportPanelNames, kExportPanelJPEGQualityStr));
}



// ---------------------------------------------------------------------------
//	€ HandleRenderingPanel
// ---------------------------------------------------------------------------

void
CGSJPEGDevice::HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
	static LPopupButton		*colorMenu;
	
	static short			lastColorDepth = 2;
	
	inherited::HandleRenderingPanel(inMode, inView, inMsg);
	
	if (inMode == GSExportPanelSetup) {
		colorMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportRenderingColorDepth));
		
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportGrayscaleStr));		// item 1
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportColorStr));			// item 2
		colorMenu->SetCurrentMenuItem(lastColorDepth);
	} else if (inMode == GSExportPanelAccept) {
		lastColorDepth = colorMenu->GetCurrentMenuItem();
		switch (lastColorDepth) {
			case 1: sprintf(mDeviceName, "jpeggray"); break;	// item 1 = Grayscale
			case 2: sprintf(mDeviceName, "jpeg"); break;		// item 2 = Color
		}
	}
}



// ---------------------------------------------------------------------------
//	€ HandleJPEGQualityPanel
// ---------------------------------------------------------------------------

void
CGSJPEGDevice::HandleJPEGQualityPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
#pragma unused(inMsg)
	
	static LSlider			*qualitySlider;
	
	static int				lastQuality = 50;
	
	if (inMode == GSExportPanelSetup) {
		qualitySlider = dynamic_cast<LSlider*> (inView->FindPaneByID(kExportJPEGQuality));
		qualitySlider->SetValue(lastQuality);
	} else if (inMode == GSExportPanelAccept) {
		lastQuality = mQuality = qualitySlider->GetValue();
	}
}



// ---------------------------------------------------------------------------
//	€ SubmitDeviceParameters
// ---------------------------------------------------------------------------

void
CGSJPEGDevice::SubmitDeviceParameters()
{
	char	paramStr[1024];
	
	inherited::SubmitDeviceParameters();
	
	sprintf(paramStr, "/JPEGQ %d\n", mQuality);
	GetDLL()->ExecStr(paramStr);
}



