/* ============================================================================	*/
/*	CUtilities.cp						written by Bernd Heller in 1999, 2000	*/
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


#include <Processes.h>
#include <Folders.h>
#include <Threads.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <path2fss.h>

#include <LFileStream.h>
#include <LString.h>
#include <UModalDialogs.h>
#include <UDesktop.h>
#include <UAppleEventsMgr.h>

#include "CUtilities.h"
#include "MacGSViewConstants.h"



// ---------------------------------------------------------------------------------
//	€ ConvertPathToFSSpec												[static]
// ---------------------------------------------------------------------------------
//	Converts a path name to a FSSpec

OSErr
CUtilities::ConvertPathToFSSpec(const char * inPathName, FSSpecPtr outSpec)
{
	return __path2fss(inPathName, outSpec);
}



// ---------------------------------------------------------------------------------
//	€ ConvertFSSpecToPath												[static]
// ---------------------------------------------------------------------------------
//	Converts a FSSpec to a path name in Mac format (':' is used, not '/')

void
CUtilities::ConvertFSSpecToPath(const FSSpec *s, char *p, int pLen)
{
	DirInfo		block;
	Str255		dirName;
	int			totLen, dirLen;
	
	totLen = s->name[0];
	memcpy(p, s->name + 1, totLen);
	
	block.ioDrParID = s->parID;
	block.ioNamePtr = dirName;
	while (block.ioDrParID != fsRtParID) {
		block.ioVRefNum = s->vRefNum;
		block.ioFDirIndex = -1;
		block.ioDrDirID = block.ioDrParID;
		::PBGetCatInfoSync((CInfoPBPtr) &block);
		
		dirLen = dirName[0];
		if (dirLen + totLen + 1 >= pLen - 1) goto error;
		memmove(p + dirLen + 1, p, totLen);
		memcpy(p, dirName + 1, dirLen);
		p[dirLen] = ':';
		totLen += dirLen + 1;
	}
	
	p[totLen] = 0;
	return;

error:
	p[0] = 0;
}



// ---------------------------------------------------------------------------------
//	€ GetProcessName													[static]
// ---------------------------------------------------------------------------------
//	Returns this process's name

void
CUtilities::GetProcessName(Str255& inName)
{
	ProcessSerialNumber		thisProcessSN;
	ProcessInfoRec			thisProcessInfo;
	
	if (::GetCurrentProcess( &thisProcessSN ) == noErr) {
		thisProcessInfo.processName = inName;
		thisProcessInfo.processLocation = 0;
		thisProcessInfo.processAppSpec = 0;
		::GetProcessInformation(&thisProcessSN, &thisProcessInfo);
	}
}



// ---------------------------------------------------------------------------------
//	€ FileBeginsWith													[static]
// ---------------------------------------------------------------------------------
//	Checks if a file begins with a byte sequence

bool
CUtilities::FileBeginsWith(const FSSpec& inFileSpec, const char* inCmpStr, int inLen)
{
	if (!inCmpStr || !inLen) return false;
	
	bool	result;
	SInt32	len = inLen;
	char	*readStr;
	
	if ((readStr = (char*) malloc(len)) == NULL) return false;
	
	LFileStream		theFile(inFileSpec);
	
	theFile.OpenDataFork(fsRdPerm);
	theFile.SetMarker(0, streamFrom_Start);
	
	ExceptionCode err = theFile.GetBytes(readStr, len);
	if (err == 0) {
		if (len != inLen)
			result = false;
		else
			result = !memcmp(readStr, inCmpStr, inLen);
	} else {
		result = false;
	}
	
	theFile.CloseDataFork();
	
	free(readStr);
	
	return result;
}



// ---------------------------------------------------------------------------------
//	€ GetNewTempFile													[static]
// ---------------------------------------------------------------------------------
//	Returns the FSSpec of a new (and unique) temp file

