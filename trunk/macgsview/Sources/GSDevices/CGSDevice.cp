/* ============================================================================	*/
/*	CGSDevice.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	Generic gs rendering device													*/
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


#ifdef PowerPlant_PCH
	#include PowerPlant_PCH
#endif

#include <UReanimator.h>
#include <UAppleEventsMgr.h>
#include <UDesktop.h>
#include <UModalDialogs.h>
#include <UCursor.h>
#include <LEditText.h>
#include <LPopupButton.h>
#include <LRadioButton.h>
#include <LStaticText.h>
#include <LSlider.h>
#include <LWindow.h>
#include <LString.h>
#include <LMultiPanelView.h>
#include <LPopupGroupBox.h>
#include <LCheckBoxGroupBox.h>
#include <UProfiler.h>

#include <PP_KeyCodes.h>

#include <string.h>
#include <stdlib.h>

#include <Files.h>
#include <Threads.h>

#include "CGSApplication.h"
#include "CGSDevice.h"
#include "CGSDocument.h"
#include "CUtilities.h"



// ---------------------------------------------------------------------------
//	€ CGSDevice
// ---------------------------------------------------------------------------
//	Constructor

CGSDevice::CGSDevice(CGSDocument* inDocument)
{
	mDocument		= inDocument;
	
	mFileType		= '????';
	mFileCreator	= '????';
	
	mNumPanels		= 0;
	mShowPageRangePanel = true;
	mShowDialog		= true;
	
	mFirstPage = mLastPage = mCurrentPage = 0;
	
	sprintf(mDeviceName, "");
	sprintf(mOutputFilePath, "");
	mIsMultiplePageDevice	= false;
	mDeviceIsExporting		= false;
	mDeviceIsSelfDestruct	= false;
	mDeviceIsPrinting		= false;
	mPrefaceSubmitted		= false;
	mDeviceIsSetUp			= false;
	mRenderThread			= 0;
	SetIsThreadedDevice(true);
	SetIsScreenDevice(false);
	
	SetTransparency(1, 1);
	SetPageSize(612.0, 792.0);
	SetResolution(72.0);
	SetUseHalftoneScreen(false);
	
	mDLL = new CGSSharedLib;
	
	int err = mDLL->Init();
	if (err) {
		// error, caller has to test if GetDLL() returns 0
		delete mDLL;
		mDLL = 0;
		ErrorDialog(err);
	}
}



// ---------------------------------------------------------------------------
//	€ ~CGSDevice
// ---------------------------------------------------------------------------
//	Destructor

CGSDevice::~CGSDevice()
{
	Abort();
	
	if (mDeviceIsExporting && mDocument) {
		mDocument->SetExportDevice(0);
	}
	
	if (GetDLL())
		delete GetDLL();
}



// ---------------------------------------------------------------------------
//	€ Abort
// ---------------------------------------------------------------------------

void
CGSDevice::Abort()
{
	// shutdown a possibly working thread
	if (mIsThreadedDevice && mRenderThread) {
		GetDLL()->Abort();
		while(mRenderThread || !GetDLL()->Aborted()) {
			GetDLL()->Abort();
			::SetThreadState(mRenderThread, kReadyThreadState, kNoThreadID);
			::YieldToAnyThread();
		}
	}
}



// ---------------------------------------------------------------------------
//	€ SetOutputFile
// ---------------------------------------------------------------------------

void
CGSDevice::SetOutputFile(FSSpec& inFSSpec)
{
	mOutputFileFSSpec = inFSSpec;
	CUtilities::ConvertFSSpecToPath(&mOutputFileFSSpec, mOutputFilePath, 1024);
}



// ---------------------------------------------------------------------------
//	€ SetOutputFile
// ---------------------------------------------------------------------------

void
CGSDevice::SetOutputFile(char* inPath)
{
	strcpy(mOutputFilePath, inPath);
	CUtilities::ConvertPathToFSSpec(mOutputFilePath, &mOutputFileFSSpec);
}



#pragma mark -
#pragma mark === Device Access ===


// ---------------------------------------------------------------------------
//	€ RenderPage
// ---------------------------------------------------------------------------

void
CGSDevice::RenderPage(int inFirstPage, int inLastPage)
{
	mLastError = 0;
	
	if (mDocument) {
		GSThreadArgs*	args = new GSThreadArgs;
		args->object	= this;
		args->arg1		= inFirstPage;
		args->arg2		= inLastPage;
		
		if (mIsThreadedDevice) {
			OSErr		err;
			const int   threadStackSize = 512 * 1024;
			
			::ThreadBeginCritical();
			err = ::NewThread(kCooperativeThread, CGSDevice::sRenderPage, (void*) args,
							  threadStackSize, kNewSuspend, nil, &mRenderThread);
			::SetThreadTerminator(mRenderThread, CGSDevice::sRenderThreadDied, (void*) args);
			::SetThreadState(mRenderThread, kReadyThreadState, kNoThreadID);
			LCommander::SetUpdateCommandStatus(true);
			::ThreadEndCritical();
			::YieldToThread(mRenderThread);
		} else {
			CGSDevice::sRenderPage(args);
			CGSDevice::sRenderThreadDied(0, args);
		}
	}
}


