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

/* $Id$ */
/* Pattern color mapping for Ghostscript library */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gspath2.h"
#include "gxcspace.h"		/* for gscolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxpcolor.h"
#include "gxp1impl.h"
#include "gxclist.h"
#include "gxcldev.h"
#include "gzstate.h"

#if RAW_PATTERN_DUMP
unsigned int global_pat_index = 0;
#endif

/* Define the default size of the Pattern cache. */
#define max_cached_patterns_LARGE 50
#define max_pattern_bits_LARGE 100000
#define max_cached_patterns_SMALL 5
#define max_pattern_bits_SMALL 1000
uint
gx_pat_cache_default_tiles(void)
{
#if arch_small_memory
    return max_cached_patterns_SMALL;
#else
    return (gs_debug_c('.') ? max_cached_patterns_SMALL :
	    max_cached_patterns_LARGE);
#endif
}
ulong
gx_pat_cache_default_bits(void)
{
#if arch_small_memory
    return max_pattern_bits_SMALL;
#else
    return (gs_debug_c('.') ? max_pattern_bits_SMALL :
	    max_pattern_bits_LARGE);
#endif
}

/* Define the structures for Pattern rendering and caching. */
private_st_color_tile();
private_st_color_tile_element();
private_st_pattern_cache();
private_st_device_pattern_accum();
private_st_pattern_trans();

/* ------ Pattern rendering ------ */

/* Device procedures */
static dev_proc_open_device(pattern_accum_open);
static dev_proc_close_device(pattern_accum_close);
static dev_proc_fill_rectangle(pattern_accum_fill_rectangle);
static dev_proc_copy_mono(pattern_accum_copy_mono);
static dev_proc_copy_color(pattern_accum_copy_color);
static dev_proc_get_bits_rectangle(pattern_accum_get_bits_rectangle);

/* The device descriptor */
static const gx_device_pattern_accum gs_pattern_accum_device =
{std_device_std_body_open(gx_device_pattern_accum, 0,
			  "pattern accumulator",
			  0, 0, 72, 72),
 {
     /* NOTE: all drawing procedures must be defaulted, not forwarded. */
     pattern_accum_open,
     NULL,
     NULL,
     NULL,
     pattern_accum_close,
     NULL,
     NULL,
     pattern_accum_fill_rectangle,
     gx_default_tile_rectangle,
     pattern_accum_copy_mono,
     pattern_accum_copy_color,
     NULL,
     gx_default_get_bits,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     gx_default_copy_alpha,
     NULL,
     gx_default_copy_rop,
     gx_default_fill_path,
     gx_default_stroke_path,
     gx_default_fill_mask,
     gx_default_fill_trapezoid,
     gx_default_fill_parallelogram,
     gx_default_fill_triangle,
     gx_default_draw_thin_line,
     gx_default_begin_image,
     gx_default_image_data,
     gx_default_end_image,
     gx_default_strip_tile_rectangle,
     gx_default_strip_copy_rop,
     gx_get_largest_clipping_box,
     gx_default_begin_typed_image,
     pattern_accum_get_bits_rectangle,
     NULL,
     gx_default_create_compositor,
     NULL,
     gx_default_text_begin,
     gx_default_finish_copydevice,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL
 },
 0,				/* target */
 0, 0, 0, 0			/* bitmap_memory, bits, mask, instance */
};


static int 
dummy_free_up_bandlist_memory(gx_device *cldev, bool b)
{
    return 0;
}

int 
pattern_clist_open_device(gx_device *dev)
{
    /* This function is defiled only for clist_init_bands. */
    return gs_clist_device_procs.open_device(dev);
}

static dev_proc_create_buf_device(dummy_create_buf_device)
{
    gx_device_memory *mdev = (gx_device_memory *)*pbdev;

    gs_make_mem_device(mdev, gdev_mem_device_for_bits(target->color_info.depth),
		mem, 0, target);
    return 0;
}
static dev_proc_size_buf_device(dummy_size_buf_device)
{
    return 0;
}
static dev_proc_setup_buf_device(dummy_setup_buf_device)
{
    return 0;
}
static dev_proc_destroy_buf_device(dummy_destroy_buf_device)
{
}

#ifndef MaxPatternBitmap_DEFAULT
#  define MaxPatternBitmap_DEFAULT (8*1024*1024) /* reasonable on most modern hosts */
#endif

