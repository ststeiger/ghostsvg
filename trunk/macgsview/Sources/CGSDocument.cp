/* ============================================================================	*/
/*	CGSDocument.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	This class is an instance of LDocument.										*/
/*	Here are all important methods for a LDocument object.						*/
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


#include <LString.h>
#include <LCmdBevelButton.h>

#include <UPrintingMgr.h>
#include <UDesktop.h>
#include <UModalDialogs.h>
#include <UAppleEventsMgr.h>
#include <UProfiler.h>
#include <UResourceMgr.h>
#include <UEnvironment.h>

#include <FSp_fopen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Folders.h>
#include <Errors.h>
#include <Sound.h>

#include "MacGSViewConstants.h"

#include "CGSApplication.h"
#include "CGSDocument.h"
#include "CGSDocumentWindow.h"
#include "CGSSharedLib.h"
#include "CNavCustomDialog.h"
#include "CGSToolbar.h"

#include "CGSMacDevice.h"
#include "CGSPDFDevice.h"
#include "CGSPSDevice.h"
#include "CGSEPSDevice.h"
#include "CGSBMPDevice.h"
#include "CGSJPEGDevice.h"
#include "CGSPCXDevice.h"
#include "CGSPNGDevice.h"
#include "CGSPxMDevice.h"
#include "CGSTIFFDevice.h"
#include "CGSASCIIDevice.h"
#include "CGSAIDevice.h"
#include "CGSFaxDevice.h"



SInt32			CGSDocument::sSaveDialogFileTypeNr = 9;		// PDF
OSType			CGSDocument::sSaveDialogFileType = kCGSSaveFormatPDF;

TPPrDlg			CGSDocument::sPrintDialog = 0;
short			CGSDocument::sPrintDialogFirstItem = 0;
PItemUPP		CGSDocument::sPrintItemProc = 0;
ModalFilterUPP	CGSDocument::sPrintFltrProc = 0;


// ---------------------------------------------------------------------------
//	€ CGSDocument
// ---------------------------------------------------------------------------
//	Constructor:	Instantiates a CGSDocumentWindow and adds it to the Window
//					Menu, then opens the according file.

CGSDocument::CGSDocument(LCommander *inSuper, FSSpec *inFileSpec, bool inAutoCreateWindow)
				: LSingleDoc(inSuper)
{
	mDocumentReady	= false;
	
	mFileAlias		= 0;
	mDocumentInfo	= 0;
	mPrintRecordH	= 0;
	mIsPDF			= false;
	mWindow			= 0;
	
	SetExportDevice(0);
	
	if (inAutoCreateWindow) {
		mWindow = LWindow::CreateWindow(PPob_DocumentWindow, this);
		if (mWindow == NULL || ((CGSDocumentWindow*) mWindow)->GetDLL() == 0) {
			Close();
			return;
		}
	}
	
	// set file
	mFile = new LFile(*inFileSpec);
	mFileAlias = mFile->MakeAlias();	// create an alias to the file
	
	::ThreadBeginCritical();
	::NewThread(kCooperativeThread, CGSDocument::sOpenFile, this, 0, kNewSuspend, nil, &mOpenThread);
	::SetThreadState(mOpenThread, kReadyThreadState, kNoThreadID);
	::ThreadEndCritical();
	::YieldToThread(mOpenThread);
}



// ---------------------------------------------------------------------------
//	€ ~CGSDocument
// ---------------------------------------------------------------------------
//	Destructor

CGSDocument::~CGSDocument()
{
	if (GetExportDevice()) {
		if (GetToolbar()) {
			GetToolbar()->HideStopButton();
			GetToolbar()->SetStatusText(LStr255(kSTRListToolbar, kToolbarAbortingStr));
			GetToolbar()->SetProgress(-1);
		}
		if (GetExportDevice()->GetDLL()) {
			GetExportDevice()->GetDLL()->Abort();
			while (GetExportDevice())
				::YieldToAnyThread();
		}
	}
	
	if (mFileAlias)
		::DisposeHandle((Handle) mFileAlias);
	
	if (mDocumentInfo)
		psfree(mDocumentInfo);
}



// ---------------------------------------------------------------------------
//	€ ObeyCommand
// ---------------------------------------------------------------------------
//	Respond to commands