// ---------------------------------------------------------------------------
//	€ sRenderPage													[static]
// ---------------------------------------------------------------------------
//  static warapper function for RenderPage() which can be used as thread
//  entry functions.

pascal void*
CGSDevice::sRenderPage(void* arg)
{
	int err;
	
	err = ((GSThreadArgs*) arg)->object->RenderPageThread(((GSThreadArgs*) arg)->arg1,
														  ((GSThreadArgs*) arg)->arg2);
	return 0;
}


// ---------------------------------------------------------------------------
//	€ sRenderThreadDied												[static]
// ---------------------------------------------------------------------------

pascal void
CGSDevice::sRenderThreadDied(ThreadID id, void* arg)
{
	CGSDevice	*device = ((GSThreadArgs*) arg)->object;
	device->RenderThreadDied(id, arg);
	device->mRenderThread = 0;
	delete arg;
	
	// export device has selfdestruction
	if (device->mDeviceIsSelfDestruct) {
		delete device;
	}
}


// ---------------------------------------------------------------------------
//	€ RenderThreadDied
// ---------------------------------------------------------------------------

void
CGSDevice::RenderThreadDied(ThreadID id, void* arg)
{
#pragma unused(id, arg)
}


// ---------------------------------------------------------------------------
//	€ RenderPageThread
// ---------------------------------------------------------------------------

