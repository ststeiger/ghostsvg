/* ============================================================================	*/
/*	CMemPicture.h							written by Bernd Heller in 1999		*/
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


#ifndef _H_CMemPicture
#define _H_CMemPicture
#pragma once

#include <LView.h>

#include <QDOffscreen.h>


class	CMemPicture : public LView
{
	public:
		enum { class_ID = FOUR_CHAR_CODE('MPic') };
		
							CMemPicture(PicHandle inHdl = 0);
							CMemPicture(LStream *inStream);
							~CMemPicture();
		
		PicHandle			GetPicture() { return mPict; }
		void				SetPicture(PicHandle inHdl);
		virtual void		CalcPictureSize(Rect& outSizeRect);
		virtual void		CalcFrameSize(Rect& outSizeRect);
		void				PictureDone();
		
		bool				GetDrawBuffered() { return mDrawBuffered; }
		void				SetDrawBuffered(bool inFlag);
		void				SetAntiAliasing(bool inFlag) { mAntiAliasing = inFlag; }
		
		void				CopyPict();
		OSErr				SavePictureToFile(FSSpec *inFSSpec, bool inReplacing);
		OSErr				SavePictureToResource(FSSpec *inFSSpec, OSType inCreator, OSType inType,
								   ResType inResType, short inResNum, bool inReplacing);
		
		virtual void		DrawSelf();
		void				DrawToRect(Rect *drawRect);
		
	protected:
		PicHandle			mPict;
		GWorldPtr			mPicBuffer;
		
		bool				mDrawBuffered;
		bool				mAntiAliasing;
};

#endif _H_CMemPicture