/* Allocate a pattern accumulator, with an initial refct of 0. */
gx_device_forward *
gx_pattern_accum_alloc(gs_memory_t * mem, gs_memory_t * storage_memory, 
		       gs_pattern1_instance_t *pinst, client_name_t cname)
{
    gx_device *tdev = pinst->saved->device;
    int depth = (pinst->template.PaintType == 2 ? 1 : tdev->color_info.depth);
    int raster = (pinst->size.x * depth + 7) / 8;
    int64_t size = (int64_t)raster * pinst->size.y;
    gx_device_forward *fdev;
    int force_no_clist = 0;
    int max_pattern_bitmap = tdev->MaxPatternBitmap == 0 ? MaxPatternBitmap_DEFAULT :
				tdev->MaxPatternBitmap;

    /* 
     * If the target device can accumulate a pattern stream and the language
     * client supports high level patterns (ps and pdf only) we don't need a
     * raster or clist representation for the pattern, but the code goes
     * through the motions of creating the device anyway.  Later when the
     * pattern paint procedure is called  an error is returned and whatever
     * has been set up here is destroyed.  We try to make sure the same path
     * is taken in the code even though the device is never used because
     * there are pathological problems (see Bug689851.pdf) where the pattern
     * is so large we can't even allocate the memory for the device and the
     * dummy clist path must be used.  None of this discussion is relevant if
     * the client language does not support high level patterns or the device
     * cannot accumulate the pattern stream.      
     */
    if (pinst->saved->have_pattern_streams == 0 && (*dev_proc(pinst->saved->device, 
	pattern_manage))((gx_device *)pinst->saved->device, 
	0, pinst, pattern_manage__can_accum) == 1)
	force_no_clist = 1;
    /* Do not allow pattern to be a clist if it uses transparency.  We
       will want to fix this at some point */

    if (force_no_clist || (size < max_pattern_bitmap && !pinst->is_clist) || pinst->template.PaintType != 1 || 
                        pinst->template.uses_transparency ) {

	gx_device_pattern_accum *adev = gs_alloc_struct(mem, gx_device_pattern_accum,
			&st_device_pattern_accum, cname);

	if (adev == 0)
	    return 0;
#ifdef DEBUG
	if (pinst->is_clist)
	    eprintf("not using clist even though clist is requested\n");
#endif
	pinst->is_clist = false;
	gx_device_init((gx_device *)adev,
		       (const gx_device *)&gs_pattern_accum_device,
		       mem, true);
	adev->instance = pinst;
	adev->bitmap_memory = storage_memory;
        adev->device_icc_profile = tdev->device_icc_profile;
        rc_increment(tdev->device_icc_profile);
	fdev = (gx_device_forward *)adev;
    } else {
	gx_device_buf_procs_t buf_procs = {dummy_create_buf_device,
	dummy_size_buf_device, dummy_setup_buf_device, dummy_destroy_buf_device};
	gx_device_clist *cdev = gs_alloc_struct(mem, gx_device_clist,
			&st_device_clist, cname);
	gx_device_clist_writer *cwdev = (gx_device_clist_writer *)cdev;
	const int data_size = 1024*32;
	byte *data;

	if (cdev == 0)
	    return 0;
	/* We're not shure how big area do we need here.
	   Definitely we need 1 state in 'states'.
	   Not sure whether we need to create tile_cache, etc..
	   Note it is allocated in non-gc memory,
	   because the garbager descriptor for
	   gx_device_clist do not enumerate 'data'
	   and its subfields, assuming they do not relocate.
	   We place command list files to non-gc memory
	   due to same reason.
	 */
	data = gs_alloc_bytes(storage_memory->non_gc_memory, data_size, cname);
	if (data == NULL) {
	    gs_free_object(mem, cdev, cname);
	    return 0;
	}
	pinst->is_clist = true;
	memset(cdev, 0, sizeof(*cdev));
	cwdev->params_size = sizeof(gx_device_clist);
	cwdev->static_procs = NULL;
	cwdev->dname = "pattern-clist";
	cwdev->memory = mem;
	cwdev->stype = &st_device_clist;
	cwdev->stype_is_dynamic = false;
	cwdev->finalize = NULL;
	rc_init(cwdev, mem, 1);
	cwdev->retained = true;
	cwdev->is_open = false;
	cwdev->max_fill_band = 0;
	cwdev->color_info = tdev->color_info;
	cwdev->cached_colors = tdev->cached_colors;
	cwdev->width = pinst->size.x;
	cwdev->height = pinst->size.y;
        cwdev->LeadingEdge = tdev->LeadingEdge;
	/* Fields left zeroed :
	float MediaSize[2];
	float ImagingBBox[4];
	bool ImagingBBox_set;
	*/
	cwdev->HWResolution[0] = tdev->HWResolution[0];
	cwdev->HWResolution[1] = tdev->HWResolution[1];
	/* Fields left zeroed :
	float MarginsHWResolution[2];
	float Margins[2];
	float HWMargins[4];
	long PageCount;
	long ShowpageCount;
	int NumCopies;
	bool NumCopies_set;
	bool IgnoreNumCopies;
	*/
        cwdev->icc_cache_cl = NULL;
        cwdev->icc_table = NULL;
	cwdev->UseCIEColor = tdev->UseCIEColor;
	cwdev->LockSafetyParams = true;
	/* gx_page_device_procs page_procs; */
	cwdev->procs = gs_clist_device_procs;
	cwdev->procs.open_device = pattern_clist_open_device;
	gx_device_copy_color_params((gx_device *)cwdev, tdev);
	cwdev->target = tdev;
	clist_init_io_procs(cdev, true);
	cwdev->data = data;
	cwdev->data_size = data_size;
	cwdev->buf_procs = buf_procs ;
	cwdev->band_params.page_uses_transparency = false;
	cwdev->band_params.BandWidth = pinst->size.x;
	cwdev->band_params.BandHeight = pinst->size.x;
	cwdev->band_params.BandBufferSpace = 0;
	cwdev->do_not_open_or_close_bandfiles = false;
	cwdev->bandlist_memory = storage_memory->non_gc_memory;
	cwdev->free_up_bandlist_memory = dummy_free_up_bandlist_memory;
	cwdev->disable_mask = 0;
	cwdev->page_uses_transparency = false;
	cwdev->pinst = pinst;
	set_dev_proc(cwdev, get_clipping_box, gx_default_get_clipping_box);
	fdev = (gx_device_forward *)cdev;
        cdev->common.device_icc_profile = tdev->device_icc_profile;
        rc_increment(tdev->device_icc_profile);
    }
    check_device_separable((gx_device *)fdev);
    gx_device_forward_fill_in_procs(fdev);
    return fdev;
}

