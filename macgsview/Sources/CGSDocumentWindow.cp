/* ============================================================================	*/
/*	CGSDocumentWindow.cp					written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	Instantiates from LWindow.													*/
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


#include <LMenu.h>
#include <LMenuBar.h>
#include <LCmdBevelButton.h>
#include <LListener.h>
#include <LStaticText.h>
#include <LScrollerView.h>
#include <LString.h>
#include <UAMModalDialogs.h>
#include <UWindows.h>
#include <UCursor.h>
#include <UModalDialogs.h>
#include <UDrawingUtils.h>
#include <UDrawingState.h>
#include <UScreenPort.h>
#include <LEditText.h>
#include <UDesktop.h>
#include <UProcess.h>
#include <PP_KeyCodes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <PictUtils.h>
#include <Sound.h>
#include <ToolUtils.h>

#include "MacGSViewConstants.h"
#include "CGSSharedLib.h"
#include "CGSDocument.h"
#include "CGSDocumentWindow.h"
#include "CGSMacDevice.h"
#include "CGSApplication.h"



// ---------------------------------------------------------------------------------
//	€ CGSDocumentWindow
// ---------------------------------------------------------------------------------
//	Constructor

CGSDocumentWindow::CGSDocumentWindow()
{
	Init();
}



// ---------------------------------------------------------------------------------
//	€ CGSDocumentWindow
// ---------------------------------------------------------------------------------
//	Constructor

CGSDocumentWindow::CGSDocumentWindow( const SWindowInfo &inWindowInfo )
		: LWindow( inWindowInfo )
{
	Init();
}



// ---------------------------------------------------------------------------------
//	€ CGSDocumentWindow
// ---------------------------------------------------------------------------------
//	Constructor

CGSDocumentWindow::CGSDocumentWindow( ResIDT inWINDid, UInt32 inAttributes,
									LCommander *inSuperCommander )
		: LWindow( inWINDid, inAttributes, inSuperCommander )
{
	Init();
}



// ---------------------------------------------------------------------------------
//	€ CGSDocumentWindow
// ---------------------------------------------------------------------------------
//	Constructor

CGSDocumentWindow::CGSDocumentWindow( LStream *inStream )
		: LWindow( inStream )
{
	Init();
}



// ---------------------------------------------------------------------------------
//	€ ~CGSDocumentWindow
// ---------------------------------------------------------------------------------
//	Destructor

CGSDocumentWindow::~CGSDocumentWindow()
{
	Rect	dummyRect;
	CGSApplication::GetPreferences()->SetWindowSize(mUserBounds, CalcStandardBounds(dummyRect));
}



// ---------------------------------------------------------------------------------
//	€ Init
// ---------------------------------------------------------------------------------
//	Initialization needed from all Constructors

void
CGSDocumentWindow::Init()
{
	mDocument = 0;
	mZoom = 100;
	mPage = 0;
}



// ---------------------------------------------------------------------------------
//	€ FinishCreateSelf
// ---------------------------------------------------------------------------------
//	Sets standard values for Page- and Zoom-Button on the window

void
CGSDocumentWindow::FinishCreateSelf()
{
	// set default window size/zoomstate
	Rect	winRect;
	bool	winZoomed;
	CGSApplication::GetPreferences()->GetWindowSize(winRect, winZoomed);
	ResizeWindowTo(winRect.right - winRect.left, winRect.bottom - winRect.top);
	if (winZoomed)
		SendAESetZoom();
	
	mToolbar = dynamic_cast<CGSToolbar*> (FindPaneByID(kWindowGSToolbar));
	mView = dynamic_cast<CGSMacDevice*> (FindPaneByID(kWindowGSView));
	if (mView)
		mView->SetWindow(this);
}



// ---------------------------------------------------------------------------------
//	€ SetDocument
// ---------------------------------------------------------------------------------
//	Sets mDocument of the Window and the view

void
CGSDocumentWindow::SetDocument(CGSDocument* theDocument)
{
	mDocument = theDocument;
	if (mView) mView->SetDocument(mDocument);
}



// ---------------------------------------------------------------------------------
//	€ GetDLL
// ---------------------------------------------------------------------------------

CGSSharedLib*
CGSDocumentWindow::GetDLL()
{
	return mView->GetDLL();
}



