/* ============================================================================	*/
/*	CNavCustomDialog.cp						written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	This class provides customized NavServices dialogs.							*/
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


#include <LFileTypeList.h>
#include <LString.h>
#include <UDesktop.h>
#include <UModalDialogs.h>

#include <string.h>

#include "CNavCustomDialog.h"
#include "CUtilities.h"



pascal void (* CNavCustomDialog::mCustomCallBackProc) (NavEventCallbackMessage, NavCBRecPtr, NavCallBackUserData) = 0;
void (* CNavCustomDialog::mCustomPopupMenuProc)(OSType menuCreator, OSType menuType, NavCBRecPtr params) = 0;
void (* CNavCustomDialog::mSetupCustomControlsProc)(NavCBRecPtr callBackParams, short inFirstItem) = 0;
void (* CNavCustomDialog::mAcceptCustomControlsProc)(NavCBRecPtr callBackParams, short inFirstItem) = 0;
void (* CNavCustomDialog::mCustomProcessEventProc)(NavCBRecPtr callBackParams, short inFirstItem) = 0;


// ---------------------------------------------------------------------------
//	€ CNavCustomDialog
// ---------------------------------------------------------------------------
//	Constructor

CNavCustomDialog::CNavCustomDialog()
{
	mTypeList = 0;
	NavGetDefaultDialogOptions(&mOptions);
	
	::AECreateDesc(typeNull, 0, 0, &mDefaultLocation);
	
	mFileType	 = FOUR_CHAR_CODE('????');
	mFileCreator = LFileTypeList::GetProcessSignature();
	
	mCustomControlsDITL = 0;
	
	mDefaultMenuItem.version = 0;
	mDefaultMenuItem.menuType = mDefaultMenuItem.menuCreator = 0;
	LString::CopyPStr("\p", mDefaultMenuItem.menuItemName);
}



// ---------------------------------------------------------------------------
//	€ ~CNavCustomDialog
// ---------------------------------------------------------------------------
//	Destructor

CNavCustomDialog::~CNavCustomDialog()
{
	if (mTypeList)
		delete mTypeList;
	
	::AEDisposeDesc(&mDefaultLocation);
}



// ---------------------------------------------------------------------------
//	€ ChooseFolder
// ---------------------------------------------------------------------------
//	