Boolean
CGSDocument::ObeyCommand(CommandT inCommand, void* ioParam)
{
	Boolean		cmdHandled = true;

	switch (inCommand) {
		case cmd_PageSetup:
			SetupPage();
			break;
		
		case cmd_Print:
			DoPrint();
			break;
			
		case cmd_PrintOne:
			break;
			
		default:
			cmdHandled = LDocument::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}



// ---------------------------------------------------------------------------------
//	€ SpendTime
// ---------------------------------------------------------------------------------
//	Used to check if the file has been moved (the alias has changed).

void
CGSDocument::SpendTime(const EventRecord &inMacEvent)
{
	#pragma unused (inMacEvent)
	
	static UInt32	nextSynchTicks = 0;
	UInt32			currentTicks = ::TickCount();
	
	if (currentTicks > nextSynchTicks)
	{	
		// Ask the Alias Manager for the document¹s file location...
		Boolean			aliasChanged = false;
		FSSpec			newSpec;
		FolderType		folder = 0;
		::ResolveAlias(NULL, mFileAlias, &newSpec, &aliasChanged);
		if (aliasChanged) {
			// The file location has changed; update the FSSpec and the window title
			mFile->SetSpecifier(newSpec);
			if (mWindow)
				mWindow->SetDescriptor(newSpec.name);
		}
		
		// Close the document if the user moved the file into the Trash
		
		// Find the Trash folder
		OSStatus		trashStatus;
		SInt16			trashVRefNum;
		SInt32			trashDirID;
		trashStatus = ::FindFolder(kOnSystemDisk, kTrashFolderType, kDontCreateFolder,
								   &trashVRefNum, &trashDirID);
		
		// We need to walk up the file¹s parent folder hierarchy to ensure
		// that the user hasn¹t moved it into a folder inside the Trash
		//
		// We ignore the aliasChanged flag because the parent folder
		// hierarchy can change without affecting the alias
		if (trashStatus == noErr) {
			do {
				// If we¹ve reached a root folder, we know
				// the file¹s not in the Trash
				if (newSpec.parID == fsRtParID)
					break;
				
				// If the Trash is a parent of the original file,
				// close the window
				if ((newSpec.vRefNum == trashVRefNum) && (newSpec.parID == trashDirID)) {
					Close();
					break;
				}
			} while(::FSMakeFSSpec(newSpec.vRefNum, newSpec.parID, "\p", &newSpec) == noErr);
		}
	}
}



#pragma mark -
#pragma mark === Open File ===



pascal void*
CGSDocument::sOpenFile(void *arg)
{
	CGSDocument		*obj = (CGSDocument*) arg;
	
	obj->OpenFile();
	obj->mOpenThread = 0;
	
/*	::YieldToThread(0);	// needed to let LEventDispatcher execute, otherwise an ActivateEvent
						// will be dispatched to the already deleted window
	
	if (!obj->mDocumentReady)
		obj->Close();
*/	
	return 0;
}

// ---------------------------------------------------------------------------
//	€ OpenFile
// ---------------------------------------------------------------------------
//	Opens a file

void
CGSDocument::OpenFile()
{
#if __option (profile)
	StProfileSection	profiler("\pMacGSView OpenFile profile", 500, 500);
#endif
	
	FSSpec		theFileSpec;
	mFile->GetSpecifier(theFileSpec);
	
	mIsSpecified = true;				// dont ask for save on closing
	
	// Set window Title
	if (mWindow) {
		mWindow->SetDescriptor(theFileSpec.name);
		((CGSDocumentWindow*) mWindow)->SetDocument(this);
		mWindow->Show();
		
		// Install window proxy icon
		::SetWindowProxyAlias(mWindow->GetMacPort(), mFileAlias);
		::SetWindowModified(mWindow->GetMacPort(), !mIsSpecified);
	}
	
	// check if file is PDF
	if (CUtilities::FileBeginsWith(theFileSpec, "%PDF", 4)) {		// is PDF
		mIsPDF = true;
		if (mWindow) {
			((CGSDocumentWindow*) mWindow)->GetToolbar()->SetStatusText(LStr255(kSTRListToolbar, kToolbarScanningPDFStr));
			((CGSDocumentWindow*) mWindow)->GetToolbar()->ShowStatusText();
			((CGSDocumentWindow*) mWindow)->GetToolbar()->ShowProgressBar();
		}
		if (ScanPDFFile()) {		// not able to create mDocumentInfo?
			// error dialog!
			return;
		}
		if (mWindow) {
			((CGSDocumentWindow*) mWindow)->GetToolbar()->HideProgressBar();
			((CGSDocumentWindow*) mWindow)->GetToolbar()->HideStatusText();
		}
	} else {														// is PS/EPSF
		mIsPDF = false;
		if (mWindow) {
			((CGSDocumentWindow*) mWindow)->GetToolbar()->SetStatusText(LStr255(kSTRListToolbar, kToolbarScanningPSStr));
			((CGSDocumentWindow*) mWindow)->GetToolbar()->ShowStatusText();
			((CGSDocumentWindow*) mWindow)->GetToolbar()->ShowProgressBar();
		}
		if (ScanPSFile()) {	// not able to create mDocumentInfo?
			// error dialog!
			return;
		}
		if (mWindow) {
			((CGSDocumentWindow*) mWindow)->GetToolbar()->HideProgressBar();
			((CGSDocumentWindow*) mWindow)->GetToolbar()->HideStatusText();
		}
	}
	
	// warn DSC only if we have a window
	if (mWindow && !HasDSC() && CGSApplication::GetPreferences()->GetShowDSCDialog()) {
		SInt16                 itemHit;
		AlertStdAlertParamRec  alertParam;
		
		alertParam.movable       = true;
		alertParam.helpButton    = false;
		alertParam.filterProc    = NewModalFilterProc(UModalAlerts::GetModalEventFilter());
		alertParam.defaultText   = (unsigned char *) kAlertDefaultOKText;
		alertParam.cancelText    = nil;
		alertParam.otherText     = nil;
		alertParam.defaultButton = kAlertStdAlertOKButton;
		alertParam.cancelButton  = kAlertStdAlertCancelButton;
		alertParam.position      = kWindowDefaultPosition;
		
		if (UAppleEventsMgr::InteractWithUser() == noErr) {
			::ThreadBeginCritical();
			UDesktop::Deactivate();
			::StandardAlert(kAlertCautionAlert, LStr255(kSTRListGeneral, kDSCWarningStr),
							nil, &alertParam, &itemHit);
			UDesktop::Activate();
			::ThreadEndCritical();
		}
		
		::DisposeRoutineDescriptor(alertParam.filterProc);
	}
	
	StartRepeating();
	
	mDocumentReady = true;
	
	if (mWindow) {
		((CGSDocumentWindow*) mWindow)->InitZoom();
		((CGSDocumentWindow*) mWindow)->GotoPage(1);
	}
}



// ---------------------------------------------------------------------------
//	€ ScanPDFFile
// ---------------------------------------------------------------------------
//	Scans a PDF file for page breaks, media sizes, ... and fills the
//	information into the mDocumentInfo structure.

int
CGSDocument::ScanPDFFile()
{
	// generate DSC file for the PDF
	FSSpec	theDscFile;
	char	paramStr[2048];
	char	tempStr[1024];
	char	tempFilePath[2048];
	int		err = 0;
	
	if(!CUtilities::GetNewTempFile(theDscFile))
		return -1;
	CUtilities::ConvertFSSpecToPath(&theDscFile, tempFilePath, 2048);
	
	GetFilePath(tempStr);
	
	CGSSharedLib	conversionLib;
	err = conversionLib.Init();
	if (err) {
		// error
		return err;
	}
	
	sprintf(paramStr, "/PDFname (%s) def /DSCname (%s) def (pdf2dsc.ps) runlibfile\n",
			tempStr, tempFilePath);
	err = conversionLib.ExecStr(paramStr);
	
	err = ScanPSFile(&theDscFile);
	::FSpDelete(&theDscFile);
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ ScanPSFile
// ---------------------------------------------------------------------------
//	Scans a PostScript file for page breaks, media sizes, ... and fills the
//	the information into the mDocumentInfo structure.

int
CGSDocument::ScanPSFile(FSSpecPtr inFileSpecPtr)
{
	char		filePath[1024];
	FSSpec		theFileSpec;
	FILE		*thePSFile;
	
	if (inFileSpecPtr)
		theFileSpec = *inFileSpecPtr;
	else
		mFile->GetSpecifier(theFileSpec);
	
	CUtilities::ConvertFSSpecToPath(&theFileSpec, filePath, 1024);
	if ((thePSFile = fopen(filePath, "r")) == NULL) {
		// cannot open file
		CUtilities::ShowErrorDialog("\pfopen failed in ScanPSFile", 0);
		return -1;
	}
	
	// Get length of the data fork
	long fileLength;
	mFile->OpenDataFork(fsRdPerm);
	::GetEOF(mFile->GetDataForkRefNum(), &fileLength);
	mFile->CloseDataFork();
	
	mHasDSC = true;
	
	if ((mDocumentInfo = psscan(thePSFile)) == NULL) {
		// no mDocumentInfo, create a default struct
		
		mDocumentInfo = (document *) malloc(sizeof(document));
		memset((void *) mDocumentInfo, 0, sizeof(document));
		mDocumentInfo->pages = (page *) malloc(sizeof(page));
		memset((void *) mDocumentInfo->pages, 0, sizeof(page));
		
		mHasDSC = false;
		mDocumentInfo->numpages = 1;
		mDocumentInfo->pages[0].len		= fileLength;
		mDocumentInfo->pages[0].begin	= 0;
		mDocumentInfo->pages[0].end		= mDocumentInfo->pages->len;
	} else if (IsEPS() && (mDocumentInfo->numpages == 0)) {
		// we change mDocumentInfo to make the EPS appear as a one-page ps document
		mDocumentInfo->numpages = 1;
		mDocumentInfo->pages = (page *) malloc(sizeof(page));
		memset((void *) mDocumentInfo->pages, 0, sizeof(page));
		mDocumentInfo->pages[0].len		= fileLength;
		mDocumentInfo->pages[0].begin	= 0;
		mDocumentInfo->pages[0].end		= mDocumentInfo->pages[0].len;
		
		mDocumentInfo->beginheader   =
		mDocumentInfo->endheader     =
		mDocumentInfo->lenheader     = 0;
		
		mDocumentInfo->begindefaults =
		mDocumentInfo->enddefaults   =
		mDocumentInfo->lendefaults   = 0;
		
		mDocumentInfo->beginprolog   =
		mDocumentInfo->endprolog     =
		mDocumentInfo->lenprolog     = 0;
		
		mDocumentInfo->beginsetup    =
		mDocumentInfo->endsetup      =
		mDocumentInfo->lensetup      = 0;
		
		mDocumentInfo->begintrailer  =
		mDocumentInfo->endtrailer    =
		mDocumentInfo->lentrailer    = 0;
	} else if (mDocumentInfo->numpages == 0) {
		mDocumentInfo->pages = (page *) malloc(sizeof(page));
		memset((void *) mDocumentInfo->pages, 0, sizeof(page));
		
		mHasDSC = false;
		mDocumentInfo->numpages = 1;
		mDocumentInfo->pages[0].len		= fileLength;
		mDocumentInfo->pages[0].begin	= 0;
		mDocumentInfo->pages[0].end		= mDocumentInfo->pages->len;
	}
	
	/* Jeff 18.01.2000 */
	for (int i=1; i < mDocumentInfo->numpages; i++) {
		mDocumentInfo->pages[i-1].len = mDocumentInfo->pages[i].begin -
										mDocumentInfo->pages[i-1].begin;
	}
	/* Jeff 18.01.2000 */
	
	fclose(thePSFile);
		
	return 0;
}



#pragma mark -
#pragma mark === Export ===


// ---------------------------------------------------------------------------
//	€ AskSaveAs
// ---------------------------------------------------------------------------
//	

Boolean
CGSDocument::AskSaveAs(FSSpec &outFSSpec, Boolean inRecordIt)
{
#pragma unused (inRecordIt)
	
	OSErr	err;
	
	// create a NavServices Save dialog
	
	if (UEnvironment::GetOSVersion () < 0x0900)
		err = DoSaveAsDialogWorkaround(outFSSpec);	// nasty 8.5/8.6 bug in NavServices
	else
		err = DoSaveAsDialog(outFSSpec);
	
	if (err == noErr) {
		// save file
		return DoSaveAs(outFSSpec, sSaveDialogFileType);
	} else if (err != userCanceledErr) {
		CUtilities::ShowErrorDialog("\pPutFile failed in AskSaveAs", err);
		return false;
	}
	
	return false;
}



// ---------------------------------------------------------------------------
//	€ DoSaveAsDialog
// ---------------------------------------------------------------------------
//	

OSErr
CGSDocument::DoSaveAsDialog(FSSpec& outFSSpec)
{
	FSSpec			currentFile;
	mFile->GetSpecifier(currentFile);
	
	CNavMenuItemSpecArray	popupItems(13);
	popupItems.SetMenuItem( 0, LStr255(kSTRListSaveDialog, kSaveDialogAIDocument),		kCGSSaveFormatAI);
	popupItems.SetMenuItem( 1, LStr255(kSTRListSaveDialog, kSaveDialogASCIIDocument),	kCGSSaveFormatASCII);
	popupItems.SetMenuItem( 2, LStr255(kSTRListSaveDialog, kSaveDialogBMPDocument),		kCGSSaveFormatBMP);
	popupItems.SetMenuItem( 3, LStr255(kSTRListSaveDialog, kSaveDialogEPSFDocument),	kCGSSaveFormatEPSF);
	popupItems.SetMenuItem( 4, LStr255(kSTRListSaveDialog, kSaveDialogFaxDocument),		kCGSSaveFormatFax);
	popupItems.SetMenuItem( 5, LStr255(kSTRListSaveDialog, kSaveDialogJPEGDocument),	kCGSSaveFormatJPEG);
	popupItems.SetMenuItem( 6, LStr255(kSTRListSaveDialog, kSaveDialogPCXDocument),		kCGSSaveFormatPCX);
	popupItems.SetMenuItem( 7, LStr255(kSTRListSaveDialog, kSaveDialogPNGDocument),		kCGSSaveFormatPNG);
	popupItems.SetMenuItem( 8, LStr255(kSTRListSaveDialog, kSaveDialogPxMDocument),		kCGSSaveFormatPxM);
	popupItems.SetMenuItem( 9, LStr255(kSTRListSaveDialog, kSaveDialogPDFDocument),		kCGSSaveFormatPDF);
	popupItems.SetMenuItem(10, LStr255(kSTRListSaveDialog, kSaveDialogPICTDocument),	kCGSSaveFormatPICT);
	popupItems.SetMenuItem(11, LStr255(kSTRListSaveDialog, kSaveDialogPSDocument),		kCGSSaveFormatPS);
	popupItems.SetMenuItem(12, LStr255(kSTRListSaveDialog, kSaveDialogTIFFDocument),	kCGSSaveFormatTIFF);
	
	CNavCustomDialog	saveDialog;
	NavDialogOptions	options = saveDialog.GetOptions();
	options.preferenceKey = 'save';
	options.popupExtension = popupItems.GetHandle();
	options.dialogOptionFlags = kNavDontAddTranslateItems;
	LString::CopyPStr(currentFile.name , options.savedFileName);
	LString::CopyPStr(LStr255(kSTRListSaveDialog, kSaveDialogTitle), options.windowTitle);
	LString::CopyPStr(LStr255(kSTRListGeneral, kApplicationNameStr), options.clientName);
	LString::CopyPStr(LStr255(kSTRListSaveDialog, kSaveDialogMessageStr), options.message);
	saveDialog.SetOptions(options);
	
	saveDialog.SetFileType('TEXT');
	saveDialog.SetFileCreator(/*kNavGenericSignature*/ '****');
	saveDialog.SetCustomPopupMenuProc(CGSDocument::HandleSaveDialogPopupMenu);
	saveDialog.SetDefaultMenuItem(popupItems.GetMenuItem(sSaveDialogFileType));
	
	// call PutFile to make our Save As dialog appear and work
	return saveDialog.PutFile(outFSSpec);
}



// ---------------------------------------------------------------------------
//	€ DoSaveAs
// ---------------------------------------------------------------------------
//	

Boolean
CGSDocument::DoSaveAs(FSSpec& inFSSpec, OSType inType, bool inThreaded)
{
	SetExportDevice(CGSDocument::CreateDeviceFromSaveFormat(inType));
	
	if (GetExportDevice() && !GetExportDevice()->GetDLL()) {
		delete GetExportDevice();
		SetExportDevice(0);
	}
	
	if (GetExportDevice() && GetExportDevice()->GetDLL()) {
		GetExportDevice()->SetDocument(this);
		GetExportDevice()->SetIsThreadedDevice(inThreaded);
		GetExportDevice()->SetDeviceIsExporting(true);
		GetExportDevice()->SetIsSelfDestruct(true);
		GetExportDevice()->Save(inFSSpec);
		// device will delete itself after rendering/aborting
		// it will also reset the mExportDevice entry
	} else {
		return false;
	}
	
	return true;
}



// ---------------------------------------------------------------------------
//	€ CreateDeviceFromSaveFormat									[static]
// ---------------------------------------------------------------------------
//	

CGSDevice*
CGSDocument::CreateDeviceFromSaveFormat(OSType inCGSSaveFormat)
{
	switch (inCGSSaveFormat) {
		case kCGSSaveFormatAI:		return new CGSAIDevice();
		case kCGSSaveFormatASCII:	return new CGSASCIIDevice();
		case kCGSSaveFormatBMP:		return new CGSBMPDevice();
		case kCGSSaveFormatEPSF:	return new CGSEPSDevice();
		case kCGSSaveFormatFax:		return new CGSFaxDevice();
		case kCGSSaveFormatJPEG:	return new CGSJPEGDevice();
		case kCGSSaveFormatPCX:		return new CGSPCXDevice();
		case kCGSSaveFormatPDF:		return new CGSPDFDevice();
		case kCGSSaveFormatPICT:	CGSMacDevice*	dev = new CGSMacDevice();
									dev->FinishCreateSelf();
									return dev;
		case kCGSSaveFormatPNG:		return new CGSPNGDevice();
		case kCGSSaveFormatPS:		return new CGSPSDevice();
		case kCGSSaveFormatPxM:		return new CGSPxMDevice();
		case kCGSSaveFormatTIFF:	return new CGSTIFFDevice();
		default:
			CUtilities::ShowErrorDialog("\punknown device requested in CreateDeviceFromSaveFormat()", inCGSSaveFormat);
	}
	
	return NULL;
}



// ---------------------------------------------------------------------------
//	€ GetExtensionFromSaveFormat									[static]
// ---------------------------------------------------------------------------
//	

void
CGSDocument::GetExtensionFromSaveFormat(OSType inCGSSaveFormat, LStr255& outExtension)
{
	switch (inCGSSaveFormat) {
		case kCGSSaveFormatAI:		outExtension = "\p.ai";   break;
		case kCGSSaveFormatASCII:	outExtension = "\p.txt";  break;
		case kCGSSaveFormatBMP:		outExtension = "\p.bmp";  break;
		case kCGSSaveFormatEPSF:	outExtension = "\p.epsf"; break;
		case kCGSSaveFormatFax:		outExtension = "\p.fax";  break;
		case kCGSSaveFormatJPEG:	outExtension = "\p.jpg";  break;
		case kCGSSaveFormatPCX:		outExtension = "\p.pcx";  break;
		case kCGSSaveFormatPDF:		outExtension = "\p.pdf";  break;
		case kCGSSaveFormatPICT:	outExtension = "\p.pict"; break;
		case kCGSSaveFormatPNG:		outExtension = "\p.png";  break;
		case kCGSSaveFormatPS:		outExtension = "\p.ps";   break;
		case kCGSSaveFormatPxM:		outExtension = "\p.ppm";  break;
		case kCGSSaveFormatTIFF:	outExtension = "\p.tiff"; break;
		default:
			CUtilities::ShowErrorDialog("\punknown device requested in GetExtensionFromSaveFormat()", inCGSSaveFormat);
	}
}



// ---------------------------------------------------------------------------
//	€ HandleSaveDialogPopupMenu										[static]
// ---------------------------------------------------------------------------
//	

void
CGSDocument::HandleSaveDialogPopupMenu(OSType inCreator, OSType inType, NavCBRecPtr inParams)
{
#pragma unused(inCreator)
	
	const int	maxFileNameLength = 32;
	LStr255		ext, fileName;
	
	// get current filename
	::NavCustomControl(inParams->context, kNavCtlGetEditFileName, fileName);
	sSaveDialogFileType = inType;
	CGSDocument::GetExtensionFromSaveFormat(inType, ext);
	
	// if the name contains a file extension, remove it
	int pos = fileName.ReverseFind("\p.");
	if (pos > 0)
		fileName.Remove(pos, 255);
	
	// if the name is too long for MacOS, shorten it
	if (fileName.Length() > maxFileNameLength - ext.Length())
		fileName.Remove(fileName.Length() - ext.Length(), 255);
	
	// append new file extension to name
	fileName.Append(ext);
	::NavCustomControl(inParams->context, kNavCtlSetEditFileName, fileName);
}



// ---------------------------------------------------------------------------
//	€ WriteEPSPreviewResource
// ---------------------------------------------------------------------------
//	Generates a 72dpi PICT of the EPS and adds it as PICT #256 to the document

void
CGSDocument::WriteEPSPreviewResource()
{
	StCurResFile	saveCurRes;
	
	// generate preview
	CGSMacDevice	preview;
	preview.SetIsThreadedDevice(false);
	preview.SetDrawBuffered(false);
	preview.SetAntiAliasing(false);
	preview.SetResolution(72);
	preview.SetColorDepth(24);
	preview.SetUseExternalFonts(false);
	
	preview.SetDocument(this);
	preview.FinishCreateSelf();
	preview.RenderPage(kSubmitWholeDocument);
	
	// store preview in resource PICT 256
	FSSpec		fileSpec;
	mFile->GetSpecifier(fileSpec);
	preview.SavePictureToResource(&fileSpec, LFileTypeList::GetProcessSignature(), 'EPSF',
								  'PICT', 256, true);
}



#pragma mark -
#pragma mark === SaveAs Dialog Workaround ===



// ---------------------------------------------------------------------------
//	€ DoSaveAsDialogWorkaround
// ---------------------------------------------------------------------------
//	

OSErr
CGSDocument::DoSaveAsDialogWorkaround(FSSpec& outFSSpec)
{
	FSSpec			currentFile;
	mFile->GetSpecifier(currentFile);
	
	CNavCustomDialog	saveDialog;
	NavDialogOptions	options = saveDialog.GetOptions();
	options.preferenceKey = 'save';
	options.dialogOptionFlags = kNavNoTypePopup;
	LString::CopyPStr(currentFile.name , options.savedFileName);
	LString::CopyPStr(LStr255(kSTRListSaveDialog, kSaveDialogTitle), options.windowTitle);
	LString::CopyPStr(LStr255(kSTRListGeneral, kApplicationNameStr), options.clientName);
	LString::CopyPStr(LStr255(kSTRListSaveDialog, kSaveDialogMessageStr), options.message);
	saveDialog.SetOptions(options);
	
	// we use the same DITL as in the Convert dialog
	saveDialog.SetCustomControls(kSaveAsDialogDITL, 400, 40);
	saveDialog.SetSetupCustomControlsProc(CGSDocument::SaveAsWorkaroundSetupCustomControls);
	saveDialog.SetAcceptCustomControlsProc(CGSDocument::SaveAsWorkaroundAcceptCustomControls);
	saveDialog.SetCustomProcessEventProc(CGSDocument::SaveAsWorkaroundCustomProcessEvent);
	
	saveDialog.SetFileType('TEXT');
	saveDialog.SetFileCreator(LFileTypeList::GetProcessSignature());
	
	// call PutFile to make our Save As dialog appear and work
	return saveDialog.PutFile(outFSSpec);
}



// ---------------------------------------------------------------------------
//	€ SaveAsWorkaroundSetupCustomControls							[static]
// ---------------------------------------------------------------------------

void
CGSDocument::SaveAsWorkaroundSetupCustomControls(NavCBRecPtr callBackParams, short inFirstItem)
{
	ControlHandle	itemHandle;
	short			itemType;
	Rect			itemRect;
	
	::GetDialogItem((DialogPtr) callBackParams->window, inFirstItem+kSaveAsDialogFormatPopup,
					&itemType, &((Handle) itemHandle), &itemRect);
	
	::SetControlValue(itemHandle, sSaveDialogFileTypeNr);
	
	SaveAsWorkaroundAcceptCustomControls(callBackParams, inFirstItem);
	HandleSaveDialogPopupMenu(0, sSaveDialogFileType, callBackParams);
}



// ---------------------------------------------------------------------------
//	€ SaveAsWorkaroundCustomProcessEvents							[static]
// ---------------------------------------------------------------------------

void
CGSDocument::SaveAsWorkaroundCustomProcessEvent(NavCBRecPtr callBackParams, short inFirstItem)
{
	ControlHandle	itemHandle;
	short			itemType;
	Rect			itemRect;
	
	::GetDialogItem((DialogPtr) callBackParams->window, inFirstItem+kConvertDialogFormatPopup,
					&itemType, &((Handle) itemHandle), &itemRect);
	
	SInt32	menuItem = ::GetControlValue(itemHandle);
	
	if (menuItem != sSaveDialogFileTypeNr) {
		SaveAsWorkaroundAcceptCustomControls(callBackParams, inFirstItem);
		HandleSaveDialogPopupMenu(0, sSaveDialogFileType, callBackParams);
	}
}



// ---------------------------------------------------------------------------
//	€ SaveAsWorkaroundAcceptCustomControls							[static]
// ---------------------------------------------------------------------------

void
CGSDocument::SaveAsWorkaroundAcceptCustomControls(NavCBRecPtr callBackParams, short inFirstItem)
{
	ControlHandle	itemHandle;
	short			itemType;
	Rect			itemRect;
	
	::GetDialogItem((DialogPtr) callBackParams->window, inFirstItem+kConvertDialogFormatPopup,
					&itemType, &((Handle) itemHandle), &itemRect);
	
	sSaveDialogFileTypeNr = ::GetControlValue(itemHandle);
	switch (sSaveDialogFileTypeNr) {
		case 1:		sSaveDialogFileType = kCGSSaveFormatAI;		break;
		case 2:		sSaveDialogFileType = kCGSSaveFormatASCII;	break;
		case 3:		sSaveDialogFileType = kCGSSaveFormatBMP;	break;
		case 4:		sSaveDialogFileType = kCGSSaveFormatEPSF;	break;
		case 5:		sSaveDialogFileType = kCGSSaveFormatFax;	break;
		case 6:		sSaveDialogFileType = kCGSSaveFormatJPEG;	break;
		case 7:		sSaveDialogFileType = kCGSSaveFormatPxM;	break;
		case 8:		sSaveDialogFileType = kCGSSaveFormatPCX;	break;
		case 9:		sSaveDialogFileType = kCGSSaveFormatPDF;	break;
		case 10:	sSaveDialogFileType = kCGSSaveFormatPICT;	break;
		case 11:	sSaveDialogFileType = kCGSSaveFormatPNG;	break;
		case 12:	sSaveDialogFileType = kCGSSaveFormatPS;		break;
		case 13:	sSaveDialogFileType = kCGSSaveFormatTIFF;	break;
	}
}



#pragma mark -
#pragma mark === Printing ===



// ---------------------------------------------------------------------------
//	€ SetupPage
// ---------------------------------------------------------------------------
//	Handle the Page Setup command

void
CGSDocument::SetupPage()
{
	StDesktopDeactivator	deactivator;
	OSErr					err;
	
	::PrOpen();
	err = ::PrError();
	if (err == noErr) {
		if (!mPrintRecordH) {
			mPrintRecordH = (THPrint) ::NewHandle(sizeof(TPrint));
			ThrowIfMemFail_(mPrintRecordH);
			memcpy(*mPrintRecordH,
				   *CGSApplication::GetPreferences()->GetDefaultPrintRecord(),
				   sizeof(TPrint));
		}
		::PrStlDialog(mPrintRecordH);
		::PrClose();
	} else {
		CUtilities::ShowErrorDialog("\pPrOpen failed in SetupPage", err);
	}
}



// ---------------------------------------------------------------------------
//	€ DoPrint
// ---------------------------------------------------------------------------
//	This controls every kind of printing

void
CGSDocument::DoPrint()
{
	CGSMacDevice	printDevice(this);
	if (printDevice.GetDLL() == 0) {
		return;
	}
	printDevice.SetIsThreadedDevice(false);
	printDevice.SetDrawBuffered(true);
	printDevice.FinishCreateSelf();
	
	
	TPrStatus	tprStatus;
	GrafPtr		oldPort;
	
	::GetPort(&oldPort);
	
	::PrOpen();
	if (::PrError() == noErr) {
		if (!mPrintRecordH) {	// get applications default print record
			mPrintRecordH = (THPrint) ::NewHandle(sizeof(TPrint));
			ThrowIfMemFail_(mPrintRecordH);
			memcpy(*mPrintRecordH,
				   *CGSApplication::GetPreferences()->GetDefaultPrintRecord(),
				   sizeof(TPrint));
		}
		::PrValidate(mPrintRecordH);
		
		TGetRslBlk		resGetInf;
		resGetInf.iOpCode = getRslDataOp;
		::PrGeneral((Ptr)(&resGetInf));
		
		if (resGetInf.iError == noErr) {
			short		maxXRes=0, maxYRes=0;
			for (short i=0; i<resGetInf.iRslRecCnt; i++) {	// find greatest resolution
				if (resGetInf.rgRslRec[i].iXRsl > maxXRes ||
						resGetInf.rgRslRec[i].iYRsl > maxYRes) {
					maxXRes = resGetInf.rgRslRec[i].iXRsl;
					maxYRes = resGetInf.rgRslRec[i].iYRsl;
				}
			}
			
			TSetRslBlk	resSetInf;
			resSetInf.iOpCode	= setRslOp;
			resSetInf.iXRsl		= maxXRes;
			resSetInf.iYRsl		= maxYRes;
			resSetInf.hPrint	= mPrintRecordH;
			::PrGeneral((Ptr) &resSetInf);
		}
		
		// Print Loop (from Apple's technote 1092 (a print loop that cares)
		
		sPrintDialog = ::PrJobInit(mPrintRecordH);
		if (::PrError() == noErr &&
				::PrDlgMain(mPrintRecordH, NewPDlgInitProc(CGSDocument::ExtendPrintDialog))) {
			TPrPort		*printerPortPtr;
			short		numberOfPages	= GetNumberOfPages(),
						numberOfCopies	= (*mPrintRecordH)->prJob.iCopies,
						firstPage		= (*mPrintRecordH)->prJob.iFstPage,
						lastPage		= (*mPrintRecordH)->prJob.iLstPage;
			
			if (numberOfPages == 0 && mDocumentInfo->epsf)
				numberOfPages = 1;
			
			(*mPrintRecordH)->prJob.iFstPage = iPrPgFst;
			(*mPrintRecordH)->prJob.iLstPage = iPrPgMax;
			
			if(numberOfPages < lastPage)
				lastPage = numberOfPages;
			
			// setup print device
			
			PrintPrefs		*printPrefs = CGSApplication::GetPreferences()->GetPrintSettings();
			
			printDevice.SetUseExternalFonts(printPrefs->renderSettings.useExternalFonts);
			
			if (printPrefs->useHalftoneSettings) {
				printDevice.SetUseHalftoneScreen(true);
				printDevice.SetHalftoneScreen(printPrefs->halftoneSettings.frequency,
											  printPrefs->halftoneSettings.angle,
											  printPrefs->halftoneSettings.spotType);
			} else {
				printDevice.SetUseHalftoneScreen(false);
			}
			
			float	pageSizeX, pageSizeY;
			pageSizeX	 = - (*mPrintRecordH)->rPaper.left;
			pageSizeX	+= (*mPrintRecordH)->rPaper.right;
			pageSizeX	*= 72.0 / (*mPrintRecordH)->prInfo.iHRes;
			pageSizeY	 = - (*mPrintRecordH)->rPaper.top;
			pageSizeY	+= (*mPrintRecordH)->rPaper.bottom;
			pageSizeY	*= 72.0 / (*mPrintRecordH)->prInfo.iVRes;
			printDevice.SetPageSize(pageSizeX, pageSizeY);
			
			if (printPrefs->useRenderSettings) {
				printDevice.SetResolution(printPrefs->renderSettings.resolution);
			} else {
				if ((*mPrintRecordH)->prInfoPT.iHRes > (*mPrintRecordH)->prInfo.iHRes) {
					// this is a hack, as the powerprint driver seems to fill in the wrong
					// field in the structure. In Carbon there is no accessor to prInfoPT!!!
					printDevice.SetResolution((*mPrintRecordH)->prInfoPT.iHRes,
											  (*mPrintRecordH)->prInfoPT.iVRes);
				} else {
					printDevice.SetResolution((*mPrintRecordH)->prInfo.iHRes,
											  (*mPrintRecordH)->prInfo.iVRes);
				}
			}
			
			if (!HasDSC()) {
				for (short copy=1; copy<=numberOfCopies; copy++) {	// copy loop
					printDevice.PrintNonDSCDocument(mPrintRecordH);
				}
			} else {
				for (short copy=1; copy<=numberOfCopies; copy++) {	// copy loop
					printerPortPtr = ::PrOpenDoc(mPrintRecordH, NULL, NULL);
					
					if (printPrefs->useRenderSettings) {
						switch (printPrefs->renderSettings.colorDepth) {
							case kBlackWhite:		printDevice.SetColorDepth(1);	break;
							case k16Grays:			printDevice.SetColorDepth(4);	break;
							case k256Grays:			printDevice.SetColorDepth(7);	break;
							case k256Colors:		printDevice.SetColorDepth(8);	break;
							case kMillionColors:	printDevice.SetColorDepth(24);	break;
						}
					} else {
						printDevice.SetColorDepth(((printerPortPtr->gPort).portBits.rowBytes < 0) ? 24 : 1);
					}
					
					if (::PrError() == noErr) {
						short	pageNumber = firstPage;
						while (pageNumber <= lastPage && ::PrError() == noErr) {	// page loop
							if ((pageNumber-firstPage) % iPFMaxPgs == 0) {
								if (pageNumber != firstPage) {
									::PrCloseDoc(printerPortPtr);
									if (((*mPrintRecordH)->prJob.bJDocLoop == bSpoolLoop) &&
										(::PrError() == noErr))
										::PrPicFile(mPrintRecordH, NULL, NULL, NULL, &tprStatus);
								}
							}
							
							printDevice.RenderPage(pageNumber);
							
							::PrOpenPage(printerPortPtr, NULL);
							if(::PrError() == noErr)
								printDevice.DrawToRect(&((*mPrintRecordH)->rPaper));
							::PrClosePage(printerPortPtr);
							pageNumber++;
						}
					}
					::PrCloseDoc(printerPortPtr);
				}
			}
		} else ::PrSetError(iPrAbort);
	}
	
	if (((*mPrintRecordH)->prJob.bJDocLoop == bSpoolLoop) && (::PrError() == noErr))
		::PrPicFile(mPrintRecordH, NULL, NULL, NULL, &tprStatus);
	
	OSErr	printErr = ::PrError();
	::PrClose();
	
	
	if (printErr != noErr && printErr != iPrAbort) {
		CUtilities::ShowErrorDialog("\pDoPrint failed", printErr);
	}
	
	::SetPort(oldPort);
}



// ---------------------------------------------------------------------------
//	€ ExtendPrintDialog												[static]
// ---------------------------------------------------------------------------
//	Modifies the standard print dialog

pascal TPPrDlg
CGSDocument::ExtendPrintDialog(THPrint* printHandle)
{
#pragma unused(printHandle)
	
	sPrintDialogFirstItem = ::CountDITL((DialogPtr) &(sPrintDialog->Dlg));
	::AppendDialogItemList((DialogPtr) sPrintDialog, kPrintDialogDITL, appendDITLBottom);
	
	// change the event handler to our's
	sPrintItemProc = sPrintDialog->pItemProc;
	sPrintFltrProc = sPrintDialog->pFltrProc;
	sPrintDialog->pItemProc = NewPItemProc(CGSDocument::HandlePrintDialog);
	sPrintDialog->pFltrProc = NewModalFilterProc(CGSDocument::PrintDialogEventFilter);
	// !!!!! Do we have to call DisposeRoutinProc for any of these??
	
	// set controls according to the preferences
	PrintPrefs		*prefs = CGSApplication::GetPreferences()->GetPrintSettings();
	ControlHandle	itemHandle;
	short			itemType;
	Rect			itemRect;
	
	// Render group
	::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgRenderGroupCheckBox, &itemType, &((Handle) itemHandle), &itemRect);
	::SetControlValue(itemHandle, prefs->useRenderSettings);
	
	if (prefs->renderSettings.resolution > 0) {
		::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgRenderResEditText, &itemType, &((Handle) itemHandle), &itemRect);
		::SetDialogItemText((Handle) itemHandle, LStr255(prefs->renderSettings.resolution));
	}
	
	::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgRenderColPopup, &itemType, &((Handle) itemHandle), &itemRect);
	::SetControlValue(itemHandle, prefs->renderSettings.colorDepth);
	
	// Font group
	::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgFontUseTTCheckBox, &itemType, &((Handle) itemHandle), &itemRect);
	::SetControlValue(itemHandle, prefs->renderSettings.useExternalFonts);
	
	// Halftone group
	::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgHalftoneGroupCheckBox, &itemType, &((Handle) itemHandle), &itemRect);
	::SetControlValue(itemHandle, prefs->useHalftoneSettings);
	
	::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgHalftoneSpotPopup, &itemType, &((Handle) itemHandle), &itemRect);
	::SetControlValue(itemHandle, prefs->halftoneSettings.spotType);
	
	if (prefs->halftoneSettings.frequency > 0) {
		::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgHalftoneFreqEditText, &itemType, &((Handle) itemHandle), &itemRect);
		::SetDialogItemText((Handle) itemHandle, LStr255(prefs->halftoneSettings.frequency));
	}
	
	if (prefs->halftoneSettings.angle >= 0) {
		::GetDialogItem((DialogPtr) sPrintDialog, sPrintDialogFirstItem+kPrDlgHalftoneAngleEditText, &itemType, &((Handle) itemHandle), &itemRect);
		::SetDialogItemText((Handle) itemHandle, LStr255(prefs->halftoneSettings.angle));
	}
	
	return sPrintDialog;
}



