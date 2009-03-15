/* Copyright (C) 2006-2008 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* XPS interpreter - analyze page checking for transparency.
 * This is a stripped down parser that looks for alpha values < 1.0 in
 * any part of the page.
 */

#include "ghostxps.h"

int
xps_remote_resource_dictionary_has_transparency(xps_context_t *ctx, char *source_att)
{
    //dputs("page has transparency: uses a remote resource; not parsed; being conservative\n");
    return 1;
}

int
xps_resource_dictionary_has_transparency(xps_context_t *ctx, xps_item_t *root)
{
    char *source;
    xps_item_t *node;

    source = xps_att(root, "Source");
    if (source)
	return xps_remote_resource_dictionary_has_transparency(ctx, source);

    for (node = xps_down(root); node; node = xps_next(node))
    {
	// TODO: ... all kinds of stuff can be here, brushes, elements, whatnot
    }

    return 0;
}

int
xps_brush_has_transparency(xps_context_t *ctx, xps_item_t *root)
{
    char *opacity_att;
    char *color_att;
    xps_item_t *node;

    gs_color_space *colorspace;
    float samples[32];

    if (!strcmp(xps_tag(root), "SolidColorBrush"))
    {
	opacity_att = xps_att(root, "Opacity");
	if (opacity_att)
	{
	    if (atof(opacity_att) < 1.0)
	    {
		//dputs("page has transparency: SolidColorBrush Opacity\n");
		return 1;
	    }
	}

	color_att = xps_att(root, "Color");
	if (color_att)
	{
	    xps_parse_color(ctx, color_att, &colorspace, samples);
	    if (samples[0] < 1.0)
	    {
		//dputs("page has transparency: SolidColorBrush Color has alpha\n");
		return 1;
	    }
	}
    }

    if (!strcmp(xps_tag(root), "VisualBrush"))
    {
	char *opacity_att = xps_att(root, "Opacity");
	if (opacity_att)
	{
	    if (atof(opacity_att) < 1.0)
	    {
		//dputs("page has transparency: VisualBrush Opacity\n");
		return 1;
	    }
	}

	for (node = xps_down(root); node; node = xps_next(node))
	{
	    if (!strcmp(xps_tag(node), "VisualBrush.Visual"))
	    {
		if (xps_element_has_transparency(ctx, xps_down(node)))
		    return 1;
	    }
	}
    }

    // TODO: ImageBrush
    // TODO: LinearGradientBrush
    // TODO: RadialGradientBrush

    return 0;
}

int
xps_path_has_transparency(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Path.OpacityMask"))
	{
	    //dputs("page has transparency: Path.OpacityMask\n");
	    return 1;
	}

	if (!strcmp(xps_tag(node), "Path.Stroke"))
	{
	    if (xps_brush_has_transparency(ctx, xps_down(node)))
		return 1;
	}

	if (!strcmp(xps_tag(node), "Path.Fill"))
	{
	    if (xps_brush_has_transparency(ctx, xps_down(node)))
		return 1;
	}
    }

    return 0;
}

int
xps_glyphs_has_transparency(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Glyphs.OpacityMask"))
	{
	    //dputs("page has transparency: Glyphs.OpacityMask\n");
	    return 1;
	}

	if (!strcmp(xps_tag(node), "Glyphs.Fill"))
	{
	    if (xps_brush_has_transparency(ctx, xps_down(node)))
		return 1;
	}
    }

    return 0;
}

int
xps_canvas_has_transparency(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Canvas.Resources"))
	{
	    if (xps_resource_dictionary_has_transparency(ctx, xps_down(node)))
		return 1;
	}

	if (!strcmp(xps_tag(node), "Canvas.OpacityMask"))
	{
	    //dputs("page has transparency: Canvas.OpacityMask\n");
	    return 1;
	}

	if (xps_element_has_transparency(ctx, node))
	    return 1;
    }

    return 0;
}

int
xps_element_has_transparency(xps_context_t *ctx, xps_item_t *node)
{
    char *opacity_att;
    char *stroke_att;
    char *fill_att;

    gs_color_space *colorspace;
    float samples[32];

    stroke_att = xps_att(node, "Stroke");
    if (stroke_att)
    {
	xps_parse_color(ctx, stroke_att, &colorspace, samples);
	if (samples[0] < 1.0)
	{
	    //dprintf1("page has transparency: Stroke alpha=%g\n", samples[0]);
	    return 1;
	}
    }

    fill_att = xps_att(node, "Fill");
    if (fill_att)
    {
	xps_parse_color(ctx, fill_att, &colorspace, samples);
	if (samples[0] < 1.0)
	{
	    //dprintf1("page has transparency: Fill alpha=%g\n", samples[0]);
	    return 1;
	}
    }

    opacity_att = xps_att(node, "Opacity");
    if (opacity_att)
    {
	if (atof(opacity_att) < 1.0)
	{
	    //dprintf1("page has transparency: Opacity=%g\n", atof(opacity_att));
	    return 1;
	}
    }

    if (xps_att(node, "OpacityMask"))
    {
	//dputs("page has transparency: OpacityMask\n");
	return 1;
    }

    if (!strcmp(xps_tag(node), "Path"))
	if (xps_path_has_transparency(ctx, node))
	    return 1;
    if (!strcmp(xps_tag(node), "Glyphs"))
	if (xps_glyphs_has_transparency(ctx, node))
	    return 1;
    if (!strcmp(xps_tag(node), "Canvas"))
	if (xps_canvas_has_transparency(ctx, node))
	    return 1;

    return 0;
}
