/* ============================================================================	*/
/*	CGSMacDevice.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "gdevmac"											*/
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


#include "CGSMacDevice.h"
#include "CGSSharedLib.h"
#include "CGSApplication.h"

#include <LString.h>
#include <UDesktop.h>

#include <string.h>



// ---------------------------------------------------------------------------------
//	€ CGSMacDevice
// ---------------------------------------------------------------------------------
//	Constructor

CGSMacDevice::CGSMacDevice(CGSDocument* inDocument)
		: CGSDevice(inDocument)
{
	sprintf(mDeviceName, "mac");
	
	SetIsScreenDevice(true);
	SetIsThreadedDevice(true);
	SetColorDepth(24);
	SetUseExternalFonts(false);
	
	mWaitingForDevice		= false;
	mWindow					= 0;
	
	mFileType				= 'PICT';
	mFileCreator			= 'ogle';
}



// ---------------------------------------------------------------------------------
//	€ CGSMacDevice
// ---------------------------------------------------------------------------------
//	Stream Constructor

CGSMacDevice::CGSMacDevice(LStream *inStream)
		: CPagePicture(inStream)
{
	sprintf(mDeviceName, "mac");
	
	SetIsScreenDevice(true);
	SetIsThreadedDevice(true);
	SetColorDepth(24);
	SetUseExternalFonts(false);
	
	mWaitingForDevice		= false;
	mWindow					= 0;
	mDocument				= 0;
	
	mFileType				= 'PICT';
	mFileCreator			= 'ogle';
}



// ---------------------------------------------------------------------------------
//	€ ~CGSMacDevice
// ---------------------------------------------------------------------------------
//	Destructor

CGSMacDevice::~CGSMacDevice()
{
}



// ---------------------------------------------------------------------------------
//	€ FinishCreateSelf
// ---------------------------------------------------------------------------------
//	Add the view to the listeners for messages from the callback function

void
CGSMacDevice::FinishCreateSelf()
{
	CGSSharedLib::Broadcaster()->AddListener(this);
}



// ---------------------------------------------------------------------------------
//	€ AdjustCursorSelf
// ---------------------------------------------------------------------------------

void
CGSMacDevice::AdjustCursorSelf(Point /* inPortPt */, const EventRecord& /* inMacEvent */)
{
	if (!mDocument->IsReady())
		UCursor::SetWatch();
	else if (mDocument->HasDSC() && IsRendering())
		UCursor::SetWatch();
	else
		UCursor::SetTheCursor(curs_HandOpen);
}



// ---------------------------------------------------------------------------------
//	€ Click
// ---------------------------------------------------------------------------------
//	Handle click in content region. Brings up the hand cursor to drag the content.

void
CGSMacDevice::Click(SMouseDownEvent &inMouseDown)
{
#pragma unused(inMouseDown)
	
	Point		prevClickCoord, mouseCoord;
	
	::GetMouse(&prevClickCoord);
	UCursor::SetTheCursor(curs_HandClosed);
	while(::StillDown()) {
		::GetMouse(&mouseCoord);
		if (mouseCoord.h != prevClickCoord.h || mouseCoord.v != prevClickCoord.v) {
			ScrollPinnedImageBy(prevClickCoord.h - mouseCoord.h,
								prevClickCoord.v - mouseCoord.v, true);
			prevClickCoord = mouseCoord;
		}
	}
	UCursor::SetTheCursor(curs_HandOpen);
}



// ---------------------------------------------------------------------------------
//	€ ListenToMessage
// ---------------------------------------------------------------------------------
//	Listens to messages from the callback function. Needed to know when a device
//	opens or closes or the display has to be sync'ed.

void
CGSMacDevice::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage) {
		case msg_GSOpenDevice:
			if (mWaitingForDevice) {	// view wants a device
				mWaitingForDevice = false;
				
				// Set GS device used for rendering
				mDevice = (unsigned char *) ioParam;
				
				// Get PICT used by mDevice
				long	thePict;
				GetDLL()->GetPict(mDevice, (PicHandle*) &thePict);
				
				// Make the PICT the displayed view
				SetPicture((PicHandle) thePict);
			}
			break;
		
		case msg_GSCloseDevice:
			if (mDevice == (unsigned char *) ioParam) {
				mDevice = 0;
				if (mRenderThread) {
					::SetThreadState(mRenderThread, kReadyThreadState, kNoThreadID);
				}
			}
			break;
		
		case msg_GSPage:
			// msg_GSPage is only needed for displaying/printing non-DSC documents
			// DSC documents know when a page is done when the thread died
			if (mDevice == (unsigned char *) ioParam) {
				if (mDeviceIsPrinting) {
					PictureDone();
					::PrOpenPage(mPrinterPort, NULL);
					if(::PrError() == noErr)
						DrawToRect(&((*mPrintRecordH)->rPaper));
					::PrClosePage(mPrinterPort);
				} else {
					if (!mDocument->HasDSC()) {
						SuspendRendering();
					}
				}
			}
			break;
		
		default:
			break;
	}
}



