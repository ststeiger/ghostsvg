/* ============================================================================	*/
/*	CGSApplication.cp						written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	This class is an instance of LDocApplictaion.								*/
/*	Here are all important methods for the application object.					*/
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


// CGSApplication includes
#include "MacGSViewConstants.h"
#include "CGSApplication.h"
#include "CGSSharedLib.h"
#include "CBroadcastEditText.h"
#include "CGSDocument.h"
#include "CGSDocumentWindow.h"
#include "CGSToolbar.h"
#include "CNavCustomDialog.h"

#include "CGSDevice.h"
#include "CGSBMPDevice.h"

#include "CWindowMenu.h"
#include "ABalloon.h"

#include <Printing.h>
#include <Errors.h>

// PP includes
#include <PP_DebugMacros.h>

#include <LFileStream.h>

#include <LThread.h>
#include <UThread.h>

#include <LStaticText.h>
#include <LProgressBar.h>
#include <LPicture.h>
#include <LGrowZone.h>
#include <PP_Messages.h>
#include <PP_Resources.h>
#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>
#include <UEnvironment.h>
#include <UAttachments.h>
#include <UPrintingMgr.h>
#include <LDocument.h>
#include <LMenuBar.h>
#include <LModelDirector.h>
#include <UScreenPort.h>
#include <LComparator.h>
#include <LCMAttachment.h>
#include <UCMMUtils.h>
#include <LUndoer.h>
#include <LMenu.h>
#include <UDesktop.h>
#include <LDialogBox.h>
#include <UKeyFilters.h>
#include <LTabGroup.h>
#include <LRadioGroup.h>

#include <UStandardDialogs.h>
#if PP_StdDialogs_Option == PP_StdDialogs_Conditional
	#include <UConditionalDialogs.h>
#endif

#include <UControlRegistry.h>

#include <LDebugStream.h>

#include <UDebugNew.h>
#include <UDebugUtils.h>
#include <UHeapUtils.h>
#include <UMemoryEater.h>
#include <UOnyx.h>
#include <UProcess.h>
#include <UVolume.h>

#if PP_Debug
	#include <LSIOUXAttachment.h>
	#include <LDebugMenuAttachment.h>
	#include <LCommanderTree.h>
	#include <LPaneTree.h>
	#include <LTreeWindow.h>
#endif

#include <LWindow.h>
#include <LPrintout.h>
#include <LPlaceHolder.h>

#include <Appearance.h>

#if PP_Debug
	#include "MetroNubUtils.h"
#endif

#if __option (profile)
	#include <profiler.h>
#endif


#include <LDialogBox.h>
#include <LOffscreenView.h>
#include <UModalDialogs.h>




// ---------------------------------------------------------------------------
//	Prototypes

void	AppMain();		// main loop
#if PP_Debug
	void	PP_NewHandler() throw (PP_STD::bad_alloc);	// defined in PPDebug_New.cp
#endif

const CommandT	cmd_DoTest						= FOUR_CHAR_CODE('teSt');


// Init static members
CGSApplication*		CGSApplication::sAppInstance = 0;
OSType				CGSApplication::sOpenDialogFileType = 0;
OSType				CGSApplication::sConvertDialogFileType = kCGSSaveFormatPDF;
SInt32				CGSApplication::sConvertDialogFileTypeNr = 9;		// PDF

// ===========================================================================
//	€ main
// ===========================================================================

int main()
{
#if PP_Debug
		// First things first. Install a new_handler.
	(void)PP_STD::set_new_handler(PP_NewHandler);
#endif
	
		// Don't let exceptions propagate outside of main
	try {
		AppMain();
	} catch (...) {
		SignalStringLiteral_("Exception caught in main");
	}
	
	return 0;
}


// ---------------------------------------------------------------------------
//	€ AppMain
// ---------------------------------------------------------------------------

