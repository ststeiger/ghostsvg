/* ============================================================================	*/
/*	CGSBMPDevice.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "bmpmono", "bmp16", "bmp256", "bmp16m"				*/
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


#include "CGSBMPDevice.h"


// ---------------------------------------------------------------------------------
//	€ CGSBMPDevice
// ---------------------------------------------------------------------------------
//	Constructor

CGSBMPDevice::CGSBMPDevice(CGSDocument* inDocument)
		: CGSDevice(inDocument)
{
	mFileType		= 'BMPf';
	mFileCreator	= 'ogle';
}



// ---------------------------------------------------------------------------
//	€ HandleRenderingPanel
// ---------------------------------------------------------------------------

void
CGSBMPDevice::HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
	static LPopupButton		*colorMenu;
	
	static short			lastColorDepth = 4;
	
	inherited::HandleRenderingPanel(inMode, inView, inMsg);
	
	if (inMode == GSExportPanelSetup) {
		colorMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportRenderingColorDepth));
		
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportBlackWhiteStr));		// item 1
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExport16ColorsStr));			// item 2
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExport256ColorsStr));		// item 3
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportMillionsColorsStr));	// item 4
		colorMenu->SetCurrentMenuItem(lastColorDepth);
	} else if (inMode == GSExportPanelAccept) {
		lastColorDepth = colorMenu->GetCurrentMenuItem();
		switch (lastColorDepth) {
			case 1: sprintf(mDeviceName, "bmpmono"); break;	// item 1 = Black and White
			case 2: sprintf(mDeviceName, "bmp16"); break;	// item 2 = 16 Colors
			case 3: sprintf(mDeviceName, "bmp256"); break;	// item 3 = 256 Colors
			case 4: sprintf(mDeviceName, "bmp16m"); break;	// item 4 = Millions of Colors
		}
	}
}


