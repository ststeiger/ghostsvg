/* Copyright (C) 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
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
/* Rendering for Gouraud triangle shadings */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxdevcli.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxshade.h"
#include "gxshade4.h"
#include "vdtrace.h"

#define VD_TRACE_TRIANGLE_PATCH 0

/* ---------------- Triangle mesh filling ---------------- */

private void 
patch_set_color_values(const mesh_fill_state_t * pfs, float *cc, const patch_color_t *c)
{
    memcpy(cc, c->cc.paint.values, sizeof(c->cc.paint.values[0]) * pfs->num_components);
}

/* Initialize the fill state for triangle shading. */
void
mesh_init_fill_state(mesh_fill_state_t * pfs, const gs_shading_mesh_t * psh,
		     const gs_rect * rect, gx_device * dev,
		     gs_imager_state * pis)
{
    shade_init_fill_state((shading_fill_state_t *) pfs,
			  (const gs_shading_t *)psh, dev, pis);
    pfs->pshm = psh;
    shade_bbox_transform2fixed(rect, pis, &pfs->rect);
}

/* Initialize the recursion state for filling one triangle. */
private void
shading_init_fill_triangle(mesh_fill_state_t * pfs,
  const shading_vertex_t *va, const shading_vertex_t *vb, const shading_vertex_t *vc,
  bool check_clipping)
{
    pfs->depth = 1;
    pfs->frames[0].va.p = va->p;
    patch_set_color_values(pfs, pfs->frames[0].va.cc, &va->c);
    pfs->frames[0].vb.p = vb->p;
    patch_set_color_values(pfs, pfs->frames[0].vb.cc, &vb->c);
    pfs->frames[0].vc.p = vc->p;
    patch_set_color_values(pfs, pfs->frames[0].vc.cc, &vc->c);
    pfs->frames[0].check_clipping = check_clipping;
}
void
mesh_init_fill_triangle(mesh_fill_state_t * pfs,
  const mesh_vertex_t *va, const mesh_vertex_t *vb, const mesh_vertex_t *vc,
  bool check_clipping)
{
    pfs->depth = 1;
    pfs->frames[0].va = *va;
    pfs->frames[0].vb = *vb;
    pfs->frames[0].vc = *vc;
    pfs->frames[0].check_clipping = check_clipping;
}

#define SET_MIN_MAX_3(vmin, vmax, a, b, c)\
  if ( a < b ) vmin = a, vmax = b; else vmin = b, vmax = a;\
  if ( c < vmin ) vmin = c; else if ( c > vmax ) vmax = c

#define MIDPOINT_FAST(a,b) arith_rshift_1((a) + (b) + 1)