OSErr
CNavCustomDialog::ChooseFolder(FSSpec& outFolderFSSpec,
							   void pascal (*inEventProc)(),
							   Boolean pascal (*inFilterProc)(AEDesc *,void *,void *,NavFilterModes))
{
	OSErr	err;
	
	NavEventUPP			eventProc = NULL;
	NavObjectFilterUPP	filterProc = NULL;
	
	if (inEventProc)	eventProc = NewNavEventProc(inEventProc);
	else				eventProc = NewNavEventProc(DefaultCallBackProc);
	
	if (inFilterProc)	filterProc = NewNavObjectFilterProc(inFilterProc);
	
	UDesktop::Deactivate();
	err = ::NavChooseFolder(&mDefaultLocation, &mReply, &mOptions,
							eventProc, filterProc, this);
	UDesktop::Activate();
	
	if (err == noErr && mReply.validRecord) {
		AEKeyword	theKeyword;
		DescType	actualType;
		Size		actualSize;
		
		err = ::AEGetNthPtr(&(mReply.selection), 1, typeFSS, &theKeyword,
							&actualType, &outFolderFSSpec, sizeof(outFolderFSSpec),
							&actualSize);
		
		::NavDisposeReply(&mReply);
	}
	
	::DisposeRoutineDescriptor(eventProc);
	if (inFilterProc)	::DisposeRoutineDescriptor(filterProc);
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ GetFile
// ---------------------------------------------------------------------------
//	

OSErr
CNavCustomDialog::GetFile(AEDescList& outSelection,
						  void pascal (*inEventProc)(),
						  Boolean pascal (*inPreviewProc)(),
						  Boolean pascal (*inFilterProc)(AEDesc *,void *,void *,NavFilterModes))
{
	OSErr				err;
	
	NavEventUPP			eventProc = NULL;
	NavObjectFilterUPP	filterProc = NULL;
	NavPreviewUPP		previewProc = NULL;
	
	if (inEventProc)	eventProc = NewNavEventProc(inEventProc);
	else				eventProc = NewNavEventProc(DefaultCallBackProc);
	
	if (inPreviewProc)	previewProc = NewNavPreviewProc(inPreviewProc);
	if (inFilterProc)	filterProc = NewNavObjectFilterProc(inFilterProc);
	
	NavTypeListHandle	typeList = 0;
	if (mTypeList)
		typeList = mTypeList->TypeListHandle();
	
	UDesktop::Deactivate();
	err = ::NavGetFile(&mDefaultLocation, &mReply, &mOptions,
					   eventProc, previewProc, filterProc,
					   typeList, this);
	UDesktop::Activate();
	
	if ((err == noErr) && mReply.validRecord) {
		outSelection = mReply.selection;
	}
	
	::DisposeRoutineDescriptor(eventProc);
	if (inPreviewProc)	::DisposeRoutineDescriptor(previewProc);
	if (inFilterProc)	::DisposeRoutineDescriptor(filterProc);
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ FinishGetFile
// ---------------------------------------------------------------------------
//	

void
CNavCustomDialog::FinishGetFile()
{
	::NavDisposeReply(&mReply);
}



// ---------------------------------------------------------------------------
//	€ PutFile
// ---------------------------------------------------------------------------
//	

OSErr
CNavCustomDialog::PutFile(FSSpec &outFSSpec, void pascal (*inProc)())
{
	OSErr			err;
	NavEventUPP		eventProc = NULL;
	
	if (inProc)	eventProc = NewNavEventProc(inProc);
	else		eventProc = NewNavEventProc(DefaultCallBackProc);
	
	UDesktop::Deactivate();
	err = ::NavPutFile(&mDefaultLocation, &mReply, &mOptions,
					   eventProc, mFileType, mFileCreator, this);
	UDesktop::Activate();
	
	if (err == noErr && mReply.validRecord) {
		AEKeyword	theKeyword;
		DescType	actualType;
		Size		actualSize;
		
		err = ::AEGetNthPtr(&(mReply.selection), 1, typeFSS, &theKeyword,
							&actualType, &outFSSpec, sizeof(outFSSpec),
							&actualSize);
		
		if (err == noErr) {
			mReply.translationNeeded = false;
			::NavCompleteSave(&mReply, kNavTranslateInPlace);
			::NavDisposeReply(&mReply);
		}
	}
	
	::DisposeRoutineDescriptor(eventProc);
	
	return err;
}



// ---------------------------------------------------------------------------
//	€ SetFileTypeList
// ---------------------------------------------------------------------------
//	

void
CNavCustomDialog::SetFileTypeList(OSType inList[], SInt16 numTypes)
{
	if (mTypeList)
		delete mTypeList;
	
	mTypeList = new LFileTypeList(numTypes, inList);
}



// ---------------------------------------------------------------------------
//	€ DefaultCallBackProc
// ---------------------------------------------------------------------------
//	

pascal void
CNavCustomDialog::DefaultCallBackProc(NavEventCallbackMessage callBackSelector, 
									  NavCBRecPtr callBackParams, 
									  NavCallBackUserData callBackUD)
{
	static Handle		ditlHandle = 0;
	static bool			customRectDone = false;
	static short		firstItem = 0, lastWidth, lastHeight;
	
	CNavCustomDialog	*object = (CNavCustomDialog*)callBackUD;
	OSErr				err;
	
	switch (callBackSelector) {
		case kNavCBEvent:
			if (callBackParams->eventData.eventDataParms.event->what == updateEvt)
				UModalAlerts::ProcessModalEvent(*(callBackParams->eventData.eventDataParms.event));
			else
				if (mCustomProcessEventProc)
					object->mCustomProcessEventProc(callBackParams, firstItem);
			break;
		
		case kNavCBPopupMenuSelect:
			NavMenuItemSpecPtr	currMenuItem = (NavMenuItemSpecPtr)
									(callBackParams->eventData.eventDataParms.param);
			
			if (mCustomPopupMenuProc)
				mCustomPopupMenuProc(currMenuItem->menuCreator, currMenuItem->menuType, callBackParams);
			break;
		
		case kNavCBCustomize:
			if (object->mCustomControlsDITL) {
				if ((callBackParams->customRect.right == 0) && (callBackParams->customRect.bottom == 0)) {
					// first round
					callBackParams->customRect.right = callBackParams->customRect.left + object->mWidth;
					callBackParams->customRect.bottom = callBackParams->customRect.top + object->mHeight;
				} else {
					if (lastWidth != callBackParams->customRect.right)
						if (callBackParams->customRect.right < object->mWidth)	// is NavServices width too small for us?
							callBackParams->customRect.right = object->mWidth;
					
					if (lastHeight != callBackParams->customRect.bottom)	// is NavServices height too small for us?
						if (callBackParams->customRect.bottom < object->mHeight)
							callBackParams->customRect.bottom = object->mHeight;
					
				}
				lastWidth = callBackParams->customRect.right;
				lastHeight = callBackParams->customRect.bottom;	
			}
			break;
		
		case kNavCBStart:
			if (object->mCustomControlsDITL) {
				ditlHandle = ::GetResource('DITL', object->mCustomControlsDITL);
				if (ditlHandle) {
					err = ::NavCustomControl(callBackParams->context, kNavCtlAddControlList, ditlHandle);
					if (err == noErr) {
						::NavCustomControl(callBackParams->context,kNavCtlGetFirstControlID,&firstItem);
						if (mSetupCustomControlsProc)
							mSetupCustomControlsProc(callBackParams, firstItem);
					}
				}
					
			}
			
			if (object->mDefaultMenuItem.menuType > 0)
				::NavCustomControl(callBackParams->context, kNavCtlSelectCustomType,
								   &(object->mDefaultMenuItem));
			break;

/* This message is not sent with older NavServcies (MacOS 8.5/6) */		
/*		case kNavCBAccept:
			if (mAcceptCustomControlsProc)
				mAcceptCustomControlsProc((DialogPtr) callBackParams->window, firstItem);
			break;
*/		
		case kNavCBTerminate:
			if (mAcceptCustomControlsProc)
				mAcceptCustomControlsProc(callBackParams, firstItem);
			
			customRectDone = false;
			if (ditlHandle) {
				::ReleaseResource(ditlHandle);
				ditlHandle = 0;
			}
			break;
		
		default:
			if (mCustomCallBackProc)
				(*mCustomCallBackProc)(callBackSelector, callBackParams, callBackUD);
	}
}













// ---------------------------------------------------------------------------
//	€ CNavMenuItemSpecArray
// ---------------------------------------------------------------------------
//	

CNavMenuItemSpecArray::CNavMenuItemSpecArray(SInt16 inElementCount)
{
	mHandle = 0;
	mElementCount = inElementCount;
	
	if (inElementCount > 0)
		mHandle = (NavMenuItemSpecHandle) ::NewHandle(inElementCount * sizeof(NavMenuItemSpec));
}



// ---------------------------------------------------------------------------
//	€ ~CNavMenuItemSpecArray
// ---------------------------------------------------------------------------
//	

CNavMenuItemSpecArray::~CNavMenuItemSpecArray()
{
	if (mHandle)
		::DisposeHandle( (Handle) mHandle );
}



// ---------------------------------------------------------------------------
//	€ SetMenuItem
// ---------------------------------------------------------------------------
//	

void
CNavMenuItemSpecArray::SetMenuItem(SInt16 inNum, StringPtr inName, OSType inType,
								   OSType inCreator)
{
	if (inNum < mElementCount) {
		NavMenuItemSpecPtr 	item = &((*mHandle)[inNum]);
		item->version		= kNavMenuItemSpecVersion;
		item->menuType		= inType;
		item->menuCreator	= inCreator;
		LString::CopyPStr(inName, item->menuItemName);
	}
}



// ---------------------------------------------------------------------------
//	€ GetMenuItem
// ---------------------------------------------------------------------------
//	

NavMenuItemSpec
CNavMenuItemSpecArray::GetMenuItem(SInt16 inNum)
{
	if (inNum < mElementCount) {
		return (*mHandle)[inNum];
	} else {
		NavMenuItemSpec	empty = {0, 0 ,0 ,"\p"};
		return empty;
	}
}



// ---------------------------------------------------------------------------
//	€ GetMenuItem
// ---------------------------------------------------------------------------
//	

NavMenuItemSpec
CNavMenuItemSpecArray::GetMenuItem(OSType inType)
{
	for (int i=0; i<mElementCount; i++) {
		if ((*mHandle)[i].menuType == inType)
			return (*mHandle)[i];
	}
	
	NavMenuItemSpec	empty = {0, 0 ,0 ,"\p"};
	return empty;
}



// ---------------------------------------------------------------------------
//	€ GetHandle
// ---------------------------------------------------------------------------
//	

NavMenuItemSpecHandle
CNavMenuItemSpecArray::GetHandle()
{
	return mHandle;
}


