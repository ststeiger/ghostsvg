/* ============================================================================	*/
/*	CGSSharedLib.cp							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	Get an instance of this class to handle communication with the GS DLL.		*/
/*	It inits the connection when you make a ExecStr() call and the DLL is not	*/
/*	initialized.																*/
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


#include <LString.h>
#include <LFileStream.h>
#include <UModalDialogs.h>
#include <UAppleEventsMgr.h>
#include <UDesktop.h>

#include <Threads.h>
#include <Errors.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "CUtilities.h"
#include "CGSApplication.h"
#include "CGSSharedLib.h"
#include "MacGSViewConstants.h"


LBroadcaster*	CGSSharedLib::sBroadcaster = new LBroadcaster();
LFileStream*	CGSSharedLib::sStdoutDest = NULL;

// ---------------------------------------------------------------------------
//	€ CGSSharedLib
// ---------------------------------------------------------------------------
//	Constructor

CGSSharedLib::CGSSharedLib()
{
	mLibID			= 0;
	mAbortStatus	= 0;
	mInUse			= false;
	mIsInitialized	= false;
	mFileBuffer		= NULL;
	
	mArgumentCount	= 0;
	mArgumentList	= 0;
	
	Open();
}


// ---------------------------------------------------------------------------
//	€ ~CGSSharedLib
// ---------------------------------------------------------------------------
//	Destructor: Quits the DLL and closes the command connection to it

CGSSharedLib::~CGSSharedLib()
{
	Exit();
	
	if (mFileBuffer)
		free(mFileBuffer);
	
	if (mArgumentList)
		FreeArgumentList();
	
	// unload library
	Close();
}


// ---------------------------------------------------------------------------
//	€ Open
// ---------------------------------------------------------------------------
//	Opens the DLL

void
CGSSharedLib::Open()
{
	OSErr				err;
	Ptr					mainAddr;
	Str255				errLib;
	CFragSymbolClass	dummy;
	
	// load a new instance of the library
	err = ::GetSharedLibrary("\pGhostScriptLib PPC", kPowerPCCFragArch, kPrivateCFragCopy,
							 &mLibID, &mainAddr, errLib);
	
	if (err != noErr) {
		SInt16					itemHit;
		AlertStdAlertParamRec	alertParam;
		
		alertParam.movable			= true;
		alertParam.helpButton		= false;
		alertParam.filterProc		= NewModalFilterProc(UModalAlerts::GetModalEventFilter());
		alertParam.defaultText		= (unsigned char *) kAlertDefaultOKText;
		alertParam.cancelText		= nil;
		alertParam.otherText		= nil;
		alertParam.defaultButton	= kAlertStdAlertOKButton;
		alertParam.cancelButton		= kAlertStdAlertCancelButton;
		alertParam.position			= kWindowDefaultPosition;
		
		if (UAppleEventsMgr::InteractWithUser() == noErr) {
			::ThreadBeginCritical();
			UDesktop::Deactivate();
			
			if (err == cfragNoClientMemErr || err == cfragNoPrivateMemErr) {
				::StandardAlert(kAlertCautionAlert,
								LStr255(kSTRListGeneral, kNotEnoughMemErrorStr),
								nil, &alertParam, &itemHit);
			} else {
				::StandardAlert(kAlertCautionAlert,
								LStr255(kSTRListGeneral, kLibNotFoundStr),
								nil, &alertParam, &itemHit);
			}
			UDesktop::Activate();
			::ThreadEndCritical();
		}
		
		::DisposeRoutineDescriptor(alertParam.filterProc);
		return;
	}
	
	// get function pointers
	err = ::FindSymbol(mLibID, "\pgsdll_init",			(Ptr*) &GSInit, &dummy);
	err = ::FindSymbol(mLibID, "\pgsdll_exit",			(Ptr*) &GSExit, &dummy);
	err = ::FindSymbol(mLibID, "\pgsdll_revision",		(Ptr*) &GSRevision, &dummy);
	err = ::FindSymbol(mLibID, "\pgsdll_execute_begin",	(Ptr*) &GSExecuteBegin, &dummy);
	err = ::FindSymbol(mLibID, "\pgsdll_execute_cont",	(Ptr*) &GSExecuteCont, &dummy);
	err = ::FindSymbol(mLibID, "\pgsdll_execute_end",	(Ptr*) &GSExecuteEnd, &dummy);
	err = ::FindSymbol(mLibID, "\pgsdll_get_pict",		(Ptr*) &GSGetPict, &dummy);
	
	// get version
	GSRevision(&product, &copyright, &revision, &revisiondate);
}



