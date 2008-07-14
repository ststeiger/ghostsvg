/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$*/

/* Top-level API implementation of XML Paper Specification */

/* Language wrapper implementation (see pltop.h) */

#include "ghostxps.h"

#include "pltop.h"

#include "gxdevice.h" /* so we can include gxht.h below */
#include "gxht.h" /* gsht1.h is incomplete, we need storage size of gs_halftone */
#include "gsht1.h"

static int xps_install_halftone(xps_context_t *ctx, gx_device *pdevice);

#define XPS_PARSER_MIN_INPUT_SIZE 8192

/*
 * The XPS interpeter is identical to pl_interp_t.
 * The XPS interpreter instance is derived from pl_interp_instance_t.
 */

typedef struct xps_interp_instance_s xps_interp_instance_t;

struct xps_interp_instance_s
{
    pl_interp_instance_t pl;		/* common part: must be first */

    pl_page_action_t pre_page_action;   /* action before page out */
    void *pre_page_closure;		/* closure to call pre_page_action with */
    pl_page_action_t post_page_action;  /* action before page out */
    void *post_page_closure;		/* closure to call post_page_action with */

    xps_context_t *ctx;
};

/* version and build date are not currently used */
#define XPS_VERSION NULL
#define XPS_BUILD_DATE NULL

const pl_interp_characteristics_t *
xps_imp_characteristics(const pl_interp_implementation_t *pimpl)
{
    static pl_interp_characteristics_t xps_characteristics =
    {
	"XPS",    
	"PK", /* string to recognize XPS files */
	"Artifex",
	XPS_VERSION,
	XPS_BUILD_DATE,
	XPS_PARSER_MIN_INPUT_SIZE, /* Minimum input size */
    };
    return &xps_characteristics;
}

static int  
xps_imp_allocate_interp(pl_interp_t **ppinterp,
	const pl_interp_implementation_t *pimpl,
	gs_memory_t *pmem)
{
    static pl_interp_t interp; /* there's only one interpreter */
    *ppinterp = &interp;
    return 0;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int
xps_imp_allocate_interp_instance(pl_interp_instance_t **ppinstance,
	pl_interp_t *pinterp,
	gs_memory_t *pmem)
{
    xps_interp_instance_t *instance;
    xps_context_t *ctx;
    gs_state *pgs;

    dputs("-- xps_imp_allocate_interp_instance --\n");

    instance = (xps_interp_instance_t *) gs_alloc_bytes(pmem,
	    sizeof(xps_interp_instance_t), "xps_imp_allocate_interp_instance");

    ctx = (xps_context_t *) gs_alloc_bytes(pmem,
	    sizeof(xps_context_t), "xps_imp_allocate_interp_instance");

    pgs = gs_state_alloc(pmem);

    memset(ctx, 0, sizeof(xps_context_t));

    if (!instance || !ctx || !pgs)
    {
	if (instance)
            gs_free_object(pmem, instance, "xps_imp_allocate_interp_instance");
        if (ctx)
            gs_free_object(pmem, ctx, "xps_imp_allocate_interp_instance");
        if (pgs)
            gs_state_free(pgs);
        return gs_error_VMerror;
    }

    ctx->instance = instance;
    ctx->memory = pmem;
    ctx->pgs = pgs;
    ctx->fontdir = NULL;

    /* TODO: load some builtin ICC profiles here */
    ctx->gray = gs_cspace_new_DeviceGray(ctx->memory); /* profile for gray images */
    ctx->cmyk = gs_cspace_new_DeviceCMYK(ctx->memory); /* profile for cmyk images */
    ctx->srgb = gs_cspace_new_DeviceRGB(ctx->memory);
    ctx->scrgb = gs_cspace_new_DeviceRGB(ctx->memory);

    instance->pre_page_action = 0;
    instance->pre_page_closure = 0;
    instance->post_page_action = 0;
    instance->post_page_closure = 0;

    instance->ctx = ctx;

    xps_init_font_cache(ctx);

    *ppinstance = (pl_interp_instance_t *)instance;

    return 0;
}

/* Set a client language into an interperter instance */
static int
xps_imp_set_client_instance(pl_interp_instance_t *pinstance,
	pl_interp_instance_t *pclient,
	pl_interp_instance_clients_t which_client)
{
    /* ignore */
    return 0;
}

static int
xps_imp_set_pre_page_action(pl_interp_instance_t *pinstance,
	pl_page_action_t action, void *closure)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    instance->pre_page_action = action;
    instance->pre_page_closure = closure;
    return 0;
}