private mesh_frame_t *
mesh_subdivide_triangle(mesh_fill_state_t *pfs, mesh_frame_t *fp)
{
    int i;
    float dabx, daby, dbcx, dbcy, dacx, dacy;
    float r2ab, r2bc, r2ac;
    float r2min, r2max;
    float tri_area_2;

    dabx = (float)(fp->vb.p.x - fp->va.p.x);
    daby = (float)(fp->vb.p.y - fp->va.p.y);
    dbcx = (float)(fp->vc.p.x - fp->vb.p.x);
    dbcy = (float)(fp->vc.p.y - fp->vb.p.y);
    dacx = (dabx + dbcx);
    dacy = (daby + dbcy);
    r2ab = dabx * dabx + daby * daby;
    r2bc = dbcx * dbcx + dbcy * dbcy;
    r2ac = dacx * dacx + dacy * dacy;

    SET_MIN_MAX_3(r2min, r2max, r2ab, r2bc, r2ac);
    tri_area_2 = (float)(fp->va.p.y * (fp->vc.p.x - fp->vb.p.x) +
	fp->vb.p.y * (fp->va.p.x - fp->vc.p.x) +
	fp->vc.p.y * (fp->vb.p.x - fp->va.p.x));

    if (fabs(tri_area_2) < 0.5 * r2max) {
	/* skinny triangle, subdivide longest edge */
	mesh_vertex_t tmp;
	/* first, sort by length, so that AB is longest edge */
	if (r2bc > r2ac) {
	    if (r2bc > r2ab) {
		/* BC is longest edge, rotate */
		tmp = fp->va;
		fp->va = fp->vb;
		fp->vb = fp->vc;
		fp->vc = tmp;
	    }
	} else {
	    if (r2ac > r2ab) {
		/* AC is longest edge, rotate */
		tmp = fp->va;
		fp->va = fp->vc;
		fp->vc = fp->vb;
		fp->vb = tmp;
	    }
	}

#define VAB fp[1].va
	VAB.p.x = MIDPOINT_FAST(fp->va.p.x, fp->vb.p.x);
	VAB.p.y = MIDPOINT_FAST(fp->va.p.y, fp->vb.p.y);
	for (i = 0; i < pfs->num_components; ++i) {
	    float ta = fp->va.cc[i], tb = fp->vb.cc[i];

	    VAB.cc[i] = (ta + tb) * 0.5;
	}
	/* Fill in the rest of the triangles. */
	fp[1].vb = fp->vb;
	fp[1].vc = fp->vc;
	fp->vb = VAB;
	fp[1].check_clipping = fp->check_clipping;
	return fp + 1;
#undef VAB
    } else {
	/* Reasonably shaped, subdivide into 4 triangles at midpoints */
    /*
     * Subdivide the triangle and recur.  The only subdivision method
     * that doesn't seem to create anomalous shapes divides the
     * triangle in 4, using the midpoints of each side.
     *
     * If the original vertices are A, B, C, we fill the sub-triangles
     * in the following order:
     *	(A, AB, AC) - fp[3]
     *	(AB, AC, BC) - fp[2]
     *	(AC, BC, C) - fp[1]
     *	(AB, B, BC) - fp[0]
     */
#define VAB fp[3].vb
#define VAC fp[2].vb
#define VBC fp[1].vb
    VAB.p.x = MIDPOINT_FAST(fp->va.p.x, fp->vb.p.x);
    VAB.p.y = MIDPOINT_FAST(fp->va.p.y, fp->vb.p.y);
    VAC.p.x = MIDPOINT_FAST(fp->va.p.x, fp->vc.p.x);
    VAC.p.y = MIDPOINT_FAST(fp->va.p.y, fp->vc.p.y);
    VBC.p.x = MIDPOINT_FAST(fp->vb.p.x, fp->vc.p.x);
    VBC.p.y = MIDPOINT_FAST(fp->vb.p.y, fp->vc.p.y);
    for (i = 0; i < pfs->num_components; ++i) {
	float ta = fp->va.cc[i], tb = fp->vb.cc[i], tc = fp->vc.cc[i];

	VAB.cc[i] = (ta + tb) * 0.5;
	VAC.cc[i] = (ta + tc) * 0.5;
	VBC.cc[i] = (tb + tc) * 0.5;
    }
    /* Fill in the rest of the triangles. */
    fp[3].va = fp->va;
    fp[3].vc = VAC;
    fp[2].va = VAB;
    fp[2].vc = VBC;
    fp[1].va = VAC;
    fp[1].vc = fp->vc;
    fp->va = VAB;
    fp->vc = VBC;
#undef VAB
#undef VAC
#undef VBC
    fp[3].check_clipping = fp[2].check_clipping =
	fp[1].check_clipping = fp->check_clipping;
    return fp + 3;
    }
}
#undef MIDPOINT_FAST