/*
 * Initialize a pattern accumulator.
 * Client must already have set instance and bitmap_memory.
 *
 * Note that mask and bits accumulators are only created if necessary.
 */
static int
pattern_accum_open(gx_device * dev)
{
    gx_device_pattern_accum *const padev = (gx_device_pattern_accum *) dev;
    const gs_pattern1_instance_t *pinst = padev->instance;
    gs_memory_t *mem = padev->bitmap_memory;
    gx_device_memory *mask = 0;
    gx_device_memory *bits = 0;
    /*
     * The client should preset the target, because the device for which the
     * pattern is being rendered may not (in general, will not) be the same
     * as the one that was current when the pattern was instantiated.
     */
    gx_device *target =
	(padev->target == 0 ? gs_currentdevice(pinst->saved) :
	 padev->target);
    int width = pinst->size.x;
    int height = pinst->size.y;
    int code = 0;
    bool mask_open = false;

    /*
     * C's bizarre coercion rules force us to copy HWResolution in pieces
     * rather than using a single assignment.
     */
#define PDSET(dev)\
  ((dev)->width = width, (dev)->height = height,\
   /*(dev)->HWResolution = target->HWResolution*/\
   (dev)->HWResolution[0] = target->HWResolution[0],\
   (dev)->HWResolution[1] = target->HWResolution[1])

    PDSET(padev);
    padev->color_info = target->color_info;

    /* If we have transparency, then fix the color info 
       now so that the mem device allocates the proper
       buffer space for the pattern template.  We can
       do this since the transparency code all */

    if(pinst->template.uses_transparency){

        /* Allocate structure that we will use for the trans pattern */
        padev->transbuff = gs_alloc_struct(mem,gx_pattern_trans_t,&st_pattern_trans,"pattern_accum_open(trans)");
        padev->transbuff->transbytes = NULL;
        padev->transbuff->pdev14 = NULL;

    } else {

        padev->transbuff = NULL;

    }

    if (pinst->uses_mask) {
        mask = gs_alloc_struct( mem,
                                gx_device_memory,
                                &st_device_memory,
		                "pattern_accum_open(mask)"
                                );
        if (mask == 0)
	    return_error(gs_error_VMerror);
        gs_make_mem_mono_device(mask, mem, 0);
        PDSET(mask);
        mask->bitmap_memory = mem;
        mask->base = 0;
        code = (*dev_proc(mask, open_device)) ((gx_device *) mask);
        if (code >= 0) {
	    mask_open = true;
	    memset(mask->base, 0, mask->raster * mask->height);
	}
    }

    if (code >= 0) {

        if (pinst->template.uses_transparency){
            
            /* In this case, we will grab the buffer created
               by the graphic state's device (which is pdf14) and
               we will be tiling that into a transparency group buffer
               to blend with the pattern accumulator's target.  Since
               all the transparency stuff is planar format, it is
               best just to keep the data in that form */

	    gx_device_set_target((gx_device_forward *)padev, target);

        } else {

	switch (pinst->template.PaintType) {
	    case 2:		/* uncolored */
		gx_device_set_target((gx_device_forward *)padev, target);
		break;
	    case 1:		/* colored */
		bits = gs_alloc_struct(mem, gx_device_memory,
				       &st_device_memory,
				       "pattern_accum_open(bits)");
		if (bits == 0)
		    code = gs_note_error(gs_error_VMerror);
		else {
		    gs_make_mem_device(bits,
			    gdev_mem_device_for_bits(padev->color_info.depth),
				       mem, -1, target);
		    PDSET(bits);
#undef PDSET
		    bits->color_info = padev->color_info;
		    bits->bitmap_memory = mem;
		    code = (*dev_proc(bits, open_device)) ((gx_device *) bits);
		    gx_device_set_target((gx_device_forward *)padev,
					 (gx_device *)bits);
		}
	}
    }
    }
    if (code < 0) {
	if (bits != 0)
	    gs_free_object(mem, bits, "pattern_accum_open(bits)");
        if (mask != 0) {
	    if (mask_open)
		(*dev_proc(mask, close_device)) ((gx_device *) mask);
	    gs_free_object(mem, mask, "pattern_accum_open(mask)");
        }
	return code;
    }
    padev->mask = mask;
    padev->bits = bits;
    /* Retain the device, so it will survive anomalous grestores. */
    gx_device_retain(dev, true);
    return code;
}

gx_pattern_trans_t*
new_pattern_trans_buff(gs_memory_t *mem)
{

    gx_pattern_trans_t *result;


    /* Allocate structure that we will use for the trans pattern */
    result = gs_alloc_struct(mem, gx_pattern_trans_t, &st_pattern_trans, "new_pattern_trans_buff");
    result->transbytes = NULL;
    result->pdev14 = NULL;

    return(result);

}


/* Close an accumulator and free the bits. */
static int
pattern_accum_close(gx_device * dev)
{
    gx_device_pattern_accum *const padev = (gx_device_pattern_accum *) dev;
    gs_memory_t *mem = padev->bitmap_memory;

    /*
     * If bits != 0, it is the target of the device; reference counting
     * will close and free it.
     */
    gx_device_set_target((gx_device_forward *)padev, NULL);
    padev->bits = 0;
    if (padev->mask != 0) {
        (*dev_proc(padev->mask, close_device)) ((gx_device *) padev->mask);
        gs_free_object(mem, padev->mask, "pattern_accum_close(mask)");
        padev->mask = 0;
    }

    if (padev->transbuff != 0) {
        gs_free_object(mem,padev->target,"pattern_accum_close(transbuff)");
        padev->transbuff = NULL;
    }

    /* Un-retain the device now, so reference counting will free it. */
    gx_device_retain(dev, false);
    return 0;
}

