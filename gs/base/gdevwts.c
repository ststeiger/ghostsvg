/* Copyright (C) 2006-2007 Artifex Software, Inc.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* ALPHA: Sample Device that provides WTS screening and IMDI color management */
/* TODO: this should be configurable */

#include "errno_.h"
#include "string_.h"

#include "gdevprn.h"
#include "gdevmem.h"
#include "gsdevice.h"
#include "gxfrac.h"
#include "gsht.h"
#include "gxwts.h"
#include "gswts.h"
#include "gxgetbit.h"
#include "gscms.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"

#ifndef cmm_gcmmhlink_DEFINED
    #define cmm_gcmmhlink_DEFINED
    typedef void* gcmmhlink_t;
#endif

#ifndef cmm_gcmmhprofile_DEFINED
    #define cmm_gcmmhprofile_DEFINED
    typedef void* gcmmhprofile_t;
#endif


/* Memory arg is included in ghostpcl branch but not main branch. */
#define GS_NOTE_ERROR(m, e) gs_note_error(e)

#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

typedef struct {
    wts_screen_t *wts;
    byte *cell;
    int width_padded;
} wts_cooked_halftone;

typedef struct gx_device_wts_s {
    gx_device_common;
    gx_prn_device_common;
    wts_cooked_halftone wcooked[4];
} gx_device_wts;

static dev_proc_print_page(wtscmyk_print_page);

/* 8-bit-per-plane separated CMYK color. */

static const gx_device_procs wtscmyk_procs = {
    gdev_prn_open, NULL, NULL, gdev_prn_output_page, gdev_prn_close,
    NULL, cmyk_8bit_map_color_cmyk, NULL, NULL, NULL, NULL, NULL, NULL,
    gdev_prn_get_params, gdev_prn_put_params,
    cmyk_8bit_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device
};

const gx_device_wts gs_wtscmyk_device = {
    prn_device_body(gx_device_wts, wtscmyk_procs, "wtscmyk",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* Margins */
			4, 32, 255, 255, 256, 256, wtscmyk_print_page)
};

/* RGB with imdi conversion to CMYK and wts halftoning */

static dev_proc_open_device(wtsimdi_open_device);
static dev_proc_close_device(wtsimdi_close_device);
static dev_proc_print_page(wtsimdi_print_page);
static dev_proc_create_buf_device(wtsimdi_create_buf_device);
static dev_proc_get_bits(wtsimdi_get_bits);
static dev_proc_get_bits_rectangle(wtsimdi_contone_get_bits_rectangle);
static dev_proc_get_bits_rectangle(wtsimdi_halftoned_get_bits_rectangle);

typedef struct cached_color_s {
    gx_color_index color_index;
    byte cmyk[4];
} cached_color;

#define COLOR_CACHE_SIZE 4096
/*
 * Hash function should preserve low bits (for images) and MUST map white and
 * black to different slots
 */
#if COLOR_CACHE_SIZE == 256
#  define COLOR_TO_CACHE_INDEX(color) (((color + 0x000001) ^ (color >> 8) ^ (color >> 16)) & 0xff)
#elif COLOR_CACHE_SIZE == 4096
#  define COLOR_TO_CACHE_INDEX(color) ((color ^ (color>>4) ^ (color>>8)) & 0xfff)
#elif COLOR_CACHE_SIZE == 8192
#  define COLOR_TO_CACHE_INDEX(color) (((color + 0x010204) ^ (((color << 24) | color) >> 11)) & 0x1fff)
#elif COLOR_CACHE_SIZE == 16384
#  define COLOR_TO_CACHE_INDEX(color) (((color + 0x010204) ^ (((color << 24) | color) >> 10)) & 0x3fff)
#elif COLOR_CACHE_SIZE == 32768
#  define COLOR_TO_CACHE_INDEX(color) (((color + 0x010204) ^ (((color << 24) | color) >> 9)) & 0x7fff)
#elif COLOR_CACHE_SIZE == 65536
#  define COLOR_TO_CACHE_INDEX(color) (((color + 0x010204) ^ (((color << 24) | color) >> 8)) & 0xffff)
#else
#  define COLOR_TO_CACHE_INDEX(color) 0
#endif

typedef struct gx_device_wtsimdi_s {
    gx_device_common;
    gx_prn_device_common;
    wts_cooked_halftone wcooked[4];
    gsicc_device_cm_t *postrender_cm;
    gsicc_link_t *icc_link;
    cached_color *color_cache;
    cached_color current_color;
#ifdef DEBUG
    long color_cache_hit, color_cache_collision, cache_fill_empty, color_is_current;
#endif
} gx_device_wtsimdi;

static const gx_device_procs wtsimdi_procs =
{
    wtsimdi_open_device, NULL, NULL, gdev_prn_output_page, wtsimdi_close_device,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb,
    NULL, NULL, NULL, NULL, NULL, wtsimdi_get_bits,
    gdev_prn_get_params, gdev_prn_put_params,
    NULL, NULL, NULL, NULL, gx_page_device_get_page_device
};

const gx_device_wtsimdi gs_wtsimdi_device = {
    prn_device_body(gx_device_wtsimdi, wtsimdi_procs, "wtsimdi",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* Margins */
			3, 24, 255, 255, 256, 256, wtsimdi_print_page)
};

