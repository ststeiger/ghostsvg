/* ============================================================================	*/
/*	CGSPxMDevice.h							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	device wrapper class for "pbm", "pgm", "ppm"								*/
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


#ifndef _H_CGSPxMDevice
#define _H_CGSPxMDevice
#pragma once

#include "CGSDevice.h"


class CGSPxMDevice : public CGSDevice
{
	public:
							CGSPxMDevice(CGSDocument* inDocument=0);
		
	protected:
		virtual void		HandleRenderingPanel(GSExportPanelMode inMode, LView* inView, MessageT inMsg);
};

#endif _H_CGSPxMDevice

