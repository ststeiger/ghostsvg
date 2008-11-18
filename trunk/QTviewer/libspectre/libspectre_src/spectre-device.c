/* This file is part of Libspectre.
 *
 * Copyright (C) 2007 Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Libspectre is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * Libspectre is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spectre-device.h"
#include "spectre-gs.h"
#include "spectre-utils.h"
#include "spectre-private.h"

/* ghostscript stuff */
#include <ghostscript/gdevdsp.h>

struct SpectreDevice {
	struct document *doc;
	
	int width, height;
	int row_length; /*! Size of a horizontal row (y-line) in the image buffer */
	unsigned char *gs_image; /*! Image buffer we received from Ghostscript library */
	unsigned char *user_image;
	int page_called;
};

static int
spectre_open (void *handle, void *device)
{
	return 0;
}

static int
spectre_preclose (void *handle, void *device)
{
	return 0;
}

static int
spectre_close (void *handle, void *device)
{
	return 0;
}

static int
spectre_presize (void *handle, void *device, int width, int height,
		 int raster, unsigned int format)
{
	SpectreDevice *sd;

	if (!handle)
		return 0;

	sd = (SpectreDevice *)handle;
	sd->width = width;
	sd->height = height;
	sd->row_length = raster;
	sd->gs_image = NULL;
	sd->user_image = malloc (sd->row_length * sd->height);
	
	return 0;
}

static int
spectre_size (void *handle, void *device, int width, int height, int raster,
	      unsigned int format, unsigned char *pimage)
{
	SpectreDevice *sd;

	if (!handle)
		return 0;

	sd = (SpectreDevice *)handle;
	sd->gs_image = pimage;

	return 0;
}

static int
spectre_sync (void *handle, void *device)
{
	return 0;
}

static int
spectre_page (void *handle, void * device, int copies, int flush)
{
	SpectreDevice *sd;

	if (!handle)
		return 0;
	
	sd = (SpectreDevice *)handle;
	sd->page_called = TRUE;
	memcpy (sd->user_image, sd->gs_image, sd->row_length * sd->height);
	
	return 0;
}

static int
spectre_update (void *handle, void *device, int x, int y, int w, int h)
{
	SpectreDevice *sd;
	int i;

	if (!handle)
		return 0;

	sd = (SpectreDevice *)handle;
	if (!sd->gs_image || sd->page_called || !sd->user_image)
		return 0;

	for (i = y; i < y + h; ++i) {
		memcpy (sd->user_image + sd->row_length * i + x * 4,
			sd->gs_image + sd->row_length * i + x * 4, w * 4);
	}
	
	return 0;
}

static const display_callback spectre_device = {
	sizeof (display_callback),
	DISPLAY_VERSION_MAJOR,
	DISPLAY_VERSION_MINOR,
	spectre_open,
	spectre_preclose,
	spectre_close,
	spectre_presize,
	spectre_size,
	spectre_sync,
	spectre_page,
	spectre_update
};

SpectreDevice *
spectre_device_new (struct document *doc)
{
	SpectreDevice *device;

	device = calloc (1, sizeof (SpectreDevice));
	if (!device)
		return NULL;

	device->doc = psdocreference (doc);
	
	return device;
}