// ---------------------------------------------------------------------------------
//	€ FindCommandStatus
// ---------------------------------------------------------------------------------
//	Enables View Menu (has to be enabled whenever a document window exists).
//	Enables/Disables Page/Zoom-Buttons/Menus

void
CGSDocumentWindow::FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									 Boolean &outUsesMark, UInt16 &outMark, Str255 outName)
{
	if (!mDocument->IsReady()) {
		// handle special situtation when document is not fully loaded
		switch (inCommand) {
			case MENU_View:
			case cmd_Print:
			case cmd_SaveAs:
			case cmd_AddEPSPreview:
			case cmd_AbortRendering:
			case cmd_FirstPage:
			case cmd_PreviousPage:
			case cmd_LastPage:
			case cmd_NextPage:
			case cmd_GoToPage:
			case cmd_PageButton:
			case cmd_FitPage:
			case cmd_FitWidth:
			case cmd_ZoomButton:
			case cmd_ZoomUp:
			case cmd_ZoomDown:
			case cmd_ZoomTo:
			case cmd_Copy:
			case cmd_AbortExport:
				outEnabled = false;
				break;
			case cmd_RerenderPage:
				LString::CopyPStr(LStr255(kSTRListGeneral, kRerenderPageStr), outName);
				outEnabled = false;
				break;
			default:
				LCommander::FindCommandStatus(inCommand, outEnabled,
											  outUsesMark, outMark, outName);
		}
		return;
	}
	
	switch (inCommand) {
		case cmd_Print:
			outEnabled = true;
			break;
		
		case cmd_SaveAs:
			if (mDocument->GetExportDevice())
				outEnabled = false;
			else
				LCommander::FindCommandStatus(inCommand, outEnabled,
											  outUsesMark, outMark, outName);
			break;
		
		case cmd_AddEPSPreview:
			if (mDocument->IsEPS())
				outEnabled = true;
			else
				outEnabled = false;
			break;
			
		case MENU_View:
			if (mView->IsRendering())
				outEnabled = false;
			else
				outEnabled = true;
			break;
		
		case cmd_RerenderPage:
			if (mDocument->HasDSC())
				LString::CopyPStr(LStr255(kSTRListGeneral, kRerenderPageStr), outName);
			else
				LString::CopyPStr(LStr255(kSTRListGeneral, kRerenderDocumentStr), outName);
			
			if (mView->IsRendering())
				outEnabled = false;
			else
				outEnabled = true;
			break;
		
		case cmd_AbortRendering:
			if (mView->IsRendering())
				outEnabled = true;
			else
				outEnabled = false;
			break;
		
		case cmd_AbortExport:
			if (mDocument->GetExportDevice())	// export device is in use
				outEnabled = true;
			else
				outEnabled = false;
			break;
		
		case cmd_FirstPage:
			if (!mDocument->HasDSC() && !mView->IsRendering()) {
				outEnabled = true;
				break;
			}
		case cmd_PreviousPage:
			if (mDocument->HasDSC()) {
				if (!mView->IsRendering() && (mPage > 1))
					outEnabled = true;
				else
					outEnabled = false;
			} else {
				outEnabled = false;
			}
			break;
		
		case cmd_LastPage:
			if (!mDocument->HasDSC()) {
				outEnabled = false;
				break;
			}
		case cmd_NextPage:
			if (mDocument->HasDSC()) {
				if (!mView->IsRendering() && (mPage < mDocument->GetNumberOfPages()))
					outEnabled = true;
				else
					outEnabled = false;
			} else {
				if (mView->IsRendering())
					outEnabled = true;
				else
					outEnabled = false;
			}
			break;
		
		case cmd_GoToPage:
			if (!mDocument->HasDSC()) {
				outEnabled = false;
				break;
			}
		case cmd_PageButton:
			if (mDocument->HasDSC()) {
				if (!mView->IsRendering() && (mDocument->GetNumberOfPages() > 1))
					outEnabled = true;
				else
					outEnabled = false;
			} else {
				if (mView->IsRendering())
					outEnabled = true;
				else
					outEnabled = false;
			}
			break;
		
		case cmd_FitPage:
		case cmd_FitWidth:
		case cmd_ZoomButton:
		case cmd_ZoomTo:
			if (mView->IsRendering())
				outEnabled = false;
			else
				outEnabled = true;
			break;
		
		case cmd_ZoomUp:
			if (mView->IsRendering() || mZoom>=800)
				outEnabled = false;
			else
				outEnabled = true;
			break;
			
		case cmd_ZoomDown:
			if (mView->IsRendering() || mZoom<=25)
				outEnabled = false;
			else
				outEnabled = true;
			break;
		
		case cmd_Copy:
			if (mView->IsRendering())
				outEnabled = false;
			else
				outEnabled = true;
			break;
		
		default:
			LCommander::FindCommandStatus(inCommand, outEnabled,
										  outUsesMark, outMark, outName);
	}
}



