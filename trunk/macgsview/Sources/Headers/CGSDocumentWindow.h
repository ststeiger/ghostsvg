/* ============================================================================	*/
/*	CGSDocumentWindow.h						written by Bernd Heller in 1999		*/
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


#ifndef _H_CGSDocumentWindow
#define _H_CGSDocumentWindow
#pragma once

#include <LWindow.h>
#include <LListener.h>

#include "CGSMacDevice.h"
#include "CGSToolbar.h"


class CGSMacDevice;


class CGSDocumentWindow : public LWindow
{
	public:
		enum { class_ID = 'DocW' };
		
						CGSDocumentWindow();
						CGSDocumentWindow(const SWindowInfo &inWindowInfo);
						CGSDocumentWindow(ResIDT inWINDid,
										  UInt32 inAttributes,
										  LCommander *inSuperCommander);
						CGSDocumentWindow(LStream *inStream);
						~CGSDocumentWindow();
		
		CGSSharedLib*	GetDLL();
		CGSToolbar*		GetToolbar() { return mToolbar; };
		
		virtual void	FindCommandStatus(CommandT inCommand,
										  Boolean &outEnabled,
										  Boolean &outUsesMark,
										  UInt16 &outMark, Str255 outName);
		
		virtual Boolean	ObeyCommand(CommandT inCommand, void* ioParam);
		
		void			SetDocument(CGSDocument* theDocument);
		void			RenderContent();
		
		virtual Boolean	HandleKeyPress(const EventRecord& inKeyEvent);
		virtual void	HandleClick(const EventRecord &inMacEvent, SInt16 inPart);
		void			HandleWindowZoomButton(bool callFromMenu, bool dontRefresh=false);
		SInt32			GetCurrentPage() { return mPage; }
		void			GotoPage(SInt32 pageNumber);
		void			InitZoom();
		
	protected:
		void			Init();
		virtual void	FinishCreateSelf();
		
		void			HandleWindowPageButton();
		void			ZoomToFitPage(bool dontRefresh=false);
		void			ZoomToFitWidth(bool dontRefresh=false);
		void			GotoZoom(SInt32 inZoom, bool dontRefresh=false);
		
		void			SetCTable();
		
		CGSDocument		*mDocument;			// The parent document
		CGSMacDevice	*mView;				// The view
		
		CGSToolbar		*mToolbar;			// the toolbar
		SInt32			mPage;				// Displayed page
		SInt32			mZoom;				// Used zoom fatcor
		int				mStep;
};

#endif _H_CGSDocumentWindow

