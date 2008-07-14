#include "ghostxps.h"

enum { TILE_NONE, TILE_TILE, TILE_FLIP_X, TILE_FLIP_Y, TILE_FLIP_X_Y };

struct userdata
{
    xps_context_t *ctx;
    xps_resource_t *dict;
    xps_item_t *visual_tag;
};

static int
xps_paint_visual_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root, void *visual_tag)
{
    return xps_parse_element(ctx, dict, (xps_item_t *)visual_tag);
}

int
xps_parse_visual_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;

    char *visual_att;
    xps_item_t *visual_tag = NULL;

    visual_att = xps_att(root, "Visual");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "VisualBrush.Visual"))
	    visual_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &visual_att, &visual_tag);

    if (visual_tag)
    {
	xps_parse_tiling_brush(ctx, dict, root, xps_paint_visual_brush, visual_tag);
    }

    return 0;
}