int
CGSDevice::RenderPageThread(int inFirstPage, int inLastPage)
{
	int		err = 0;
	char	paramStr[1024];
	char	filePath[1024];
	
	mFirstPage	= inFirstPage;
	mLastPage	= inLastPage;
	
	if (mLastPage == kSubmitOnePage)
		mLastPage = mFirstPage;
	
	if (mDocument->HasDSC() && (mFirstPage == kSubmitWholeDocument)) {
		mFirstPage	= 1;
		mLastPage	= mDocument->GetNumberOfPages();
	}
	
	if (mDeviceIsExporting && mDocument->GetToolbar()) {
		mDocument->GetToolbar()->SetStatusText(LStr255(kSTRListToolbar, kToolbarExportingStr));
		mDocument->GetToolbar()->SetProgressMinMax(mFirstPage, mLastPage);
		mDocument->GetToolbar()->ShowProgressBar();
		mDocument->GetToolbar()->ShowStatusText();
		mDocument->GetToolbar()->ShowStopButton();
	}
	
	// submit page or document
	if (mDocument->IsPDF()) {
		if (!mDeviceIsExporting || (mDeviceIsExporting && mExportWithDocSize))
			mDocument->GetPageSize(mFirstPage, mPageSize.h, mPageSize.v);
		
		// setup device
		if ((err = SetupDevice()) != 0) {
			ErrorDialog(err);
		} else {
			DeviceSetupDone();
			mDeviceIsSetUp = true;
		}
		
		if (mDeviceIsSetUp) {
			sprintf(paramStr,
					"/Page null def\n													\
					 /Page# 0 def\n														\
					 /PDFSave null def\n												\
					 /DSCPageCount 0 def\n												\
					 /DoPDFPage {dup /Page# exch store pdfgetpage pdfshowpage} def\n	\
					 GS_PDF_ProcSet begin\n												\
					 pdfdict begin\n");
			GetDLL()->ExecStr(paramStr);
			
			mDocument->GetFilePath(filePath);
			sprintf(paramStr, "(%s) (r) file pdfopen begin\n", filePath);
			if ((err = GetDLL()->ExecStr(paramStr))) {
				ErrorDialog(err);
			} else {
				mCurrentPage = mFirstPage;
				while (mCurrentPage <= mLastPage) {
					if (mDeviceIsExporting && mDocument->GetToolbar()) {
						mDocument->GetToolbar()->SetProgress(mCurrentPage);
					}
					sprintf(paramStr, "%d DoPDFPage\n", mCurrentPage++);
					if ((err = GetDLL()->ExecStr(paramStr))) {
						mLastError = err;
						ErrorDialog(err);
						break;
					}
				}
				sprintf(paramStr, "currentdict pdfclose end end end\n");
				if ((err = GetDLL()->ExecStr(paramStr))) {
					mLastError = err;
	 				ErrorDialog(err);
				}
			}
		}
	} else {
		// PS or EPS
		if (mFirstPage == kSubmitWholeDocument) {
			// this case happens only with non-DSC files !!
			
			if (!mDeviceIsExporting || (mDeviceIsExporting && mExportWithDocSize))
				mDocument->GetPageSize(1, mPageSize.h, mPageSize.v);
	
			mDeviceIsSetUp = false;
			GetDLL()->ExecStr("save\n");	// save state, needed because of a misbehaviour
											// when rerendering escher.ps
			// setup device
			if ((err = SetupDevice()) != 0) {
				mLastError = err;
				ErrorDialog(err);
			} else {
				DeviceSetupDone();
				mDeviceIsSetUp = true;
			}
			
			if (mDeviceIsSetUp) {
				
				if (mDeviceIsExporting && mDocument->GetToolbar()) {
					mDocument->GetToolbar()->SetProgress(-1);
				}
				
				// rotate page if needed
				switch (mDocument->GetPageOrientation(1)) {
					case SEASCAPE:
					case LANDSCAPE:
						sprintf(paramStr, "%f %f translate 90 rotate\n", mPageSize.h, 0.0);
						GetDLL()->ExecStr(paramStr);
				}
				
				// translate EPS
				if (mDocument->IsEPS() && mDocument->GetDocInfo()->boundingbox) {
					sprintf(paramStr, "%d %d translate\n",
							- mDocument->GetDocInfo()->boundingbox[LLX],
							- mDocument->GetDocInfo()->boundingbox[LLY]);
					GetDLL()->ExecStr(paramStr);
				}
				
				mCurrentPage = -1;
				mDocument->GetFilePath(filePath);
				GetDLL()->ExecStr("(");
				GetDLL()->ExecStr(filePath);
				if ((err = GetDLL()->ExecStr(") run\n"))) {
					mLastError = err;
					ErrorDialog(err);
				}
			}
			GetDLL()->ExecStr("restore\n");	// restore state (close device), needed because of a
											// misbehaviour when rerendering escher.ps
		} else {
			if (!mDeviceIsExporting || (mDeviceIsExporting && mExportWithDocSize))
				mDocument->GetPageSize(mFirstPage, mPageSize.h, mPageSize.v);
			
			// setup device
			if ((err = SetupDevice()) != 0) {
				mLastError = err;
				ErrorDialog(err);
			} else {
				DeviceSetupDone();
				mDeviceIsSetUp = true;
			}
			
			if (mDeviceIsSetUp) {
				
				if ((err = SubmitPSPreface())) {
					mLastError = err;
					mDeviceIsSetUp = false;
					mPrefaceSubmitted = false;
					ErrorDialog(err);
				} else {
					mCurrentPage = mFirstPage;
					while (mCurrentPage <= mLastPage) {
						GetDLL()->ExecStr("gsave\n");
						
						if (mDeviceIsExporting && mDocument->GetToolbar()) {
							LStr255	text(kSTRListToolbar, kToolbarExportingPageStr);
							text += "\p ";
							text += mCurrentPage; 
							if (mFirstPage != mLastPage) {
								text += "\p/";
								text += mFirstPage;
								text += "\p-";
								text += mLastPage;
							}
							mDocument->GetToolbar()->SetProgress(mCurrentPage);
							mDocument->GetToolbar()->SetStatusText(text);
						}
						
						// rotate page if needed
						switch (mDocument->GetPageOrientation(mCurrentPage)) {
							case SEASCAPE:
							case LANDSCAPE:
								sprintf(paramStr, "%f %f translate 90 rotate\n", mPageSize.h, 0.0);
								GetDLL()->ExecStr(paramStr);
						}
						
						// translate EPS
						if (mDocument->IsEPS() && mDocument->GetDocInfo()->boundingbox) {
							sprintf(paramStr, "%d %d translate\n",
									- mDocument->GetDocInfo()->boundingbox[LLX],
									- mDocument->GetDocInfo()->boundingbox[LLY]);
							GetDLL()->ExecStr(paramStr);
						}
						
						if ((err = SubmitPSPage(mCurrentPage))) {
							mLastError = err;
							mDeviceIsSetUp = false;
							mPrefaceSubmitted = false;
							ErrorDialog(err);
							break;
						} else {
							mCurrentPage++;
						}
						
						GetDLL()->ExecStr("grestore\n");
					}
					
					// if we export we have to submit the trailer, otherwise we can miss a output_page
					// this should be done for non-exporting too, but we would have to resubmit the preface
					if ((mDeviceIsExporting || mDeviceIsPrinting) && !mLastError) {
						SubmitPSSection(PSTrailer);
						/*if ((err = SubmitPSSection(PSTrailer))) {
							mLastError = err;
							mDeviceIsSetUp = false;
							mPrefaceSubmitted = false;
							ErrorDialog(err);
						}*/
					}
				}
			}
		}
	}
	
	SetFileTypeCreator();
	
	if (mDeviceIsExporting && mDocument->GetToolbar()) {
		mDocument->GetToolbar()->HideStopButton();
		mDocument->GetToolbar()->HideProgressBar();
		mDocument->GetToolbar()->HideStatusText();
	}
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ PreDeviceSetup
// ---------------------------------------------------------------------------
// here can be done anything what is needed before the device is active,

void
CGSDevice::PreDeviceSetup()
{
}



// ---------------------------------------------------------------------------
//	€ SetupDevice
// ---------------------------------------------------------------------------

int
CGSDevice::SetupDevice()
{
	int err;
	
	GetDLL()->ExecStr("<< ");
	SubmitDeviceParameters();
	GetDLL()->ExecStr("/OutputDevice /");
	GetDLL()->ExecStr(mDeviceName);
	err = GetDLL()->ExecStr(" >> setpagedevice\n");
	
	if (err)
		return err;
	
	GetDLL()->ExecStr("mark\n");
	SubmitDeviceParameters();
	return GetDLL()->ExecStr("currentdevice putdeviceprops setdevice\n");
}



// ---------------------------------------------------------------------------
//	€ DeviceSetupDone
// ---------------------------------------------------------------------------
// here can be done anything what is impossible before the device is active

void
CGSDevice::DeviceSetupDone()
{
	// set halftonescreen
	if (mHalftoneScreen.useScreen) {
		char	paramStr[1024];
		
		sprintf(paramStr, "%d %d ", mHalftoneScreen.frequency, mHalftoneScreen.angle);
		GetDLL()->ExecStr(paramStr);
		switch (mHalftoneScreen.type) {
			case kLineSpot:
				GetDLL()->ExecStr("{ pop } ");
				break;
			case kDotSpot:
				GetDLL()->ExecStr("{ 180 mul cos exch 180 mul cos add 2 div } ");
				break;
			case kEllipseSpot:
				GetDLL()->ExecStr("{ dup 5 mul 8 div mul exch dup mul exch add sqrt 1 exch sub } ");
				break;
		}
		GetDLL()->ExecStr("setscreen\n");
	}
	
}



// ---------------------------------------------------------------------------
//	€ SubmitDeviceParameters
// ---------------------------------------------------------------------------

void
CGSDevice::SubmitDeviceParameters()
{
	char	paramStr[1024];
	
	sprintf(paramStr, "/PageSize [%f %f]\n", mPageSize.h, mPageSize.v);
	GetDLL()->ExecStr(paramStr);
	sprintf(paramStr, "/HWResolution [%f %f]\n", mHWResolution.h, mHWResolution.v);
	GetDLL()->ExecStr(paramStr);
	sprintf(paramStr, "/TextAlphaBits %d\n", mTextAlphaBits);
	GetDLL()->ExecStr(paramStr);
	sprintf(paramStr, "/GraphicsAlphaBits %d\n", mGraphicsAlphaBits);
	GetDLL()->ExecStr(paramStr);
	
	if (mDeviceIsExporting) {
		GetDLL()->ExecStr("/OutputFile (");
		GetDLL()->ExecStr(mOutputFilePath);
		GetDLL()->ExecStr(")\n");
	}
}



// ---------------------------------------------------------------------------
//	€ SubmitPSPage
// ---------------------------------------------------------------------------
//	Submits a page of the document to the DLL for execution

int
CGSDevice::SubmitPSPage(SInt32 pageNum)
{
	if (mDocument->GetDocInfo() && (pageNum <= mDocument->GetNumberOfPages()) && (pageNum >= 1)) {
		return GetDLL()->ExecFile(mDocument->GetFile(),
								  mDocument->GetDocInfo()->pages[pageNum-1].begin,
								  mDocument->GetDocInfo()->pages[pageNum-1].len);
	} else {
		return -1;
	}
}



// ---------------------------------------------------------------------------
//	€ SubmitPSPreface
// ---------------------------------------------------------------------------
//	Submits PS-Header, -Defaults, -Prolog and -Setup section of the file to
//	the DLL for execution.

int
CGSDevice::SubmitPSPreface()
{
	int	err = 0;
	
	if (!mPrefaceSubmitted) {
		err = SubmitPSSection(PSHeader);
		if (err)
			return err;
		err = SubmitPSSection(PSDefaults);
		if (err)
			return err;
		err = SubmitPSSection(PSProlog);
		if (err)
			return err;
		err = SubmitPSSection(PSSetup);
		if (err)
			return err;
		
		mPrefaceSubmitted = true;
	}
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ SubmitPSSection
// ---------------------------------------------------------------------------
//	Submit section sect of the PS-file to the DLL for execution.

int
CGSDevice::SubmitPSSection(PSSection sect)
{
	if (mDocument->GetDocInfo()) {
		switch (sect) {
			case PSHeader:
				return GetDLL()->ExecFile(mDocument->GetFile(),
										  mDocument->GetDocInfo()->beginheader,
										  mDocument->GetDocInfo()->lenheader);
			case PSDefaults:
				return GetDLL()->ExecFile(mDocument->GetFile(),
										  mDocument->GetDocInfo()->begindefaults,
										  mDocument->GetDocInfo()->lendefaults);
			case PSProlog:
				return GetDLL()->ExecFile(mDocument->GetFile(),
										  mDocument->GetDocInfo()->beginprolog,
										  mDocument->GetDocInfo()->lenprolog);
			case PSSetup:
				return GetDLL()->ExecFile(mDocument->GetFile(),
										  mDocument->GetDocInfo()->beginsetup,
										  mDocument->GetDocInfo()->lensetup);
			case PSTrailer:
				return GetDLL()->ExecFile(mDocument->GetFile(),
										  mDocument->GetDocInfo()->begintrailer,
										  mDocument->GetDocInfo()->lentrailer);
			default:
				return -1;	// nonexisting section
		}
	} else {
		return -1;
	}
}






#pragma mark -
#pragma mark === Saving ===



// ---------------------------------------------------------------------------
//	€ Save
// ---------------------------------------------------------------------------

void
CGSDevice::Save()
{
	if (DoExportDialog()) {
		DoSave();
	} else {
		if (mDeviceIsSelfDestruct)
			delete this;
	}
}



// ---------------------------------------------------------------------------
//	€ Save
// ---------------------------------------------------------------------------

void
CGSDevice::Save(FSSpec& inOutputFile)
{
	SetOutputFile(inOutputFile);
	Save();
}



// ---------------------------------------------------------------------------
//	€ DoSave
// ---------------------------------------------------------------------------

int
CGSDevice::DoSave()
{
	if (!mIsMultiplePageDevice &&
			((mFirstPage == kSubmitWholeDocument) ||
				((mFirstPage != kSubmitWholeDocument) && (mLastPage != kSubmitOnePage)))) {
		LStr255		basePathStr, fileNameStr, extStr, newPathStr;
		short		extPos, diff;
		
		basePathStr = mOutputFilePath;
		basePathStr.Remove(basePathStr.ReverseFind("\p:")+1, 255);
		
		fileNameStr = mOutputFilePath;
		fileNameStr.Remove(0, basePathStr.Length());
		
		extStr = fileNameStr;
		extPos = extStr.ReverseFind("\p.");
		extStr.Remove(0, extPos-1);
		if (extPos > 0)
			fileNameStr.Remove(extPos, 255);
		
		diff = 31 - (fileNameStr.Length() + extStr.Length() + 3/*enough for 1...999*/);
		if (diff < 0)
			fileNameStr.Remove(fileNameStr.Length() + diff + 1, 255);
		
		newPathStr	 = basePathStr;
		newPathStr	+= fileNameStr;
		newPathStr	+= "\p.%d";
		newPathStr	+= extStr;
		
		char		newPath[256];
		sprintf(newPath, "%#s", (StringPtr) newPathStr);
		SetOutputFile(newPath);
	}
	
	RenderPage(mFirstPage, mLastPage);
	
	return 0;
}



// ---------------------------------------------------------------------------
//	€ SetFileTypeCreator()
// ---------------------------------------------------------------------------

void
CGSDevice::SetFileTypeCreator()
{
	if (mDeviceIsExporting) {
		FInfo	fileInfo;
		
		if (::FSpGetFInfo(&mOutputFileFSSpec, &fileInfo) == noErr) {
			fileInfo.fdType		= mFileType;
			fileInfo.fdCreator	= mFileCreator;
			::FSpSetFInfo(&mOutputFileFSSpec, &fileInfo);
		}
	}
}



#pragma mark -
#pragma mark === Dialog Handling ===



// ---------------------------------------------------------------------------
//	€ DoExportDialog
// ---------------------------------------------------------------------------

bool
CGSDevice::DoExportDialog()
{
	if (!mShowDialog)
		return true;
	
	StDialogHandler		theHandler(PPob_ExportDialog, this);
	LWindow*			theDialog = theHandler.GetDialog();
	LMultiPanelView*	panelView = dynamic_cast<LMultiPanelView*>
									(theDialog->FindPaneByID(kExportMultiPanelView));
	
	// let the device install its panels
	mNumPanels = 0;
	InstallPanels(theDialog);
	
	// make the multi-panel view listen to its superview
	// have to do it here explicitly because adding menu items causes wrong switch messages
	LBroadcaster	*panelViewSuper = dynamic_cast<LBroadcaster*> (panelView->GetSuperView());
	panelViewSuper->AddListener(panelView);
	
	// we want to listen to messages from the panels
	for (short i=0; i<mNumPanels; i++)
		UReanimator::LinkListenerToControls(&theHandler, mPanelList[i].panel, mPanelList[i].panelID);
	
	// let all panel handler initialize their controls
	for (short i=0; i<mNumPanels; i++)
		(this->*(mPanelList[i].handler))(GSExportPanelSetup, mPanelList[i].panel, 0);
	
	// show the dialog
	panelView->SwitchToPanel(1);
	theDialog->Show();
	
	while (true) {
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK) {
			for (short i=0; i<mNumPanels; i++)
				(this->*(mPanelList[i].handler))(GSExportPanelAccept, mPanelList[i].panel, hitMessage);
			return true;
		} else if (hitMessage == msg_Cancel) {
			for (short i=0; i<mNumPanels; i++)
				(this->*(mPanelList[i].handler))(GSExportPanelCancel, mPanelList[i].panel, hitMessage);
			return false;
		} else {
			short	i = panelView->GetCurrentIndex() - 1;
			(this->*(mPanelList[i].handler))(GSExportPanelMessage, mPanelList[i].panel, hitMessage);
		}
	}
}



// ---------------------------------------------------------------------------
//	€ InstallPanels
// ---------------------------------------------------------------------------
// is called from DoExportDialog(), this is the place to add panels

void
CGSDevice::InstallPanels(LWindow *inDialog)
{
	if (mShowPageRangePanel)
		AddPanel(inDialog, PPob_ExportPageRangePanel, &HandlePageRangePanel,
					LStr255(kSTRListExportPanelNames, kExportPanelPageRangeStr));
	AddPanel(inDialog, PPob_ExportPageSizePanel, &HandlePageSizePanel,
				LStr255(kSTRListExportPanelNames, kExportPanelPageSizeStr));
	AddPanel(inDialog, PPob_ExportRenderingPanel, &HandleRenderingPanel,
				LStr255(kSTRListExportPanelNames, kExportPanelRenderingStr));
	AddPanel(inDialog, PPob_ExportHalftonePanel, &HandleHalftonePanel,
				LStr255(kSTRListExportPanelNames, kExportPanelHalftonesStr));
}



// ---------------------------------------------------------------------------
//	€ AddPanel
// ---------------------------------------------------------------------------

void
CGSDevice::AddPanel(LWindow *inDialog, ResIDT inPanel, PanelHandler inHandler,
					ConstStringPtr inMenuName)
{
	LPopupGroupBox		*panelMenu = dynamic_cast<LPopupGroupBox*>
										(inDialog->FindPaneByID(kExportPopupGroupBox));
	LMultiPanelView		*panelView = dynamic_cast<LMultiPanelView*>
										(inDialog->FindPaneByID(kExportMultiPanelView));
	
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
//	€ HandlePageRangePanel
// ---------------------------------------------------------------------------

void
CGSDevice::HandlePageRangePanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
	static LRadioButton		*currentPageRB, *allPagesRB, *pageRangeRB;
	static LEditText		*fromField, *toField;
	static LStaticText		*toText;
	
	if (inMode == GSExportPanelSetup) {
			// get pointers to our controls (only once in a dialogs life)
		currentPageRB	= dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportPageRangeCurrentPage));
		allPagesRB		= dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportPageRangeAllPages));
		pageRangeRB		= dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportPageRangePageRange));
		fromField		= dynamic_cast<LEditText*> (inView->FindPaneByID(kExportPageRangeFirstPage));
		toField			= dynamic_cast<LEditText*> (inView->FindPaneByID(kExportPageRangeLastPage));
		toText			= dynamic_cast<LStaticText*> (inView->FindPaneByID(kExportPageRangeToText));
		
			// initialize with default values
		// non-DSC files can only be exported with "all pages"
		if (!mDocument->HasDSC()) {
			allPagesRB->SetValue(Button_On);
			allPagesRB->Disable();
			pageRangeRB->Disable();
			currentPageRB->Disable();
			fromField->Disable();
			toField->Disable();
			toText->Disable();
		} else {
			currentPageRB->SetValue(Button_On);
		}
	} else if (inMode == GSExportPanelAccept) {
		// read current values from the controls and store them
		if (currentPageRB->GetValue()) {
			// current page
			mFirstPage = mDocument->GetCurrentPage();
			mLastPage  = kSubmitOnePage;
		} else if (allPagesRB->GetValue()) {
			// all pages
			mFirstPage = kSubmitWholeDocument;
		} else {
			// page range "from to"
			mFirstPage = fromField->GetValue();
			if (mFirstPage < 1)
				mFirstPage = 1;
			mLastPage = toField->GetValue();
			if (mLastPage > mDocument->GetNumberOfPages())
				mLastPage = mDocument->GetNumberOfPages();
			if (mFirstPage == mLastPage)
				mLastPage = kSubmitOnePage;
		}
	} else if (inMode == GSExportPanelMessage) {
		if (inMsg == msg_SelectPageRange && mDocument->HasDSC())
			pageRangeRB->SetValue(Button_On);
	}
}



