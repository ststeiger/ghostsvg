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

/* XPS interpreter - tiles for pattern rendering */

#include "ghostxps.h"

/*
 * Parse a tiling brush (visual and image brushes at this time) common
 * properties. Use the callback to draw the individual tiles.
 */

enum { TILE_NONE, TILE_TILE, TILE_FLIP_X, TILE_FLIP_Y, TILE_FLIP_X_Y };

struct tile_closure_s
{
    xps_context_t *ctx;
    xps_resource_t *dict;
    xps_item_t *tag;
    gs_rect viewbox;
    int tile_mode;
    void *user;
    int (*func)(xps_context_t*, xps_resource_t*, xps_item_t*, void*);
};

static int
xps_paint_tiling_brush_clipped(struct tile_closure_s *c)
{
    xps_context_t *ctx = c->ctx;
    gs_rect saved_bounds;
    int code;

    gs_moveto(ctx->pgs, c->viewbox.p.x, c->viewbox.p.y);
    gs_lineto(ctx->pgs, c->viewbox.p.x, c->viewbox.q.y);
    gs_lineto(ctx->pgs, c->viewbox.q.x, c->viewbox.q.y);
    gs_lineto(ctx->pgs, c->viewbox.q.x, c->viewbox.p.y);
    gs_closepath(ctx->pgs);
    gs_clip(ctx->pgs);
    gs_newpath(ctx->pgs);

    /* tile paint functions use a different device
     * with a different coord space, so we have to
     * tweak the bounds.
     */

    saved_bounds = ctx->bounds;

    // dprintf4("tiled bounds [%g %g %g %g]\n", saved_bounds.p.x, saved_bounds.p.y, saved_bounds.q.x, saved_bounds.q.y);

    ctx->bounds = c->viewbox; // transform?

    code = c->func(c->ctx, c->dict, c->tag, c->user);

    ctx->bounds = saved_bounds;

    return code;
}

static int
xps_paint_tiling_brush(const gs_client_color *pcc, gs_state *pgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    struct tile_closure_s *c = ppat->client_data;
    xps_context_t *ctx = c->ctx;
    gs_state *saved_pgs;

    saved_pgs = ctx->pgs;
    ctx->pgs = pgs;

    gs_gsave(ctx->pgs);
    xps_paint_tiling_brush_clipped(c);
    gs_grestore(ctx->pgs);

    if (c->tile_mode == TILE_FLIP_X || c->tile_mode == TILE_FLIP_X_Y)
    {
	gs_gsave(ctx->pgs);
	gs_translate(ctx->pgs, c->viewbox.q.x * 2, 0.0);
	gs_scale(ctx->pgs, -1.0, 1.0);
	xps_paint_tiling_brush_clipped(c);
	gs_grestore(ctx->pgs);
    }

    if (c->tile_mode == TILE_FLIP_Y || c->tile_mode == TILE_FLIP_X_Y)
    {
	gs_gsave(ctx->pgs);
	gs_translate(ctx->pgs, 0.0, c->viewbox.q.y * 2);
	gs_scale(ctx->pgs, 1.0, -1.0);
	xps_paint_tiling_brush_clipped(c);
	gs_grestore(ctx->pgs);
    }

    if (c->tile_mode == TILE_FLIP_X_Y)
    {
	gs_gsave(ctx->pgs);
	gs_translate(ctx->pgs, c->viewbox.q.x * 2, c->viewbox.q.y * 2);
	gs_scale(ctx->pgs, -1.0, -1.0);
	xps_paint_tiling_brush_clipped(c);
	gs_grestore(ctx->pgs);
    }

    ctx->pgs = saved_pgs;

    return 0;
}