// ---------------------------------------------------------------------------------
//	€ SuspendRendering
// ---------------------------------------------------------------------------------

void
CGSMacDevice::SuspendRendering()
{
	if (!mRenderThread)
		return;
	
	if (mWindow)
		mWindow->GetToolbar()->HideChasingArrows();
	
	if (mDrawBuffered && mPicBuffer) {
		// we use a draw buffer, so we can wait until we know if another
		// page will come or if the device will be closed before we draw
		// the page to screen. so we know if the page is the last page and
		// can set the menus accordingly before the user has to do another
		// next page command.
		if (!mStopThread) {
			// this can only happen once (for the first page)
			mStopThread = true;
			PictureFinished();
		} else {
			// delay every page display until cmd_NextPage
			::SetThreadState(mRenderThread, kStoppedThreadState, kNoThreadID);
		}
	} else {
		// we cannot foresee the last page, so it is possible to do a next page
		// command although the last page was rendered. this will not cause an
		// error, but it is bad user interface.
		PictureFinished();
		::SetThreadState(mRenderThread, kStoppedThreadState, kNoThreadID);
	}
}



// ---------------------------------------------------------------------------------
//	€ ResumeRendering
// ---------------------------------------------------------------------------------

void
CGSMacDevice::ResumeRendering()
{
	if (!mRenderThread)
		return;
	
	if (mWindow)
		mWindow->GetToolbar()->ShowChasingArrows();
	
	if (mDrawBuffered && mPicBuffer)
		PictureFinished();
	
	::SetThreadState(mRenderThread, kReadyThreadState, kNoThreadID);
}



// ---------------------------------------------------------------------------
//	€ RenderPage
// ---------------------------------------------------------------------------

void
CGSMacDevice::RenderPage(int inPage)
{
	mWaitingForDevice = true;
	mStopThread = false;
	
	if (mWindow && !mDeviceIsExporting && !mDeviceIsPrinting)
		mWindow->GetToolbar()->ShowChasingArrows();
	
	if (mAntiAliasing && !mDeviceIsExporting && !mDeviceIsPrinting)
		SetResolution(mHWResolution.h * 2);
	
	CGSDevice::RenderPage(inPage);
}

void
CGSMacDevice::RenderThreadDied(ThreadID id, void* arg)
{
#pragma unused(id, arg)
	if (mAntiAliasing && !mDeviceIsExporting && !mDeviceIsPrinting)
		SetResolution(mHWResolution.h / 2);
	
	if (mWindow && (mLastError != kUserForcedAbortError))
		PictureFinished();
	
	LCommander::SetUpdateCommandStatus(true);
	
	if (mWindow && !mDeviceIsExporting && !mDeviceIsPrinting)
		mWindow->GetToolbar()->HideChasingArrows();
}



// ---------------------------------------------------------------------------
//	€ PictureFinished
// ---------------------------------------------------------------------------

void
CGSMacDevice::PictureFinished()
{
	PictureDone();
	if (mWindow) {
		FocusDraw(this);
		DrawSelf();
	}
}



// ---------------------------------------------------------------------------
//	€ PrintNonDSCDocument
// ---------------------------------------------------------------------------

