/* Copyright (C) 2006 artofcode LLC.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gslt_image_jpeg.c 2713 2007-01-02 22:22:14Z henrys $ */
/* gslt image loading implementation for JPEG images */

#include "std.h"
#include "gsmemory.h"
#include "stream.h"
#include "strimpl.h"
#include "gsstate.h"
#include "jpeglib_.h"
#include "sdct.h"
#include "sjpeg.h"
#include "gslt_image.h"
#include "gserror.h"

/* import the gs structure descriptor for gslt_image_t */
extern const gs_memory_struct_type_t st_gslt_image;

static int gslt_report_error(stream_state * st, const char *str)
{
    (void) gs_throw1(-1, "%s", str);
    return 0;
}

gslt_image_t *
gslt_image_decode_jpeg(gs_memory_t *mem, byte *buf, int len)
{
    gslt_image_t *image;
    jpeg_decompress_data jddp;
    stream_DCT_state state;
    stream_cursor_read rp;
    stream_cursor_write wp;
    int code, error = gs_okay;
    int wlen;
    byte *wbuf;

    s_init_state((stream_state*)&state, &s_DCTD_template, mem);
    state.report_error = gslt_report_error;

    s_DCTD_template.set_defaults((stream_state*)&state);

    state.jpeg_memory = mem;
    state.data.decompress = &jddp;

    jddp.template = s_DCTD_template;
    jddp.memory = mem;
    jddp.scanline_buffer = NULL;

    if ((code = gs_jpeg_create_decompress(&state)) < 0) {
        error = gs_throw(-1, "cannot gs_jpeg_create_decompress");
	return NULL;
    }

    s_DCTD_template.init((stream_state*)&state);

    rp.ptr = buf - 1;
    rp.limit = buf + len - 1;

    /* read the header only by not having a write buffer */
    wp.ptr = 0;
    wp.limit = 0;

    code = s_DCTD_template.process((stream_state*)&state, &rp, &wp, true);
    if (code != 1) {
	error = gs_throw(-1, "premature EOF or error in jpeg");
	return NULL;
    }

    image = gs_alloc_struct_immovable(mem, gslt_image_t,
	&st_gslt_image, "jpeg gslt_image");
    if (image == NULL) {
	error = gs_throw(-1, "unable to allocate jpeg gslt_image");
	gs_jpeg_destroy(&state);
	return NULL;
    }

    image->width = jddp.dinfo.output_width;
    image->height = jddp.dinfo.output_height;
    image->components = jddp.dinfo.output_components;
    image->bits = 8;
    image->stride = image->width * image->components;

    if (image->components == 1)
        image->colorspace = GSLT_GRAY;
    if (image->components == 3)
        image->colorspace = GSLT_RGB;
    if (image->components == 4)
        image->colorspace = GSLT_CMYK;

    if (jddp.dinfo.density_unit == 1)
    {
        image->xres = jddp.dinfo.X_density;
        image->yres = jddp.dinfo.Y_density;
    }
    else if (jddp.dinfo.density_unit == 2)
    {
        image->xres = jddp.dinfo.X_density * 2.54;
        image->yres = jddp.dinfo.Y_density * 2.54;
    }
    else
    {
        image->xres = 96;
        image->yres = 96;
    }

    wlen = image->stride * image->height;
    wbuf = gs_alloc_bytes(mem, wlen, "decodejpeg");
    if (!wbuf) {
        error = gs_throw1(-1, "out of memory allocating samples: %d", wlen);
	gs_free_object(mem, image, "free jpeg gslt_image");
	gs_jpeg_destroy(&state);
	return NULL;
    }
    image->samples = wbuf;

    wp.ptr = wbuf - 1;
    wp.limit = wbuf + wlen - 1;

    code = s_DCTD_template.process((stream_state*)&state, &rp, &wp, true);
    if (code != EOFC) {
        error = gs_throw1(-1, "error in jpeg (code = %d)", code);
#ifndef DEBUG /* return whatever we got when debugging */
	gs_free_object(mem, image, "free jpeg gslt_image");
	gs_jpeg_destroy(&state);
	return NULL;
#endif
    }

    gs_jpeg_destroy(&state);

    return image;
}