// ---------------------------------------------------------------------------
//	€ Close
// ---------------------------------------------------------------------------
//	Closes the DLL

void
CGSSharedLib::Close()
{
	if (mLibID) {
		::CloseConnection(&mLibID);
		mLibID = 0;
	}
}



// ---------------------------------------------------------------------------
//	€ Init
// ---------------------------------------------------------------------------
//	Startup the GS DLL with constant command line arguments, after the init
//	ExecStr() can be used. Is done automatically whenever needed.

int
CGSSharedLib::Init()
{
	if (mLibID == 0) {
		// we don't have a connection to the lib, perhaps not enough memory
		// the caller has to handle this!
		return -1;
	}
	
	if (revision < 600) {	// check revision here if needed 
		return -1;
	}
	
	if (mArgumentList == 0)
		SetDefaultArgumentList();
	
	if (mIsInitialized) {	// exit library first if initialized
		GSExecuteEnd();
		GSExit();
	}
	
	int		errCode;
	
	if ((errCode = GSInit(Callback, (HWND) this, mArgumentCount, mArgumentList)) != 0)
		return errCode;
	if ((errCode = GSExecuteBegin()) != 0)
		return errCode;
	
	mIsInitialized	= true;
	
	return 0;	// everything went fine, we can start to send commands
}



// ---------------------------------------------------------------------------
//	€ FreeArgumentList
// ---------------------------------------------------------------------------
//	frees mArgumentList

void
CGSSharedLib::FreeArgumentList()
{
	if (mArgumentList) {
		int	i = 0;
		while (mArgumentList[i]) {
			free(mArgumentList[i]);
			i++;
		}
		free(mArgumentList);
	}
	
	mArgumentList = 0;
	mArgumentCount = 0;
}



// ---------------------------------------------------------------------------
//	€ SetDefaultArgumentList
// ---------------------------------------------------------------------------
//	sets mArgumentList to default values

void
CGSSharedLib::SetDefaultArgumentList()
{
#if PP_Debug
	SetArgumentList(5, "MacGSView", "-dNOPAUSE", "-dNODISPLAY", "-dFIXEDRESOLUTION",
					   "-dFIXEDMEDIA");
#else
	SetArgumentList(6, "MacGSView", "-q", "-dNOPAUSE", "-dNODISPLAY", "-dFIXEDRESOLUTION",
					   "-dFIXEDMEDIA");
#endif
}



// ---------------------------------------------------------------------------
//	€ SetArgumentList
// ---------------------------------------------------------------------------
//	sets mArgumentList to submitted values

void
CGSSharedLib::SetArgumentList(int argc, ...)
{
	if (argc > 0) {
		if (mArgumentList)
			FreeArgumentList();
		
		va_list argPtr;
		va_start(argPtr, argc);
		mArgumentList = (char**) malloc(sizeof(char*) * 100);
		for (int i=0; i<argc; i++) {
			mArgumentList[mArgumentCount] = (char*) malloc(100);
			strcpy(mArgumentList[mArgumentCount++], va_arg(argPtr, char*));
		}
		
		mArgumentList[mArgumentCount] = NULL;
	}
}