#if DUMMY_WTS_HALFTONE_LINE
static void
wts_halftone_line(void **wts, int y, int width, int n_planes,
		  long band_offset_x, long band_offset_y,
		  byte **dst, const byte *src)
{
    int x;
    int plane_ix;

    for (plane_ix = 0; plane_ix < n_planes; plane_ix++) {
	byte *dline = dst[plane_ix];
	for (x = 0; x < width; x += 8) {
	    byte b = 0;
	    if (src[(x + 0) * n_planes + plane_ix] > 0x20) b |= 0x80;
	    if (src[(x + 1) * n_planes + plane_ix] > 0x40) b |= 0x40;
	    if (src[(x + 2) * n_planes + plane_ix] > 0x60) b |= 0x20;
	    if (src[(x + 3) * n_planes + plane_ix] > 0x80) b |= 0x10;
	    if (src[(x + 4) * n_planes + plane_ix] > 0xa0) b |= 0x08;
	    if (src[(x + 5) * n_planes + plane_ix] > 0xc0) b |= 0x04;
	    if (src[(x + 6) * n_planes + plane_ix] > 0xe0) b |= 0x02;
	    if (src[(x + 7) * n_planes + plane_ix] > 0xfe) b |= 0x01;
	    dline[x >> 3] = b;
	}
    }
}
#endif

#if 0
/* RJW: Not used currently */
static void
wts_halftone_line_16(wts_cooked_halftone *wch, int y, int width, int n_planes,
		     long band_offset_x, long band_offset_y,
		     byte **dst, const byte *src)
{
    int x;
    int plane_ix;
    wts_screen_sample_t *samples;

    for (plane_ix = 0; plane_ix < n_planes; plane_ix++) {
	wts_screen_t *w = wch[plane_ix].wts;
	byte *dline = dst[plane_ix];
	int imax;

	for (x = 0; x < width;) {
	    int i;
	    int n_samples;
	    int cx, cy;

	    wts_get_samples(w, band_offset_x + x, band_offset_y + y, &cx, &cy, &n_samples);
	    samples = w->samples + cy * w->cell_width + cx;

	    imax = min(width - x, n_samples);
	    for (i = 0; i < imax; i += 8) {
		byte b = 0;
		int src_ix = x * 4 + plane_ix;
#if 0
		if (src[src_ix + 0 * 4] > (samples[i + 0] >> 7)) b |= 0x80;
		if (src[src_ix + 1 * 4] > (samples[i + 1] >> 7)) b |= 0x40;
		if (src[src_ix + 2 * 4] > (samples[i + 2] >> 7)) b |= 0x20;
		if (src[src_ix + 3 * 4] > (samples[i + 3] >> 7)) b |= 0x10;
		if (src[src_ix + 4 * 4] > (samples[i + 4] >> 7)) b |= 0x08;
		if (src[src_ix + 5 * 4] > (samples[i + 5] >> 7)) b |= 0x04;
		if (src[src_ix + 6 * 4] > (samples[i + 6] >> 7)) b |= 0x02;
		if (src[src_ix + 7 * 4] > (samples[i + 7] >> 7)) b |= 0x01;
#else
#if 0
		b |= (src[src_ix + 0 * 4] > (samples[i + 0] >> 7)) << 7;
		b |= (src[src_ix + 1 * 4] > (samples[i + 1] >> 7)) << 6;
		b |= (src[src_ix + 2 * 4] > (samples[i + 2] >> 7)) << 5;
		b |= (src[src_ix + 3 * 4] > (samples[i + 3] >> 7)) << 4;
		b |= (src[src_ix + 4 * 4] > (samples[i + 4] >> 7)) << 3;
		b |= (src[src_ix + 5 * 4] > (samples[i + 5] >> 7)) << 2;
		b |= (src[src_ix + 6 * 4] > (samples[i + 6] >> 7)) << 1;
		b |= (src[src_ix + 7 * 4] > (samples[i + 7] >> 7)) << 0;
#else
		b = (((unsigned int)(((int)(samples[i + 0] >> 7)) - ((int)src[src_ix + 0 * 4]))) >> 24) & 0x80;
		b |= (((unsigned int)(((int)(samples[i + 1] >> 7)) - ((int)src[src_ix + 1 * 4]))) >> 24) & 0x40;
		b |= (((unsigned int)(((int)(samples[i + 2] >> 7)) - ((int)src[src_ix + 2 * 4]))) >> 24) & 0x20;
		b |= (((unsigned int)(((int)(samples[i + 3] >> 7)) - ((int)src[src_ix + 3 * 4]))) >> 24) & 0x10;
		b |= (((unsigned int)(((int)(samples[i + 4] >> 7)) - ((int)src[src_ix + 4 * 4]))) >> 24) & 0x08;
		b |= (((unsigned int)(((int)(samples[i + 5] >> 7)) - ((int)src[src_ix + 5 * 4]))) >> 24) & 0x04;
		b |= (((unsigned int)(((int)(samples[i + 6] >> 7)) - ((int)src[src_ix + 6 * 4]))) >> 24) & 0x02;
		b |= (((unsigned int)(((int)(samples[i + 7] >> 7)) - ((int)src[src_ix + 7 * 4]))) >> 24) & 0x01;
#endif
#endif
		dline[x >> 3] = b;
		x += 8;
	    }
	}
    }
}
#endif

