/* ============================================================================	*/
/*	CGSAIDevice.cp							written by Bernd Heller in 2000		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "ps2ai"											*/
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
#include "CGSAIDevice.h"


// ---------------------------------------------------------------------------------
//	€ CGSAIDevice
// ---------------------------------------------------------------------------------
//	Constructor

CGSAIDevice::CGSAIDevice(CGSDocument* inDocument)
		: CGSDevice(inDocument)
{
	mFileType		= 'AI  ';	//!!!
	mFileCreator	= 'ttxt';	//!!!
	
	
	mIsMultiplePageDevice = true;	// this is not true, but the only way to prevent
									// the additional %d in the filename which is not
									// handled by the ps2ai script
	
	if (GetDLL() == 0)
		return;
}



// ---------------------------------------------------------------------------
//	€ InstallPanels
// ---------------------------------------------------------------------------
// installs only the PageRange panel

void
CGSAIDevice::InstallPanels(LWindow *inDialog)
{
	AddPanel(inDialog, PPob_ExportPageRangePanel, &HandlePageRangePanel,
				LStr255(kSTRListExportPanelNames, kExportPanelPageRangeStr));
}



// ---------------------------------------------------------------------------
//	€ SetupDevice
// ---------------------------------------------------------------------------

int
CGSAIDevice::SetupDevice()
{
	int err;
	char	paramStr[1024];
	
	sprintf(paramStr, "/jout true def /joutput (%s) def\n", mOutputFilePath);
	GetDLL()->ExecStr(paramStr);
	if ((err = GetDLL()->ExecStr("(ps2ai.ps) runlibfile\n")))
		ErrorDialog(err);
	
	return 0;
}