// ---------------------------------------------------------------------------
//	€ AppendArgumentList
// ---------------------------------------------------------------------------
//	appends submitted values to mArgumentList

void
CGSSharedLib::AppendArgumentList(int argc, ...)
{
	if (argc > 0) {
		va_list argPtr;
		va_start(argPtr, argc);
		
		if (mArgumentList == 0)
			mArgumentList = (char**) malloc(sizeof(char*) * 100);
		
		for (int i=0; i<argc; i++) {
			mArgumentList[mArgumentCount] = (char*) malloc(100);
			strcpy(mArgumentList[mArgumentCount++], va_arg(argPtr, char*));
		}
		
		mArgumentList[mArgumentCount] = NULL;
	}
}



// ---------------------------------------------------------------------------
//	€ Reset
// ---------------------------------------------------------------------------
//	Quits gs and starts it up again

int
CGSSharedLib::Reset()
{
	if (mIsInitialized) {
		GSExecuteEnd();
		GSExit();
	}
	
	return Init();
}


// ---------------------------------------------------------------------------
//	€ Exit
// ---------------------------------------------------------------------------
//	Exits the DLL. When an error number is given, it decides how to exit.

void
CGSSharedLib::Exit(int err)
{
	if (mIsInitialized) {
		if (err <= -100)	// no fatal error, do execute_end first
			GSExecuteEnd();
		GSExit();
		mIsInitialized = false;
	}
}



// ---------------------------------------------------------------------------
//	€ GetPict
// ---------------------------------------------------------------------------
//	Returns the pointer to the PICT used by the device dev

int
CGSSharedLib::GetPict(unsigned char *dev, PicHandle *pict)
{
	return GSGetPict(dev, pict);
}


// ---------------------------------------------------------------------------
//	€ ExecStr
// ---------------------------------------------------------------------------
//	Executes the command string which must be \0 terminated (\n is needed to
//  start the action)

int
CGSSharedLib::ExecStr(char *str)
{
	return ExecStrn(str, strlen(str));
}


// ---------------------------------------------------------------------------
//	€ ExecStrn
// ---------------------------------------------------------------------------
//	Executes n bytes of the command string (\n is needed to start the action)

int
CGSSharedLib::ExecStrn(char *str, int n)
{
	int		errCode;
	
	if (!mIsInitialized) {
		if ((errCode = Init()) != 0)
			return errCode;
	}
	
	mInUse = true;
	errCode = GSExecuteCont(str, n);
	mInUse = false;
	
	if (mAbortStatus) {
		mAbortStatus = false;
		errCode = kUserForcedAbortError;	// ensures that the thread ends
	}
	
	if (abs(errCode) == GSDLL_INIT_QUIT) errCode = 0;
	if (errCode) {
		Exit(errCode);
	}
	
	return errCode;
}


// ---------------------------------------------------------------------------
//	€ ExecFile
// ---------------------------------------------------------------------------
//	Uses ExecCmd to send a part of a file as command string to the DLL
//	(it uses 4096-byte-blocks, not sure how much is possible for GS)

ExceptionCode
CGSSharedLib::ExecFile(LFile *inFile, SInt32 inOffset, SInt32 inLength)
{
	const int			BufferSize = 65535;
	
	ExceptionCode		errCode;
	
	FSSpec				theFileSpec;
	inFile->GetSpecifier(theFileSpec);
	LFileStream			theFile(theFileSpec);
	
	SInt32				readBytes;
	
	if (mFileBuffer == NULL) {
		if ((mFileBuffer = (char*) malloc(BufferSize)) == NULL)
			return -1;
	}
	
	// Open the file
	theFile.OpenDataFork(fsRdPerm);
	theFile.SetMarker(inOffset, streamFrom_Start);
	
	if (inLength == -1)
		inLength = theFile.GetLength();
	
	do {
		if (inLength < BufferSize)
			readBytes = inLength;
		else
			readBytes = BufferSize;
		
		if ((errCode = theFile.GetBytes(mFileBuffer, readBytes)) != 0) {
			CUtilities::ShowErrorDialog("\pGetBytes failed in ExecFile", errCode);
			break;
		}
		if ((errCode = ExecStrn(mFileBuffer, readBytes)) != 0)
			break;
		
		inLength -= readBytes;
	} while (inLength);
	
	// Close the File
	theFile.CloseDataFork();
	
	return errCode;
}


