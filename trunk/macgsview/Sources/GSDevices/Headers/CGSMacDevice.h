/* ============================================================================	*/
/*	CGSMacDevice.h							written by Bernd Heller in 1999		*/
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


#ifndef _H_CGSMacDevice
#define _H_CGSMacDevice
#pragma once

#include "CGSDevice.h"
#include "CGSDocumentWindow.h"
#include "CPagePicture.h"

#include "MacGSViewConstants.h"

#include <UCursor.h>
#include <LListener.h>


class CGSDocumentWindow;


class CGSMacDevice : public CGSDevice, public CPagePicture, public LListener
{
	public:
			enum { class_ID = 'GSVw' };
			
							CGSMacDevice(CGSDocument* inDocument = 0);
							CGSMacDevice(LStream* inStream);
							
							~CGSMacDevice();
		
		void				SetWindow(CGSDocumentWindow* inWindow)	{ mWindow = inWindow; }
		void				SetColorDepth(int inColorBits)			{ mBitsPerPixel = inColorBits; }
		void				SetUseExternalFonts(bool inFlag)		{ mUseExternalFonts = inFlag; }
		
		virtual void		FinishCreateSelf();
		virtual void		AdjustCursorSelf(Point /*inPortPt*/, const EventRecord& /*inMacEvent*/);
		virtual void		Click(SMouseDownEvent &inMouseDown);
		
		int					PrintNonDSCDocument(THPrint inPrintRecordH);
		
		virtual void		RenderPage(int inFirstPage = kSubmitWholeDocument);
		void				SuspendRendering();
		void				ResumeRendering();
		void				PictureFinished();
		
	protected:
		// member functions
		virtual void		HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg);
		
		virtual void		SubmitDeviceParameters();
		virtual int			SetupDevice();
		virtual void		DeviceSetupDone();
		
		virtual void		ListenToMessage(MessageT inMessage, void *ioParam);
		
		virtual void		RenderThreadDied(ThreadID id, void* arg);
		
		//data members
		CGSDocumentWindow	*mWindow;				// Parent Window
		unsigned char		*mDevice;				// Displaying mac device
		
		bool				mWaitingForDevice;		// waiting to get a device
		bool				mStopThread;			// stop the thread?
		
		TPrPort				*mPrinterPort;
		THPrint				mPrintRecordH;
		
		int					mBitsPerPixel;			// color depth
		bool				mUseExternalFonts;		// use external (TrueType) fonts
};


#endif _H_CGSMacDevice