/* Fill a rectangle */
static int
pattern_accum_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			     gx_color_index color)
{
    gx_device_pattern_accum *const padev = (gx_device_pattern_accum *) dev;

    if (padev->bits)
	(*dev_proc(padev->target, fill_rectangle))
	    (padev->target, x, y, w, h, color);
    if (padev->mask)
        return (*dev_proc(padev->mask, fill_rectangle))
	    ((gx_device *) padev->mask, x, y, w, h, (gx_color_index) 1);
     else
        return 0;
}

/* Copy a monochrome bitmap. */
static int
pattern_accum_copy_mono(gx_device * dev, const byte * data, int data_x,
		    int raster, gx_bitmap_id id, int x, int y, int w, int h,
			gx_color_index color0, gx_color_index color1)
{
    gx_device_pattern_accum *const padev = (gx_device_pattern_accum *) dev;

    /* opt out early if nothing to render (some may think this a bug) */
    if (color0 == gx_no_color_index && color1 == gx_no_color_index)
        return 0;
    if (padev->bits)
	(*dev_proc(padev->target, copy_mono))
	    (padev->target, data, data_x, raster, id, x, y, w, h,
	     color0, color1);
    if (padev->mask) {
        if (color0 != gx_no_color_index)
	    color0 = 1;
        if (color1 != gx_no_color_index)
	    color1 = 1;
        if (color0 == 1 && color1 == 1)
	    return (*dev_proc(padev->mask, fill_rectangle))
	        ((gx_device *) padev->mask, x, y, w, h, (gx_color_index) 1);
        else
	    return (*dev_proc(padev->mask, copy_mono))
	        ((gx_device *) padev->mask, data, data_x, raster, id, x, y, w, h,
	         color0, color1);
    } else
        return 0;
}

/* Copy a color bitmap. */
static int
pattern_accum_copy_color(gx_device * dev, const byte * data, int data_x,
		    int raster, gx_bitmap_id id, int x, int y, int w, int h)
{
    gx_device_pattern_accum *const padev = (gx_device_pattern_accum *) dev;

    if (padev->bits)
	(*dev_proc(padev->target, copy_color))
	    (padev->target, data, data_x, raster, id, x, y, w, h);
    if (padev->mask)
        return (*dev_proc(padev->mask, fill_rectangle))
	    ((gx_device *) padev->mask, x, y, w, h, (gx_color_index) 1);
    else
        return 0;
}

/* Read back a rectangle of bits. */
/****** SHOULD USE MASK TO DEFINE UNREAD AREA *****/
static int
pattern_accum_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    gx_device_pattern_accum *const padev = (gx_device_pattern_accum *) dev;
    const gs_pattern1_instance_t *pinst = padev->instance;

    if (padev->bits)
	return (*dev_proc(padev->target, get_bits_rectangle))
	    (padev->target, prect, params, unread);

	if (pinst->template.PaintType == 2)
		return 0;
	else
		return_error(gs_error_Fatal); /* shouldn't happen */
}

/* ------ Color space implementation ------ */

/* Free all entries in a pattern cache. */
static bool
pattern_cache_choose_all(gx_color_tile * ctile, void *proc_data)
{
    return true;
}
static void
pattern_cache_free_all(gx_pattern_cache * pcache)
{
    gx_pattern_cache_winnow(pcache, pattern_cache_choose_all, NULL);
}

/* Allocate a Pattern cache. */
gx_pattern_cache *
gx_pattern_alloc_cache(gs_memory_t * mem, uint num_tiles, ulong max_bits)
{
    gx_pattern_cache *pcache =
    gs_alloc_struct(mem, gx_pattern_cache, &st_pattern_cache,
		    "gx_pattern_alloc_cache(struct)");
    gx_color_tile *tiles =
    gs_alloc_struct_array(mem, num_tiles, gx_color_tile,
			  &st_color_tile_element,
			  "gx_pattern_alloc_cache(tiles)");
    uint i;

    if (pcache == 0 || tiles == 0) {
	gs_free_object(mem, tiles, "gx_pattern_alloc_cache(tiles)");
	gs_free_object(mem, pcache, "gx_pattern_alloc_cache(struct)");
	return 0;
    }
    pcache->memory = mem;
    pcache->tiles = tiles;
    pcache->num_tiles = num_tiles;
    pcache->tiles_used = 0;
    pcache->next = 0;
    pcache->bits_used = 0;
    pcache->max_bits = max_bits;
    pcache->free_all = pattern_cache_free_all;
    for (i = 0; i < num_tiles; tiles++, i++) {
	tiles->id = gx_no_bitmap_id;
	/* Clear the pointers to pacify the GC. */
	uid_set_invalid(&tiles->uid);
	tiles->tbits.data = 0;
	tiles->tmask.data = 0;
	tiles->index = i;
	tiles->cdev = NULL;
        tiles->ttrans = NULL;
    }
    return pcache;
}
/* Ensure that an imager has a Pattern cache. */
static int
ensure_pattern_cache(gs_imager_state * pis)
{
    if (pis->pattern_cache == 0) {
	gx_pattern_cache *pcache =
	gx_pattern_alloc_cache(pis->memory,
			       gx_pat_cache_default_tiles(),
			       gx_pat_cache_default_bits());

	if (pcache == 0)
	    return_error(gs_error_VMerror);
	pis->pattern_cache = pcache;
    }
    return 0;
}

