// ===========================================================================
//	PrefixCommon.h		©1999 Metrowerks Inc. All rights reserved.
// ===========================================================================
//	This file contains settings/material common to all targets for the
//	prefix file.

	// Make sure the key debug macro is defined (this is originally #defined
	// in PP_Debug.h, but we #define it ourselves in the enclosing prefix
	// file to use it as our "switch" macro for the debug and release builds).
	
#ifndef PP_Debug
	#error "PP_Debug is not yet #defined"
#endif

	// Set up some Mac OS related Conditional Macros...
#define OLDROUTINENAMES						0
#define OLDROUTINELOCATIONS					0

	// Set some PP macros
#define	PP_StdDialogs_Option				2	// Nav Services only
#define	PP_Uses_Old_Integer_Types			0	// nope.
#define	PP_MenuUtils_Option					1	// classic
#define PP_Uses_PowerPlant_Namespace		0	// 0 = No, 1 = Yes
#define PP_Obsolete_AllowTargetSwitch		0	// no obsolete stuff
#define PP_Suppress_Notes_20				1	// no update notes for old projects

	// For the MSL
#define	__dest_os							__mac_os

	// Set some PP Debugging macros
#define	PP_Debug_Obsolete_Support			0		// No support for obsolete stuff

#if PP_Debug

		// Of course these PowerPlant macros need to be turned on
	#define Debug_Throw
	#define Debug_Signal
	
		// Ensure all the PP debugging macros are set as we want
		// them to be. Non-Metrowerks supports are turned off by
		// default in the project stationery.
	#define	PP_MoreFiles_Support			0
	#define	PP_Spotlight_Support			0
	#define	PP_QC_Support					0
	#define PP_DebugNew_Support				1

		// Set DebugNew to full strength
	#define DEBUG_NEW						DEBUG_NEW_LEAKS

#else

		// Not debugging, so make sure everything is off
		// (only used for final builds)
	#define	PP_MoreFiles_Support			0
	#define	PP_Spotlight_Support			0
	#define	PP_QC_Support					0
	#define PP_DebugNew_Support				0
	
		// DebugNew should be off.
	#define DEBUG_NEW						DEBUG_NEW_OFF
#endif

#include <MacTypes.h>
#include <MixedMode.h>

	// Tho one would normally conditionalize this #include with
	// a check for PP_DebugNew_Support, DebugNew.h must always be
	// brought in else use of the NEW macro will generate compile
	// errors in final targets. However, it must be conditionalized
	// by the C++ check to avoid being pulled into C sources and
	// confusing the compiler.
	
#ifdef __cplusplus
	#include "DebugNew.h"
#endif