// ---------------------------------------------------------------------------------
//	€ ObeyCommand
// ---------------------------------------------------------------------------------
//	Handle commands

Boolean
CGSDocumentWindow::ObeyCommand( CommandT inCommand, void* ioParam )
{
	Boolean cmdHandled = true;
	
	SPoint32		scrollPos;
	mView->GetScrollPosition(scrollPos);
	
	switch (inCommand)
	{
		case cmd_AddEPSPreview:
			mDocument->WriteEPSPreviewResource();
			break;
		
		case cmd_RerenderPage:
			if (!mView->IsRendering())
				RenderContent();
			break;
		
		case cmd_AbortRendering:
			mView->GetDLL()->Abort();
			::SetThreadState(mView->GetThread(), kReadyThreadState, kNoThreadID);
			break;
		
		case cmd_AbortExport:
			mDocument->GetToolbar()->HideStopButton();
			mDocument->GetToolbar()->SetStatusText(LStr255(kSTRListToolbar, kToolbarAbortingStr));
			mDocument->GetToolbar()->SetProgress(-1);
			mDocument->GetExportDevice()->GetDLL()->Abort();
			break;
		
		case cmd_FirstPage:
			if (mDocument->HasDSC()) {
				if (!mView->IsRendering()) {
					GotoPage(1);
					mView->ScrollPinnedImageTo(scrollPos.h, 0, false);
				}
			} else {
				if (!mView->IsRendering()) {
					RenderContent();
					mView->ScrollPinnedImageTo(scrollPos.h, 0, false);
				}
			}
			break;
		
		case cmd_PreviousPage:
			if (mDocument->HasDSC() && (mPage > 1) &&  !mView->IsRendering()) {
				SDimension32	imageSize;
				mView->GetImageSize(imageSize);
				
				GotoPage(mPage-1);
				mView->ScrollPinnedImageTo(scrollPos.h, imageSize.height, false);
			}
			break;
		
		case cmd_NextPage:
			if (mDocument->HasDSC()) {
				if (mPage < mDocument->GetNumberOfPages() && !mView->IsRendering()) {
					GotoPage(mPage+1);
					mView->ScrollPinnedImageTo(scrollPos.h, 0, false);
				}
			} else {
				if (mView->IsRendering()) {
					mView->ScrollPinnedImageTo(scrollPos.h, 0, false);
					mView->ResumeRendering();
				}
			}
			break;
		
		case cmd_LastPage:
			if (mDocument->HasDSC() && !mView->IsRendering()) {
				GotoPage(mDocument->GetNumberOfPages());
				mView->ScrollPinnedImageTo(scrollPos.h, 0, false);
			}
			break;
		
		case cmd_FitPage:
			if (!mView->IsRendering())
				ZoomToFitPage();
			break;
		
		case cmd_FitWidth:
			if (!mView->IsRendering())
				ZoomToFitWidth();
			break;
		
		case cmd_ZoomTo:
			if (!mView->IsRendering())
				HandleWindowZoomButton(true);
			break;
		
		case cmd_ZoomButton:
			if (!mView->IsRendering())
				HandleWindowZoomButton(false);
			break;
		
		case cmd_ZoomUp:
			if (!mView->IsRendering())
				GotoZoom(mZoom*1.44);
			break;
		
		case cmd_ZoomDown:
			if (!mView->IsRendering())
				GotoZoom(mZoom/1.44);
			break;
		
		case cmd_GoToPage:
			if (!mView->IsRendering())
				HandleWindowPageButton();
			break;
		
		case cmd_PageButton:
			if (mDocument->HasDSC()) {
				if (!mView->IsRendering())
					HandleWindowPageButton();
			} else {
				mView->ResumeRendering();
			}
			break;
		
		case cmd_Copy:
			mView->CopyPict();
			break;
		
		default:
			cmdHandled = LCommander::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}


// ---------------------------------------------------------------------------
//	€ HandleKeyPress
// ---------------------------------------------------------------------------
//	Respond to key commands

Boolean
CGSDocumentWindow::HandleKeyPress(const EventRecord& inKeyEvent)
{
	if (inKeyEvent.what == keyDown) {
		SDimension16	step;
		mView->GetFrameSize(step);
		
		switch (inKeyEvent.message & charCodeMask) {
			case char_Home:
				ProcessCommand(cmd_FirstPage);
				return true;
				
			case char_End:
				ProcessCommand(cmd_LastPage);
				return true;
				
			case char_PageDown:
				if (mView->ScrollPinnedImageBy(0, step.height, true) == false)
					ProcessCommand(cmd_NextPage);
				return true;
				
			case char_PageUp:
				if (mView->ScrollPinnedImageBy(0, -step.height, true) == false)
					ProcessCommand(cmd_PreviousPage);
				return true;
				
			case char_UpArrow:
				mView->ScrollPinnedImageBy(0, -step.height, true);
				return true;
				
			case char_DownArrow:
				mView->ScrollPinnedImageBy(0, step.height, true);
				return true;
				
			case char_LeftArrow:
				mView->ScrollPinnedImageBy(-step.width, 0, true);
				return true;
				
			case char_RightArrow:
				mView->ScrollPinnedImageBy(step.width, 0, true);
				return true;
		}
	}
	
	return LWindow::HandleKeyPress(inKeyEvent);
}



// ---------------------------------------------------------------------------------
//	€ HandleClick
// ---------------------------------------------------------------------------------

void
CGSDocumentWindow::HandleClick(const EventRecord &inMacEvent, SInt16 inPart)
{
	bool	handled = false;
	
	switch (inPart) {
		case inProxyIcon:
			if (!handled) {
				OSStatus status = ::TrackWindowProxyDrag(GetMacPort(), inMacEvent.where);
				if(status != errUserWantsToDragWindow) handled = false;
			}
		
		case inDrag:
			if (!handled) {
				EventRecord	event = inMacEvent;
				if (::IsWindowPathSelectClick(GetMacPort(), &event)) {
					SInt32 itemSelected;
					if (::WindowPathSelect(GetMacPort(), NULL, &itemSelected ) == noErr) {
						// If the menu item selected is not the title of the window, switch
						// to the Finder, since the window chosen probably isn¹t visible
						if (LoWord(itemSelected) > 1) {
							ProcessSerialNumber finderPSN = UProcess::GetPSN(FOUR_CHAR_CODE('MACS'),
																			 FOUR_CHAR_CODE('FNDR'));
							UProcess::SetFront(finderPSN);
						}
					}
					handled = true;
				}
			}
		
		default:
			if (!handled) LWindow::HandleClick(inMacEvent, inPart);
	}
}



// ---------------------------------------------------------------------------------
//	€ InitZoom
// ---------------------------------------------------------------------------------

void
CGSDocumentWindow::InitZoom()
{
	LCmdBevelButton	*zoomButton = dynamic_cast<LCmdBevelButton*> (FindPaneByID(kWindowZoomButton));
	zoomButton->SetCurrentMenuItem(CGSApplication::GetPreferences()->GetDefaultZoomMenuItem());
	mPage = 1;
	HandleWindowZoomButton(false, true);
	mPage = 0;
}



// ---------------------------------------------------------------------------------
//	€ HandleWindowZoomButton
// ---------------------------------------------------------------------------------

void
CGSDocumentWindow::HandleWindowZoomButton(bool callFromMenu, bool dontRefresh)
{
	LCmdBevelButton		*zoomButton = dynamic_cast<LCmdBevelButton*>
									  (FindPaneByID(kWindowZoomButton));
	SInt16				currentItem = zoomButton->GetCurrentMenuItem();
	
	if (callFromMenu)
		currentItem = zoomToValue;
	
	switch (currentItem) {
		case zoomToValue:
			SInt32	newZoom = mZoom;
			if (UAMModalDialogs::AskForOneNumber(this, PPob_ZoomToDialog, 'Zoom', newZoom))
				GotoZoom(newZoom, dontRefresh);
			break;
		
		case zoomFitPage:
			ZoomToFitPage(dontRefresh);
			break;
		
		case zoomFitWidth:
			ZoomToFitWidth(dontRefresh);
			break;
		
		default:
			LStr255		theMenuItemText;
			zoomButton->GetMenuItemText(currentItem, theMenuItemText);
			zoomButton->SetDescriptor(theMenuItemText);
			theMenuItemText.Remove(theMenuItemText.Length() - 1, 2);	// Remove the ' %'
			GotoZoom((SInt32) theMenuItemText, dontRefresh);
			break;
	}
}



// ---------------------------------------------------------------------------------
//	€ ZoomToFitPage
// ---------------------------------------------------------------------------------

void
CGSDocumentWindow::ZoomToFitPage(bool dontRefresh)
{
	SDimension16	frame;
	float			pageWidth, pageHeight, fitWidthZoom, fitHeightZoom;
	
	mView->GetFrameSize(frame);
	mDocument->GetPageSize(mPage, pageWidth, pageHeight);
	
	fitHeightZoom = (frame.height-12) / pageHeight * 100.0;
	fitWidthZoom  = (frame.width-12) / pageWidth * 100.0;
	
	if (fitHeightZoom < fitWidthZoom)
		GotoZoom(fitHeightZoom, dontRefresh);
	else
		GotoZoom(fitWidthZoom, dontRefresh);
}



// ---------------------------------------------------------------------------------
//	€ ZoomToFitWidth
// ---------------------------------------------------------------------------------

void
CGSDocumentWindow::ZoomToFitWidth(bool dontRefresh)
{
	SDimension16	frame;
	float			pageWidth, pageHeight;
	
	mView->GetFrameSize(frame);
	mDocument->GetPageSize(mPage, pageWidth, pageHeight);
	
	GotoZoom(((frame.width-12) / pageWidth * 100.0), dontRefresh);
}



// ---------------------------------------------------------------------------------
//	€ GotoZoom
// ---------------------------------------------------------------------------------

void
CGSDocumentWindow::GotoZoom(SInt32 inZoom, bool dontRefresh)
{
	if (mZoom == inZoom)
		return;
	
	if (inZoom < 0)
		return;
	
	// set new zoom value
	mZoom = inZoom;
	
	// display new zoom factor
	LCmdBevelButton		*zoomButton = dynamic_cast<LCmdBevelButton*>
									  (FindPaneByID(kWindowZoomButton));
	LStr255		zoomString = mZoom;
	zoomString += "\p %";
	zoomButton->SetDescriptor(zoomString);
	
	// Is mZoom a menu value?
	LStr255		menuItemText;
	SInt16		menuItemCount = ::CountMItems(zoomButton->GetMacMenuH());
	
	zoomButton->SetCurrentMenuItem(zoomToValue);	// assume its no standard value
	for (SInt16 menuItem = 1; menuItem < menuItemCount; menuItem++) {
		if (zoomButton->GetMenuItemText(menuItem, menuItemText) == zoomString) {
			zoomButton->SetCurrentMenuItem(menuItem);
			break;
		}
	}
	
	if (!dontRefresh)
		RenderContent();
}




// ---------------------------------------------------------------------------------
//	€ HandleWindowPageButton
// ---------------------------------------------------------------------------------

void
CGSDocumentWindow::HandleWindowPageButton()
{
	StDialogHandler	theHandler(PPob_GotoPageDialog, this);
	
	LWindow*		theDialog = theHandler.GetDialog();
	LEditText		*theField = dynamic_cast<LEditText*> (theDialog->FindPaneByID('Page'));
	LStaticText		*theMaxPages = dynamic_cast<LStaticText*> (theDialog->FindPaneByID('PgMx'));
	LStr255			theMaxPagesString = "of ";
	theMaxPagesString += (short) mDocument->GetNumberOfPages();
	
	SInt32	pageNew = mPage;
	
	theMaxPages->SetDescriptor(theMaxPagesString);
	theField->SetValue(pageNew);
	theField->SelectAll();
	theDialog->SetLatentSub(theField);
	
	theDialog->Show();
	
	while (true) {
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_Cancel) {
			return;
		} else if (hitMessage == msg_OK) {
			pageNew = theField->GetValue();
			break;
		}
	}
	
	//ShowSelf();		// is needed, but dont know why?!?
	
	if (pageNew > mDocument->GetNumberOfPages() || pageNew <= 0)	// entered Page doesnt exist
		::SysBeep(1);		// Beep to indicate error
	else
		GotoPage(pageNew);
}