/* Free pattern cache and its components. */
void
gx_pattern_cache_free(gx_pattern_cache *pcache)
{
    pattern_cache_free_all(pcache);
    gs_free_object(pcache->memory, pcache->tiles, "gx_pattern_cache_free");
    pcache->tiles = NULL;
    gs_free_object(pcache->memory, pcache, "gx_pattern_cache_free");
}

/* Get and set the Pattern cache in a gstate. */
gx_pattern_cache *
gstate_pattern_cache(gs_state * pgs)
{
    return pgs->pattern_cache;
}
void
gstate_set_pattern_cache(gs_state * pgs, gx_pattern_cache * pcache)
{
    pgs->pattern_cache = pcache;
}

/* Free a Pattern cache entry. */
static void
gx_pattern_cache_free_entry(gx_pattern_cache * pcache, gx_color_tile * ctile)
{
    if ((ctile->id != gx_no_bitmap_id) && !ctile->is_dummy) {
	gs_memory_t *mem = pcache->memory;
	gx_device_memory *pmdev;
	ulong used = 0;

	/*
	 * We must initialize the memory device properly, even though
	 * we aren't using it for drawing.
	 */
	gs_make_mem_mono_device_with_copydevice(&pmdev, mem, NULL);
	if (ctile->tmask.data != 0) {
	    pmdev->width = ctile->tmask.size.x;
	    pmdev->height = ctile->tmask.size.y;
	    /*mdev.color_info.depth = 1;*/
	    gdev_mem_bitmap_size(pmdev, &used);
	    gs_free_object(mem, ctile->tmask.data,
			   "free_pattern_cache_entry(mask data)");
	    ctile->tmask.data = 0;	/* for GC */
	}
	if (ctile->tbits.data != 0) {
	    ulong tbits_used = 0;

	    pmdev->width = ctile->tbits.size.x;
	    pmdev->height = ctile->tbits.size.y;
	    pmdev->color_info.depth = ctile->depth;
	    gdev_mem_bitmap_size(pmdev, &tbits_used);
	    used += tbits_used;
	    gs_free_object(mem, ctile->tbits.data,
			   "free_pattern_cache_entry(bits data)");
	    ctile->tbits.data = 0;	/* for GC */
	}
	if (ctile->cdev != NULL) {
	    dev_proc(&ctile->cdev->common, close_device)((gx_device *)&ctile->cdev->common);
            /* Free up the icc based stuff in the clist device.  I am puzzled
               why the other objects are not released */
            clist_icc_freetable(ctile->cdev->common.icc_table, 
                            ctile->cdev->common.memory);
            rc_decrement(ctile->cdev->common.icc_cache_cl,
                            "gx_pattern_cache_free_entry");
	    gs_free_object(ctile->cdev->common.memory, ctile->cdev, "free_pattern_cache_entry(pattern-clist)");
	    ctile->cdev = NULL;
	}

        if (ctile->ttrans != NULL) {
         
            if ( ctile->ttrans->pdev14 == NULL) {

                /* This can happen if we came from the clist */
                gs_free_object(mem,ctile->ttrans->transbytes,"free_pattern_cache_entry(transbytes)");
                ctile->ttrans->transbytes = NULL;

            } else {

	        dev_proc(ctile->ttrans->pdev14, close_device)((gx_device *)ctile->ttrans->pdev14);
                ctile->ttrans->pdev14 = NULL;  /* should be ok due to pdf14_close */
                ctile->ttrans->transbytes = NULL;  /* should be ok due to pdf14_close */
            }

            used += ctile->ttrans->n_chan*ctile->ttrans->planestride;
	    gs_free_object(mem, ctile->ttrans,
			   "free_pattern_cache_entry(ttrans)");
            ctile->ttrans = NULL;

        }

	pcache->tiles_used--;
	pcache->bits_used -= used;
	ctile->id = gx_no_bitmap_id;
        gx_device_retain((gx_device *)pmdev, false);
    }
}

/*
 * Add a Pattern cache entry.  This is exported for the interpreter.
 * Note that this does not free any of the data in the accumulator
 * device, but it may zero out the bitmap_memory pointers to prevent
 * the accumulated bitmaps from being freed when the device is closed.
 */
