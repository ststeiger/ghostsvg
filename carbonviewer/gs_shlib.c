/* 
   carbonviewer

   a simple viewer example for the Ghostscript shared library
   
   this implements the ghostscript wrapper
   $Id$

*/

#include <Carbon.h>
#include <stdlib.h>
#include <stdio.h>
#include "iapi.h"
#include "gs_shlib.h"


void cv_free_gs(gsapi_t *gsapi)
{
	if (gsapi != NULL) free(gsapi);
}

gsapi_t *cv_load_gs(void)
{
	OSStatus result = noErr;
	gsapi_t *gsapi;
	int	err;
	ConstStr63Param libname = "\pGhostscriptLib PPC";
	CFragConnectionID connection;
	Str255 error_msg;
	CFragSymbolClass class;
	
	// allocate our function pointers
	gsapi = malloc(sizeof(*gsapi));
	
	// load the shlib
	result = GetSharedLibrary(libname, kPowerPCCFragArch, kPrivateCFragCopy, &connection, NULL, error_msg);
	if (result != noErr) goto bail;
	
	// look up the function pointers
	result = FindSymbol(connection, "\pgsapi_revision", (Ptr*)&(gsapi->revision), &class);
	if (result != noErr) goto bail;
	err = gsapi->revision(&gsapi->version, sizeof(gsapi_revision_t));
	// TODO: verify version?
	
	result = FindSymbol(connection, "\pgsapi_new_instance", (Ptr*)&(gsapi->new_instance), &class);
	result = FindSymbol(connection, "\pgsapi_delete_instance", (Ptr*)&(gsapi->delete_instance), &class);
	result = FindSymbol(connection, "\pgsapi_set_stdio", (Ptr*)&(gsapi->set_stdio), &class);
	result = FindSymbol(connection, "\pgsapi_set_poll", (Ptr*)&(gsapi->set_poll), &class);
	result = FindSymbol(connection, "\pgsapi_set_display_callback", (Ptr*)&(gsapi->set_display_callback), &class);
	result = FindSymbol(connection, "\pgsapi_init_with_args", (Ptr*)&(gsapi->init_with_args), &class);
	result = FindSymbol(connection, "\pgsapi_run_string_begin", (Ptr*)&(gsapi->run_string_begin), &class);
	result = FindSymbol(connection, "\pgsapi_run_string_continue", (Ptr*)&(gsapi->run_string_continue), &class);
	result = FindSymbol(connection, "\pgsapi_run_string_end", (Ptr*)&(gsapi->run_string_end), &class);
	result = FindSymbol(connection, "\pgsapi_run_string_with_length", (Ptr*)&(gsapi->run_string_with_length), &class);
	result = FindSymbol(connection, "\pgsapi_run_string", (Ptr*)&(gsapi->run_string), &class);
	result = FindSymbol(connection, "\pgsapi_run_file", (Ptr*)&(gsapi->run_file), &class);
	result = FindSymbol(connection, "\pgsapi_exit", (Ptr*)&(gsapi->exit), &class);
	result = FindSymbol(connection, "\pgsapi_set_visual_tracer", (Ptr*)&(gsapi->set_visual_tracer), &class);
	
	if (result == noErr) {
		return gsapi;
	}

bail:
	cv_free_gs(gsapi);
	return NULL;
}