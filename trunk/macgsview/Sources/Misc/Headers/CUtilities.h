/* ============================================================================	*/
/*	CUtilities.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	This class contains methods which dont exit in other classes.				*/
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


#ifndef _H_CUtilities
#define _H_CUtilities
#pragma once

#include <Files.h>


typedef enum { kMillimeters=1, kCentimeters=2, kInches=3, kPoints=4 } PageSizeUnit;

typedef enum { kA3=1, kA4=2, kA5=3, kA6=4, kLegal=5, kLetter=6 } PaperSize;


class CUtilities
{
		public:
			static OSErr	ConvertPathToFSSpec(const char * inPathName, FSSpecPtr outSpec);
			static void		ConvertFSSpecToPath(const FSSpec *s, char *p, int pLen);
			static void		GetProcessName(Str255& inName);
			static bool		FileBeginsWith(const FSSpec& inFileSpecPtr, const char* inCmpStr, int inLen);
			static bool		GetNewTempFile(FSSpec& outFileSpec);
			static void		ShowErrorDialog(StringPtr inErrDesc, OSErr inErrNr);
			static double	ConvertUnit(PageSizeUnit inSrcUnit, PageSizeUnit inDestUnit, double inValue);
			static void		GetStandardPageSize(PaperSize inFormat, float& outWidth, float& outHeight);
};

#endif _H_CUtilities
