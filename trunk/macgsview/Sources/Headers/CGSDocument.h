/* ============================================================================	*/
/*	CGSDocument.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	This class is an instance of LSingleDoc.									*/
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


#ifndef _H_CGSDocument
#define _H_CGSDocument
#pragma once

#include <LSingleDoc.h>
#include <LPeriodical.h>
#include <LString.h>

#include <Navigation.h>
#include <Printing.h>
#include <Threads.h>

#include <stdio.h>

#include "CGSSharedLib.h"
#include "CGSToolbar.h"
#include "ps.h"


class CGSDevice;

class CGSDocument : public LSingleDoc, public LPeriodical
{
	public:
							CGSDocument(LCommander *inSuper, FSSpec *inFileSpec,
									bool inAutoCreateWindow=true);
							~CGSDocument();
		
		CGSToolbar*			GetToolbar();
		
		CGSDevice*			GetExportDevice() { return mExportDevice; }
		void				SetExportDevice(CGSDevice* inDevice)
								{	if (inDevice && mExportDevice) return;
								  	mExportDevice = inDevice;         
									LCommander::SetUpdateCommandStatus(true); }
		
		document*			GetDocInfo() { return mDocumentInfo; }
		bool				IsReady() { return mDocumentReady; }
		
		Boolean				ObeyCommand(CommandT inCommand, void* ioParam);
		
		virtual Boolean		AskSaveAs(FSSpec& outFSSpec, Boolean inRecordIt);
		virtual OSErr		DoSaveAsDialog(FSSpec& outFSSpec);
		virtual OSErr		DoSaveAsDialogWorkaround(FSSpec& outFSSpec);
		
		virtual Boolean		DoSaveAs(FSSpec& inFSSpec, OSType inType, bool inThreaded=true);
		static CGSDevice*	CreateDeviceFromSaveFormat(OSType inCGSSaveFormat);
		static void			GetExtensionFromSaveFormat(OSType inCGSSaveFormat, LStr255& outExtension);
		
		void				SetupPage();
		virtual void		DoPrint();
		
		void				GetFilePath(char *outPath);
		int					GetNumberOfPages();
		void				GetPageSize(SInt32 pageNum, float& xSize, float& ySize);
		int					GetPageOrientation(SInt32 pageNum);
		SInt32				GetCurrentPage();
		
		bool				HasDSC() { return mHasDSC; }
		bool				IsPDF() { return mIsPDF; }
		bool				IsEPS() { return mDocumentInfo->epsf; }
		
		void				WriteEPSPreviewResource();
		
	protected:
		void				OpenFile();
		int					ScanPDFFile();
		int					ScanPSFile(FSSpecPtr inFileSpecPtr=0);
		virtual	void		SpendTime(const EventRecord& inMacEvent);
		static void			HandleSaveDialogPopupMenu(OSType inCreator,
												  OSType inType,
												  NavCBRecPtr inParams);
		
		static TPPrDlg			sPrintDialog;
		static ModalFilterUPP	sPrintFltrProc;
		static PItemUPP			sPrintItemProc;
		static short			sPrintDialogFirstItem;
		static pascal TPPrDlg 	ExtendPrintDialog(THPrint* printHandle);
		static pascal void		HandlePrintDialog(TPPrDlg dialog, short item);
		static pascal Boolean	PrintDialogEventFilter(DialogPtr dialog, EventRecord *event,
													   DialogItemIndex *itemHit);
		
		static void			SaveAsWorkaroundSetupCustomControls(NavCBRecPtr callBackParams, short inFirstItem);
		static void			SaveAsWorkaroundAcceptCustomControls(NavCBRecPtr callBackParams, short inFirstItem);
		static void			SaveAsWorkaroundCustomProcessEvent(NavCBRecPtr callBackParams, short inFirstItem);
		
		static SInt32		sSaveDialogFileTypeNr;	// needed for SaveAsWorkaround
		static OSType		sSaveDialogFileType;
		AliasHandle			mFileAlias;
		document			*mDocumentInfo;
		CGSDevice			*mExportDevice;
		bool				mDocumentReady;
		bool				mHasDSC;
		bool				mIsPDF;
		
		// this is all only needed to create the open thread
		ThreadID			mOpenThread;
		static pascal void	*sOpenFile(void *arg);
};



#endif _H_CGSDocument