// ---------------------------------------------------------------------------
//	€ HandlePrintDialog												[static]
// ---------------------------------------------------------------------------
//	Handles events in the modified print dialog

pascal void
CGSDocument::HandlePrintDialog(TPPrDlg dialog, short item)
{
	ControlHandle	itemHandle;
	short			itemType;
	Rect			itemRect;
	short			value;
	long			lvalue;
	Str255			str;
	
	short	myItem = item - sPrintDialogFirstItem;
	
	if (myItem < 1) {
		CallPItemProc(sPrintItemProc,(DialogPtr) dialog, item);
		if (dialog->fDone && dialog->fDoIt) {
			// dialog with OK accepted, set preferences
			PrintPrefs		*prefs = CGSApplication::GetPreferences()->GetPrintSettings();
			
			// Render group
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgRenderGroupCheckBox, &itemType, &((Handle) itemHandle), &itemRect);
			prefs->useRenderSettings = ::GetControlValue(itemHandle);
			
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgRenderResEditText, &itemType, &((Handle) itemHandle), &itemRect);
			::GetDialogItemText((Handle) itemHandle, str);
			::StringToNum(str, &lvalue);
			prefs->renderSettings.resolution = lvalue;
			
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgRenderColPopup, &itemType, &((Handle) itemHandle), &itemRect);
			prefs->renderSettings.colorDepth = (ColorDepth) ::GetControlValue(itemHandle);
			
			// Font group
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgFontUseTTCheckBox, &itemType, &((Handle) itemHandle), &itemRect);
			prefs->renderSettings.useExternalFonts = ::GetControlValue(itemHandle);
			
			// Halftone group
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgHalftoneGroupCheckBox, &itemType, &((Handle) itemHandle), &itemRect);
			prefs->useHalftoneSettings = ::GetControlValue(itemHandle);
			
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgHalftoneSpotPopup, &itemType, &((Handle) itemHandle), &itemRect);
			prefs->halftoneSettings.spotType = (SpotType) ::GetControlValue(itemHandle);
			
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgHalftoneFreqEditText, &itemType, &((Handle) itemHandle), &itemRect);
			::GetDialogItemText((Handle) itemHandle, str);
			if (str[0] > 0) {
				::StringToNum(str, &lvalue);
				prefs->halftoneSettings.frequency = lvalue;
			} else {
				prefs->halftoneSettings.frequency = -1;
			}
			
			::GetDialogItem((DialogPtr) dialog, sPrintDialogFirstItem+kPrDlgHalftoneAngleEditText, &itemType, &((Handle) itemHandle), &itemRect);
			::GetDialogItemText((Handle) itemHandle, str);
			if (str[0] > 0) {
				::StringToNum(str, &lvalue);
				prefs->halftoneSettings.angle = lvalue;
			} else {
				prefs->halftoneSettings.angle = -1;
			}
		}
	} else {
		::GetDialogItem((DialogPtr) dialog, item, &itemType, &((Handle) itemHandle), &itemRect);
		switch (myItem) {
			case kPrDlgRenderGroupCheckBox:
				value = ::GetControlValue(itemHandle);
				::SetControlValue(itemHandle, value != 1);
				break;
				
			case kPrDlgFontUseTTCheckBox:
				value = ::GetControlValue(itemHandle);
				::SetControlValue(itemHandle, value != 1);
				break;
				
			case kPrDlgHalftoneGroupCheckBox:
				value = ::GetControlValue(itemHandle);
				::SetControlValue(itemHandle, value != 1);
				break;
		}
	}
}



