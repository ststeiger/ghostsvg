/* ============================================================================	*/
/*	CNavCustomDialog.h						written by Bernd Heller in 1999		*/
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


#ifndef _H_CNavCustomDialog
#define _H_CNavCustomDialog
#pragma once

#include <Navigation.h>

#include <LFileTypeList.h>

class CNavCustomDialog
{
	public:
							CNavCustomDialog();
							~CNavCustomDialog();
							
		OSErr				PutFile(FSSpec &outFSSpec, void pascal (*inProc)() = 0);
		OSErr				ChooseFolder(FSSpec& outFolderFSSpec, void pascal (*inProc)() = 0,
										 Boolean pascal (*inFilterProc)(AEDesc *,void *,void *,NavFilterModes) = 0);
		OSErr				GetFile(AEDescList& outSelection, void pascal (*inProc)() = 0,
									Boolean pascal (*inPreviewProc)() = 0,
									Boolean pascal (*inFilterProc)(AEDesc *,void *,void *,NavFilterModes) = 0);
		void				FinishGetFile();
		
		void				SetOptions(NavDialogOptions inOptions)
								{ mOptions = inOptions; }
		NavDialogOptions	GetOptions()
								{ return mOptions; }
		
		void				SetFileType(OSType inType)
								{ mFileType = inType; }
		OSType				GetFileType()
								{ return mFileType; }
		void				SetFileCreator(OSType inCreator)
								{ mFileCreator = inCreator; }
		OSType				GetFileCreator()
								{ return mFileCreator; }
		void				SetFileTypeList(OSType inList[], SInt16 numTypes);
		
		void				SetDefaultLocation(AEDesc *inLocation)
								{ ::AEDisposeDesc(&mDefaultLocation);
								  ::AEDuplicateDesc(inLocation, &mDefaultLocation); }
		AEDesc *			GetDefaultLocation()
								{ return &mDefaultLocation; }
		
		void				SetDefaultMenuItem(const NavMenuItemSpec& inMenuItem)
								{ mDefaultMenuItem = inMenuItem; }
		
		NavReplyRecord *	GetReply()
								{ return &mReply; }
		
		void				SetCustomCallBackProc(void pascal (*inProc)(NavEventCallbackMessage callBackSelector, 
																		NavCBRecPtr callBackParams, 
																		NavCallBackUserData callBackUD))
								{ mCustomCallBackProc = inProc; }
		void *				GetCustomCallBackProc()
								{ return (void *) mCustomCallBackProc; }
		
		void				SetCustomControls(short inCtrlDITL, short inWidth, short inHeight)
								{ mCustomControlsDITL = inCtrlDITL;	mWidth = inWidth; mHeight = inHeight; }
		void				SetSetupCustomControlsProc(void (*inSetupProc) (NavCBRecPtr callBackParams, short inFirstItem))
								{ mSetupCustomControlsProc = inSetupProc; }
		void				SetAcceptCustomControlsProc(void (*inAcceptProc) (NavCBRecPtr callBackParams, short inFirstItem))
								{ mAcceptCustomControlsProc = inAcceptProc; }
		void				SetCustomProcessEventProc(void (*inProcessEventProc) (NavCBRecPtr callBackParams, short inFirstItem))
								{ mCustomProcessEventProc = inProcessEventProc; }
		
		void				SetCustomPopupMenuProc(void (*inProc) (OSType menuCreator, OSType menuType, NavCBRecPtr params))
								{ mCustomPopupMenuProc = inProc; }
		
		virtual void		ReleaseCustomControls() {}
		
	protected:
		static pascal void	DefaultCallBackProc(NavEventCallbackMessage callBackSelector, 
												NavCBRecPtr callBackParams, 
												NavCallBackUserData callBackUD); 
		
		AEDesc				mDefaultLocation;
		OSType				mFileType;
		OSType				mFileCreator;
		LFileTypeList		*mTypeList;
		NavReplyRecord		mReply;
		NavDialogOptions	mOptions;
		NavMenuItemSpec		mDefaultMenuItem;
		
		// custom control stuff
		short				mCustomControlsDITL;
		short				mHeight;
		short				mWidth;
		static void			(*mSetupCustomControlsProc) (NavCBRecPtr callBackParams, short inFirstItem);
		static void			(*mAcceptCustomControlsProc) (NavCBRecPtr callBackParams, short inFirstItem);
		static void			(*mCustomProcessEventProc) (NavCBRecPtr callBackParams, short inFirstItem);
		
		static void			(*mCustomPopupMenuProc) (OSType menuCreator, OSType menuType, NavCBRecPtr params);
		static pascal void	(*mCustomCallBackProc) (NavEventCallbackMessage callBackSelector,
													NavCBRecPtr callBackParams,
													NavCallBackUserData callBackUD);
};


class CNavMenuItemSpecArray
{
	public:
								CNavMenuItemSpecArray(SInt16 inElementCount);
								~CNavMenuItemSpecArray();
								
		void					SetMenuItem(SInt16 inNum, StringPtr inName, OSType inType,
											OSType inCreator = 0);
		NavMenuItemSpec			GetMenuItem(SInt16 inNum);
		NavMenuItemSpec			GetMenuItem(OSType inType);
		NavMenuItemSpecHandle	GetHandle();
		
	protected:
		NavMenuItemSpecHandle	mHandle;
		SInt16					mElementCount;
};


#endif _H_CNavCustomDialog