void AppMain()
{
#if PP_Debug

		// If under the debugger, use the debugger for everything
		// (throw, signal, DebugNew), else use alerts.
	
	if (AmIBeingMWDebugged()) {

		UDebugging::SetDebugThrow(debugAction_Debugger);
		UDebugging::SetDebugSignal(debugAction_Debugger);

	} else {

			// Not under the MW Debug, so use alerts. If you use
			// another debugger, you could set this up the same way. Point
			// is to use the debugger's facilities when under the debugger,
			// else alerts.
		
		UDebugging::SetDebugThrow(debugAction_Alert);
		UDebugging::SetDebugSignal(debugAction_Alert);

			// Use our own error handler for DebugNew which uses alerts
			// instead of DebugStr calls.
	
		PP_DebugNewInstallPPErrorHandler_();
	}

#else
		// In final build mode so nothing should be seen. These are
		// commented out because gDebugThrow and gDebugSignal are
		// initialized to debugAction_Nothing -- assignment here is
		// unnecessary (but left in as a comment for illustration).
	
	UDebugging::SetDebugThrow(debugAction_Nothing);
	UDebugging::SetDebugSignal(debugAction_Nothing);
	
#endif	

		// Clean up any "leaks" that might have occured at static
		// initialization time.
	{
		SLResetLeaks_();
		DebugNewForgetLeaks_();
	}


		// Normal initializations
	InitializeHeap(5);
	UQDGlobals::InitializeToolbox(&qd);
	::InitCursor();
	::FlushEvents(everyEvent, nil);
	
	
		// Check Debugging environment
#if PP_Debug
	UDebugUtils::CheckEnvironment();
#endif
	
		// Install a GrowZone to catch low-memory situations	
	LGrowZone* theGZ = new LGrowZone(20000);
	ValidateObject_(theGZ);
	SignalIf_(theGZ->MemoryIsLow());

		// Create the application object and run. The scope of
		// the object is limited as we need to control when
		// the object's destructor is called (in relation to
		// the code that follows it).
	{
		CGSApplication	theApp;
		theApp.Run();
	}
		
#if PP_Debug

		// This cleanup isn't necessary (they are items that are to
		// remain for the duration of the application's run time. When
		// the app quits and the Process Manager reclaims the heap,
		// the memory occupied by these items will be released). This
		// is just done to keep things like DebugNew and Spotlight
		// quiet.
	
	LMenuBar*	theBar = LMenuBar::GetCurrentMenuBar();
	delete theBar;

	URegistrar::DisposeClassTable();

	LPeriodical::DeleteIdlerAndRepeaterQueues();

	UMemoryEater::DeleteMemoryLists();

	LModelDirector*	theDirector = LModelDirector::GetModelDirector();
	delete theDirector;

	LModelObject::DestroyLazyList();

	UScreenPort::Dispose();
	
	LComparator*	theCompare = LComparator::GetComparator();
	delete theCompare;
	
	LLongComparator*	theLongCompare = LLongComparator::GetComparator();
	delete theLongCompare;

	DisposeOf_(theGZ);
	
#endif

	DebugNewReportLeaks_();
}



// ---------------------------------------------------------------------------
//	€ CGSApplication
// ---------------------------------------------------------------------------
//	Application object constructor

CGSApplication::CGSApplication()
{
	sAppInstance = this;
	
	CheckEnvironment();
	
		// Register debugging classes
#if PP_Debug
	RegisterClass_(LCommanderTree);
	RegisterClass_(LPaneTree);
	RegisterClass_(LTreeWindow);
#endif
	
	// Register PowerPlant classes used in the PPob(s)
	RegisterClass_(LTabGroup);
	RegisterClass_(LRadioGroup);
	RegisterClass_(LWindow);
	RegisterClass_(LPrintout);
	RegisterClass_(LPlaceHolder);
	RegisterClass_(LView);
	RegisterClass_(LOffscreenView);
	RegisterClass_(LPicture);
	RegisterClass_(LDialogBox);
	RegisterClass_(LWindowThemeAttachment);
	
	// Register the Appearance Manager/GA classes
	UControlRegistry::RegisterClasses();
	
	// Register Custom Classes
	RegisterClass_(ABalloon);
	RegisterClass_(CGSDocumentWindow);
	RegisterClass_(CGSToolbar);
	RegisterClass_(CGSMacDevice);
	RegisterClass_(CBroadcastEditText);
	
	// Add SIOUX Attachment - will only be needed while testing
#if PP_Debug
	AddAttachment(new LSIOUXAttachment);
#endif
	
	// get main thread
	new UMainThread;
	AddAttachment(new LYieldAttachment(-1));
	
	// preload NavServices
	::NavLoad();
	
	// set foreground and background sleep time
	mForegroundSleep = 6;
	mBackgroundSleep = 6;
	
/***/	
		SLResetLeaks_();
		DebugNewForgetLeaks_();
/***/
	
	// Start Profiling
#if __option (profile)
//	ProfilerInit(collectDetailed, bestTimeBase, 200, 100);
//	ProfilerSetStatus(false);	// no profiling yet
#endif
}


