/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*$Id$ */

/* library context functionality for ghostscript
 * api callers get a gs_main_instance
 */

/* Capture stdin/out/err before gs.h redefines them. */
#include "stdio_.h"
#include "string_.h" /* memset */
#include "gp.h"

static void
gs_lib_ctx_get_real_stdio(FILE **in, FILE **out, FILE **err)
{
    *in = stdin;
    *out = stdout;
    *err = stderr;
}


#include "gslibctx.h"
#include "gsmemory.h"

static gs_memory_t *mem_err_print = NULL;


gs_memory_t *
gs_lib_ctx_get_non_gc_memory_t()
{
    return mem_err_print ? mem_err_print->non_gc_memory : NULL;
}


int gs_lib_ctx_init( gs_memory_t *mem )
{
    gs_lib_ctx_t *pio = 0;

    if ( mem == 0 )
        return -1;  /* assert mem != 0 */

    mem_err_print = mem;

    if (mem->gs_lib_ctx) /* one time initialization */
        return 0;

    pio = mem->gs_lib_ctx =
        (gs_lib_ctx_t*)gs_alloc_bytes_immovable(mem,
                                                sizeof(gs_lib_ctx_t),
                                                "gs_lib_ctx_init");
    if( pio == 0 )
        return -1;

    /* Wholesale blanking is cheaper than retail, and scales better when new
     * fields are added. */
    memset(pio, 0, sizeof(*pio));
    /* Now set the non zero/false/NULL things */
    pio->memory               = mem;
    gs_lib_ctx_get_real_stdio(&pio->fstdin, &pio->fstdout, &pio->fstderr );
    pio->stdin_is_interactive = true;
    /* id's 1 through 4 are reserved for Device color spaces; see gscspace.h */
    pio->gs_next_id           = 5;  /* this implies that each thread has its own complete state */
    pio->BITTAG               = GS_DEVICE_DOESNT_SUPPORT_TAGS;

    gp_get_realtime(pio->real_time_0);

    return 0;
}

gs_lib_ctx_t *gs_lib_ctx_get_interp_instance(const gs_memory_t *mem)
{
    if (mem == NULL)
        return NULL;
    return mem->gs_lib_ctx;
}

/* Provide a single point for all "C" stdout and stderr.
 */

int outwrite(const gs_memory_t *mem, const char *str, int len)
{
    int code;
    FILE *fout;
    gs_lib_ctx_t *pio = mem->gs_lib_ctx;

    if (len == 0)
        return 0;
    if (pio->stdout_is_redirected) {
        if (pio->stdout_to_stderr)
            return errwrite(mem, str, len);
        fout = pio->fstdout2;
    }
    else if (pio->stdout_fn) {
        return (*pio->stdout_fn)(pio->caller_handle, str, len);
    }
    else {
        fout = pio->fstdout;
    }
    code = fwrite(str, 1, len, fout);
    fflush(fout);
    return code;
}

int errwrite_nomem(const char *str, int len)
{
    return errwrite(mem_err_print, str, len);
}

int errwrite(const gs_memory_t *mem, const char *str, int len)
{
    int code;
    if (len == 0)
        return 0;
    if (mem->gs_lib_ctx->stderr_fn)
        return (*mem->gs_lib_ctx->stderr_fn)(mem->gs_lib_ctx->caller_handle, str, len);

    code = fwrite(str, 1, len, mem->gs_lib_ctx->fstderr);
    fflush(mem->gs_lib_ctx->fstderr);
    return code;
}

void outflush(const gs_memory_t *mem)
{
    if (mem->gs_lib_ctx->stdout_is_redirected) {
        if (mem->gs_lib_ctx->stdout_to_stderr) {
            if (!mem->gs_lib_ctx->stderr_fn)
                fflush(mem->gs_lib_ctx->fstderr);
        }
        else
            fflush(mem->gs_lib_ctx->fstdout2);
    }
    else if (!mem->gs_lib_ctx->stdout_fn)
        fflush(mem->gs_lib_ctx->fstdout);
}

void errflush_nomem(void)
{
    errflush(mem_err_print);
}
void errflush(const gs_memory_t *mem)
{
    if (!mem->gs_lib_ctx->stderr_fn)
        fflush(mem->gs_lib_ctx->fstderr);
    /* else nothing to flush */
}