// ---------------------------------------------------------------------------
//	€ HandlePageSizePanel
// ---------------------------------------------------------------------------

void
CGSDevice::HandlePageSizePanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
	static char				valueStr[50];
	static long				len;
	
	static short			lastActiveButton = 1;
	static PaperSize		lastFormat = kA4;
	static PageSizeUnit		lastUnit = kMillimeters;
	static double			lastWidth = 0, lastHeight = 0;
	
	static LRadioButton		*documentFormat, *standardFormat, *specialFormat;
	static LPopupButton		*formatMenu, *unitMenu;
	static LEditText		*widthField, *heightField;
	static LStaticText		*widthText, *heightText;
	
	if (inMode == GSExportPanelSetup) {
			// get pointers to our controls (only once in a dialogs life)
		documentFormat	= dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportPageSizeDocumentFormat));
		standardFormat	= dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportPageSizeStandardFormat));
		specialFormat	= dynamic_cast<LRadioButton*> (inView->FindPaneByID(kExportPageSizeSpecialFormat));
		formatMenu		= dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportPageSizeFormatPopup));
		unitMenu		= dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportPageSizeUnitPopup));
		widthField		= dynamic_cast<LEditText*> (inView->FindPaneByID(kExportPageSizeWidthEdit));
		heightField		= dynamic_cast<LEditText*> (inView->FindPaneByID(kExportPageSizeHeightEdit));
		widthText		= dynamic_cast<LStaticText*> (inView->FindPaneByID(kExportPageSizeWidthText));
		heightText		= dynamic_cast<LStaticText*> (inView->FindPaneByID(kExportPageSizeHeightText));
		
			// initialize with default values
		formatMenu->SetCurrentMenuItem(lastFormat);
		unitMenu->SetCurrentMenuItem(lastUnit);
		
		if (!lastWidth) {
			widthField->SetText("", 0);
		} else {
			len = sprintf(valueStr, "%.2f", lastWidth);
			widthField->SetText(valueStr, len);
		}
		if (!lastHeight) {
			heightField->SetText("", 0);
		} else {
			len = sprintf(valueStr, "%.2f", lastHeight);
			heightField->SetText(valueStr, len);
		}
		
		if (lastActiveButton == 1)		documentFormat->SetValue(Button_On);
		else if (lastActiveButton == 2)	standardFormat->SetValue(Button_On);
		else if (lastActiveButton == 3)	specialFormat->SetValue(Button_On);
		HandlePageSizePanel(GSExportPanelMessage, inView, msg_FormatRadioButtonClick);
	} else if (inMode == GSExportPanelAccept) {		// accept panel settings
		lastFormat = (PaperSize) formatMenu->GetCurrentMenuItem();
		widthField->GetText(valueStr, 50, &len); valueStr[len] = 0;
		lastWidth = atof(valueStr);
		heightField->GetText(valueStr, 50, &len); valueStr[len] = 0;
		lastHeight = atof(valueStr);
		
		mExportWithDocSize = false;
		if (documentFormat->GetValue()) {
			mExportWithDocSize = true;
			lastActiveButton = 1;
		} else if (standardFormat->GetValue()) {
			CUtilities::GetStandardPageSize((PaperSize) formatMenu->GetCurrentMenuItem(),
											mPageSize.h, mPageSize.v);
			lastActiveButton = 2;
		} else if (specialFormat->GetValue()) {
			PageSizeUnit	currUnit = (PageSizeUnit) unitMenu->GetCurrentMenuItem();
			
			widthField->GetText(valueStr, 50, &len); valueStr[len] = 0;
			mPageSize.h = CUtilities::ConvertUnit(currUnit, kPoints, atof(valueStr));
			
			heightField->GetText(valueStr, 50, &len); valueStr[len] = 0;
			mPageSize.v = CUtilities::ConvertUnit(currUnit, kPoints, atof(valueStr));
			
			lastActiveButton = 3;
		}
	} else if (inMode == GSExportPanelMessage) {	// handle other messages
		if (inMsg == msg_ChangeUnit) {
			PageSizeUnit	currUnit = (PageSizeUnit) unitMenu->GetCurrentMenuItem();
			double			value;
			
			widthField->GetText(valueStr, 50, &len); valueStr[len] = 0;
			value = CUtilities::ConvertUnit(lastUnit, currUnit, atof(valueStr));
			len = sprintf(valueStr, "%.2f", value);
			widthField->SetText(valueStr, len);
			
			heightField->GetText(valueStr, 50, &len); valueStr[len] = 0;
			value = CUtilities::ConvertUnit(lastUnit, currUnit, atof(valueStr));
			len = sprintf(valueStr, "%.2f", value);
			heightField->SetText(valueStr, len);
			
			lastUnit = currUnit;
		} else if (inMsg == msg_FormatRadioButtonClick) {
			if (documentFormat->GetValue()) {
				formatMenu->Disable();
				unitMenu->Disable();
				widthField->Disable();
				heightField->Disable();
				widthText->Disable();
				heightText->Disable();
			} else if (standardFormat->GetValue()) {
				formatMenu->Enable();
				unitMenu->Disable();
				widthField->Disable();
				heightField->Disable();
				widthText->Disable();
				heightText->Disable();
			} else if (specialFormat->GetValue()) {
				formatMenu->Disable();
				unitMenu->Enable();
				widthField->Enable();
				heightField->Enable();
				widthText->Enable();
				heightText->Enable();
			}
		}
	}
}



