/* ============================================================================	*/
/*	CGSPxMDevice.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "pbm", "pgm", "ppm"								*/
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


#include "CGSPxMDevice.h"


// ---------------------------------------------------------------------------------
//	€ CGSPxMDevice
// ---------------------------------------------------------------------------------
//	Constructor

CGSPxMDevice::CGSPxMDevice(CGSDocument* inDocument)
		: CGSDevice(inDocument)
{
	mFileType		= 'PPGM';
	mFileCreator	= 'GKON';
}



// ---------------------------------------------------------------------------
//	€ HandleRenderingPanel
// ---------------------------------------------------------------------------

void
CGSPxMDevice::HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
	static LPopupButton		*colorMenu;
	
	static short			lastColorDepth = 3;
	
	inherited::HandleRenderingPanel(inMode, inView, inMsg);
	
	if (inMode == GSExportPanelSetup) {
		colorMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportRenderingColorDepth));
		
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportBlackWhiteStr));		// item 1
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportGrayscaleStr));		// item 2
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportMillionsColorsStr));	// item 3
		colorMenu->SetCurrentMenuItem(lastColorDepth);
	} else if (inMode == GSExportPanelAccept) {
		lastColorDepth = colorMenu->GetCurrentMenuItem();
		switch (lastColorDepth) {
			case 1: sprintf(mDeviceName, "pbm"); break;	// item 1 = Black and White
			case 2: sprintf(mDeviceName, "pgm"); break;	// item 2 = Grayscale
			case 3: sprintf(mDeviceName, "ppm"); break;	// item 3 = Millions of Colors
		}
	}
}