// ---------------------------------------------------------------------------
//	€ PrintDialogEventFilter										[static]
// ---------------------------------------------------------------------------
//	Event filter proc for the modified print dialog

pascal Boolean
CGSDocument::PrintDialogEventFilter(DialogPtr dialog, EventRecord *event,
									DialogItemIndex *itemHit)
{
	SInt16	activeEditField = ((DialogPeek) dialog)->editField - sPrintDialogFirstItem;
	if (activeEditField > 0) {	// is it one of our edit fields?
		if ((event->what == keyDown) || (event->what == autoKey)) {
			char	theKey = event->message & charCodeMask;
			
			// if it's not a number or a control code, reject it
			if ((theKey >= 0x20 && theKey < 0x30) || (theKey > 0x39)) {
				::SysBeep(1);
				return true;
			}
		}
	}
	
	// no event for us
	return CallModalFilterProc(sPrintFltrProc, dialog, event, itemHit);
}



#pragma mark -
#pragma mark === DocumentInfo Accessors ===



// ---------------------------------------------------------------------------
//	€ GetFilePath
// ---------------------------------------------------------------------------
//	Returns a string with the path to the document.

void
CGSDocument::GetFilePath(char *outPath)
{
	FSSpec		theFSSpec;
	
	mFile->GetSpecifier(theFSSpec);
	CUtilities::ConvertFSSpecToPath(&theFSSpec, outPath, 255);
}