static int
xps_imp_set_post_page_action(pl_interp_instance_t *pinstance,
	pl_page_action_t action, void *closure)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    instance->post_page_action = action;
    instance->post_page_closure = closure;
    return 0;
}

static int
xps_imp_set_device(pl_interp_instance_t *pinstance, gx_device *pdevice)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;
    int code;

    dputs("-- xps_imp_set_device --\n");

    gs_opendevice(pdevice);

    code = gs_setdevice_no_erase(ctx->pgs, pdevice);
    if (code < 0)
	goto cleanup_setdevice;

    gs_setaccuratecurves(ctx->pgs, true);  /* NB not sure */

    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    code = gs_gsave(ctx->pgs);
    if (code < 0)
	goto cleanup_gsave;

    code = gs_erasepage(ctx->pgs);
    if (code < 0)
	goto cleanup_erase;

    code = xps_install_halftone(ctx, pdevice);
    if (code < 0)
	goto cleanup_halftone;

    return 0;

cleanup_halftone:
cleanup_erase:
    /* undo gsave */
    gs_grestore_only(ctx->pgs);     /* destroys gs_save stack */

cleanup_gsave:
    /* undo setdevice */
    gs_nulldevice(ctx->pgs);

cleanup_setdevice:
    /* nothing to undo */
    return code;
}

static int   
xps_imp_get_device_memory(pl_interp_instance_t *pinstance, gs_memory_t **ppmem)
{
    /* huh? we do nothing here */
    return 0;
}

/* Parse a cursor-full of data */
static int
xps_imp_process(pl_interp_instance_t *pinstance, stream_cursor_read *pcursor)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;    
    xps_context_t *ctx = instance->ctx;
    return xps_process_data(ctx, pcursor);
}

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
static int
xps_imp_flush_to_eoj(pl_interp_instance_t *pinstance, stream_cursor_read *pcursor)
{
    /* assume XPS cannot be pjl embedded */
    pcursor->ptr = pcursor->limit;
    return 0;
}

/* Parser action for end-of-file */
static int
xps_imp_process_eof(pl_interp_instance_t *pinstance)
{
    return 0;
}

/* Report any errors after running a job */
static int
xps_imp_report_errors(pl_interp_instance_t *pinstance,
	int code,           /* prev termination status */
	long file_position, /* file position of error, -1 if unknown */
	bool force_to_cout  /* force errors to cout */
	)
{
    return 0;
}

/* Prepare interp instance for the next "job" */
static int
xps_imp_init_job(pl_interp_instance_t *pinstance)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;

    dputs("-- xps_imp_init_job --\n");

    ctx->first_part = NULL;
    ctx->last_part = NULL;

    ctx->start_part = NULL;

    ctx->zip_state = 0;

    ctx->use_transparency = 1;
    if (getenv("XPS_DISABLE_TRANSPARENCY"))
	ctx->use_transparency = 0;

    ctx->opacity_only = 0;
    ctx->fill_rule = 0;

    return 0;
}

