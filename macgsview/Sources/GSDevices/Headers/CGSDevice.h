/* ============================================================================	*/
/*	CGSDevice.h								written by Bernd Heller in 1999		*/
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


#ifndef _H_CGSDevice
#define _H_CGSDevice
#pragma once


#include <LCommander.h>
#include <LWindow.h>
#include <LStaticText.h>
#include <LEditText.h>
#include <LPopupButton.h>
#include <LThread.h>
#include <LView.h>
#include <LString.h>

#include "CGSDocument.h"
#include "CGSSharedLib.h"
#include "CUtilities.h"
#include "CGSPreferences.h"

#include "MacGSViewConstants.h"

#include <Threads.h>


#pragma def_inherited on


#define		kSubmitWholeDocument	-1
#define		kSubmitOnePage			-1


class CGSDevice;	// forward defnition

typedef struct {
	CGSDevice	*object;
	int			arg1;
	int			arg2;
	int			arg3;
	int			arg4;
	int			arg5;
} GSThreadArgs;



enum PSSection {PSPreface, PSHeader, PSDefaults, PSProlog, PSSetup, PSTrailer};


class CGSDevice : public LCommander
{
	protected:
	
	enum GSExportPanelMode {GSExportPanelSetup, GSExportPanelAccept, GSExportPanelCancel,
							GSExportPanelMessage};
	typedef void (CGSDevice::* PanelHandler)(GSExportPanelMode inMode, LView* inView,
											 MessageT inMsg);
	typedef struct {
		SInt16			panelNumber;
		ResIDT			panelID;
		LView			*panel;
		PanelHandler	handler;
	} GSExportPanel;
	
	
	public:
							CGSDevice(CGSDocument* inDocument = 0);
							~CGSDevice();
		
		CGSSharedLib*		GetDLL() { return mDLL; }
		
		void			SetDocument(CGSDocument* inDocument)	{ mDocument = inDocument;
																  mPrefaceSubmitted = false; }
		
		void			SetIsSelfDestruct(bool inFlag)		{ mDeviceIsSelfDestruct = inFlag; }
		void			SetIsThreadedDevice(bool inFlag)	{ mIsThreadedDevice = inFlag; }
		void			SetIsScreenDevice(bool inFlag)		{ mIsScreenDevice = inFlag; }
		void			SetDeviceIsExporting(bool inFlag)	{ mDeviceIsExporting = inFlag; }
		
		void			SetOutputFile(char* inPath);
		void			SetOutputFile(FSSpec& inFSSpec);
		void			SetPageRange(short inFirstPage = kSubmitWholeDocument, short inLastPage = kSubmitOnePage)
								{ mFirstPage = inFirstPage; mLastPage = inLastPage; }
		
		virtual void		SetDeviceShowsPageRange(bool inFlag)	{ mShowPageRangePanel = inFlag; }
		virtual void		SetDeviceShowsDialog(bool inFlag)	{ mShowDialog = inFlag; }
		
		void			SetTransparency(int inTextBits, int inGraphicBits)
									{ mGraphicsAlphaBits	= inGraphicBits;
									  mTextAlphaBits		= inTextBits; }
		void			SetPageSize(float inH, float inV)
									{ mPageSize.h	= inH;
									  mPageSize.v	= inV; }
		void			SetResolution(float inResolution)
									{ mHWResolution.h	= inResolution;
									  mHWResolution.v	= inResolution; }
		void			SetResolution(float inHRes, float inVRes)
									{ mHWResolution.h	= inHRes;
									  mHWResolution.v	= inVRes; }
		void			SetUseHalftoneScreen(bool inFlag) { mHalftoneScreen.useScreen = inFlag; }
		void			SetHalftoneScreen(int inFrequency, int inAngle, SpotType inType)
									{ mHalftoneScreen.frequency = inFrequency;
									  mHalftoneScreen.angle = inAngle;
									  mHalftoneScreen.type = inType;
									}
		
		bool				DoExportDialog();
		virtual void		Save();
		virtual void		Save(FSSpec& inOutputFile);
		int					DoSave();
		
		void				RenderPage(int inFirstPage = kSubmitWholeDocument,
									   int inLastPage  = kSubmitOnePage);
		bool				IsRendering() { return (mRenderThread != 0); }
		ThreadID			GetThread() { return mRenderThread; }
		
		SInt32				GetCurrentPage() { return mCurrentPage; }
		
	protected:
		// member functions
		virtual void		PreDeviceSetup();
		virtual void		SubmitDeviceParameters();
		virtual int			SetupDevice();
		virtual void		DeviceSetupDone();
		virtual void		SetFileTypeCreator();
		
		void				Abort();	// protected, because it does not work, if called
										// from another class: use GetDLL()->Abort() instead
		
		static pascal void*	sRenderPage(void* arg);
		static pascal void	sRenderThreadDied(ThreadID id, void* arg);
		virtual void		RenderThreadDied(ThreadID id, void* arg);
		virtual int			RenderPageThread(int inFirstPage, int inLastPage);
		
		int					SubmitPSPreface();
		int					SubmitPSSection(PSSection sect);
		int					SubmitPSPage(SInt32 pageNum);
		
		
		virtual bool		ErrorDialog(int inError);
		
		// export dialog stuff
		short				mNumPanels;
		GSExportPanel		mPanelList[50];
		void				AddPanel(LWindow *inDialog, ResIDT inPanel, PanelHandler inHandler,
									 ConstStringPtr inMenuName);
		virtual void		InstallPanels(LWindow *inDialog);
		
		virtual void		HandlePageRangePanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg);
		virtual void		HandlePageSizePanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg);
		virtual void		HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg);
		virtual void		HandleHalftonePanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg);
		
		// data members
		CGSDocument			*mDocument;					// Current Document
		CGSSharedLib		*mDLL;						// DLL instance used for rendering
		ThreadID			mRenderThread;				// page rendering thread
		int					mLastError;					// last error happened while rendering
		
		bool				mDeviceIsSetUp;				// has the device been inited
		bool				mPrefaceSubmitted;			// has PSHeader... been submitted?
		
		SInt32				mFirstPage;
		SInt32				mCurrentPage;				// page in progress, -1 if undefined
		SInt32				mLastPage;
		
		bool				mExportWithDocSize;			// if true, get page size from docinfo while exporting
		bool				mShowPageRangePanel;		// if false, page range panel must not be shown
		bool				mShowDialog;				// if false, device must not show the dialog
		
		FSSpec				mOutputFileFSSpec;			// FSSpec of the output file
		OSType				mFileType;					// file type of output file
		OSType				mFileCreator;				// file creator of output file
		
		bool				mIsScreenDevice;			// is a screen device (currently
														// only gdevmac), sets no output file
		bool				mIsMultiplePageDevice;		// can render multiple pages in one document
		bool				mIsThreadedDevice;			// runs as a thread in the background
		bool				mDeviceIsSelfDestruct;		// device deletes itself
		bool				mDeviceIsExporting;			// device is used to export
		bool				mDeviceIsPrinting;			// true if the device is currently used
														// to print a non-DSC document
		
		char				mDeviceName[256];			// name of the Device
		char				mOutputFilePath[1024];		// path to the output file
		int					mGraphicsAlphaBits;
		int					mTextAlphaBits;
		
		struct				{ float h;
							  float v; } mHWResolution;	// resolution
		struct				{ float h;
							  float v; } mPageSize;		// page size
		
		struct				{ bool			useScreen;
							  int			frequency;
							  int			angle;
							  SpotType		type;
							} mHalftoneScreen;			// halftone screen
};



#endif _H_CGSDevice