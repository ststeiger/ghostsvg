/* ============================================================================	*/
/*	CMemPicture.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
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


#include <UResourceMgr.h>
#include <UDrawingState.h>
#include <UCursor.h>
#include <LClipboard.h>

#include <stdlib.h>
#include <string.h>

#include <PictUtils.h>

#include "CMemPicture.h"



// ---------------------------------------------------------------------------------
//	€ CMemPicture
// ---------------------------------------------------------------------------------
//	Constructor

CMemPicture::CMemPicture(PicHandle inHdl)
{
	mPict = 0;
	mPicBuffer = 0;
	mDrawBuffered = false;
	mAntiAliasing = false;
	SetPicture( inHdl );
}



// ---------------------------------------------------------------------------------
//	€ CMemPicture
// ---------------------------------------------------------------------------------
//	Constructor

CMemPicture::CMemPicture(LStream *inStream)
	: LView( inStream )
{
	mPict = 0;
	mPicBuffer = 0;
	mDrawBuffered = false;
	mAntiAliasing = false;
}



// ---------------------------------------------------------------------------------
//	€ ~CMemPicture
// ---------------------------------------------------------------------------------
//	Destructor

CMemPicture::~CMemPicture()
{
	if (mPict) ::KillPicture(mPict);
	if (mPicBuffer) ::DisposeGWorld(mPicBuffer);
}



// ---------------------------------------------------------------------------------
//	€ CopyPict
// ---------------------------------------------------------------------------------
//	Copy the PICT to the clipboard.

void
CMemPicture::CopyPict()
{
	if (mPict)
		(LClipboard::GetClipboard())->SetData('PICT', (Handle)mPict, true);
}



// ---------------------------------------------------------------------------------
//	€ SavePictureToFile
// ---------------------------------------------------------------------------------
//	Saves the PICT into a file specified in the FSSpec

OSErr
CMemPicture::SavePictureToFile(FSSpec *inFSSpec, bool inReplacing)
{
	OSErr	result = noErr;
	
	if (!inReplacing)
		result = ::FSpCreate(inFSSpec, 'ogle', 'PICT', smSystemScript);
		
	if (result == noErr) {
		short	pictFileRefNum = 0;
		
		result = ::FSpOpenDF(inFSSpec, fsRdWrPerm, &pictFileRefNum);
		if (result == noErr) {
			long	zeroData	= 0,
					dataLength	= 4;
			
			for (int count=0; count < 512/dataLength; count++)
				result = ::FSWrite(pictFileRefNum, &dataLength, &zeroData);
			
			if (result == noErr) {
				dataLength = ::GetHandleSize((Handle) mPict);
				::HLock((Handle) mPict);
				result = ::FSWrite( pictFileRefNum, &dataLength, *mPict );
				::HUnlock((Handle) mPict);
			}
			::FSClose(pictFileRefNum);
		} else {
			return result;
		}
	}
	
	return result;
}



// ---------------------------------------------------------------------------------
//	€ SavePictureToResource
// ---------------------------------------------------------------------------------
//	Saves the PICT to the resource fork of a file specified in the FSSpec.
//	If needed it creates a resource fork with inCreator/inType.

OSErr
CMemPicture::SavePictureToResource(FSSpec *inFSSpec, OSType inCreator, OSType inType,
								   ResType inResType, short inResNum, bool inReplacing)
{
	OSErr	err = noErr;
	
	// open and/or create resource fork
	StCurResFile();	// save current resource file
	
	SInt16	refNum;
	if ((refNum = ::FSpOpenResFile(inFSSpec, fsRdWrPerm)) == -1) {
		// Couldn't open resource file
		if ((err = ::ResError()) != fnfErr)
			return err;
		
		::FSpCreateResFile(inFSSpec, inCreator, inType, smSystemScript);
		refNum = ::FSpOpenResFile(inFSSpec, fsRdWrPerm);
	}
	
	::UseResFile(refNum);
	{	// the brace is NEEDED!!!, otherwise the ~StCurResFile may be called before ~StNewResource
		StNewResource pictResource(inResType, inResNum, ::GetHandleSize((Handle) mPict));
		if (!inReplacing && pictResource.ResourceExisted()) {
			pictResource.DontWrite();
		} else {
			::HLock(pictResource.Get());
			::HLock((Handle) mPict);
			memcpy(*pictResource.Get(), *mPict, ::GetHandleSize((Handle) mPict));
			::HUnlock(pictResource.Get());
			::HUnlock((Handle) mPict);
		}
	}
	
	// close resource fork
	::CloseResFile(refNum);
	
	return err;
}



// ---------------------------------------------------------------------------------
//	€ SetDrawBuffered
// ---------------------------------------------------------------------------------

void
CMemPicture::SetDrawBuffered(bool inFlag)
{
	if (mDrawBuffered && !inFlag) {
		// switch from buffered to unbuffered
		if (mPicBuffer) {
			::DisposeGWorld(mPicBuffer);
			mPicBuffer = 0;
		}
	}
	
	mDrawBuffered = inFlag;
}



// ---------------------------------------------------------------------------------
//	€ SetPicture
// ---------------------------------------------------------------------------------
//	Sets the current PICT to inHdl

void
CMemPicture::SetPicture(PicHandle inHdl)
{
	if (!inHdl) return;
	
	if (mPict) ::KillPicture(mPict);
	mPict = inHdl;
	
	Rect	frameRect;
	CalcFrameSize(frameRect);
	ResizeImageTo(frameRect.right, frameRect.bottom, false);
	
	if (mDrawBuffered) {
		Rect		picRect;
		CalcPictureSize(picRect);
		
		if (mPicBuffer) ::DisposeGWorld(mPicBuffer);
		
		::NewGWorld(&mPicBuffer, 32, &picRect, NULL, NULL, useTempMem);
		if (!mPicBuffer)
			::NewGWorld(&mPicBuffer, 32, &picRect, NULL, NULL, 0L);
		
		if (!mPicBuffer) {
			return;
		} else {
			CGrafPtr	thePort;
			GDHandle	theDevice;
			
			::GetGWorld(&thePort, &theDevice);
			::LockPixels(::GetGWorldPixMap(mPicBuffer));
			::SetGWorld(mPicBuffer, NULL);
			::DrawPicture(mPict, &picRect);
			::UnlockPixels(::GetGWorldPixMap(mPicBuffer));
			::SetGWorld(thePort, theDevice);
		}
	}
}



// ---------------------------------------------------------------------------------
//	€ CalcPictureSize
// ---------------------------------------------------------------------------------

void
CMemPicture::CalcPictureSize(Rect& outSizeRect)
{
	PictInfo	picInf;
	::GetPictInfo(mPict, &picInf, 0, 0, 0, 0);
	
	outSizeRect.top		= 0;
	outSizeRect.left	= 0;
	outSizeRect.bottom	= picInf.sourceRect.bottom - picInf.sourceRect.top;
	outSizeRect.right	= picInf.sourceRect.right  - picInf.sourceRect.left;
	
	if (mAntiAliasing) {
		outSizeRect.right	/= 2;
		outSizeRect.bottom	/= 2;
	}
}



// ---------------------------------------------------------------------------------
//	€ CalcFrameSize
// ---------------------------------------------------------------------------------

void
CMemPicture::CalcFrameSize(Rect& outSizeRect)
{
	CalcPictureSize(outSizeRect);
}



// ---------------------------------------------------------------------------------
//	€ PictureDone
// ---------------------------------------------------------------------------------
//	Is called when the pict mPict was drawn. In SetPicture it can be still in work

void
CMemPicture::PictureDone()
{
	if (mPicBuffer) {
		CGrafPtr	thePort;
		GDHandle	theDevice;
		
		UCursor::SetWatch();
		::GetGWorld(&thePort, &theDevice);
		::LockPixels(::GetGWorldPixMap(mPicBuffer));
		::SetGWorld(mPicBuffer, NULL);
		::DrawPicture(mPict, &(mPicBuffer->portRect));
		::UnlockPixels(::GetGWorldPixMap(mPicBuffer));
		::SetGWorld(thePort, theDevice);
		UCursor::SetArrow();
	}
}


// ---------------------------------------------------------------------------------
//	€ DrawSelf
// ---------------------------------------------------------------------------------
//	Draw the PICT

void
CMemPicture::DrawSelf()
{
	if (mPict) {
		if (mPicBuffer) {
			DrawToRect(&(mPicBuffer->portRect));
		} else {
			PictInfo	picInf;
			::GetPictInfo(mPict, &picInf, 0, 0, 0, 0);
			
			if (mAntiAliasing) {
				picInf.sourceRect.right		/= 2;
				picInf.sourceRect.bottom	/= 2;
			}
			
			DrawToRect(&(picInf.sourceRect));
		}
	}
}



// ---------------------------------------------------------------------------------
//	€ DrawToRect
// ---------------------------------------------------------------------------------
//	Draw the PICT to a specific Rect

void
CMemPicture::DrawToRect(Rect *drawRect)
{
	if (mPict && drawRect) {
		static RGBColor		white = {0xffff,0xffff,0xffff},
							black = {0x0000,0x0000,0x0000};
		::RGBForeColor(&black);
		::RGBBackColor(&white);
		
		if (mPicBuffer) {
			GrafPtr		currPort;
			::GetPort(&currPort);
			::LockPixels(::GetGWorldPixMap(mPicBuffer));
			::CopyBits(&((GrafPtr)mPicBuffer)->portBits, &((GrafPtr)currPort)->portBits,
					   &(mPicBuffer->portRect), drawRect,
					   srcCopy, nil);
			::UnlockPixels(::GetGWorldPixMap(mPicBuffer));
		} else {
			UCursor::SetWatch();
			::DrawPicture(mPict, drawRect);
			UCursor::SetArrow();
		}
	}
}