static void
wts_halftone_line_8(wts_cooked_halftone *wch, int y, int width, int n_planes,
		    long band_offset_x, long band_offset_y,
		    byte * dst, const byte * src)
{
    int x;
    int plane_ix;
    byte *samples;
    int halftoned_bytes = (width + 7) >> 3;

    for (plane_ix = 0; plane_ix < n_planes; plane_ix++) {
	wts_screen_t *w = wch[plane_ix].wts;
	int width_padded = wch[plane_ix].width_padded;
	byte * dline = dst + plane_ix * halftoned_bytes;
	/*byte * dline = dst[plane_ix];*/
	int imax;

	for (x = 0; x < width;) {
	    int i;
	    int n_samples;
	    int cx, cy;

	    wts_get_samples(w, band_offset_x + x, band_offset_y + y, &cx, &cy, &n_samples);
	    samples = wch[plane_ix].cell + cy * width_padded + cx;

	    imax = min(width - x, n_samples);
	    for (i = 0; i < imax; i += 8) {
		byte b = 0;
		int src_ix = x * 4 + plane_ix;
		b = (((unsigned int)(((int)(samples[i + 0])) - ((int)src[src_ix + 0 * 4]))) >> 24) & 0x80;
		b |= (((unsigned int)(((int)(samples[i + 1])) - ((int)src[src_ix + 1 * 4]))) >> 24) & 0x40;
		b |= (((unsigned int)(((int)(samples[i + 2])) - ((int)src[src_ix + 2 * 4]))) >> 24) & 0x20;
		b |= (((unsigned int)(((int)(samples[i + 3])) - ((int)src[src_ix + 3 * 4]))) >> 24) & 0x10;
		b |= (((unsigned int)(((int)(samples[i + 4])) - ((int)src[src_ix + 4 * 4]))) >> 24) & 0x08;
		b |= (((unsigned int)(((int)(samples[i + 5])) - ((int)src[src_ix + 5 * 4]))) >> 24) & 0x04;
		b |= (((unsigned int)(((int)(samples[i + 6])) - ((int)src[src_ix + 6 * 4]))) >> 24) & 0x02;
		b |= (((unsigned int)(((int)(samples[i + 7])) - ((int)src[src_ix + 7 * 4]))) >> 24) & 0x01;
		*dline++ = b;
		x += 8;
	    }
	}
    }
}

static int
wts_load_halftone(gs_memory_t *mem, wts_cooked_halftone *wch, const char *fn)
{
    FILE *f = fopen(fn, "rb");
    int size;
    byte *buf;
    wts_screen_t *wts;
    int width_padded;
    byte *cooked_cell;
    int y;

    if (f == 0) return gs_error_undefinedfilename;
    fseek(f, 0, 2);
    size = ftell(f);
    fseek(f, 0, 0);
    buf = gs_malloc(mem, size, 1, "wts_load_halftone");
    if (buf == 0) {
	return gs_error_VMerror;
    }
    (void)fread(buf, 1, size, f);
    fclose(f);
    wts = gs_wts_from_buf(buf, size);
    gs_free(mem, buf, size, 1, "wts_load_halftone");
    wch->wts = wts;
    width_padded = wts->cell_width + 7;
    wch->width_padded = width_padded;
    cooked_cell = gs_malloc(mem, width_padded * wts->cell_height, 1,
			    "wts_load_halftone");
    if (cooked_cell == 0) {
	return gs_error_VMerror;
    }

    wch->cell = cooked_cell;
    for (y = 0; y < wts->cell_height; y++) {
	wts_screen_sample_t *line = &wts->samples[y * wts->cell_width];
	byte *dstline = cooked_cell + y * width_padded;
	int x;
	for (x = 0; x < width_padded; x++) {
	    wts_screen_sample_t sample = line[x % wts->cell_width];
	    dstline[x] = (254 * (int)sample + 0x7fc0) / 0x8ff0;
	}
    }

#if 0
    /* Note: we'll need to fix the free discipline here when we change it
       in gswts.c */
    free(wts->samples);
    wts->samples = NULL;
#endif

    return 0;
}

static int
wts_init_halftones(gx_device_wts *wdev, int n_planes)
{
    int i;
    int code;

    for (i = 0; i < n_planes; i++) {
	if (wdev->wcooked[i].wts == 0) {
	    char wts_fn[256];

	    sprintf(wts_fn, "wts_plane_%d", i);
            {
              FILE *f;
              if ((f=fopen(wts_fn,"r"))) {
                fclose(f);
              } else {
	        sprintf(wts_fn, "/usr/local/lib/ghostscript/wts_plane_%d", i);
              }
            }
	    code = wts_load_halftone(wdev->memory, &wdev->wcooked[i], wts_fn);
	    if (code < 0)
		return gs_throw1(code, "could not open file '%s'", wts_fn);
	}
    }
    return 0;
}

static int
wtscmyk_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    gx_device_wts *wdev = (gx_device_wts *)pdev;
    int cmyk_bytes = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
    /* Output bytes have to be padded to 16 bits. */
    int y;
    byte *cmyk_line = 0;
    byte *data;
    int code = 0;
    int pbm_bytes;
    int n_planes = pdev->color_info.num_components;
    byte *dst = NULL;
    FILE *ostream[4] = {0};
    int i;

    /* Initialize the wts halftones. */
    code = wts_init_halftones(wdev, n_planes);
    if (code < 0) goto out;

    cmyk_line = (byte *)gs_malloc(pdev->memory, cmyk_bytes, 1, "wtscmyk_print_page(in)");
    if (cmyk_line == 0) {
	code = GS_NOTE_ERROR(pdev->memory, gs_error_VMerror);
	goto out;
    }
    pbm_bytes = (pdev->width + 7) >> 3;
    dst = gs_malloc(pdev->memory, pbm_bytes * n_planes, 1,
		    "wtscmyk_print_page");
    if (dst == 0) {
	code = GS_NOTE_ERROR(pdev->memory, gs_error_VMerror);
	goto out;
    }

    /* Create additional output streams. */
    for (i = 0; i < n_planes; i++) {
	if (i == 0) {
	    ostream[i] = prn_stream;
	} else {
	    char fn[256];
	    char plane_name[4] = "cmyk";
	    int fname_len = strlen(pdev->fname);

	    if (fname_len >= 5 && fname_len < 256) {
		strcpy(fn, pdev->fname);
		if (!strcmp(fn + fname_len - 5, "c.pbm"))
		    fn[fname_len - 5] = plane_name[i];
	    }
	    ostream[i] = fopen(fn, "wb");
	}
	fprintf(ostream[i], "P4\n%d %d\n", pdev->width, pdev->height);
    }