// ---------------------------------------------------------------------------
//	€ ~CGSApplication
// ---------------------------------------------------------------------------
//	Application object destructor

CGSApplication::~CGSApplication()
{
	// Stop Profiling
#if __option (profile)
//	ProfilerDump("\pMacGSView Profile");
//	ProfilerTerm();
#endif
	
	::NavUnload();
}


// ---------------------------------------------------------------------------------
//	€ CheckEnvironment
// ---------------------------------------------------------------------------------
//	Check for required system software components. Returns true if somethinis missing.

void
CGSApplication::CheckEnvironment()
{
	bool	haveToExit = false;
	
	UEnvironment::InitEnvironment();
	
	// Check for MacOS 8.1 or later
	if (UEnvironment::GetOSVersion () < 0x0810) haveToExit = true;
	// Check for Thread Manager
	if (!UEnvironment::HasFeature(env_HasThreadManager)) haveToExit = true;
	// Check for Appearance Manager 1.0.1 or later
	if (!UEnvironment::HasFeature(env_HasAppearance)) haveToExit = true;
	if (UEnvironment::GetAppearanceVersion () < 0x0101) haveToExit = true;
	// Check for CodeFragmentManager
	if (!UEnvironment::HasGestaltAttribute(gestaltCFMAttr, gestaltCFMPresent)) haveToExit = true;
	// Chek for Navigation Service
	if (!::NavServicesAvailable()) haveToExit = true;
	
	if (haveToExit) {
		// Normal initializations
		InitializeHeap(5);
		UQDGlobals::InitializeToolbox(&qd);
		::InitCursor();
		::FlushEvents(everyEvent, nil);
		
		::StopAlert(kSystemTooOldAlert, nil);
		
		::ExitToShell();
	}
}



// ---------------------------------------------------------------------------
//		€ Initialize
// ---------------------------------------------------------------------------

void
CGSApplication::Initialize()
{
	// Install the Window Menu
	CWindowMenu		*windowMenu = new CWindowMenu(MENU_Window);
	AddAttachment(windowMenu);
	
	// set default sleep time, while active
	SetSleepTime(mForegroundSleep);
}


// ---------------------------------------------------------------------------
//	€ StartUp
// ---------------------------------------------------------------------------
//	Perform an action in response to the Open Application AppleEvent.

void
CGSApplication::StartUp()
{
}



// ---------------------------------------------------------------------------
//	€ ShowAboutBox
// ---------------------------------------------------------------------------
//	Display the About Box for the Application

void
CGSApplication::ShowAboutBox()
{
	StDialogHandler	theHandler(PPob_AboutBoxDialog, this);
	LWindow*		theDialog = theHandler.GetDialog();
	
	// enter ghostscript version
	CGSSharedLib	lib;
	LStaticText		*versText = dynamic_cast<LStaticText*> (theDialog->FindPaneByID(kAboutBoxGSVersion));
	LStr255			versStr;
	long			majorVersion = lib.revision/100,
					minorVersion = lib.revision - majorVersion*100;
	short			year = lib.revisiondate/10000,
					month = (lib.revisiondate - year*10000) / 100,
					day = lib.revisiondate - year*10000 - month*100;
	
	DateTimeRec		revisionDate = {year, month, day, 0, 0, 0, 0};
	unsigned long	revisionDateSeconds;
	Str255			revisionDateStr;
	
	::DateToSeconds(&revisionDate, &revisionDateSeconds);
	DateString(revisionDateSeconds, shortDate, revisionDateStr, NULL);
	
	versStr  = lib.product;
	versStr += " Version ";
	versStr += majorVersion;
	versStr += ".";
	if (minorVersion < 10)	versStr += "0";
	versStr += minorVersion;
	versStr += " (";
	versStr += revisionDateStr;
	versStr += ")\r";
	versStr += lib.copyright;
	
	versText->SetText(versStr);
	
	theDialog->Show();
	
	while (true) {
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
			break;
	}
}



// ---------------------------------------------------------------------------
//	€ ObeyCommand
// ---------------------------------------------------------------------------
//	Respond to Commands. Returns true if the Command was handled, false if not.