int
mesh_fill_triangle(mesh_fill_state_t *pfs)
{
    gs_imager_state *pis = pfs->pis;
    mesh_frame_t *fp = &pfs->frames[pfs->depth - 1];
    int ci;

    for (;;) {
	bool check = fp->check_clipping;

	/*
	 * Fill the triangle with vertices at fp->va.p, fp->vb.p, and
	 * fp->vc.p with color fp->va.cc.  If check is true, check for
	 * whether the triangle is entirely inside the rectangle, entirely
	 * outside, or partly inside; if check is false, assume the triangle
	 * is entirely inside.
	 */
	if (check) {
	    fixed xmin, ymin, xmax, ymax;

	    SET_MIN_MAX_3(xmin, xmax, fp->va.p.x, fp->vb.p.x, fp->vc.p.x);
	    SET_MIN_MAX_3(ymin, ymax, fp->va.p.y, fp->vb.p.y, fp->vc.p.y);
	    if (xmin >= pfs->rect.p.x && xmax <= pfs->rect.q.x &&
		ymin >= pfs->rect.p.y && ymax <= pfs->rect.q.y
		) {
		/* The triangle is entirely inside the rectangle. */
		check = false;
	    } else if (xmin >= pfs->rect.q.x || xmax <= pfs->rect.p.x ||
		       ymin >= pfs->rect.q.y || ymax <= pfs->rect.p.y
		       ) {
		/* The triangle is entirely outside the rectangle. */
		goto next;
	    }
	}
	if (fp < &pfs->frames[countof(pfs->frames) - 3]) {
	/* Check whether the colors fall within the smoothness criterion. */
	    for (ci = 0; ci < pfs->num_components; ++ci) {
		float
		    c0 = fp->va.cc[ci], c1 = fp->vb.cc[ci], c2 = fp->vc.cc[ci];
		float cmin, cmax;

		SET_MIN_MAX_3(cmin, cmax, c0, c1, c2);
		if (cmax - cmin > pfs->cc_max_error[ci])
		    goto nofill;
	    }
	}
    fill:
	/* Fill the triangle with the color. */
	{
	    gx_device_color dev_color;
	    const gs_color_space *pcs = pfs->direct_space;
	    gs_client_color fcc;
	    int code;

#if 0
	    memcpy(&fcc.paint, fp->va.cc, sizeof(fcc.paint));
#else
	    /* Average the colors at the vertices. */
	    {
		int ci;

		for (ci = 0; ci < pfs->num_components; ++ci)
		    fcc.paint.values[ci] =
			(fp->va.cc[ci] + fp->vb.cc[ci] + fp->vc.cc[ci]) / 3.0;
	    }
#endif
	    (*pcs->type->restrict_color)(&fcc, pcs);
	    (*pcs->type->remap_color)(&fcc, pcs, &dev_color, pis,
				      pfs->dev, gs_color_select_texture);
	    /****** SHOULD ADD adjust ON ANY OUTSIDE EDGES ******/
	    /*
	     * See the comment in gx_dc_pattern2_fill_rectangle in gsptype2.c
	     * re the choice of path filling vs. direct triangle fill.
	     */
	    if (pis->fill_adjust.x != 0 || pis->fill_adjust.y != 0) {
		gx_path *ppath = gx_path_alloc(pis->memory, "Gt_fill");
#		if VD_TRACE
		    vd_trace_interface * vd_trace_save = vd_trace1;
#		endif

		gx_path_add_point(ppath, fp->va.p.x, fp->va.p.y);
		gx_path_add_line(ppath, fp->vb.p.x, fp->vb.p.y);
		gx_path_add_line(ppath, fp->vc.p.x, fp->vc.p.y);
#		if VD_TRACE
		    vd_quad(fp->va.p.x, fp->va.p.y,
			    fp->va.p.x, fp->va.p.y,
			    fp->vb.p.x, fp->vb.p.y,
			    fp->vc.p.x, fp->vc.p.y, 0, RGB(255, 0, 0));
		    if (!VD_TRACE_DOWN)
			vd_trace1 = NULL;
#		    if TENSOR_SHADING_DEBUG
			triangle_cnt++;
#		    endif
#		endif
		code = shade_fill_path((const shading_fill_state_t *)pfs,
				       ppath, &dev_color);
#		if VD_TRACE
		    vd_trace1 = vd_trace_save;
#		endif
		gx_path_free(ppath, "Gt_fill");
	    } else {
		code = (*dev_proc(pfs->dev, fill_triangle))
		    (pfs->dev, fp->va.p.x, fp->va.p.y,
		     fp->vb.p.x - fp->va.p.x, fp->vb.p.y - fp->va.p.y,
		     fp->vc.p.x - fp->va.p.x, fp->vc.p.y - fp->va.p.y,
		     &dev_color, pis->log_op);
		if (code < 0)
		    return code;
	    }
	}
    next:
	if (fp == &pfs->frames[0])
	    return 0;
	--fp;
	continue;
    nofill:
	/*
	 * The colors don't converge.  Does the region color more than
	 * a single pixel?
	 */
	{
	    gs_fixed_rect region;

	    SET_MIN_MAX_3(region.p.x, region.q.x,
			  fp->va.p.x, fp->vb.p.x, fp->vc.p.x);
	    SET_MIN_MAX_3(region.p.y, region.q.y,
			  fp->va.p.y, fp->vb.p.y, fp->vc.p.y);
	    if (region.q.x - region.p.x <= fixed_1 &&
		region.q.y - region.p.y <= fixed_1) {
		/*
		 * More precisely, does the bounding box of the region
		 * span more than 1 pixel center in either X or Y?
		 */
		int nx =
		    fixed2int_pixround(region.q.x) -
		    fixed2int_pixround(region.p.x);
		int ny =
		    fixed2int_pixround(region.q.y) -
		    fixed2int_pixround(region.p.y);

		if (!(nx > 1 && ny != 0) || (ny > 1 && nx != 0))
		    goto fill;
	    }
	}
	fp = mesh_subdivide_triangle(pfs, fp);
    }
}

/* ---------------- Gouraud triangle shadings ---------------- */