#if 0
    dprintf2(OPTIONAL_MEM(pdev->memory) "Output file name: %s %d\n", pdev->fname, sizeof(dst));
#endif

    /* For each raster line */
    for (y = 0; y < pdev->height; y++) {
	code = gdev_prn_get_bits(pdev, y, cmyk_line, &data);
	if (code < 0)
	    break;		/* return the code below after cleanup */
	wts_halftone_line_8(wdev->wcooked, y, pdev->width, n_planes, 
			    wdev->band_offset_x, wdev->band_offset_y, dst, data);
	for (i = 0; i < n_planes; i++)
	    if (ostream[i])
		fwrite(dst + i * pbm_bytes, 1, pbm_bytes, ostream[i]);
    }
out:
    /* Clean up... */
    gs_free(pdev->memory, cmyk_line, cmyk_bytes, 1, "wtscmyk_print_page(in)");
    gs_free(pdev->memory, dst, pbm_bytes, 1, "wtscmyk_print_page");
    for (i = 1; i < n_planes; i++) {
	/* Don't close ostream[0], because gdev_prn_close will. */
	if (ostream[i])
	    fclose(ostream[i]);
    }
    return code;
}

/*
 * Open IMDI device.
 * Load ICC device link profile (to map sRGB to FOGRA CMYK).
 */

static int
wtsimdi_open_device(gx_device *dev)
{
    gx_device_wtsimdi *idev = (gx_device_wtsimdi*)dev;
    int i;
    gsicc_rendering_param_t rendering_params;
    gsicc_device_cm_t *postrender_cm;

    /*
     * We replace create_buf_device so we can replace copy_alpha 
     * for memory device, but not clist.
     */

    idev->printer_procs.buf_procs.create_buf_device = 
	wtsimdi_create_buf_device;

    idev->postrender_cm = 
            (gsicc_device_cm_t*) gs_malloc(dev->memory->non_gc_memory,
                                            1, sizeof(gsicc_device_cm_t),
                                            "wtsimdi_open_device");
    idev->icc_link = 
            (gsicc_link_t*) gs_malloc(dev->memory->non_gc_memory,
                                            1, sizeof(gsicc_link_t),
                                            "wtsimdi_open_device");

    idev->postrender_cm->device_link_profile = NULL;  /* not used now. could be later */
    postrender_cm = idev->postrender_cm;
    postrender_cm->memory = dev->memory->non_gc_memory;
    if (postrender_cm == NULL)
        return gs_throw(-1, "Could not create post render cm object for WTS device");

    /* Get the profile handles and create the link.  We probably want
       to make this settable rather than using the defaults but that will come */
    postrender_cm->rgb_profile = gsicc_get_profile_handle_file(DEFAULT_RGB_ICC,
                    strlen(DEFAULT_RGB_ICC)+1, postrender_cm->memory);
    if (postrender_cm->rgb_profile == NULL)
        return gs_throw(-1, "Could not create RGB input profile for WTS device");

    postrender_cm->cmyk_profile = gsicc_get_profile_handle_file(DEFAULT_CMYK_ICC, 
                    strlen(DEFAULT_CMYK_ICC)+1, postrender_cm->memory);
    if (postrender_cm->cmyk_profile == NULL)
        return gs_throw(-1, "Could not create CMYK output profile for WTS device");

    if (postrender_cm->rgb_profile->num_comps != 3)
	return gs_throw1(-1, "WTS input profile must have 3 input channels. got %d.", 
                            postrender_cm->rgb_profile->num_comps);
    if (postrender_cm->cmyk_profile->num_comps != 4)
	return gs_throw1(-1, "WTS output profile must have 4 output channels. got %d.", 
                        postrender_cm->cmyk_profile->num_comps);

    /* Set up the rendering parameters */
    rendering_params.black_point_comp = true;
    rendering_params.object_type = GS_DEVICE_DOESNT_SUPPORT_TAGS;  /* Already rendered */
    rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;

    idev->icc_link->link_handle = gscms_get_link(postrender_cm->rgb_profile->profile_handle, 
                    postrender_cm->cmyk_profile->profile_handle, &rendering_params);
    idev->icc_link->is_identity = 0;
    idev->icc_link->contextptr = NULL;
    idev->icc_link->icc_link_cache = NULL;
    idev->icc_link->includes_softproof = false;

    if (idev->icc_link->link_handle == NULL)
        return gs_throw(-1, "Could not create ICC link for WTS device"); 

    /* Not sure this is the best way to do this with most CMMs. Since CMM has a cache usually */
    /* Allocate at least 1 for sytems that return NULL if requested count is 0 */
    idev->color_cache = (cached_color *)gs_malloc(idev->memory, max(COLOR_CACHE_SIZE, 1),
				sizeof(cached_color), "wtsimdi_open_device(color_cache)");
    if (idev->color_cache == NULL) {
	return_error(gs_error_VMerror);
    }
    for (i=0; i<COLOR_CACHE_SIZE; i++)		/* clear cache to empty */
	idev->color_cache[i].color_index = gx_no_color_index;
    idev->current_color.color_index = gx_no_color_index;

    /* guarantee the device bands */
    ((gx_device_printer *)dev)->space_params.banding_type = BandingAlways;
    return gdev_prn_open(dev);

    return(-1);
}

/*
 * Close device and clean up ICC structures.
 */