/* Wrap up interp instance after a "job" */
static int
xps_imp_dnit_job(pl_interp_instance_t *pinstance)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;

    dputs("-- xps_imp_dnit_job --\n");

    while (ctx->next_page)
    {
        xps_part_t *pagepart;
	pagepart = xps_find_part(ctx, ctx->next_page->name);
	if (!pagepart)
	    dputs("  page part missing\n");
	else if (!pagepart->complete)
	    dputs("  page part incomplete\n");
	else
	{
	    xps_relation_t *rel;
	    for (rel = pagepart->relations; rel; rel = rel->next)
	    {
		xps_part_t *subpart = xps_find_part(ctx, rel->target);
		if (!subpart)
		    dprintf1("  resource '%s' missing\n", rel->target);
		else if (!subpart->complete)
		    dprintf1("  resource '%s' incomplete\n", rel->target);
		// TODO: recursive resource check...
	    }
	}

        ctx->next_page = ctx->next_page->next;
    }

    if (getenv("XPS_DEBUG_PARTS"))
        xps_debug_parts(ctx);
    if (getenv("XPS_DEBUG_TYPES"))
    {
        xps_debug_type_map(ctx, "Default", ctx->defaults);
        xps_debug_type_map(ctx, "Override", ctx->overrides);
    }
    if (getenv("XPS_DEBUG_PAGES"))
        xps_debug_fixdocseq(ctx);

    /* Free XPS parsing stuff */
    {
        xps_part_t *part = ctx->first_part;
        while (part)
        {
            xps_part_t *next = part->next;
            xps_free_part(ctx, part);
            part = next;
        }

        xps_free_fixed_pages(ctx);
        xps_free_fixed_documents(ctx);

        xps_free_type_map(ctx, ctx->overrides);
	ctx->overrides = NULL;
        xps_free_type_map(ctx, ctx->defaults);
	ctx->defaults = NULL;
    }

    return 0;
}

/* Remove a device from an interperter instance */
static int
xps_imp_remove_device(pl_interp_instance_t *pinstance)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;

    int code = 0;       /* first error status encountered */
    int error;

    dputs("-- xps_imp_remove_device --\n");

    /* return to original gstate  */
    gs_grestore_only(ctx->pgs);        /* destroys gs_save stack */

    /* Deselect device */
    /* NB */
    error = gs_nulldevice(ctx->pgs);
    if (code >= 0)
        code = error;

    return code;
}

/* Deallocate a interpreter instance */
static int
xps_imp_deallocate_interp_instance(pl_interp_instance_t *pinstance)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;
    gs_memory_t *mem = ctx->memory;

    dputs("-- xps_imp_deallocate_interp_instance --\n");

    /* language clients don't free the font cache machinery */

    // free gstate?
    gs_free_object(mem, ctx, "xps_imp_deallocate_interp_instance");
    gs_free_object(mem, instance, "xps_imp_deallocate_interp_instance");

    return 0;
}