// ---------------------------------------------------------------------------
//	€ RedirectStdoutToFile											[static]
// ---------------------------------------------------------------------------

int
CGSSharedLib::RedirectStdoutToFile(FSSpec* inFileSpec)
{
	OSErr	err;
	
	if (inFileSpec == NULL) {
		if (sStdoutDest) {
			delete sStdoutDest;
			sStdoutDest = NULL;
		}
		return 0;
	}
	
	if ((sStdoutDest = new LFileStream(*inFileSpec)) == NULL)
		return -1;
	
	// try to create the file, no matter if we need
	::FSpCreate(inFileSpec, 'ttxt', 'TEXT', smSystemScript);
	if ((err = sStdoutDest->OpenDataFork(fsWrPerm)) < 0) {
		delete sStdoutDest;
		sStdoutDest = NULL;
		return err;
	}
	
	return 0;
}



// ---------------------------------------------------------------------------
//	€ Callback														[static]
// ---------------------------------------------------------------------------
//	Handles the callbacks from the DLL

int
CGSSharedLib::Callback(int message, char *str, unsigned long count)
{
	switch (message) {
		case GSDLL_STDIN:
			return 0;
		
		case GSDLL_STDOUT:
	#if PP_Debug
			if (str != NULL)
				fwrite(str, 1, count, stdout);
	#endif
			if (str != NULL && sStdoutDest != NULL) {
				SInt32	constCount = count;		// idiotic, but needed
				
				// replace \n by \r, to make the file mac conform
				for (int i=0; i<count; i++)
					if (str[i] == '\n') str[i] = '\r';
				
				sStdoutDest->PutBytes(str, constCount);
			}
			return count;
		
		case GSDLL_DEVICE:
	#if PP_Debug
			fprintf(stdout,"Callback: DEVICE %p %s\n", str, count ? "open" : "close");
	#endif
			if (count) {
				CGSSharedLib::Broadcaster()->BroadcastMessage( msg_GSOpenDevice, str );
			} else {
				CGSSharedLib::Broadcaster()->BroadcastMessage( msg_GSCloseDevice, str );
			}
			
			break;
		
		case GSDLL_SYNC:
	#if PP_Debug
			fprintf(stdout,"Callback: SYNC %p\n", str);
	#endif
			CGSSharedLib::Broadcaster()->BroadcastMessage(msg_GSSync, str);
			break;
		
		case GSDLL_PAGE:
	#if PP_Debug
			fprintf(stdout,"Callback: PAGE %p\n", str);
	#endif
			CGSSharedLib::Broadcaster()->BroadcastMessage(msg_GSPage, str);
			break;
		
		case GSDLL_SIZE:
	#if PP_Debug
			fprintf(stdout,"Callback: SIZE %p width=%d height=%d\n", str,
					(int)(count & 0xffff), (int)((count>>16) & 0xffff));
	#endif
			break;
		
		case GSDLL_POLL:
			// abort rendering ?
			if (((CGSSharedLib *) count)->mAbortStatus) {
				return 1;
			} else {
				static UInt32	lastYieldTicks = 0;
				if ((::TickCount() - lastYieldTicks) > 10) {
					lastYieldTicks = ::TickCount();
					::YieldToAnyThread();
				}
				return 0;
			}
		
	#if PP_Debug
		default:
			fprintf(stdout,"Callback: Unknown message=%d\n",message);
	#endif
	}
	
	return 0;
}




