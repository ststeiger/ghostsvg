/* ============================================================================	*/
/*	CPagePicture.h							written by Bernd Heller in 2000		*/
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


#ifndef _H_CPagePicture
#define _H_CPagePicture
#pragma once

#include "CMemPicture.h"


class CPagePicture : public CMemPicture
{
	public:
		enum { class_ID = FOUR_CHAR_CODE('PPic') };
		
							CPagePicture(PicHandle inHdl = 0);
							CPagePicture(LStream *inStream);
							~CPagePicture();
		
		virtual void		SetPicture(PicHandle inHdl);
		virtual void		CalcFrameSize(Rect& outSizeRect);
		virtual void		DrawSelf();
	
	protected:
		Rect				mPicRect;
};



#endif _H_CPagePicture