static void make_bitmap(gx_strip_bitmap *, const gx_device_memory *, gx_bitmap_id);
int
gx_pattern_cache_add_entry(gs_imager_state * pis,
		   gx_device_forward * fdev, gx_color_tile ** pctile)
{
    gx_pattern_cache *pcache;
    const gs_pattern1_instance_t *pinst;
    ulong used = 0, mask_used = 0, trans_used = 0;
    gx_bitmap_id id;
    gx_color_tile *ctile;
    int code = ensure_pattern_cache(pis);
    extern dev_proc_open_device(pattern_clist_open_device);
    gx_device_memory *mmask = NULL;
    gx_device_memory *mbits = NULL;
    gx_pattern_trans_t *trans = NULL;


    if (code < 0)
	return code;
    pcache = pis->pattern_cache;
    if (fdev->procs.open_device != pattern_clist_open_device) {
	gx_device_pattern_accum *padev = (gx_device_pattern_accum *)fdev;
	
	mbits = padev->bits;
	mmask = padev->mask;
	pinst = padev->instance;
        trans = padev->transbuff;

	/*
	 * Check whether the pattern completely fills its box.
	 * If so, we can avoid the expensive masking operations
	 * when using the pattern.
	 */
	if (mmask != 0) {
	    int y;

	    for (y = 0; y < mmask->height; y++) {
		const byte *row = scan_line_base(mmask, y);
		int w;

		for (w = mmask->width; w > 8; w -= 8)
		    if (*row++ != 0xff)
			goto keep;
		if ((*row | (0xff >> w)) != 0xff)
		    goto keep;
	    }
	    /* We don't need a mask. */
	    mmask = 0;
	  keep:;
	}
        /* Need to get size of buffers that are being added to the cache */
	if (mbits != 0)
	    gdev_mem_bitmap_size(mbits, &used);
	if (mmask != 0) {
	    gdev_mem_bitmap_size(mmask, &mask_used);
	    used += mask_used;
	}
        if (trans != 0) {
            trans_used = trans->planestride*trans->n_chan;
            used += trans_used;
        }

    } else {
	gx_device_clist *cdev = (gx_device_clist *)fdev;
	gx_device_clist_writer * cldev = (gx_device_clist_writer *)cdev;

	code = clist_end_page(cldev);
	if (code < 0)
	    return code;
	pinst = cdev->writer.pinst;
	/* HACK: we would like to copy the pattern clist stream into the 
	   tile cache memory and properly account its size,
	   but we have no time for this development now.
	   Therefore the stream is stored outside the cache. */
	used = 0;
    }
    id = pinst->id;
    ctile = &pcache->tiles[id % pcache->num_tiles];
    gx_pattern_cache_free_entry(pcache, ctile);
    /* If too large then start freeing entries */
    while (pcache->bits_used + used > pcache->max_bits &&
	   pcache->bits_used != 0	/* allow 1 oversized entry (?) */
	) {
	pcache->next = (pcache->next + 1) % pcache->num_tiles;
	gx_pattern_cache_free_entry(pcache, &pcache->tiles[pcache->next]);
    }
    ctile->id = id;
    ctile->depth = fdev->color_info.depth;
    ctile->uid = pinst->template.uid;
    ctile->tiling_type = pinst->template.TilingType;
    ctile->step_matrix = pinst->step_matrix;
    ctile->bbox = pinst->bbox;
    ctile->is_simple = pinst->is_simple;
    ctile->has_overlap = pinst->has_overlap;
    ctile->is_dummy = false;
    if (fdev->procs.open_device != pattern_clist_open_device) {
	if (mbits != 0) {
	    make_bitmap(&ctile->tbits, mbits, gs_next_ids(pis->memory, 1));
	    mbits->bitmap_memory = 0;	/* don't free the bits */
	} else
	    ctile->tbits.data = 0;
	if (mmask != 0) {
	    make_bitmap(&ctile->tmask, mmask, id);
	    mmask->bitmap_memory = 0;	/* don't free the bits */
	} else
	    ctile->tmask.data = 0;
        if (trans != 0) {
            ctile->ttrans = trans;
        }
            
	ctile->cdev = NULL;
	pcache->bits_used += used;
    } else {
	gx_device_clist *cdev = (gx_device_clist *)fdev;
	gx_device_clist_writer *cwdev = (gx_device_clist_writer *)fdev;

	ctile->tbits.data = 0;
	ctile->tbits.size.x = 0;
	ctile->tbits.size.y = 0;
	ctile->tmask.data = 0;
	ctile->tmask.size.x = 0;
	ctile->tmask.size.y = 0;
	ctile->cdev = cdev;
#if 0 /* Don't free - tile cache is used by clist reader. */
	gs_free_object(cwdev->bandlist_memory, cwdev->data, "gx_pattern_cache_add_entry");
	cwdev->data = NULL;
	cwdev->states = NULL;
	cwdev->cbuf = NULL;
	cwdev->cnext = NULL;
	cwdev->cend = NULL;
	cwdev->ccl = NULL;
#endif
	/* Prevent freeing files on pattern_paint_cleanup : */
	cwdev->do_not_open_or_close_bandfiles = true;
    }
    pcache->tiles_used++;
    *pctile = ctile;
    return 0;
}

/* Get entry for reading a pattern from clist. */
int
gx_pattern_cache_get_entry(gs_imager_state * pis, gs_id id, gx_color_tile ** pctile)
{
    gx_pattern_cache *pcache;
    gx_color_tile *ctile;
    int code = ensure_pattern_cache(pis);

    if (code < 0)
	return code;
    pcache = pis->pattern_cache;
    ctile = &pcache->tiles[id % pcache->num_tiles];
    gx_pattern_cache_free_entry(pis->pattern_cache, ctile);
    ctile->id = id;
    *pctile = ctile;
    return 0;
}

bool 
gx_pattern_tile_is_clist(gx_color_tile *ptile)
{
    return ptile != NULL && ptile->cdev != NULL;
}

/* Add a dummy Pattern cache entry.  Stubs a pattern tile for interpreter when
   device handles high level patterns. */