// ---------------------------------------------------------------------------------
//	€ GotoPage
// ---------------------------------------------------------------------------------
//	Goto a certain page.

void
CGSDocumentWindow::GotoPage(SInt32 pageNumber)
{
	LCmdBevelButton		*pageButton = dynamic_cast<LCmdBevelButton*>
									  (FindPaneByID(kWindowPageButton));
	
	if (mDocument->HasDSC()) {
		if (pageNumber == mPage)	// is it really a new page?
			return;
		
		mPage = pageNumber;
		
		// Display new page number
		LStr255		thePageStr(kSTRListPageNumber, kPageNumberPageStr);
		thePageStr += "\p ";
		thePageStr += mPage;
		thePageStr += "\p ";
		thePageStr += LStr255(kSTRListPageNumber, kPageNumberOfStr);
		thePageStr += "\p ";
		thePageStr += (short) mDocument->GetNumberOfPages();
		pageButton->SetDescriptor(thePageStr);
	} else {
		pageButton->SetDescriptor(LStr255(kSTRListPageNumber, kPageNumberNextPageStr));
	}
	
	// Render new page
	RenderContent();
}



// ---------------------------------------------------------------------------------
//	€ RenderContent
// ---------------------------------------------------------------------------------
//	Tell the view to render the current page.