static int
wtsimdi_close_device(gx_device *dev)
{
    gx_device_wtsimdi *idev = (gx_device_wtsimdi*)dev;
    gsicc_device_cm_t *postrender_cm = idev->postrender_cm;

    gscms_release_link(idev->icc_link);
    rc_decrement(postrender_cm->rgb_profile, "wtsimdi_close_device");
    rc_decrement(postrender_cm->cmyk_profile, "wtsimdi_close_device");

    gs_free(postrender_cm->memory, postrender_cm, 1, sizeof(gsicc_device_cm_t), 
             "wtsimidi_close");
    gs_free(dev->memory->non_gc_memory, idev->icc_link, 1, sizeof(gsicc_link_t), 
             "wtsimidi_close");
    gs_free(dev->memory, idev->color_cache, COLOR_CACHE_SIZE, 
	sizeof(cached_color), "wtsimdi_close_device(color_cache)");
    return gdev_prn_close(dev);
}

/* Resolve a color to cmyk values, using the one-element cache. */
static int
wtsimdi_resolve_one(gx_device_wtsimdi *idev, gx_color_index color)
{
    if (color != idev->current_color.color_index) { /* quick out for same color */
	int hash = COLOR_TO_CACHE_INDEX(color);	/* 24 bits down to cache index */

	if (idev->color_cache[hash].color_index == color) {
	    /* cache hit */
#ifdef DEBUG
	    idev->color_cache_hit++;
#endif
	    idev->current_color = idev->color_cache[hash];
	} else {

	    /* cache collision or empty, fill it */

#ifdef DEBUG
	    if (idev->color_cache[hash].color_index == gx_no_color_index)
		idev->cache_fill_empty++;
	    else
		idev->color_cache_collision++;
#endif

        /* Transform the color */
        gscms_transform_color(idev->icc_link, &color,
            &(idev->current_color.cmyk), 1, NULL);

	    idev->color_cache[hash] = idev->current_color;

	}
    }
#ifdef DEBUG
    else
	idev->color_is_current++;
#endif
    return 0;
}

/* Fill a rectangle with a color. */
static int
wtsimdi_fill_rectangle(gx_device * dev,
			  int x, int y, int w, int h, gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gx_device_wtsimdi * idev = (gx_device_wtsimdi *)
	    ((mdev->target) ? (gx_device_wts *)(mdev->target)
			    : (gx_device_wts *)dev);
    wts_cooked_halftone * wch = idev->wcooked;
    int code, comp_value;
    int halftoned_bytes = (idev-> width + 7) >> 3;
    int width_padded, imax;
    byte * dst, * base;
    byte * samples;
    uint raster, plane_ix;
    int first_byte, end_x;

    fit_fill(dev, x, y, w, h);

    base = scan_line_base(mdev, y);
    raster = mdev->raster;

    end_x = x + w - 1;
    first_byte = x >> 3;

    /*
     * Check if this is a new color.  To minimize color conversion
     * time, we keep a couple of values cached in the wtsimdi device.
     */
    if ((code = wtsimdi_resolve_one(idev, color)) < 0)
	return code;

    for (; h--; y++) {
        base = scan_line_base(mdev, y);
        for (plane_ix = 0; plane_ix < 4; plane_ix++) {
	    int nfill = (end_x >> 3) - first_byte;

	    width_padded = wch[plane_ix].width_padded;
	    dst = base + first_byte + plane_ix * halftoned_bytes;
	    comp_value = idev->current_color.cmyk[plane_ix];
	    if (comp_value == 0) {
		if (nfill == 0) {
		    dst[0] &= (0xff << (8 - (x & 7))) |
			((1 << (7 - (end_x & 7))) - 1);
		} else {
		    dst[0] &= (0xff << (8 - (x & 7)));
                    memset(&dst[1], 0, nfill-1);
		    dst[nfill] &= ((1 << (7 - (end_x & 7))) - 1);
		}
	    } else if (comp_value == 0xff) {
		if (nfill == 0) {
		    dst[0] |= ~((0xff << (8 - (x & 7))) |
			((1 << (7 - (end_x & 7))) - 1));
		} else {
		    dst[0] |= ~(0xff << (8 - (x & 7)));
                    memset(&dst[1], 0xff, nfill-1);
		    dst[nfill] |= ~((1 << (7 - (end_x & 7))) - 1);
		}
	    } else {
		byte save_left = dst[0];
		byte save_right = dst[nfill];
		int i;

		for (i = 0; i < nfill + 1;) {
		    int n_samples;
		    int cx, cy;
		    int j;

		    wts_get_samples(wch[plane_ix].wts, ((dev->band_offset_x + x) & -8) + (i << 3),
				    (dev->band_offset_y + y), &cx, &cy, &n_samples);
		    samples = wch[plane_ix].cell + cy * width_padded + cx;

		    imax = min((nfill + 1 - i) << 3, n_samples);
		    for (j = 0; j < imax; j += 8) {
			int b;
			b = (((unsigned int)(((int)(samples[j + 0])) - comp_value) >> 24)) & 0x80;
			b |= (((unsigned int)(((int)(samples[j + 1])) - comp_value) >> 24)) & 0x40;
			b |= (((unsigned int)(((int)(samples[j + 2])) - comp_value) >> 24)) & 0x20;
			b |= (((unsigned int)(((int)(samples[j + 3])) - comp_value) >> 24)) & 0x10;
			b |= (((unsigned int)(((int)(samples[j + 4])) - comp_value) >> 24)) & 0x08;
			b |= (((unsigned int)(((int)(samples[j + 5])) - comp_value) >> 24)) & 0x04;
			b |= (((unsigned int)(((int)(samples[j + 6])) - comp_value) >> 24)) & 0x02;
			b |= (((unsigned int)(((int)(samples[j + 7])) - comp_value) >> 24)) & 0x01;
			dst[i + (j >> 3)] = b;
		    }
		    dst[0] = (save_left & (0xff << (8 - (x & 7)))) |
			(dst[0] & ~(0xff << (8 - (x & 7))));
		    dst[nfill] = (save_right & ((1 << (7 - (end_x & 7))) - 1)) |
			(dst[nfill] & ~((1 << (7 - (end_x & 7))) - 1));
		    i += (j >> 3);
		}
	    }
        }
    }
    return 0;
}

