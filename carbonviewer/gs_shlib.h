/* 
   carbonviewer

   a simple viewer example for the Ghostscript shared library
   
   this defines the ghostscript wrapper
   $Id$

*/

#ifndef GS_SHLIB_H
#define GS_SHLIB_H

#include "iapi.h"

typedef struct gsapi_s {
	gsapi_revision_t version;
    int (*revision)  (gsapi_revision_t *pr, int len);
    int (*new_instance) (gs_main_instance **pinstance, void *caller_handle);
    void (*delete_instance) (gs_main_instance *instance);
    int (*set_stdio) (gs_main_instance *instance, int(*stdin_fn)(void *caller_handle, char *buf, int len), int(*stdout_fn)(void *caller_handle, const char *str, int len), int(*stderr_fn)(void *caller_handle, const char *str, int len));
    int (*set_poll) (gs_main_instance *instance, int(*poll_fn)(void *caller_handle));
    int (*set_display_callback) (gs_main_instance *instance, display_callback *callback);
    int (*init_with_args) (gs_main_instance *instance, int argc, char **argv);
    int (*run_string_begin) (gs_main_instance *instance, int user_errors, int *pexit_code);
    int (*run_string_continue) (gs_main_instance *instance, const char *str, unsigned int length, int user_errors, int *pexit_code);
    int (*run_string_end) (gs_main_instance *instance, int user_errors, int *pexit_code);
    int (*run_string_with_length) (gs_main_instance *instance, const char *str, unsigned int length, int user_errors, int *pexit_code);
    int (*run_string) (gs_main_instance *instance, const char *str, int user_errors, int *pexit_code);
    int (*run_file) (gs_main_instance *instance, const char *file_name, int user_errors, int *pexit_code);
    int (*exit) (gs_main_instance *instance);
    int (*set_visual_tracer) (struct vd_trace_interface_s *I);
} gsapi_t;


gsapi_t *cv_load_gs(void);
void cv_free_gs(gsapi_t *gsapi);

#endif /* GS_SHLIB_H */