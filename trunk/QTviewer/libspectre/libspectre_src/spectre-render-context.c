/* This file is part of Libspectre.
 * 
 * Copyright (C) 2007 Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Libspectre is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2; or (at your option)
 * any later version.
 *
 * Libspectre is distributed in the hope that it will be useful;
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not; write to the Free Software
 * Foundation; Inc.; 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>

#include "spectre-render-context.h"
#include "spectre-page.h"
#include "spectre-private.h"
#include "spectre-utils.h"

SpectreRenderContext *
spectre_render_context_new (void)
{
	SpectreRenderContext *rc;

	rc = malloc (sizeof (SpectreRenderContext));
	if (!rc)
		return NULL;

	rc->x_scale = 1.0;
	rc->y_scale = 1.0;
	rc->orientation = 0;
	rc->x_dpi = 72.0;
	rc->y_dpi = 72.0;
	rc->width = -1;
	rc->height = -1;
	rc->text_alpha_bits = 4;
	rc->graphic_alpha_bits = 2;
	rc->use_platform_fonts = TRUE;
	
	return rc;
}

void
spectre_render_context_free (SpectreRenderContext *rc)
{
	if (!rc)
		return;

	free (rc);
}

void
spectre_render_context_set_scale (SpectreRenderContext *rc,
				  double                x_scale,
				  double                y_scale)
{
	_spectre_return_if_fail (rc != NULL);

	rc->x_scale = x_scale;
	rc->y_scale = y_scale;
}

void 
spectre_render_context_get_scale (SpectreRenderContext *rc,
				  double               *x_scale,
				  double               *y_scale)
{
	_spectre_return_if_fail (rc != NULL);

	if (x_scale)
		*x_scale = rc->x_scale;
	if (y_scale)
		*y_scale = rc->y_scale;
}

void
spectre_render_context_set_rotation (SpectreRenderContext *rc,
				     unsigned int          rotation)
{
	_spectre_return_if_fail (rc != NULL);

	rotation %= 360;

	if (rotation >= 0 && rotation < 90)
		rc->orientation = SPECTRE_ORIENTATION_PORTRAIT;
	else if (rotation >= 90 && rotation < 180)
		rc->orientation = SPECTRE_ORIENTATION_LANDSCAPE;
	else if (rotation >= 180 && rotation < 270)
		rc->orientation = SPECTRE_ORIENTATION_REVERSE_PORTRAIT;
	else if (rotation >= 270 && rotation < 360)
		rc->orientation = SPECTRE_ORIENTATION_REVERSE_LANDSCAPE;
}
	
unsigned int
spectre_render_context_get_rotation (SpectreRenderContext *rc)
{
	_spectre_return_val_if_fail (rc != NULL, 0);

	switch (rc->orientation) {
	default:
	case SPECTRE_ORIENTATION_PORTRAIT:
		return 0;
	case SPECTRE_ORIENTATION_LANDSCAPE:
		return 90;
	case SPECTRE_ORIENTATION_REVERSE_PORTRAIT:
		return 180;
	case SPECTRE_ORIENTATION_REVERSE_LANDSCAPE:
		return 270;
	}
	
	return 0;
}

void
spectre_render_context_set_resolution (SpectreRenderContext *rc,
				       double                x_dpi,
				       double                y_dpi)
{
	_spectre_return_if_fail (rc != NULL);

	rc->x_dpi = x_dpi;
	rc->y_dpi = y_dpi;
}

void
spectre_render_context_get_resolution (SpectreRenderContext *rc,
				       double               *x_dpi,
				       double               *y_dpi)
{
	_spectre_return_if_fail (rc != NULL);

	if (x_dpi)
		*x_dpi = rc->x_dpi;
	if (y_dpi)
		*y_dpi = rc->y_dpi;
}

void
spectre_render_context_set_page_size (SpectreRenderContext *rc,
				      int                   width,
				      int                   height)
{
	_spectre_return_if_fail (rc != NULL);
		
	rc->width = width;
	rc->height = height;
}

void
spectre_render_context_get_page_size (SpectreRenderContext *rc,
				      int                  *width,
				      int                  *height)
{
	_spectre_return_if_fail (rc != NULL);

	if (width)
		*width = rc->width;
	if (height)
		*height = rc->height;
}

void
spectre_render_context_set_use_platform_fonts (SpectreRenderContext *rc,
					       int                   use_platform_fonts)
{
	_spectre_return_if_fail (rc != NULL);

	rc->use_platform_fonts = use_platform_fonts;
}

int
spectre_render_context_get_use_platform_fonts (SpectreRenderContext *rc)
{
	_spectre_return_val_if_fail (rc != NULL, FALSE);

	return rc->use_platform_fonts;
}

void
spectre_render_context_set_antialias_bits (SpectreRenderContext *rc,
					   int                   graphics_bits,
					   int                   text_bits)
{
	_spectre_return_if_fail (rc != NULL);

	rc->graphic_alpha_bits = graphics_bits;
	rc->text_alpha_bits = text_bits;
}

void
spectre_render_context_get_antialias_bits (SpectreRenderContext *rc,
					   int                  *graphics_bits,
					   int                  *text_bits)
{
	_spectre_return_if_fail (rc != NULL);

	if (graphics_bits)
		*graphics_bits = rc->graphic_alpha_bits;
	if (text_bits)
		*text_bits = rc->text_alpha_bits;
}