static int
wtsimdi_copy_mono(gx_device * dev,
		  const byte * data, int sourcex, int sraster, gx_bitmap_id id,
	int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gx_device_wtsimdi * idev = (gx_device_wtsimdi *)
	    ((mdev->target) ? (gx_device_wts *)(mdev->target)
			    : (gx_device_wts *)dev);
    wts_cooked_halftone * wch = idev->wcooked;
    int code, comp_value;
    int halftoned_bytes = (idev-> width + 7) >> 3;
    int width_padded, imax;
    byte * dst, * base;
    byte * samples;
    uint raster, plane_ix;
    int first_byte, end_x;
    const byte *src;
    int sshift;

    if (zero != gx_no_color_index)
	return gx_default_copy_mono(dev, data, sourcex, sraster, id,
				    x, y, w, h, zero, one);
    if (x < 0) {
	sourcex -= x;
	w += x;
	x = 0;
    }
    src = data + ((sourcex - (x & 7)) >> 3);
    if (y < 0) {
	src -= sraster * y;
	h += y;
	y = 0;
    }
    fit_fill(dev, x, y, w, h);

    base = scan_line_base(mdev, y);
    raster = mdev->raster;

    end_x = x + w - 1;
    first_byte = x >> 3;

    /*
     * Check if this is a new color.  To minimize color conversion
     * time, we keep a couple of values cached in the wtsimdi device.
     */
#define COPY_MONO_BLACK_IS_K 
#define COPY_MONO_OPTIMIZATION

#ifdef COPY_MONO_BLACK_IS_K 
    if (one == 0) {
	/* FIXME: This should rely on tag bits, but copy_mono black is (probably) only used for text */
	idev->current_color.cmyk[0] =idev->current_color.cmyk[1] =  idev->current_color.cmyk[2] = 0;
	idev->current_color.cmyk[3] = 0xff;	/* 100% K channel, 0% C, M, Y */
	idev->current_color.color_index = 0;
    } else
#endif
    if ((code = wtsimdi_resolve_one(idev, one)) < 0)
	return code;

    sshift = 8 - ((sourcex - x) & 7);

    for (; h--; y++) {
        base = scan_line_base(mdev, y);
        for (plane_ix = 0; plane_ix < 4; plane_ix++) {
	    int nfill = (end_x >> 3) - first_byte;
	    int i;
	    byte smask, save_left, save_right;

	    width_padded = wch[plane_ix].width_padded;
	    dst = base + first_byte + plane_ix * halftoned_bytes;
	    save_left = dst[0];
	    save_right = dst[nfill];
	    comp_value = idev->current_color.cmyk[plane_ix];
#ifdef  COPY_MONO_OPTIMIZATION
	    if (comp_value == 0) {
		for (i = 0; i < nfill + 1; i++) {
		    if ((smask = (src[i] << 8 | src[i + 1]) >> sshift) != 0)
			dst[i] = dst[i] & ~smask;
		}
	    } else if (comp_value == 0xff) {
		for (i = 0; i < nfill + 1; i++) {
		    if ((smask = (src[i] << 8 | src[i + 1]) >> sshift) != 0)
			dst[i] = smask | (dst[i] & ~smask) ;
		}
	    } else
#endif
	    {
		for (i = 0; i < nfill + 1;) {
		    int n_samples;
		    int cx, cy;
		    int j;

		    wts_get_samples(wch[plane_ix].wts, ((dev->band_offset_x + x) & -8) + (i << 3),
				    (dev->band_offset_y + y), &cx, &cy, &n_samples);
		    samples = wch[plane_ix].cell + cy * width_padded + cx;

		    imax = min((nfill + 1 - i) << 3, n_samples);
		    for (j = 0; j < imax; j += 8) {
			smask = (src[i] << 8 | src[i + 1]) >> sshift;
			if (smask) {
			    byte b;
			    b = (((unsigned int)(((int)(samples[j + 0])) - comp_value) >> 24)) & 0x80;
			    b |= (((unsigned int)(((int)(samples[j + 1])) - comp_value) >> 24)) & 0x40;
			    b |= (((unsigned int)(((int)(samples[j + 2])) - comp_value) >> 24)) & 0x20;
			    b |= (((unsigned int)(((int)(samples[j + 3])) - comp_value) >> 24)) & 0x10;
			    b |= (((unsigned int)(((int)(samples[j + 4])) - comp_value) >> 24)) & 0x08;
			    b |= (((unsigned int)(((int)(samples[j + 5])) - comp_value) >> 24)) & 0x04;
			    b |= (((unsigned int)(((int)(samples[j + 6])) - comp_value) >> 24)) & 0x02;
			    b |= (((unsigned int)(((int)(samples[j + 7])) - comp_value) >> 24)) & 0x01;
			    dst[i] = (b & smask) | (dst[i] & ~smask);
			}
			i++;
		    }
		}
	    }
	    /* Restore edge areas on left and right that may have been overwritten */
	    dst[0] = (save_left & (0xff << (8 - (x & 7)))) |
			(dst[0] & ~(0xff << (8 - (x & 7))));
	    dst[nfill] = (save_right & ((1 << (7 - (end_x & 7))) - 1)) |
			(dst[nfill] & ~((1 << (7 - (end_x & 7))) - 1));
        }
	src += sraster;
    }
    return 0;
}


/*
 * This is a clone of gx_default_get_bits except that we are adding
 * GB_HALFTONED to the parameter options.  This will tell the buffer
 * device to halftone the data (if it is not already halftoned).
 */
