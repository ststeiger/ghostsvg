/* ============================================================================	*/
/*	CGSApplication.h						written by Bernd Heller in 1999		*/
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


#ifndef _H_CGSApplication
#define _H_CGSApplication
#pragma once


#include <LClipboard.h>
#include <LDocApplication.h>

#include <Navigation.h>
#include <Printing.h>

#include "CGSPreferences.h"


class CGSApplication : public LDocApplication
{
	public:
								CGSApplication();
		virtual					~CGSApplication();
		
		void					CheckEnvironment();
		
		static CGSApplication*	GetInstance() { return sAppInstance; }
		
		virtual void			ShowAboutBox();
		virtual Boolean			ObeyCommand(CommandT inCommand, void* ioParam);
		virtual void			FindCommandStatus(CommandT inCommand,
												  Boolean &outEnabled, Boolean &outUsesMark,
												  UInt16 &outMark, Str255 outName);
		virtual void			SetupPage();
		
		static THPrint			GetDefaultPrintRecord();
		static CGSPreferences*	GetPreferences() { return &(sAppInstance->mPreferences); }
		
	protected:
		virtual void			Initialize();
		virtual void			StartUp();
		
		virtual void			ConvertFiles();
		virtual void			OpenDocument(FSSpec *inMacFSSpec);
		virtual void			ChooseDocument();
		virtual void			PrintDocument(FSSpec *inMacFSSpec);
		
		virtual void			EventResume(const EventRecord& inMacEvent);
		virtual void			EventSuspend(const EventRecord& inMacEvent);
		virtual void			EventMouseDown(const EventRecord& inMacEvent);
		
		static pascal Boolean	NavObjectFilter(AEDesc *theItem, void *info, void *callBackUD,
												NavFilterModes filterMode);
		static void				HandleOpenDialogPopupMenu(OSType inCreator, OSType inType,
														  NavCBRecPtr inParams);
		
		static void				ConvertSetupCustomControls(NavCBRecPtr callBackParams, short inFirstItem);
		static void				ConvertAcceptCustomControls(NavCBRecPtr callBackParams, short inFirstItem);
		
		static OSType			sConvertDialogFileType;
		static SInt32			sConvertDialogFileTypeNr;
		static OSType			sOpenDialogFileType;
		static THPrint			sPrintRecordH;
		static CGSApplication*	sAppInstance;
		
		CGSPreferences			mPreferences;
		
		LClipboard				mClipboard;
		
		int						mBackgroundSleep;	// how much time for other apps while in background
		int						mForegroundSleep;	// how much time for other apps while in foreground
};

#endif _H_CGSApplication