int
xps_parse_tiling_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root,
	int (*func)(xps_context_t*, xps_resource_t*, xps_item_t*, void*), void *user)
{
    xps_item_t *node;

    char *opacity_att;
    char *transform_att;
    char *viewbox_att;
    char *viewport_att;
    char *tile_mode_att;
    char *viewbox_units_att;
    char *viewport_units_att;

    xps_item_t *transform_tag = NULL;

    gs_matrix transform;
    gs_rect viewbox;
    gs_rect viewport;
    float scalex, scaley;
    int tile_mode;

    opacity_att = xps_att(root, "Opacity");
    transform_att = xps_att(root, "Transform");
    viewbox_att = xps_att(root, "Viewbox");
    viewport_att = xps_att(root, "Viewport");
    tile_mode_att = xps_att(root, "TileMode");
    viewbox_units_att = xps_att(root, "ViewboxUnits");
    viewport_units_att = xps_att(root, "ViewportUnits");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "ImageBrush.Transform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "VisualBrush.Transform"))
	    transform_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);

    gs_make_identity(&transform);
    if (transform_att)
	xps_parse_render_transform(ctx, transform_att, &transform);
    if (transform_tag)
	xps_parse_matrix_transform(ctx, transform_tag, &transform);

    viewbox.p.x = 0.0; viewbox.p.y = 0.0;
    viewbox.q.x = 1.0; viewbox.q.y = 1.0;
    if (viewbox_att)
	xps_parse_rectangle(ctx, viewbox_att, &viewbox);

    viewport.p.x = 0.0; viewport.p.y = 0.0;
    viewport.q.x = 1.0; viewport.q.y = 1.0;
    if (viewport_att)
	xps_parse_rectangle(ctx, viewport_att, &viewport);

    scalex = (viewport.q.x - viewport.p.x) / (viewbox.q.x - viewbox.p.x);
    scaley = (viewport.q.y - viewport.p.y) / (viewbox.q.y - viewbox.p.y);

    tile_mode = TILE_NONE;
    if (tile_mode_att)
    {
	if (!strcmp(tile_mode_att, "None"))
	    tile_mode = TILE_NONE;
	if (!strcmp(tile_mode_att, "Tile"))
	    tile_mode = TILE_TILE;
	if (!strcmp(tile_mode_att, "FlipX"))
	    tile_mode = TILE_FLIP_X;
	if (!strcmp(tile_mode_att, "FlipY"))
	    tile_mode = TILE_FLIP_Y;
	if (!strcmp(tile_mode_att, "FlipXY"))
	    tile_mode = TILE_FLIP_X_Y;
    }

    gs_gsave(ctx->pgs);

    xps_begin_opacity(ctx, dict, opacity_att, NULL);

    if (tile_mode != TILE_NONE)
    {
	struct tile_closure_s closure;

	gs_client_pattern gspat;
	gs_client_color gscolor;
	gs_color_space *cs;

	closure.ctx = ctx;
	closure.dict = dict;
	closure.tag = root;
	closure.tile_mode = tile_mode;
	closure.user = user;
	closure.func = func;

	closure.viewbox.p.x = viewbox.p.x;
	closure.viewbox.p.y = viewbox.p.y;
	closure.viewbox.q.x = viewbox.q.x;
	closure.viewbox.q.y = viewbox.q.y;

	gs_pattern1_init(&gspat);
	uid_set_UniqueID(&gspat.uid, gs_next_ids(ctx->memory, 1));
	gspat.PaintType = 1;
	gspat.TilingType = 1;
	gspat.PaintProc = xps_paint_tiling_brush;
	gspat.client_data = &closure;

	gspat.XStep = viewbox.q.x - viewbox.p.x;
	gspat.YStep = viewbox.q.y - viewbox.p.y;
	gspat.BBox.p.x = viewbox.p.x;
	gspat.BBox.p.y = viewbox.p.y;
	gspat.BBox.q.x = viewbox.q.x;
	gspat.BBox.q.y = viewbox.q.y;

	if (tile_mode == TILE_FLIP_X || tile_mode == TILE_FLIP_X_Y)
	{
	    gspat.BBox.q.x += gspat.XStep;
	    gspat.XStep *= 2;
	}

	if (tile_mode == TILE_FLIP_Y || tile_mode == TILE_FLIP_X_Y)
	{
	    gspat.BBox.q.y += gspat.YStep;
	    gspat.YStep *= 2;
	}

	gs_translate(ctx->pgs, viewport.p.x, viewport.p.y);
	gs_scale(ctx->pgs, scalex, scaley);
	gs_translate(ctx->pgs, -viewbox.p.x, -viewbox.p.y);

	cs = ctx->srgb;
	gs_setcolorspace(ctx->pgs, cs);
	gs_makepattern(&gscolor, &gspat, &transform, ctx->pgs, NULL);
	gs_setpattern(ctx->pgs, &gscolor);

	xps_fill(ctx);
    }
    else
    {
	gs_rect saved_bounds;

	xps_clip(ctx, &saved_bounds);

	gs_concat(ctx->pgs, &transform);

	gs_translate(ctx->pgs, viewport.p.x, viewport.p.y);
	gs_scale(ctx->pgs, scalex, scaley);
	gs_translate(ctx->pgs, -viewbox.p.x, viewbox.p.y);

	gs_moveto(ctx->pgs, viewbox.p.x, viewbox.p.y);
	gs_lineto(ctx->pgs, viewbox.p.x, viewbox.q.y);
	gs_lineto(ctx->pgs, viewbox.q.x, viewbox.q.y);
	gs_lineto(ctx->pgs, viewbox.q.x, viewbox.p.y);
	gs_closepath(ctx->pgs);
	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);

	func(ctx, dict, root, user);

	xps_unclip(ctx, &saved_bounds);
    }

    xps_end_opacity(ctx, dict, opacity_att, NULL);

    gs_grestore(ctx->pgs);

    return 0;
}