int
wtsimdi_get_bits(gx_device * dev, int y, byte * data, byte ** actual_data)
{	/*
	 * Hand off to get_bits_rectangle, being careful to avoid a
	 * possible recursion loop.
	 */
    dev_proc_get_bits((*save_get_bits)) = dev_proc(dev, get_bits);
    gs_int_rect rect;
    gs_get_bits_params_t params;
    int code;

    rect.p.x = 0, rect.p.y = y;
    rect.q.x = dev->width, rect.q.y = y + 1;
    params.options =
	(actual_data ? GB_RETURN_POINTER : 0) | GB_RETURN_COPY |
	(GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
    /* No depth specified, we always use native colors. */
	 GB_PACKING_CHUNKY | GB_COLORS_NATIVE | GB_ALPHA_NONE |
	 GB_HALFTONED);
    params.x_offset = 0;
    params.raster = bitmap_raster(dev->width * dev->color_info.depth);
    params.data[0] = data;
    params.original_y = y;
    set_dev_proc(dev, get_bits, gx_no_get_bits);
    code = (*dev_proc(dev, get_bits_rectangle))
	(dev, &rect, &params, NULL);
    if (actual_data)
	*actual_data = params.data[0];
    set_dev_proc(dev, get_bits, save_get_bits);
    return code;
}

int
wtsimdi_halftoned_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    /*
     * We should only get to this routine if the caller wants the halftoned
     * version of our output.  In all other cases we should be in
     * wtsimdi_contone_get_bits_rectangle.
     */
    if (!(params->options & GB_HALFTONED))
	return_error(gs_error_unknownerror);
    return mem_get_bits_rectangle(dev, prect, params, unread);
}


int
wtsimdi_contone_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    /*
     * Save the options since mem_get_bits_rectangle will change them to
     * indicate what it actually did.  We need the unmodified values.
     */
    gs_get_bits_options_t options = params->options;
    int original_y = params->original_y;
    byte * buffer = params->data[0];
    int code = mem_get_bits_rectangle(dev, prect, params, unread);

    if (code < 0)
	return code;
    if (options & GB_HALFTONED) {
	gx_device_memory * const mdev = (gx_device_memory *)dev;
        gx_device_wtsimdi * idev = (gx_device_wtsimdi *)
	    ((mdev->target) ? (gx_device_wts *)(mdev->target)
			    : (gx_device_wts *)dev);
	int width = dev->width;
	int n_planes = 4;
	int r_last = -1, g_last = -1, b_last = -1, r, g, b;
	int x;
	byte * src = params->data[0];
	/* Note that the following relies on objects being allocated to
	 * at least a 4-byte boundary for efficiency on x86 and to prevent
	 * alignment errors on CPU's that gripe about it. We know this is
	 * true due to object headers requiring alignment. In the future
	 * if we want to remove this invariant, we can use 'obj_align_mod'
	 * to allocate a slightly larger buffer and offset 'cmyk_data'
	 * to the proper alignment within the buffer.
	 */
        uint32_t * cmyk_buffer = gs_malloc(dev->memory,
				(width + 7), sizeof(uint32_t),
	   			"wtsimdi_contone_get_bits(cmyk_buffer)");
	uint32_t* cmyk_data = cmyk_buffer;

        if (cmyk_data == NULL)
	    return_error(gs_error_VMerror);

	for (x = 0; x < width; x++) {
	    r = *src++;
	    g = *src++;
	    b = *src++;
	    if (r != r_last || g != g_last || b != b_last) {
		gx_color_index color = (((b<<8) | g) << 8) | r;

		r_last = r, g_last = g, b_last = b;
		wtsimdi_resolve_one(idev, color);
	    }
	    *cmyk_data++ = *((uint32_t *)idev->current_color.cmyk);
	}
	wts_halftone_line_8(idev->wcooked, original_y, width, n_planes,
			    idev->band_offset_x, idev->band_offset_y,  buffer, (const byte *)cmyk_buffer);
	params->data[0] = buffer;
	gs_free(dev->memory, cmyk_buffer, halftoned_bytes * n_planes, 1,
		       	"wtsimdi_print_page(halftoned_data)");
    }
    return code;
}

/* Unfortunately there are many bugs associated with optimizing away
 *   raster operations to avoid creating a contone buffer (ie bug
 *   #691154).  Until those are fixed the following should remain true.
 */

const bool ALWAYS_CONTONE = true;

/*
 * We need to create custom memory buffer devices.  We use the default
 * create_buf_device routine and then we set our custom device procedures.
 */
static int 
wtsimdi_create_buf_device(gx_device **pbdev, gx_device *target, int y,
   const gx_render_plane_t *render_plane, gs_memory_t *mem,
   gx_band_complexity_t *band_complexity)
{
    int code = gx_default_create_buf_device(pbdev, target, y,
	render_plane, mem, band_complexity);
    /* Now set our custom device procedures. */
    if ((band_complexity && band_complexity->nontrivial_rops) || ALWAYS_CONTONE) {
	set_dev_proc(*pbdev, get_bits_rectangle,
		     wtsimdi_contone_get_bits_rectangle);
    } else {
	set_dev_proc(*pbdev, get_bits_rectangle, 
		     wtsimdi_halftoned_get_bits_rectangle);
	set_dev_proc(*pbdev, fill_rectangle, wtsimdi_fill_rectangle);
	set_dev_proc(*pbdev, copy_mono, wtsimdi_copy_mono);
	/* All procedures which are defined as mem_true24_* need to be either
	   implemented or replaced with a default implementation. The following
	   three don't have significant usage in testing with Altona.
	*/
	set_dev_proc(*pbdev, copy_alpha, gx_default_copy_alpha);
	set_dev_proc(*pbdev, copy_color, gx_default_copy_color);
    }
    return code;
}