int
gx_pattern_cache_add_dummy_entry(gs_imager_state *pis, 
	    gs_pattern1_instance_t *pinst, int depth)
{
    gx_color_tile *ctile;
    gx_pattern_cache *pcache;
    gx_bitmap_id id = pinst->id;
    int code = ensure_pattern_cache(pis);

    if (code < 0)
	return code;
    pcache = pis->pattern_cache;
    ctile = &pcache->tiles[id % pcache->num_tiles];
    gx_pattern_cache_free_entry(pcache, ctile);
    ctile->id = id;
    ctile->depth = depth;
    ctile->uid = pinst->template.uid;
    ctile->tiling_type = pinst->template.TilingType;
    ctile->step_matrix = pinst->step_matrix;
    ctile->bbox = pinst->bbox;
    ctile->is_simple = pinst->is_simple;
    ctile->has_overlap = pinst->has_overlap;
    ctile->is_dummy = true;
    memset(&ctile->tbits, 0 , sizeof(ctile->tbits));
    ctile->tbits.size = pinst->size;
    ctile->tbits.id = gs_no_bitmap_id;
    memset(&ctile->tmask, 0 , sizeof(ctile->tmask));
    ctile->cdev = NULL;
    ctile->ttrans = NULL;
    pcache->tiles_used++;
    return 0;
}

#if RAW_PATTERN_DUMP
/* Debug dump of pattern image data. Saved in
   interleaved form with global indexing in
   file name */
static void
dump_raw_pattern(int height, int width, int n_chan, int depth,
                byte *Buffer, int raster)
{
    char full_file_name[50];
    FILE *fid;
    int max_bands;
    int j,k;
    int byte_number, bit_position;
    unsigned char current_byte;
    unsigned char output_val;

    max_bands = ( n_chan < 57 ? n_chan : 56);   /* Photoshop handles at most 56 bands */
    sprintf(full_file_name,"%d)PATTERN_%dx%dx%d.raw",global_pat_index,width,height,max_bands);
    fid = fopen(full_file_name,"wb");
    if (depth > 1) {
        fwrite(Buffer,1,max_bands*height*width,fid);
    } else {
        /* Binary image. Lets get to 8 bit for debugging */
        for (j = 0; j < height; j++) {
            for (k = 0; k < width; k++) {
                byte_number = (int) ceil((( (float) k + 1.0) / 8.0)) - 1;
                current_byte = Buffer[j*raster+byte_number];
                bit_position = 7 - (k -  byte_number*8);
                output_val = ((current_byte >> bit_position) & 0x1) * 255;
                fwrite(&output_val,1,1,fid);
            }
        }
    }
    fclose(fid);
}
#endif

static void
make_bitmap(register gx_strip_bitmap * pbm, const gx_device_memory * mdev,
	    gx_bitmap_id id)
{
    pbm->data = mdev->base;
    pbm->raster = mdev->raster;
    pbm->rep_width = pbm->size.x = mdev->width;
    pbm->rep_height = pbm->size.y = mdev->height;
    pbm->id = id;
    pbm->rep_shift = pbm->shift = 0;

        /* Lets dump this for debug purposes */

#if RAW_PATTERN_DUMP
    dump_raw_pattern(pbm->rep_height, pbm->rep_width,
                        mdev->color_info.num_components, 
                        mdev->color_info.depth,
                        (unsigned char*) mdev->base,
                        pbm->raster);

        global_pat_index++;
 

#endif

}

/* Purge selected entries from the pattern cache. */
void
gx_pattern_cache_winnow(gx_pattern_cache * pcache,
  bool(*proc) (gx_color_tile * ctile, void *proc_data), void *proc_data)
{
    uint i;

    if (pcache == 0)		/* no cache created yet */
	return;
    for (i = 0; i < pcache->num_tiles; ++i) {
	gx_color_tile *ctile = &pcache->tiles[i];

	if (ctile->id != gx_no_bitmap_id && (*proc) (ctile, proc_data))
	    gx_pattern_cache_free_entry(pcache, ctile);
    }
}

/* blank the pattern accumulator device assumed to be in the graphics
   state */
int
gx_erase_colored_pattern(gs_state *pgs)
{
    int code;
    gx_device_pattern_accum *pdev = (gx_device_pattern_accum *)gs_currentdevice(pgs);

    if ((code = gs_gsave(pgs)) < 0)
	return code;
    if ((code = gs_setgray(pgs, 1.0)) >= 0) {
        gs_rect rect;
        gx_device_memory *mask;
        pgs->log_op = lop_default;
        rect.p.x = 0.0;
        rect.p.y = 0.0;
        rect.q.x = (double)pdev->width;
        rect.q.y = (double)pdev->height;

        /* we don't want the fill rectangle device call to use the
           mask */
        mask = pdev->mask;
        pdev->mask = NULL;
	code = gs_rectfill(pgs, &rect, 1);
        /* restore the mask */
        pdev->mask = mask;
        if (code < 0)
            return code;
    }
    /* we don't need wraparound here */
    return gs_grestore_only(pgs);
}