// ---------------------------------------------------------------------------
//	€ GetNumberOfPages
// ---------------------------------------------------------------------------
//	Returns total number of pages in the PS-file. Returns 1 if page number 
//	cannot be determined.

int
CGSDocument::GetNumberOfPages()
{
	if (mDocumentInfo)
		return mDocumentInfo->numpages;
	else
		return 1;
}



// ---------------------------------------------------------------------------
//	€ GetCurrentPage
// ---------------------------------------------------------------------------
//	Returns total number of pages in the PS-file. Returns 1 if page number 
//	cannot be determined.

SInt32
CGSDocument::GetCurrentPage()
{
	if (mWindow)
		return ((CGSDocumentWindow*)mWindow)->GetCurrentPage();
	else
		return 0;
}



// ---------------------------------------------------------------------------
//	€ GetPageSize
// ---------------------------------------------------------------------------
//	Returns page size for the page pageNumber.

void
CGSDocument::GetPageSize(SInt32 pageNum, float& xSize, float& ySize)
{
	bool	rotateBBox = false;
	// Assume no DocumentInfo, or page has no special/default media
	// Use default size
	CGSApplication::GetPreferences()->GetDefaultPageSize(xSize, ySize);
	
	if (CGSApplication::GetPreferences()->GetUseAlwaysDefaultPageSize())
		return;
	
	if (!mDocumentInfo) return;
	
	if (IsEPS()) {
		if (mDocumentInfo->boundingbox) {
			float	bboxWidth, bboxHeight;
			bboxWidth = mDocumentInfo->boundingbox[URX] - mDocumentInfo->boundingbox[LLX];
			bboxHeight = mDocumentInfo->boundingbox[URY] - mDocumentInfo->boundingbox[LLY];
			
			if (bboxWidth && bboxHeight) {
				xSize = bboxWidth;
				ySize = bboxHeight;
				rotateBBox = true;
				goto rotate;
			}
		}
	}
	
	if (pageNum<1 || pageNum>GetNumberOfPages()) return;
	
	if (mDocumentInfo->pages && mDocumentInfo->pages[pageNum-1].media) {
		// page has special media, so use it
		xSize = mDocumentInfo->pages[pageNum-1].media->width;
		ySize = mDocumentInfo->pages[pageNum-1].media->height;
		rotateBBox = true;
	} else if (mDocumentInfo->default_page_media) {
		// page has no special media but default, use it
		xSize = mDocumentInfo->default_page_media->width;
		ySize = mDocumentInfo->default_page_media->height;
		rotateBBox = false;
/*	} else if (mDocumentInfo->pages && mDocumentInfo->pages[pageNum-1].boundingbox) {
		// page has no media info, use page bbox
		xSize = mDocumentInfo->pages[pageNum-1].boundingbox[URX] -
				mDocumentInfo->pages[pageNum-1].boundingbox[LLX];
		ySize = mDocumentInfo->pages[pageNum-1].boundingbox[URY] -
				mDocumentInfo->pages[pageNum-1].boundingbox[LLY];
		rotateBBox = false;*/
	}


rotate:	
	// modify page size according to orientation
	if (rotateBBox) {
		switch (GetPageOrientation(pageNum)) {
			float	buf;
			case SEASCAPE:
			case LANDSCAPE:
				buf = xSize;
				xSize = ySize;
				ySize = buf;
				break;
		}
	}
}



// ---------------------------------------------------------------------------
//	€ GetPageOrientation
// ---------------------------------------------------------------------------

int
CGSDocument::GetPageOrientation(SInt32 pageNum)
{
	if (pageNum<1 || pageNum>GetNumberOfPages()) return PORTRAIT;
	if (!mDocumentInfo) return PORTRAIT;
	
	if (mDocumentInfo->pages && mDocumentInfo->pages[pageNum-1].orientation != NONE)
		return mDocumentInfo->pages[pageNum-1].orientation;
	else
		return mDocumentInfo->orientation;
}



// ---------------------------------------------------------------------------
//	€ GetToolbar
// ---------------------------------------------------------------------------

CGSToolbar*
CGSDocument::GetToolbar()
{
	if (mWindow)
		return ((CGSDocumentWindow*) mWindow)->GetToolbar();
	else
		return 0;
}