// ---------------------------------------------------------------------------
//	€ HandleRenderingPanel
// ---------------------------------------------------------------------------

void
CGSDevice::HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
#pragma unused(inMsg)
	
	// color menu is on by default and handled in the devices' own HandleRenderingPanel
	// if a device can't handle it, it must call colorMenu->Hide(); colorText->Hide()
	
	static LEditText		*resField;
	
	static short			lastResolution = 144;
	
	if (inMode == GSExportPanelSetup) {
		resField	= dynamic_cast<LEditText*> (inView->FindPaneByID(kExportRenderingResolution));
		
		// enter default values
		resField->SetValue(lastResolution);
		SetLatentSub(resField);
	} else if (inMode == GSExportPanelAccept) {
		lastResolution = resField->GetValue();
		SetResolution(lastResolution);
	}
}



// ---------------------------------------------------------------------------
//	€ HandleHalftonePanel
// ---------------------------------------------------------------------------

void
CGSDevice::HandleHalftonePanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
#pragma unused(inMsg)
	
	static LCheckBoxGroupBox	*halftoneCheckbox;
	static LPopupButton			*spotTypeMenu;
	static LEditText			*frequencyField, *angleField;
	
	static SpotType				lastType = kLineSpot;
	static short				lastFrequency = 0, lastAngle = 0;
	static bool					lastUseScreen = false;
	
	if (inMode == GSExportPanelSetup) {
		halftoneCheckbox	= dynamic_cast<LCheckBoxGroupBox*> (inView->FindPaneByID(kExportHalftoneCheckbox));
		spotTypeMenu		= dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportHalftoneSpotType));
		frequencyField		= dynamic_cast<LEditText*> (inView->FindPaneByID(kExportHalftoneFrequency));
		angleField			= dynamic_cast<LEditText*> (inView->FindPaneByID(kExportHalftoneAngle));
		
		halftoneCheckbox->SetValue(lastUseScreen);
		spotTypeMenu->SetCurrentMenuItem((short) lastType);
		if (!lastFrequency)	frequencyField->SetText("", 0);
		else				frequencyField->SetValue(lastFrequency);
		if (!lastAngle)	angleField->SetText("", 0);
		else			angleField->SetValue(lastAngle);
	} else if (inMode == GSExportPanelAccept) {
		lastUseScreen = mHalftoneScreen.useScreen = halftoneCheckbox->GetValue();
		lastType = mHalftoneScreen.type = (SpotType) spotTypeMenu->GetCurrentMenuItem();
		lastFrequency = mHalftoneScreen.frequency = frequencyField->GetValue();
		lastAngle = mHalftoneScreen.angle = angleField->GetValue();
	}
}