Boolean
CGSApplication::ObeyCommand(CommandT inCommand, void *ioParam)
{
	Boolean		cmdHandled = true;	// Assume we'll handle the command
	
	switch (inCommand) {
		case cmd_Preferences:
			mPreferences.DoPrefsDialog();
			break;
		
		case cmd_ConvertFiles:
			ConvertFiles();
			break;
		
		default:
			cmdHandled = LDocApplication::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}


// ---------------------------------------------------------------------------
//	€ FindCommandStatus
// ---------------------------------------------------------------------------
//	Determine the status of a Command for the purposes of menu updating.

void
CGSApplication::FindCommandStatus(CommandT inCommand, Boolean& outEnabled,
								  Boolean& outUsesMark, UInt16& outMark,
								  Str255 outName)
{
	ResIDT		theMenuID;
	SInt16		theMenuItem;
	
	// If command is synthetic.
	if (IsSyntheticCommand(inCommand, theMenuID, theMenuItem)) {
		if (theMenuID == MENU_Window)
			outEnabled = true;
		else
			LDocApplication::FindCommandStatus(inCommand, outEnabled,
											   outUsesMark, outMark, outName);
	} else {		// Command is non-synthetic.
		switch (inCommand) {
			case MENU_Edit:
			case MENU_Window:
				outEnabled = true;
				break;
			
			case MENU_View:
				outEnabled = false;
				break;
			
			case cmd_Preferences:
				outEnabled = true;
				break;
			
			case cmd_RerenderPage:
				LString::CopyPStr(LStr255(kSTRListGeneral, kRerenderPageStr), outName);
				outEnabled = false;
				break;
			
			case cmd_ConvertFiles:
				outEnabled = true;
				break;
			
			default:
				LDocApplication::FindCommandStatus(inCommand, outEnabled,
												   outUsesMark, outMark, outName);
				break;
		}
	}
}


// ---------------------------------------------------------------------------
//	€ EventResume
// ---------------------------------------------------------------------------
//	Respond to a Resume event

void
CGSApplication::EventResume(const EventRecord& inMacEvent)
{
	#pragma unused(inMacEvent)
	// set default sleep time, while active
	SetSleepTime(mForegroundSleep);
	LDocApplication::EventResume(inMacEvent);
}



// ---------------------------------------------------------------------------
//	€ EventSuspend
// ---------------------------------------------------------------------------
//	Respond to a Resume event

void
CGSApplication::EventSuspend(const EventRecord& inMacEvent)
{
	#pragma unused(inMacEvent)
	LDocApplication::EventSuspend(inMacEvent);
	// set higher sleep time, while in Background
	SetSleepTime(mBackgroundSleep);
}



// ---------------------------------------------------------------------------
//	€ EventMouseDown
// ---------------------------------------------------------------------------
//	Respond to a Mouse Down event (derived from LEventDispatcher to support
//	new WindowManager functions.

void
CGSApplication::EventMouseDown(const EventRecord& inMacEvent)
{
	WindowPtr	macWindowP;
	SInt16		thePart = ::MacFindWindow(inMacEvent.where, &macWindowP);
	
	switch (thePart) {
		case inProxyIcon:
			LCommander::SetUpdateCommandStatus(true);
			
			// Check for the special case of clicking in an underlying
			// window when the front window is modal.
			
			if (macWindowP != ::FrontWindow()) {
								// Clicked Window is not the front one
				LWindow *frontW = LWindow::FetchWindowObject(::FrontWindow());
				if ((frontW != nil) && frontW->HasAttribute(windAttr_Modal)) {
					 			// Front Window is Modal
					 			// Change part code to a special number.
					 			//   The only exception is a command-Drag,
					 			//   which moves, but does not re-order,
					 			//   an underlying window.
					if ((thePart != inDrag) || !(inMacEvent.modifiers & cmdKey)) {
						thePart = click_OutsideModal;
					}
				}
			}
		
			// Dispatch click to Window object
			LWindow	*theWindow = LWindow::FetchWindowObject(macWindowP);
			if (theWindow != nil) {
				theWindow->HandleClick(inMacEvent, thePart);
			}
			break;
		
		default:
			LEventDispatcher::EventMouseDown(inMacEvent);
	}
}



// ---------------------------------------------------------------------------
//	€ ConvertFiles
// ---------------------------------------------------------------------------

void
CGSApplication::ConvertFiles()
{
	OSErr	err;
	
	// choose files dialog
	
	CNavMenuItemSpecArray	popupItems(4);
	popupItems.SetMenuItem(0, LStr255(kSTRListOpenDialog, kOpenDialogPSDocument), kCGSShowFormatPS);
	popupItems.SetMenuItem(1, LStr255(kSTRListOpenDialog, kOpenDialogEPSFDocument), kCGSShowFormatEPSF);
	popupItems.SetMenuItem(2, LStr255(kSTRListOpenDialog, kOpenDialogPDFDocument), kCGSShowFormatPDF);
	popupItems.SetMenuItem(3, LStr255(kSTRListOpenDialog, kOpenDialogTextDocument), kCGSShowFormatTEXT);
	
	AEDescList			docList;
	CNavCustomDialog	openDialog;
	NavDialogOptions	openOptions = openDialog.GetOptions();
	openOptions.preferenceKey = 'open';
	openOptions.popupExtension = popupItems.GetHandle();
	openOptions.dialogOptionFlags =	kNavAllFilesInPopup + kNavDontAddTranslateItems + kNavAllowMultipleFiles;
	LString::CopyPStr(LStr255(kSTRListGeneral, kApplicationNameStr), openOptions.clientName);
	LString::CopyPStr(LStr255(kSTRListConvertDialog, kConvertDialogTitle), openOptions.windowTitle);
	LString::CopyPStr(LStr255(kSTRListConvertDialog, kConvertDialogMessageStr), openOptions.message);
	LString::CopyPStr(LStr255(kSTRListConvertDialog, kConvertDialogActionButtonStr), openOptions.actionButtonLabel);
	openDialog.SetOptions(openOptions);
	
	openDialog.SetCustomPopupMenuProc(CGSApplication::HandleOpenDialogPopupMenu);
	openDialog.SetDefaultMenuItem(popupItems.GetMenuItem(sOpenDialogFileType));
	
	if ((err = openDialog.GetFile(docList, 0, 0, CGSApplication::NavObjectFilter)) == noErr) {
		// choose destination folder and format
		FSSpec				destFSSpec;
		CNavCustomDialog	chooseDestDialog;
		NavDialogOptions	destOptions = chooseDestDialog.GetOptions();
		LString::CopyPStr(LStr255(kSTRListGeneral, kApplicationNameStr), destOptions.clientName);
		LString::CopyPStr(LStr255(kSTRListConvertDialog, kConvertDestDialogTitle), destOptions.windowTitle);
		LString::CopyPStr(LStr255(kSTRListConvertDialog, kConvertDestDialogMessageStr), destOptions.message);
		destOptions.preferenceKey = 'cnvt';
		chooseDestDialog.SetOptions(destOptions);
		
		chooseDestDialog.SetCustomControls(kConvertDialogDITL, 400, 40);
		chooseDestDialog.SetSetupCustomControlsProc(CGSApplication::ConvertSetupCustomControls);
		chooseDestDialog.SetAcceptCustomControlsProc(CGSApplication::ConvertAcceptCustomControls);
		
		if ((err = chooseDestDialog.ChooseFolder(destFSSpec)) == noErr) {
			char	destFolderPath[1024];
			CUtilities::ConvertFSSpecToPath(&destFSSpec, destFolderPath, 1024);	
			
			// convert all selected files
			SInt32		numDocs;
			OSErr 		err = ::AECountItems(&docList, &numDocs);
			ThrowIfOSErr_(err);
			
			CGSDocument		*doc;
			CGSDevice		*device = CGSDocument::CreateDeviceFromSaveFormat(sConvertDialogFileType);
			LStr255			ext, fileName;
			const int		maxFileNameLength = 32;
			
			if (device == NULL) {
				openDialog.FinishGetFile();
				return;
			}
			
			CGSDocument::GetExtensionFromSaveFormat(sConvertDialogFileType, ext);
			
			device->SetIsThreadedDevice(true);
			device->SetDeviceIsExporting(true);
			device->SetIsSelfDestruct(false);
			device->SetDeviceShowsPageRange(false);
			
			if (device->DoExportDialog()) {
				StDialogHandler	theHandler(PPob_ConversionProgressDialog, this);
				LWindow*		theDialog = theHandler.GetDialog();
				LStaticText		*fileCountText = dynamic_cast<LStaticText*> (theDialog->FindPaneByID(kConvertDialogFileCountText));
				LStaticText		*currentFileText = dynamic_cast<LStaticText*> (theDialog->FindPaneByID(kConvertDialogFileText));
				LProgressBar	*fileBar = dynamic_cast<LProgressBar*> (theDialog->FindPaneByID(kConvertDialogFileProgressBar));
				LProgressBar	*pageBar = dynamic_cast<LProgressBar*> (theDialog->FindPaneByID(kConvertDialogPageProgressBar));
				
				fileBar->SetIndeterminateFlag(false);
				fileBar->SetMinValue(0);
				fileBar->SetMaxValue(numDocs);
				fileBar->SetValue(0);
				pageBar->SetIndeterminateFlag(true);
				
				theDialog->Show();
				
				for (int i=1; i<=numDocs; i++) {
					MessageT	hitMessage = theHandler.DoDialog();
					
					if (hitMessage == msg_Cancel) {
						break;
					}
					
					AEKeyword	theKey;
					DescType	theType;
					FSSpec		docFileSpec;
					Size		theSize;
					err = ::AEGetNthPtr(&docList, i, typeFSS, &theKey, &theType,
										(Ptr) &docFileSpec, sizeof(FSSpec), &theSize);
					ThrowIfOSErr_(err);
					
					// update dialog
					fileBar->SetValue(i);
					fileCountText->SetValue(numDocs-i);
					currentFileText->SetDescriptor(docFileSpec.name);
					
					doc = new CGSDocument(this, &docFileSpec, false);
					if (doc == NULL)
						break;
					
					// wait for the open thread to finish
					while (!doc->IsReady()) {
						hitMessage = theHandler.DoDialog();
						if (hitMessage == msg_Cancel) {
							delete device;
							device = 0;
							doc->Close();
							doc = 0;
							break;
						}
						::YieldToAnyThread();
					}
					
					// needed, if cancel happened in the while loop
					if (hitMessage == msg_Cancel) {
						break;
					}
					
					// update dialog
					pageBar->SetIndeterminateFlag(!doc->HasDSC());
					pageBar->SetMinValue(0);
					pageBar->SetMaxValue(doc->GetNumberOfPages());
					
					fileName = docFileSpec.name;
					int pos = fileName.ReverseFind("\p.");
					if (pos > 0)
						fileName.Remove(pos, 255);
					if (fileName.Length() > maxFileNameLength - ext.Length())
						fileName.Remove(fileName.Length() - ext.Length(), 255);
					fileName.Append(ext);
					
					char	filePathStr[1024];
					sprintf(filePathStr, "%s%#s", destFolderPath, (unsigned char*)fileName);
					device->SetOutputFile(filePathStr);
					device->SetDocument(doc);
					
					device->SetPageRange(kSubmitWholeDocument);
					device->DoSave();
					
					UInt32 ticks = 0;
					while (device->IsRendering()) {
						if (::TickCount()-ticks > 10) {
							// update dialog
							if (doc->HasDSC()) {
								SInt32	currPage = device->GetCurrentPage(),
										maxPage = doc->GetNumberOfPages();
								if (currPage > 0 && currPage <= maxPage) {
									LStr255	str(docFileSpec.name);
									str.Append(" (");
									str.Append(currPage);
									str.Append("/");
									str.Append(maxPage);
									str.Append(")");
									currentFileText->SetDescriptor(str);
								}
								pageBar->SetValue(currPage);
							}
							
							hitMessage = theHandler.DoDialog();
							if (hitMessage == msg_Cancel) {
								delete device;
								device = 0;
								break;
							}
							
							ticks = ::TickCount();
							::YieldToAnyThread();
						}
					}
					
					if (device) device->GetDLL()->Reset();
					if (doc) doc->Close();
					
					// needed, if cancel happened in the while loop
					if (hitMessage == msg_Cancel) {
						break;
					}
				}
			}
			if (device) delete device;
		} else if (err != userCanceledErr) {
			CUtilities::ShowErrorDialog("\pchooseDestDialog.ChooseFolder() failed", err);
		}
	} else if (err != userCanceledErr) {
		CUtilities::ShowErrorDialog("\popenDialog.GetFile() failed", err);
	}
	openDialog.FinishGetFile();
	
	UpdateMenus();	// should be LCommander::SetUpdateCommandStatus(true), but doesn't work
}



// ---------------------------------------------------------------------------
//	€ ConvertSetupCustomControls									[static]
// ---------------------------------------------------------------------------

void
CGSApplication::ConvertSetupCustomControls(NavCBRecPtr callBackParams, short inFirstItem)
{
	ControlHandle	itemHandle;
	short			itemType;
	Rect			itemRect;
	
	::GetDialogItem((DialogPtr) callBackParams->window, inFirstItem+kConvertDialogFormatPopup,
					&itemType, &((Handle) itemHandle), &itemRect);
	
	::SetControlValue(itemHandle, sConvertDialogFileTypeNr);
}



// ---------------------------------------------------------------------------
//	€ ConvertAcceptCustomControls									[static]
// ---------------------------------------------------------------------------

void
CGSApplication::ConvertAcceptCustomControls(NavCBRecPtr callBackParams, short inFirstItem)
{
	ControlHandle	itemHandle;
	short			itemType;
	Rect			itemRect;
	
	::GetDialogItem((DialogPtr) callBackParams->window, inFirstItem+kConvertDialogFormatPopup,
					&itemType, &((Handle) itemHandle), &itemRect);
	
	sConvertDialogFileTypeNr = ::GetControlValue(itemHandle);
	switch (sConvertDialogFileTypeNr) {
		case 1:		sConvertDialogFileType = kCGSSaveFormatAI;		break;
		case 2:		sConvertDialogFileType = kCGSSaveFormatASCII;	break;
		case 3:		sConvertDialogFileType = kCGSSaveFormatBMP;		break;
		case 4:		sConvertDialogFileType = kCGSSaveFormatEPSF;	break;
		case 5:		sConvertDialogFileType = kCGSSaveFormatFax;		break;
		case 6:		sConvertDialogFileType = kCGSSaveFormatJPEG;	break;
		case 7:		sConvertDialogFileType = kCGSSaveFormatPxM;		break;
		case 8:		sConvertDialogFileType = kCGSSaveFormatPCX;		break;
		case 9:		sConvertDialogFileType = kCGSSaveFormatPDF;		break;
		case 10:	sConvertDialogFileType = kCGSSaveFormatPICT;	break;
		case 11:	sConvertDialogFileType = kCGSSaveFormatPNG;		break;
		case 12:	sConvertDialogFileType = kCGSSaveFormatPS;		break;
		case 13:	sConvertDialogFileType = kCGSSaveFormatTIFF;	break;
	}
}



// ---------------------------------------------------------------------------
//	€ OpenDocument
// ---------------------------------------------------------------------------
// This method is called when a file is chosen from the Open File dialog.

void
CGSApplication::OpenDocument(FSSpec *inMacFSSpec)
{
	LDocument	*theDoc = LDocument::FindByFileSpec(*inMacFSSpec);
	
	if (theDoc != nil)					// Document is already open
		theDoc->MakeCurrent();			// Make it the current document	
	else								// Make a new Document
		theDoc = new CGSDocument(this, inMacFSSpec);
}


// ---------------------------------------------------------------------------
//	€ ChooseDocument
// ---------------------------------------------------------------------------
// This method lets the user choose document(s) to open.

void
CGSApplication::ChooseDocument()
{
	CNavMenuItemSpecArray	popupItems(4);
	popupItems.SetMenuItem(0, LStr255(kSTRListOpenDialog, kOpenDialogPSDocument), kCGSShowFormatPS);
	popupItems.SetMenuItem(1, LStr255(kSTRListOpenDialog, kOpenDialogEPSFDocument), kCGSShowFormatEPSF);
	popupItems.SetMenuItem(2, LStr255(kSTRListOpenDialog, kOpenDialogPDFDocument), kCGSShowFormatPDF);
	popupItems.SetMenuItem(3, LStr255(kSTRListOpenDialog, kOpenDialogTextDocument), kCGSShowFormatTEXT);
	
	CNavCustomDialog	openDialog;
	NavDialogOptions	options = openDialog.GetOptions();
	options.preferenceKey = 'open';
	options.popupExtension = popupItems.GetHandle();
	options.dialogOptionFlags =	kNavAllFilesInPopup +
								kNavDontAddTranslateItems +
								kNavAllowMultipleFiles;
	LString::CopyPStr(LStr255(kSTRListGeneral, kApplicationNameStr), options.clientName);
	openDialog.SetOptions(options);
	openDialog.SetCustomPopupMenuProc(CGSApplication::HandleOpenDialogPopupMenu);
	openDialog.SetDefaultMenuItem(popupItems.GetMenuItem(sOpenDialogFileType));
	
	AEDescList	docList;
	OSErr	err = openDialog.GetFile(docList, 0, 0, CGSApplication::NavObjectFilter);
	if (err == noErr)
		SendAEOpenDocList(docList);
	else if (err != userCanceledErr)
		CUtilities::ShowErrorDialog("\popenDialog.GetFile failed", err);
	
	openDialog.FinishGetFile();
}



// ---------------------------------------------------------------------------
//	€ HandleOpenDialogPopupMenu										[static]
// ---------------------------------------------------------------------------
//	

void
CGSApplication::HandleOpenDialogPopupMenu(OSType inCreator, OSType inType, NavCBRecPtr inParams)
{
	#pragma unused(inCreator, inParams)
	
	sOpenDialogFileType = inType;
}


	
// ---------------------------------------------------------------------------
//	€ NavObjectFilter												[static]
// ---------------------------------------------------------------------------
// 

pascal Boolean
CGSApplication::NavObjectFilter(AEDesc *theItem, void *info, void *callBackUD,
								NavFilterModes filterMode)
{
	#pragma unused (callBackUD, filterMode)
	
	OSErr					theErr	= noErr;
	Boolean					display	= true;
	NavFileOrFolderInfo*	theInfo	= (NavFileOrFolderInfo*) info;
	
	if (theItem->descriptorType == typeFSS) {
		if (!theInfo->isFolder && theInfo->visible) {
			OSType	fileType	= theInfo->fileAndFolder.fileInfo.finderInfo.fdType;
			OSType	fileCreator	= theInfo->fileAndFolder.fileInfo.finderInfo.fdCreator;
			FSSpec		theFSSpec;
			
			display = false;
			
			switch (sOpenDialogFileType) {
				case kCGSShowFormatPS:
					if (fileType == 'TEXT' &&
						(fileCreator == 'vgrd' || fileCreator == LFileTypeList::GetProcessSignature())) {
						display = true;
					} else {
						::BlockMoveData(*(theItem->dataHandle), &theFSSpec, sizeof(FSSpec));
						if (CUtilities::FileBeginsWith(theFSSpec, "%!PS", 4))
							display = true;
					}
					break;
				
				case kCGSShowFormatEPSF:
					if (fileType == 'EPSF') {
						display = true;
					} else {
						::BlockMoveData(*(theItem->dataHandle), &theFSSpec, sizeof(FSSpec));
						if (CUtilities::FileBeginsWith(theFSSpec, "%!PS", 4))
							display = true;
					}
					break;
				
				case kCGSShowFormatPDF:
					if (fileType == 'PDF ') {
						display = true;
					} else {
						::BlockMoveData(*(theItem->dataHandle), &theFSSpec, sizeof(FSSpec));
						if (CUtilities::FileBeginsWith(theFSSpec, "%PDF", 4))
							display = true;
					}
					break;
				
				case kCGSShowFormatTEXT:
					if (fileType == 'TEXT')
						display = true;
					break;
				
				default:
					display = true;
			}
		}
	}
	
	return display;
}


// ---------------------------------------------------------------------------
//	€ PrintDocument
// ---------------------------------------------------------------------------
// This method is called in response to a Print request

void
CGSApplication::PrintDocument(FSSpec *inMacFSSpec)
{
	// Create a new document using the file spec.
	CGSDocument		*theDocument = new CGSDocument(this, inMacFSSpec);

	// Tell the document to print.
	theDocument->DoPrint();
}


// ---------------------------------------------------------------------------
//	€ SetupPage
// ---------------------------------------------------------------------------
//	Handle the Page Setup command

void
CGSApplication::SetupPage()
{
	StDesktopDeactivator	deactivator;
	OSErr					err;
	
	::PrOpen();
	err = ::PrError();
	if (err == noErr) {
		::PrStlDialog(GetPreferences()->GetDefaultPrintRecord());
		::PrClose();
	} else {
		CUtilities::ShowErrorDialog("\pPrOpen failed in SetupPage", err);
	}
}

