/* ============================================================================	*/
/*	CGSASCIIDevice.cp						written by Bernd Heller in 2000		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "ps2ascii"											*/
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
#include "CGSASCIIDevice.h"


// ---------------------------------------------------------------------------------
//	€ CGSASCIIDevice
// ---------------------------------------------------------------------------------
//	Constructor

CGSASCIIDevice::CGSASCIIDevice(CGSDocument* inDocument)
		: CGSDevice(inDocument)
{
	mFileType		= 'TEXT';
	mFileCreator	= 'ttxt';
	
	mIsMultiplePageDevice = true;
}



// ---------------------------------------------------------------------------------
//	€ ~CGSASCIIDevice
// ---------------------------------------------------------------------------------
//	Destructor

CGSASCIIDevice::~CGSASCIIDevice()
{
	// clean up redirection
	CGSSharedLib::RedirectStdoutToFile(0);
}



// ---------------------------------------------------------------------------
//	€ RenderPage
// ---------------------------------------------------------------------------

void
CGSASCIIDevice::RenderPage(int inFirstPage, int inLastPage)
{
	// we have to redirect stdout, thats why we cannot allow other threads to run
	mIsThreadedDevice = false;
	
	CGSDevice::RenderPage(inFirstPage, inLastPage);
}



// ---------------------------------------------------------------------------
//	€ InstallPanels
// ---------------------------------------------------------------------------
// installs only the PageRange panel

void
CGSASCIIDevice::InstallPanels(LWindow *inDialog)
{
	AddPanel(inDialog, PPob_ExportPageRangePanel, &HandlePageRangePanel,
				LStr255(kSTRListExportPanelNames, kExportPanelPageRangeStr));
}



// ---------------------------------------------------------------------------
//	€ SetupDevice
// ---------------------------------------------------------------------------

int
CGSASCIIDevice::SetupDevice()
{
	if (GetDLL() == 0)
		return -1;
	
	// re-init dll
	int err = GetDLL()->Init(2, "-dNOBIND", "-dWRITESYSTEMDICT");
	if (err) {
		ErrorDialog(err);
	}
	
	if ((err = GetDLL()->ExecStr("/SIMPLE true def (ps2ascii.ps) runlibfile\n")))
		ErrorDialog(err);
	
	// redirect stdout
	CGSSharedLib::RedirectStdoutToFile(0);
	return CGSSharedLib::RedirectStdoutToFile(&mOutputFileFSSpec);
}