#pragma mark -
#pragma mark === Error Handling ===



// ---------------------------------------------------------------------------
//	€ ErrorDialog
// ---------------------------------------------------------------------------

bool
CGSDevice::ErrorDialog(int inError)
{
	if (!CGSApplication::GetPreferences()->GetShowErrorDialog() || (inError == kUserForcedAbortError)) {
		return true;
	}
	
	SInt16					itemHit;
	AlertStdAlertParamRec	alertParam;
	
	alertParam.movable			= true;
	alertParam.helpButton		= false;
	alertParam.filterProc		= NewModalFilterProc(UModalAlerts::GetModalEventFilter());
	alertParam.defaultText		= LStr255(kSTRListGeneral, kAbortStr);
	alertParam.cancelText		= nil;
	alertParam.otherText		= nil;
	alertParam.defaultButton	= kAlertStdAlertOKButton;
	alertParam.cancelButton		= kAlertStdAlertCancelButton;
	alertParam.position			= kWindowDefaultPosition;
	
	
	if (UAppleEventsMgr::InteractWithUser() == noErr) {
		::ThreadBeginCritical();
		UDesktop::Deactivate();
		
		if (inError == -25) {		// VMError
			::StandardAlert(kAlertCautionAlert, LStr255(kSTRListGeneral, kNotEnoughMemErrorStr),
							nil, &alertParam, &itemHit);
		} else {					// other PS-Error
			static char		filePath[256];
			static LStr255	fileName, gsVersion;
			
			mDocument->GetFilePath(filePath);
			fileName = filePath;
			fileName.Remove(0, fileName.ReverseFind("\p:"));
	
			gsVersion = GetDLL()->revision;
			gsVersion.Insert("\p.", 2);
			
			::ParamText("\p", gsVersion, LStr255((short) inError), fileName);
			::StandardAlert(kAlertCautionAlert, LStr255(kSTRListGeneral, kPostScriptErrorStr),
							nil, &alertParam, &itemHit);
		}
		
		UDesktop::Activate();
		::ThreadEndCritical();
	}
	
	::DisposeModalFilterUPP(alertParam.filterProc);
	
	if (itemHit == kAlertStdAlertCancelButton)
		return false;
	else
		return true;
}