private int
Gt_next_vertex(const gs_shading_mesh_t * psh, shade_coord_stream_t * cs,
	       shading_vertex_t * vertex)
{
    int code = shade_next_vertex(cs, vertex);

    if (code >= 0 && psh->params.Function) {
#	if !NEW_SHADINGS
	    /* Decode the color with the function. */
	    code = gs_function_evaluate(psh->params.Function, &vertex->c.t,
					vertex->c.cc.paint.values);
#	else
	    vertex->c.t = vertex->c.cc.paint.values[0];
#	endif
    }
    return code;
}

inline private int
Gt_fill_triangle(mesh_fill_state_t * pfs, const shading_vertex_t * va,
		 const shading_vertex_t * vb, const shading_vertex_t * vc)
{
#   if !NEW_SHADINGS
	shading_init_fill_triangle(pfs, va, vb, vc, true);
	return mesh_fill_triangle(pfs);
#   else
	patch_fill_state_t pfs1;
	int code;

 	memcpy(&pfs1, (shading_fill_state_t *)pfs, sizeof(shading_fill_state_t));
	pfs1.Function = pfs->pshm->params.Function;
	init_patch_fill_state(&pfs1);
	if (INTERPATCH_PADDING) {
	    code = padding(&pfs1, &va->p, &vb->p, &va->c, &vb->c);
	    if (code < 0)
		return code;
	    code = padding(&pfs1, &vb->p, &vc->p, &vb->c, &vc->c);
	    if (code < 0)
		return code;
	    code = padding(&pfs1, &vc->p, &va->p, &vc->c, &va->c);
	    if (code < 0)
		return code;
	}
	return triangle(&pfs1, va, vb, vc);
#   endif
}

int
gs_shading_FfGt_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			       gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_FfGt_t * const psh = (const gs_shading_FfGt_t *)psh0;
    mesh_fill_state_t state;
    shade_coord_stream_t cs;
    int num_bits = psh->params.BitsPerFlag;
    int flag;
    shading_vertex_t va, vb, vc;

    mesh_init_fill_state(&state, (const gs_shading_mesh_t *)psh, rect,
			 dev, pis);
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    while ((flag = shade_next_flag(&cs, num_bits)) >= 0) {
	int code;

	switch (flag) {
	    default:
		return_error(gs_error_rangecheck);
	    case 0:
		if ((code = Gt_next_vertex(state.pshm, &cs, &va)) < 0 ||
		    (code = shade_next_flag(&cs, num_bits)) < 0 ||
		    (code = Gt_next_vertex(state.pshm, &cs, &vb)) < 0 ||
		    (code = shade_next_flag(&cs, num_bits)) < 0
		    )
		    return code;
		goto v2;
	    case 1:
		va = vb;
	    case 2:
		vb = vc;
v2:		if ((code = Gt_next_vertex(state.pshm, &cs, &vc)) < 0)
		    return code;
		if ((code = Gt_fill_triangle(&state, &va, &vb, &vc)) < 0)
		    return code;
	}
    }
    return 0;
}

int
gs_shading_LfGt_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			       gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_LfGt_t * const psh = (const gs_shading_LfGt_t *)psh0;
    mesh_fill_state_t state;
    shade_coord_stream_t cs;
    shading_vertex_t *vertex;
    shading_vertex_t next;
    int per_row = psh->params.VerticesPerRow;
    int i, code = 0;

    if (VD_TRACE_TRIANGLE_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    mesh_init_fill_state(&state, (const gs_shading_mesh_t *)psh, rect,
			 dev, pis);
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    vertex = (shading_vertex_t *)
	gs_alloc_byte_array(pis->memory, per_row, sizeof(*vertex),
			    "gs_shading_LfGt_render");
    if (vertex == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < per_row; ++i)
	if ((code = Gt_next_vertex(state.pshm, &cs, &vertex[i])) < 0)
	    goto out;
    while (!seofp(cs.s)) {
	code = Gt_next_vertex(state.pshm, &cs, &next);
	if (code < 0)
	    goto out;
	for (i = 1; i < per_row; ++i) {
	    code = Gt_fill_triangle(&state, &vertex[i - 1], &vertex[i], &next);
	    if (code < 0)
		goto out;
	    vertex[i - 1] = next;
	    code = Gt_next_vertex(state.pshm, &cs, &next);
	    if (code < 0)
		goto out;
	    code = Gt_fill_triangle(&state, &vertex[i], &vertex[i - 1], &next);
	    if (code < 0)
		goto out;
	}
	vertex[per_row - 1] = next;
    }
out:
    if (VD_TRACE_TRIANGLE_PATCH && vd_allowed('s'))
	vd_release_dc;
    gs_free_object(pis->memory, vertex, "gs_shading_LfGt_render");
    return code;
}