SpectreStatus
spectre_device_render (SpectreDevice        *device,
		       unsigned int          page,
		       SpectreRenderContext *rc,
		       int                   x,
		       int                   y,
		       int                   width,
		       int                   height,
		       unsigned char       **page_data,
		       int                  *row_length)
{
	SpectreGS *gs;
	char     **args;
	int        n_args = 12;
	int        arg = 0;
	int        success;
	char      *text_alpha, *graph_alpha;
	char      *size = NULL;
	char      *resolution, *set;
	char      *dsp_format, *dsp_handle;
	char      *width_points = NULL;
	char      *height_points = NULL;

	gs = spectre_gs_new ();
	if (!gs)
		return SPECTRE_STATUS_NO_MEMORY;

	if (!spectre_gs_create_instance (gs, device)) {
		spectre_gs_cleanup (gs, CLEANUP_DELETE_INSTANCE);
		spectre_gs_free (gs);
		
		return SPECTRE_STATUS_RENDER_ERROR;
	}

	if (!spectre_gs_set_display_callback (gs, (display_callback *)&spectre_device)) {
		spectre_gs_cleanup (gs, CLEANUP_DELETE_INSTANCE);
		spectre_gs_free (gs);
		
		return SPECTRE_STATUS_RENDER_ERROR;
	}

	width = (int) ((width * rc->x_scale) + 0.5);
	height = (int) ((height * rc->y_scale) + 0.5);

	if (rc->use_platform_fonts == FALSE)
		n_args++;
	if (rc->width != -1 && rc->height != -1)
		n_args += 3;
	
	args = calloc (sizeof (char *), n_args);
	args[arg++] = "libspectre"; /* This value doesn't really matter */
	args[arg++] = "-dMaxBitmap=10000000";
	args[arg++] = "-dDELAYSAFER";
	args[arg++] = "-dNOPAUSE";
	args[arg++] = "-dNOPAGEPROMPT";
	args[arg++] = "-sDEVICE=display";
	args[arg++] = text_alpha = _spectre_strdup_printf ("-dTextAlphaBits=%d",
							   rc->text_alpha_bits);
	args[arg++] = graph_alpha = _spectre_strdup_printf ("-dGraphicsAlphaBits=%d",
							    rc->graphic_alpha_bits);
	args[arg++] = size =_spectre_strdup_printf ("-g%dx%d", width, height);
	args[arg++] = resolution = _spectre_strdup_printf ("-r%fx%f",
							   rc->x_scale * rc->x_dpi,
							   rc->y_scale * rc->y_dpi);
	args[arg++] = dsp_format = _spectre_strdup_printf ("-dDisplayFormat=%d",
							   DISPLAY_COLORS_RGB |
							   DISPLAY_DEPTH_8 |
							   DISPLAY_ROW_ALIGN_32 |
#ifdef WORDS_BIGENDIAN
							   DISPLAY_UNUSED_FIRST |
							   DISPLAY_BIGENDIAN |
#else
							   DISPLAY_UNUSED_LAST |
							   DISPLAY_LITTLEENDIAN |
#endif
							   DISPLAY_TOPFIRST);
	args[arg++] = dsp_handle = _spectre_strdup_printf ("-sDisplayHandle=16#%llx",
							   (unsigned long long int)device);
	if (rc->use_platform_fonts == FALSE)
		args[arg++] = "-dNOPLATFONTS";

	if (rc->width != -1 && rc->height != -1) {
		args[arg++] = width_points = _spectre_strdup_printf ("-dDEVICEWIDTHPOINTS=%d",
								     rc->width);
		args[arg++] = height_points = _spectre_strdup_printf ("-dDEVICEHEIGHTPOINTS=%d",
								      rc->height);
		args[arg++] = "-dFIXEDMEDIA";
	}

	success = spectre_gs_run (gs, n_args, args);
	free (text_alpha);
	free (graph_alpha);
	free (size);
	free (width_points);
	free (height_points);
	free (resolution);
	free (dsp_format);
	free (dsp_handle);
	free (args);
	if (!success) {
		spectre_gs_free (gs);
		return SPECTRE_STATUS_RENDER_ERROR;
	}

	set = _spectre_strdup_printf ("<< /Orientation %d >> setpagedevice .locksafe",
				      rc->orientation);
	if (!spectre_gs_send_string (gs, set)) {
		free (set);
		spectre_gs_free (gs);
		return SPECTRE_STATUS_RENDER_ERROR;
	}
	free (set);

	if (!spectre_gs_send_page (gs, device->doc, page, x, y)) {
		spectre_gs_free (gs);
		return SPECTRE_STATUS_RENDER_ERROR;
	}

	*page_data = device->user_image;
	*row_length = device->row_length;

	spectre_gs_free (gs);

	return SPECTRE_STATUS_SUCCESS;
}

void
spectre_device_free (SpectreDevice *device)
{
	if (!device)
		return;

	if (device->doc) {
		psdocdestroy (device->doc);
		device->doc = NULL;
	}

	free (device);
}