/* Do static deinit of XPS interpreter */
static int
xps_imp_deallocate_interp(pl_interp_t *pinterp)
{
    /* nothing to do */
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t xps_implementation =
{
    xps_imp_characteristics,
    xps_imp_allocate_interp,
    xps_imp_allocate_interp_instance,
    xps_imp_set_client_instance,
    xps_imp_set_pre_page_action,
    xps_imp_set_post_page_action,
    xps_imp_set_device,
    xps_imp_init_job,
    xps_imp_process,
    xps_imp_flush_to_eoj,
    xps_imp_process_eof,
    xps_imp_report_errors,
    xps_imp_dnit_job,
    xps_imp_remove_device,
    xps_imp_deallocate_interp_instance,
    xps_imp_deallocate_interp,
    xps_imp_get_device_memory,
};

/* 
 * End-of-page function called by XPS parser.
 */
int
xps_show_page(xps_context_t *ctx, int num_copies, int flush)
{
    pl_interp_instance_t *pinstance = ctx->instance;
    xps_interp_instance_t *instance = ctx->instance;

    int code = 0;

    /* do pre-page action */
    if (instance->pre_page_action)
    {
        code = instance->pre_page_action(pinstance, instance->pre_page_closure);
        if (code < 0)
            return code;
        if (code != 0)
            return 0;    /* code > 0 means abort w/no error */
    }

    /* output the page */
    code = gs_output_page(ctx->pgs, num_copies, flush);
    if (code < 0)
        return code;

    /* do post-page action */ 
    if (instance->post_page_action)
    {
	code = instance->post_page_action(pinstance, instance->post_page_closure);
        if (code < 0)
            return code;
    }

    return 0;
}

/*
 * We need to install a halftone ourselves, this is not
 * done automatically.
 */

static float
identity_transfer(floatp tint, const gx_transfer_map *ignore_map)
{
    return tint;
}

/* The following is a 45 degree spot screen with the spots enumerated
 * in a defined order. */
static const byte order16x16[256] = {
    38, 11, 14, 32, 165, 105, 90, 171, 38, 12, 14, 33, 161, 101, 88, 167,
    30, 6, 0, 16, 61, 225, 231, 125, 30, 6, 1, 17, 63, 222, 227, 122,
    27, 3, 8, 19, 71, 242, 205, 110, 28, 4, 9, 20, 74, 246, 208, 106,
    35, 24, 22, 40, 182, 46, 56, 144, 36, 25, 22, 41, 186, 48, 58, 148,
    152, 91, 81, 174, 39, 12, 15, 34, 156, 95, 84, 178, 40, 13, 16, 34,
    69, 212, 235, 129, 31, 7, 2, 18, 66, 216, 239, 133, 32, 8, 2, 18,
    79, 254, 203, 114, 28, 4, 10, 20, 76, 250, 199, 118, 29, 5, 10, 21,
    193, 44, 54, 142, 36, 26, 23, 42, 189, 43, 52, 139, 37, 26, 24, 42,
    39, 12, 15, 33, 159, 99, 87, 169, 38, 11, 14, 33, 163, 103, 89, 172,
    31, 7, 1, 17, 65, 220, 229, 123, 30, 6, 1, 17, 62, 223, 233, 127,
    28, 4, 9, 20, 75, 248, 210, 108, 27, 3, 9, 19, 72, 244, 206, 112,
    36, 25, 23, 41, 188, 49, 60, 150, 35, 25, 22, 41, 184, 47, 57, 146,
    157, 97, 85, 180, 40, 13, 16, 35, 154, 93, 83, 176, 39, 13, 15, 34,
    67, 218, 240, 135, 32, 8, 3, 19, 70, 214, 237, 131, 31, 7, 2, 18,
    78, 252, 197, 120, 29, 5, 11, 21, 80, 255, 201, 116, 29, 5, 10, 21,
    191, 43, 51, 137, 37, 27, 24, 43, 195, 44, 53, 140, 37, 26, 23, 42
};

#define source_phase_x 4
#define source_phase_y 0

static int
xps_install_halftone(xps_context_t *ctx, gx_device *pdevice)
{
    gs_halftone ht;
    gs_string thresh;
    int code;

    int width = 16;
    int height = 16;
    thresh.data = order16x16;
    thresh.size = width * height;

    if (gx_device_must_halftone(pdevice))
    {
	ht.type = ht_type_threshold;
	ht.params.threshold.width = width;
	ht.params.threshold.height = height;
	ht.params.threshold.thresholds.data = thresh.data;
	ht.params.threshold.thresholds.size = thresh.size;
	ht.params.threshold.transfer = 0;
	ht.params.threshold.transfer_closure.proc = 0;

	gs_settransfer(ctx->pgs, identity_transfer);

	code = gs_sethalftone(ctx->pgs, &ht);
	if (code < 0)
	    return gs_throw(code, "could not install halftone");

	code = gs_sethalftonephase(ctx->pgs, 0, 0);
	if (code < 0)
	    return gs_throw(code, "could not set halftone phase");
    }

    return 0;
}