/* Reload a (non-null) Pattern color into the cache. */
/* *pdc is already set, except for colors.pattern.p_tile and mask.m_tile. */
int
gx_pattern_load(gx_device_color * pdc, const gs_imager_state * pis,
		gx_device * dev, gs_color_select_t select)
{
    gx_device_forward *adev = NULL;
    gs_pattern1_instance_t *pinst =
	(gs_pattern1_instance_t *)pdc->ccolor.pattern;
    gs_state *saved;
    gx_color_tile *ctile;
    gs_memory_t *mem = pis->memory;
    int code;

    if (gx_pattern_cache_lookup(pdc, pis, dev, select))
	return 0;
    /* We REALLY don't like the following cast.... */
    code = ensure_pattern_cache((gs_imager_state *) pis);
    if (code < 0)
	return code;
    /*
     * Note that adev is an internal device, so it will be freed when the
     * last reference to it from a graphics state is deleted.
     */
    adev = gx_pattern_accum_alloc(mem, pis->pattern_cache->memory, pinst, "gx_pattern_load");
    if (adev == 0)
	return_error(gs_error_VMerror);
    gx_device_set_target((gx_device_forward *)adev, dev);
    code = dev_proc(adev, open_device)((gx_device *)adev);
    if (code < 0)
	goto fail;
    saved = gs_gstate(pinst->saved);
    if (saved == 0) {
	code = gs_note_error(gs_error_VMerror);
	goto fail;
    }
    if (saved->pattern_cache == 0)
	saved->pattern_cache = pis->pattern_cache;
    gs_setdevice_no_init(saved, (gx_device *)adev);
    if (pinst->template.uses_transparency) {
        /*  This should not occur from PS or PDF, but is provided for other clients (XPS) */
	if_debug0('v', "gx_pattern_load: pushing the pdf14 compositor device into this graphics state\n");
	if ((code = gs_push_pdf14trans_device(saved)) < 0)
	    return code;
    } else {
        /* For colored patterns we clear the pattern device's
           background.  This is necessary for the anti aliasing code
           and (unfortunately) it masks a difficult to fix UMR
           affecting pcl patterns, see bug #690487.  Note we have to
           make a similar change in zpcolor.c where much of this
           pattern code is duplicated to support high level stream
           patterns. */
        if (pinst->template.PaintType == 1)
            if ((code = gx_erase_colored_pattern(saved)) < 0)
                return code;
    }

    code = (*pinst->template.PaintProc)(&pdc->ccolor, saved);
    if (code < 0) {
	dev_proc(adev, close_device)((gx_device *)adev);
	/* Freeing the state will free the device and the pdf14 compositor (if any). */
	gs_state_free(saved);
	return code;
    }
    if (pinst->template.uses_transparency) {
	if_debug0('v', "gx_pattern_load: popping the pdf14 compositor device from this graphics state\n");
	if ((code = gs_pop_pdf14trans_device(saved)) < 0)
	    return code;
    }
    /* We REALLY don't like the following cast.... */
    code = gx_pattern_cache_add_entry((gs_imager_state *)pis, 
		adev, &ctile);
    if (code >= 0) {
	if (!gx_pattern_cache_lookup(pdc, pis, dev, select)) {
	    lprintf("Pattern cache lookup failed after insertion!\n");
	    code = gs_note_error(gs_error_Fatal);
	}
    }
#ifdef DEBUG
    if (gs_debug_c('B') && adev->procs.open_device == pattern_accum_open) {
	gx_device_pattern_accum *pdev = (gx_device_pattern_accum *)adev;

        if (pdev->mask)
	    debug_dump_bitmap(pdev->mask->base, pdev->mask->raster,
			      pdev->mask->height, "[B]Pattern mask");
	if (pdev->bits)
	    debug_dump_bitmap(((gx_device_memory *) pdev->target)->base,
			      ((gx_device_memory *) pdev->target)->raster,
			      pdev->target->height, "[B]Pattern bits");
    }
#endif
    /* Free the bookkeeping structures, except for the bits and mask */
    /* data iff they are still needed. */
    dev_proc(adev, close_device)((gx_device *)adev);
    /* Free the chain of gstates. Freeing the state will free the device. */
    gs_state_free_chain(saved);
    return code;
fail:
    if (adev->procs.open_device == pattern_clist_open_device) {
	gx_device_clist *cdev = (gx_device_clist *)adev;

	gs_free_object(cdev->writer.bandlist_memory, cdev->common.data, "gx_pattern_load");
	cdev->common.data = 0;
    }
    gs_free_object(mem, adev, "gx_pattern_load");
    return code;
}

/* Remap a PatternType 1 color. */
cs_proc_remap_color(gx_remap_Pattern);	/* check the prototype */
int
gs_pattern1_remap_color(const gs_client_color * pc, const gs_color_space * pcs,
			gx_device_color * pdc, const gs_imager_state * pis,
			gx_device * dev, gs_color_select_t select)
{
    gs_pattern1_instance_t *pinst = (gs_pattern1_instance_t *)pc->pattern;
    int code;

    /* Save original color space and color info into dev color */
    pdc->ccolor = *pc;
    pdc->ccolor_valid = true;
    if (pinst == 0) {
	/* Null pattern */
	color_set_null_pattern(pdc);
	return 0;
    }
    if (pinst->template.PaintType == 2) {	/* uncolored */
	code = (pcs->base_space->type->remap_color)
	    (pc, pcs->base_space, pdc, pis, dev, select);
	if (code < 0)
	    return code;
	if (pdc->type == gx_dc_type_pure)
	    pdc->type = &gx_dc_pure_masked;
	else if (pdc->type == gx_dc_type_ht_binary)
	    pdc->type = &gx_dc_binary_masked;
	else if (pdc->type == gx_dc_type_ht_colored)
	    pdc->type = &gx_dc_colored_masked;
	else
	    return_error(gs_error_unregistered);
    } else
	color_set_null_pattern(pdc);
    pdc->mask.id = pinst->id;
    pdc->mask.m_tile = 0;
    return gx_pattern_load(pdc, pis, dev, select);
}