void
CGSDocumentWindow::RenderContent()
{
	if (!mView)	return;
	
	// set device according to preferences
	mView->SetResolution((float) mZoom / 100.0 * 72.0);
	mView->SetAntiAliasing(CGSApplication::GetPreferences()->GetAntiAliasing());
	mView->SetDrawBuffered(CGSApplication::GetPreferences()->GetDrawBuffered());
	mView->SetTransparency(CGSApplication::GetPreferences()->GetTextTransparency(),
						   CGSApplication::GetPreferences()->GetGraphicsTransparency());
	mView->SetUseExternalFonts(CGSApplication::GetPreferences()->GetUseExternalFonts());
	switch (CGSApplication::GetPreferences()->GetColorDepth()) {
		case kBlackWhite:		mView->SetColorDepth(1);	break;
		case k16Grays:			mView->SetColorDepth(4);	break;
		case k256Grays:			mView->SetColorDepth(7);	break;
		case k256Colors:		mView->SetColorDepth(8);	break;
		case kMillionColors:	mView->SetColorDepth(24);	break;
		case kMaximumColors: {
			int  depth   = UDrawingUtils::GetPortPixelDepth(UScreenPort::GetScreenPort());
			bool isColor = UDrawingUtils::DeviceSupportsColor(::GetGDevice());
			if (isColor) {
				if      (depth > 8) mView->SetColorDepth(24);
				else                mView->SetColorDepth(8);
			} else {
				if      (depth > 4) mView->SetColorDepth(7);
				else if (depth > 1) mView->SetColorDepth(4);
				else                mView->SetColorDepth(1);
			}
			break;
		}
	}
	
	if ((mDocument->IsEPS()) || (!mDocument->HasDSC()))
		mView->RenderPage(kSubmitWholeDocument);
	else
		mView->RenderPage(mPage);
}



// ---------------------------------------------------------------------------------
//	€ SetCTable
// ---------------------------------------------------------------------------------
//	Set the windows CTable to the best suited for the picture.
//	Doesnt work at all!!

void
CGSDocumentWindow::SetCTable()
{
#if 0

	PixMapHandle	thePixMap = ((CGrafPtr) GetMacPort())->portPixMap;
	
	if ((**thePixMap).pixelType == 0)		// indexed PixMap
	{
		int		numBits = (**thePixMap).pixelSize, numColors = 2;
		while (--numBits) numColors *= 2;	// numColors = 2^pixelSize
		
		PictInfo	picInf;
		GetPictInfo(mView->GetPicture(), &picInf, returnColorTable, numColors, systemMethod, 0);
		
		(**thePixMap).pmTable = picInf.theColorTable;
		PortChanged(GetMacPort());
	}

#endif
}

