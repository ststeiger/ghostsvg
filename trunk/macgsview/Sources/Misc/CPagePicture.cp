/* ============================================================================	*/
/*	CPagePicture.cp							written by Bernd Heller in 2000		*/
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


#include <UDrawingState.h>

#include "CPagePicture.h"

#include <PictUtils.h>


const int	minDistance = 4;


// ---------------------------------------------------------------------------------
//	€ CPagePicture
// ---------------------------------------------------------------------------------
//	Constructor

CPagePicture::CPagePicture(PicHandle inHdl)
	: CMemPicture(inHdl)
{
}



// ---------------------------------------------------------------------------------
//	€ CPagePicture
// ---------------------------------------------------------------------------------
//	Constructor

CPagePicture::CPagePicture(LStream *inStream)
	: CMemPicture(inStream)
{
}



// ---------------------------------------------------------------------------------
//	€ ~CPagePicture
// ---------------------------------------------------------------------------------
//	Destructor

CPagePicture::~CPagePicture()
{
}


// ---------------------------------------------------------------------------------
//	€ SetPicture
// ---------------------------------------------------------------------------------
//	Sets the current PICT to inHdl

void
CPagePicture::SetPicture(PicHandle inHdl)
{
	if (inHdl) {
		CMemPicture::SetPicture(inHdl);
		CalcPictureSize(mPicRect);
	}
}



// ---------------------------------------------------------------------------------
//	€ CalcFrameSize
// ---------------------------------------------------------------------------------

void
CPagePicture::CalcFrameSize(Rect& outSizeRect)
{
	CMemPicture::CalcPictureSize(outSizeRect);
	
	outSizeRect.bottom	+= minDistance*2 + 4;
	outSizeRect.right	+= minDistance*2 + 4;
}



// ---------------------------------------------------------------------------------
//	€ DrawSelf
// ---------------------------------------------------------------------------------

void
CPagePicture::DrawSelf()
{
	static RGBColor		white = {0xffff,0xffff,0xffff},
						gray  = {0xA000,0xA000,0xA000},
						black = {0x0000,0x0000,0x0000};
	
	if (!mPict) return;
	
	Rect		frameRect, drawRect = mPicRect;
	
	CalcLocalFrameRect(frameRect);
	
	SInt32	dh = ((frameRect.right  - frameRect.left) - mPicRect.right)  / 2,
			dv = ((frameRect.bottom - frameRect.top)  - mPicRect.bottom) / 2;
	
	if (dv < minDistance) dv = minDistance;
	if (dh < minDistance) dh = minDistance;
	
	::OffsetRect(&drawRect, dh, dv);
	
	// draw gray background
	::RGBForeColor(&gray);
	::RGBBackColor(&white);
	::PenNormal();
	
	RgnHandle	picRgn = ::NewRgn(), fillRgn = ::NewRgn();
	::RectRgn(picRgn, &drawRect);
	::RectRgn(fillRgn, &frameRect);
	::DiffRgn(fillRgn, picRgn, fillRgn);	// fillRgn = fillRgn - picRgn
	if (!::EmptyRgn(fillRgn))
		::FillRgn(fillRgn, &UQDGlobals::GetQDGlobals()->black);
	::DisposeRgn(picRgn);
	::DisposeRgn(fillRgn);
	
	// draw page border and shadow
	Rect	pageRect = drawRect;
	::RGBForeColor(&black);
	::InsetRect(&pageRect, -1, -1);
	::FrameRect(&pageRect);
	
	::PenSize(2, 2);
	::MoveTo(pageRect.right, pageRect.top + 2);
	::Line(0, mPicRect.bottom);
	::Line(-mPicRect.right, 0);
	
	// draw page
	DrawToRect(&drawRect);
}