bool
CUtilities::GetNewTempFile(FSSpec& outFileSpec)
{
	short			folderVRefNum;
	long			folderDirID;
	OSErr			err;
	
	// Get temp folder
	err = ::FindFolder(kOnSystemDisk, kChewableItemsFolderType, kCreateFolder,
					   &folderVRefNum, &folderDirID);
	if (err != noErr) {
		err = ::FindFolder(kOnSystemDisk, kTemporaryFolderType, kCreateFolder,
						   &folderVRefNum, &folderDirID);
		if (err != noErr) {
			CUtilities::ShowErrorDialog("\pFindFolder failed in GetNewTempFile", err);
			return false;
		}
	}
	
	// Get FSSpec to temp file
	LStr255		fileName(tmpnam(NULL));
	err = ::FSMakeFSSpec(folderVRefNum, folderDirID, fileName, &outFileSpec);
	
	if (err == noErr || err == fnfErr) {
		return true;
	} else {
		CUtilities::ShowErrorDialog("\pFSMakeFSSpec failed in GetNewTempFile", err);
		return false;
	}
}



// ---------------------------------------------------------------------------------
//	€ ShowErrorDialog													[static]
// ---------------------------------------------------------------------------------
//	Shows a very simple error dialog

void
CUtilities::ShowErrorDialog(StringPtr inErrDesc, OSErr inErrNr)
{
	SInt16                 itemHit;
	AlertStdAlertParamRec  alertParam;
	
	alertParam.movable       = true;
	alertParam.helpButton    = false;
	alertParam.filterProc    = NewModalFilterProc(UModalAlerts::GetModalEventFilter());
	alertParam.defaultText   = (unsigned char *) kAlertDefaultOKText;
	alertParam.cancelText    = nil;
	alertParam.otherText     = nil;
	alertParam.defaultButton = kAlertStdAlertOKButton;
	alertParam.cancelButton  = kAlertStdAlertCancelButton;
	alertParam.position      = kWindowDefaultPosition;
	
	LStr255		alertStr(kSTRListGeneral, kUnexpectedErrorStr);
	alertStr	+= inErrDesc;
	alertStr	+= "\p  (";
	alertStr	+= inErrNr;
	alertStr	+= "\p)";
	
	if (UAppleEventsMgr::InteractWithUser() == noErr) {
		::ThreadBeginCritical();
		UDesktop::Deactivate();
		::StandardAlert(kAlertCautionAlert, alertStr, nil, &alertParam, &itemHit);
		UDesktop::Activate();
		::ThreadEndCritical();
	}
	
	::DisposeRoutineDescriptor(alertParam.filterProc);
}



// ---------------------------------------------------------------------------------
//	€ ConvertUnit														[static]
// ---------------------------------------------------------------------------------

double
CUtilities::ConvertUnit(PageSizeUnit inSrcUnit, PageSizeUnit inDestUnit, double inValue)
{
	double	conversionFactor;
	
	if (inSrcUnit == inDestUnit)
		return inValue;
	
	// convert from inSrcUnit to points
	switch (inSrcUnit) {
		case kMillimeters:	conversionFactor =  1.0 / 25.4 * 72.0;		break;
		case kCentimeters:	conversionFactor = 10.0 / 25.4 * 72.0;		break;
		case kInches:		conversionFactor = 72.0;					break;
		case kPoints:		conversionFactor =  1.0;					break;
	}
	
	// convert from points to inDestUnit
	switch (inDestUnit) {
		case kMillimeters:	conversionFactor *=  1.0 /  72.0 * 25.40;	break;
		case kCentimeters:	conversionFactor *=  1.0 /  72.0 *  2.54;	break;
		case kInches:		conversionFactor *=  1.0 /  72.0;			break;
		case kPoints:		conversionFactor *=  1.0;					break;
	}
	
	// do the real conversion
	return inValue * conversionFactor;
}



// ---------------------------------------------------------------------------------
//	€ GetStandardPageSize												[static]
// ---------------------------------------------------------------------------------

void
CUtilities::GetStandardPageSize(PaperSize inFormat, float& outWidth, float& outHeight)
{
	switch (inFormat) {
		case kA3:			outWidth = 842; outHeight = 1190; break;
		case kA4:			outWidth = 595; outHeight =  842; break;
		case kA5:			outWidth = 421; outHeight =  595; break;
		case kA6:			outWidth = 297; outHeight =  421; break;
		case kLegal:		outWidth = 612; outHeight = 1008; break;
		case kLetter:		outWidth = 612; outHeight =  792; break;
	}
}