int
CGSMacDevice::PrintNonDSCDocument(THPrint inPrintRecordH)
{
	int	err = 0;
	
	// no spooled printing yet! I guess it is only used by VERY old printers
	if ((*inPrintRecordH)->prJob.bJDocLoop == bSpoolLoop)
		return -1;
	
	mDeviceIsPrinting = true;
	mPrintRecordH = inPrintRecordH;
	
	mPrinterPort = ::PrOpenDoc(inPrintRecordH, NULL, NULL);
	if (::PrError() == noErr) {
		PrintPrefs		*printPrefs = CGSApplication::GetPreferences()->GetPrintSettings();
		
		SetUseExternalFonts(printPrefs->renderSettings.useExternalFonts);
		
		if (printPrefs->useRenderSettings) {
			switch (printPrefs->renderSettings.colorDepth) {
				case kBlackWhite:		SetColorDepth(1);	break;
				case k16Grays:			SetColorDepth(4);	break;
				case k256Grays:			SetColorDepth(7);	break;
				case k256Colors:		SetColorDepth(8);	break;
				case kMillionColors:	SetColorDepth(24);	break;
			}
		} else {
			SetColorDepth(((mPrinterPort->gPort).portBits.rowBytes < 0) ? 24 : 1);
		}
		
		if (printPrefs->useHalftoneSettings) {
			SetUseHalftoneScreen(true);
			SetHalftoneScreen(printPrefs->halftoneSettings.frequency,
							  printPrefs->halftoneSettings.angle,
							  printPrefs->halftoneSettings.spotType);
		} else {
			SetUseHalftoneScreen(false);
		}
		
		RenderPage(kSubmitWholeDocument);
		::PrCloseDoc(mPrinterPort);
	} else {
		err = -1;
	}
	
	mDeviceIsPrinting = false;
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ SetupDevice
// ---------------------------------------------------------------------------

int
CGSMacDevice::SetupDevice()
{
	GetDLL()->ExecStr("mark\n");
	SubmitDeviceParameters();
	return GetDLL()->ExecStr("0 .getdevice copydevice putdeviceprops setdevice\n");
}



// ---------------------------------------------------------------------------
//	€ DeviceSetupDone
// ---------------------------------------------------------------------------

void
CGSMacDevice::DeviceSetupDone()
{
#if 0
//					CGSSharedLib::ExecStr("gsave 45 rotate\n");
	switch (mDocument->GetPageOrientation(mFirstPage)) {
		case PORTRAIT:
			break;
		case SEASCAPE:
		case LANDSCAPE:
//			CGSSharedLib::ExecStr("gsave 45 rotate\n");
/*			CGSSharedLib::ExecStr("gsave clippath pathbbox grestore\n");
			CGSSharedLib::ExecStr("4 dict begin\n");
			CGSSharedLib::ExecStr("/ury exch def /urx exch def /lly exch def /llx exch def\n");
			CGSSharedLib::ExecStr("45 rotate llx neg llx urx sub lly sub translate\n");
			CGSSharedLib::ExecStr("end\n");*/
			break;
	}
#endif
	
	CGSDevice::DeviceSetupDone();
}



// ---------------------------------------------------------------------------
//	€ SubmitDeviceParameters
// ---------------------------------------------------------------------------

void
CGSMacDevice::SubmitDeviceParameters()
{
#if 0
	switch (mDocument->GetPageOrientation(mFirstPage)) {
		case PORTRAIT:
			break;
		case SEASCAPE:
			SetPageSize(mPageSize.v, mPageSize.h);
			break;
		case LANDSCAPE:
			SetPageSize(mPageSize.v, mPageSize.h);
			break;
	}
#endif
	
	CGSDevice::SubmitDeviceParameters();
	
	char	paramStr[1024];
	sprintf(paramStr, "/BitsPerPixel %d /UseExternalFonts %s\n",
			mBitsPerPixel, mUseExternalFonts ? "true" : "false");
	GetDLL()->ExecStr(paramStr);
}



// ---------------------------------------------------------------------------
//	€ HandleRenderingPanel
// ---------------------------------------------------------------------------

void
CGSMacDevice::HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg)
{
	static LPopupButton		*colorMenu;
	
	CGSDevice::HandleRenderingPanel(inMode, inView, inMsg);
	
	if (inMode == GSExportPanelSetup) {
		colorMenu = dynamic_cast<LPopupButton*> (inView->FindPaneByID(kExportRenderingColorDepth));
		
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportBlackWhiteStr));		// item 1
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExport16GraysStr));			// item 2
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExport256GraysStr));			// item 3
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExport256ColorsStr));		// item 4
		colorMenu->AppendMenu(LStr255(kSTRListExport, kExportMillionsColorsStr));	// item 5
		colorMenu->SetCurrentMenuItem(5);
	} else if (inMode == GSExportPanelAccept) {
		switch (colorMenu->GetCurrentMenuItem()) {
			case 1: SetColorDepth(1); break;	// item 1 = Black and White
			case 2: SetColorDepth(4); break;	// item 2 = 16 Grays
			case 3: SetColorDepth(7); break;	// item 3 = 256 Grays
			case 4: SetColorDepth(8); break;	// item 4 = 256 Colors
			case 5: SetColorDepth(24); break;	// item 5 = Millions of Colors
		}
	}
}