/*
 * Create a row of output data.  The output is the same as the pkmraw
 * device.  This a pseudo 1 bit CMYK output.  Actually the output is
 * 3 byte RGB with each byte being either 0 or 255.
 *
 * The input data is 1 bit per component CMYK.  The data is separated
 * into planes.
 */
static void
write_pkmraw_row(int width, byte * data, FILE * pstream)
{
    if (pstream == NULL)
	return;
    {
	int x, bit;
	int halftoned_bytes = (width + 7) >> 3;
	byte * cdata = data;
	byte * mdata = data + halftoned_bytes;
	byte * ydata = mdata + halftoned_bytes;
	byte * kdata = ydata + halftoned_bytes;
	byte c = *cdata++;
	byte m = *mdata++;
	byte y = *ydata++;
	byte k = *kdata++;

	/*
	 * Contrary to what the documentation implies, gcc compiles putc
	 * as a procedure call.  This is ridiculous, but since we can't
	 * change it, we buffer groups of pixels ourselves and use fwrite.
	 */
	for (bit = 7, x = 0; x < width;) {
	    byte raw[80 * 3];	/* 80 is arbitrary, must be a multiple of 8 */
	    int end = min(x + sizeof(raw) / 3, width);
	    byte *outp = raw;

	    for (; x < end; x++) {

		if ((k >> bit) & 1) {
		    *outp++ = 0;	/* Set output color = black */
		    *outp++ = 0;
		    *outp++ = 0;
		} else {
		    *outp++ = 255 & (255 + ((c >> bit) & 1));
		    *outp++ = 255 & (255 + ((m >> bit) & 1));
		    *outp++ = 255 & (255 + ((y >> bit) & 1));
		}
		if (bit == 0) {
		    c = *cdata++;
		    m = *mdata++;
		    y = *ydata++;
		    k = *kdata++;
		    bit = 7;
		} else
		    bit--;
	    }
	    fwrite(raw, 1, outp - raw, pstream);
	}
	return;
    }
}

/*
 * Output the page raster.
 */

static int
wtsimdi_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    gx_device_wtsimdi *idev = (gx_device_wtsimdi*)pdev;
    int n_planes = 4;
    byte * halftoned_data;
    byte * halftoned_buffer = NULL;
    int halftoned_bytes, y;
    int code = 0;
    int width = pdev->width;
    int height = pdev->height;
    dev_proc_get_bits((*save_get_bits)) = dev_proc(pdev, get_bits);
    int output_is_nul = !strncmp(pdev->fname, "nul:", min(strlen(pdev->fname), 4)) ||
	!strncmp(pdev->fname, "/dev/null", min(strlen(pdev->fname), 9));

    /*
     * The printer device setup changed the get_bits routine to
     * gx_default_get_bits.  We want to use our own.
     */
    set_dev_proc(pdev, get_bits, wtsimdi_get_bits);

    /*
     * Initialize the WTS halftones.
     */
    code = wts_init_halftones((gx_device_wts *)idev, n_planes);
    if (code < 0) goto cleanup;

    /*
     * Allocate a buffer to hold the halftoned data.  This is 1 bit per
     * component CMYK data.
     */
    halftoned_bytes = (width + 7) >> 3;	/* bytes per component */
    halftoned_buffer = gs_malloc(pdev->memory, halftoned_bytes * n_planes, 1,
			       	"wtsimdi_print_page(halftoned_data)");
    if (halftoned_buffer == NULL) {
	code = GS_NOTE_ERROR(pdev->memory, gs_error_VMerror);
	goto cleanup;
    }
#ifdef DEBUG
    /* Collect stats on each page. With the allocation now done once in 'open' the
     * statistics won't be the same if other pages have been printed  */
    idev->color_cache_hit = idev->color_cache_collision =
	idev->color_is_current = idev->cache_fill_empty = 0;
#endif

    /* Initialize output file header. */
    if (!output_is_nul) {
	fprintf(prn_stream, "P6\n%d %d\n", width, height);
	fprintf(prn_stream, "# Image generated by %s %ld.%02ld (device=wtsimdi)\n",
	   	gs_program_name(), gs_revision_number() / 100,
	   			gs_revision_number() % 100);
	fprintf(prn_stream, "%d\n", 255);
    }

    /*
     * Get raster data and then write data to output file.
     */
    for (y = 0; y < height; y++) {
	/*
	 * The get_bit routines for our device returns a halftoned copy of
	 * the output data.  Print this data to the output file.
	 */
	code = gdev_prn_get_bits(pdev, y, halftoned_buffer, &halftoned_data);
	if (code < 0)
	    break;		/* return code below after cleanup */
	if (!output_is_nul)
	    write_pkmraw_row(width, halftoned_data, prn_stream);
    }
cleanup:
    if (idev->color_cache != NULL) {
#ifdef DEBUG
    { 
      int i, unused_color_cache_slots;

      for (i=0,unused_color_cache_slots=0; i<COLOR_CACHE_SIZE; i++)
	if (idev->color_cache[i].color_index == gx_no_color_index)
	    unused_color_cache_slots++;
	if_debug5(':',"wtsimdi_print_page color cache stats:"
	    " current=%ld, hits=%ld,"
	    " collisions=%ld, fill=%ld, unused_slots=%d\n",
	    idev->color_is_current, idev->color_cache_hit, 
	    idev->color_cache_collision, idev->cache_fill_empty, 
	    unused_color_cache_slots);
    }
#endif
    }
    if (halftoned_buffer != NULL)
	gs_free(pdev->memory, halftoned_buffer, halftoned_bytes * n_planes, 1,
		       	"wtsimdi_print_page(halftoned_buffer)");
    set_dev_proc(pdev, get_bits, save_get_bits);
    return code;
}
