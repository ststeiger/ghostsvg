/* ============================================================================	*/
/*	CGSSharedLib.h							written by Bernd Heller in 1999		*/
/* ----------------------------------------------------------------------------	*/
/*	Get ONE instance of this class to handle communication with the GS DLL.		*/
/*	It inits the connection when you make a ExecCmd() call and the DLL is not	*/
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


#ifndef _H_CGSSharedLib
#define _H_CGSSharedLib
#pragma once

#define __MACINTOSH__

extern "C"
{
	#include "gsdll.h"
}

#include <LBroadcaster.h>
#include <LFile.h>
#include <LFileStream.h>

#include <CodeFragments.h>


#define		kUserForcedAbortError	-50		// this error happens when the user forced gs to abort


// CGSSharedLib is used to connect the app with the GS Shared Library

class CGSSharedLib
{
	public:
								CGSSharedLib();
								~CGSSharedLib();
		
		static LBroadcaster*	Broadcaster() { return sBroadcaster; }
		
		void					Open();
		void					Close();
		int						Init();
		
		void					FreeArgumentList();
		void					SetDefaultArgumentList();
		void					SetArgumentList(int argc, ...);
		void					AppendArgumentList(int argc, ...);
		
		int						Reset();
		void					Exit(int err=0);
		
		void					Abort()   { if (mInUse) mAbortStatus = true; }
		bool					Aborted() { return !mAbortStatus; }
		
		int						GetPict(unsigned char *dev, PicHandle *pict);
		int						ExecStr(char *str);
		int						ExecStrn(char *str, int n);
		ExceptionCode			ExecFile(LFile *inFile, SInt32 inOffset = 0,
										 SInt32 inLength = -1);
		
		static int				RedirectStdoutToFile(FSSpec* inFileSpec);
		
		char					*product;			// the GS name
		char					*copyright;			// the copyright string
		long					revision;			// the revision number
		long					revisiondate;		// the revision date
		
	    
	protected:
		static int				Callback(int message, char *str, unsigned long count);
		
		static LBroadcaster		*sBroadcaster;
		
		CFragConnectionID		mLibID;
	    
		bool					mIsInitialized;
	    bool					mAbortStatus;
	    bool					mInUse;
	    
	    int						mArgumentCount;
	    char					**mArgumentList;
	    
		char					*mFileBuffer;
		
		static LFileStream		*sStdoutDest;
	    
	    PFN_gsdll_revision		GSRevision;
		PFN_gsdll_init			GSInit;
		PFN_gsdll_exit			GSExit;
		PFN_gsdll_execute_begin	GSExecuteBegin;
		PFN_gsdll_execute_cont	GSExecuteCont;
		PFN_gsdll_execute_end	GSExecuteEnd;
		PFN_gsdll_get_pict		GSGetPict;
};


// Broadcasting messages
const MessageT	msg_GSOpenDevice		= 'gsod';
const MessageT	msg_GSCloseDevice		= 'gscd';
const MessageT	msg_GSSync				= 'gssc';
const MessageT	msg_GSPage				= 'gspg';


#endif _H_CGSSharedLib

