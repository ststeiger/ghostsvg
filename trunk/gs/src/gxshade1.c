/* Copyright (C) 2001-2006 artofcode LLC.
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
/* Rendering for non-mesh shadings */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gspath.h"
#include "gsptype2.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxshade.h"
#include "gxshade4.h"
#include "gxdevcli.h"
#include "vdtrace.h"

#define VD_TRACE_AXIAL_PATCH 1
#define VD_TRACE_RADIAL_PATCH 1
#define VD_TRACE_FUNCTIONAL_PATCH 1


/* ---------------- Function-based shading ---------------- */

typedef struct Fb_frame_s {	/* A rudiment of old code. */
    gs_rect region;
    gs_client_color cc[4];	/* colors at 4 corners */
    int state;
} Fb_frame_t;

typedef struct Fb_fill_state_s {
    shading_fill_state_common;
    const gs_shading_Fb_t *psh;
    gs_matrix_fixed ptm;	/* parameter space -> device space */
    Fb_frame_t frame;
} Fb_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

private inline void
make_other_poles(patch_curve_t curve[4])
{
    int i, j;

    for (i = 0; i < 4; i++) {
	j = (i + 1) % 4;
	curve[i].control[0].x = (curve[i].vertex.p.x * 2 + curve[j].vertex.p.x) / 3;
	curve[i].control[0].y = (curve[i].vertex.p.y * 2 + curve[j].vertex.p.y) / 3;
	curve[i].control[1].x = (curve[i].vertex.p.x + curve[j].vertex.p.x * 2) / 3;
	curve[i].control[1].y = (curve[i].vertex.p.y + curve[j].vertex.p.y * 2) / 3;
	curve[i].straight = true;
    }
}

private int
Fb_fill_region(Fb_fill_state_t * pfs, const gs_fixed_rect *rect)
{
    patch_fill_state_t pfs1;
    patch_curve_t curve[4];
    Fb_frame_t * fp = &pfs->frame;
    int code;

    if (VD_TRACE_FUNCTIONAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    memcpy(&pfs1, (shading_fill_state_t *)pfs, sizeof(shading_fill_state_t));
    pfs1.Function = pfs->psh->params.Function;
    code = init_patch_fill_state(&pfs1);
    if (code < 0)
	return code;
    pfs1.maybe_self_intersecting = false;
    pfs1.n_color_args = 2;
    pfs1.rect = *rect;
    gs_point_transform2fixed(&pfs->ptm, fp->region.p.x, fp->region.p.y, &curve[0].vertex.p);
    gs_point_transform2fixed(&pfs->ptm, fp->region.q.x, fp->region.p.y, &curve[1].vertex.p);
    gs_point_transform2fixed(&pfs->ptm, fp->region.q.x, fp->region.q.y, &curve[2].vertex.p);
    gs_point_transform2fixed(&pfs->ptm, fp->region.p.x, fp->region.q.y, &curve[3].vertex.p);
    make_other_poles(curve);
    curve[0].vertex.cc[0] = fp->region.p.x;   curve[0].vertex.cc[1] = fp->region.p.y;
    curve[1].vertex.cc[0] = fp->region.q.x;   curve[1].vertex.cc[1] = fp->region.p.y;
    curve[2].vertex.cc[0] = fp->region.q.x;   curve[2].vertex.cc[1] = fp->region.q.y;
    curve[3].vertex.cc[0] = fp->region.p.x;   curve[3].vertex.cc[1] = fp->region.q.y;
    code = patch_fill(&pfs1, curve, NULL, NULL);
    term_patch_fill_state(&pfs1);
    if (VD_TRACE_FUNCTIONAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}

int
gs_shading_Fb_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect, 
			     const gs_fixed_rect * rect_clip,
			     gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_Fb_t * const psh = (const gs_shading_Fb_t *)psh0;
    gs_matrix save_ctm;
    int xi, yi;
    float x[2], y[2];
    Fb_fill_state_t state;

    shade_init_fill_state((shading_fill_state_t *) & state, psh0, dev, pis);
    state.psh = psh;
    /****** HACK FOR FIXED-POINT MATRIX MULTIPLY ******/
    gs_currentmatrix((gs_state *) pis, &save_ctm);
    gs_concat((gs_state *) pis, &psh->params.Matrix);
    state.ptm = pis->ctm;
    gs_setmatrix((gs_state *) pis, &save_ctm);
    /* Compute the parameter X and Y ranges. */
    {
	gs_rect pbox;

	gs_bbox_transform_inverse(rect, &psh->params.Matrix, &pbox);
	x[0] = max(pbox.p.x, psh->params.Domain[0]);
	x[1] = min(pbox.q.x, psh->params.Domain[1]);
	y[0] = max(pbox.p.y, psh->params.Domain[2]);
	y[1] = min(pbox.q.y, psh->params.Domain[3]);
    }
    for (xi = 0; xi < 2; ++xi)
	for (yi = 0; yi < 2; ++yi) {
	    float v[2];

	    v[0] = x[xi], v[1] = y[yi];
	    gs_function_evaluate(psh->params.Function, v,
				 state.frame.cc[yi * 2 + xi].paint.values);
	}
    state.frame.region.p.x = x[0];
    state.frame.region.p.y = y[0];
    state.frame.region.q.x = x[1];
    state.frame.region.q.y = y[1];
    return Fb_fill_region(&state, rect_clip);
}

/* ---------------- Axial shading ---------------- */

typedef struct A_fill_state_s {
    const gs_shading_A_t *psh;
    gs_point delta;
    double length;
    double t0, t1;
    double v0, v1, u0, u1;
} A_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

/* Note t0 and t1 vary over [0..1], not the Domain. */

private int
A_fill_region(A_fill_state_t * pfs, patch_fill_state_t *pfs1)
{
    const gs_shading_A_t * const psh = pfs->psh;
    double x0 = psh->params.Coords[0] + pfs->delta.x * pfs->v0;
    double y0 = psh->params.Coords[1] + pfs->delta.y * pfs->v0;
    double x1 = psh->params.Coords[0] + pfs->delta.x * pfs->v1;
    double y1 = psh->params.Coords[1] + pfs->delta.y * pfs->v1;
    double h0 = pfs->u0, h1 = pfs->u1;
    patch_curve_t curve[4];

    gs_point_transform2fixed(&pfs1->pis->ctm, x0 + pfs->delta.y * h0, y0 - pfs->delta.x * h0, &curve[0].vertex.p);
    gs_point_transform2fixed(&pfs1->pis->ctm, x1 + pfs->delta.y * h0, y1 - pfs->delta.x * h0, &curve[1].vertex.p);
    gs_point_transform2fixed(&pfs1->pis->ctm, x1 + pfs->delta.y * h1, y1 - pfs->delta.x * h1, &curve[2].vertex.p);
    gs_point_transform2fixed(&pfs1->pis->ctm, x0 + pfs->delta.y * h1, y0 - pfs->delta.x * h1, &curve[3].vertex.p);
    curve[0].vertex.cc[0] = curve[0].vertex.cc[1] = pfs->t0; /* The element cc[1] is set to a dummy value against */
    curve[1].vertex.cc[0] = curve[1].vertex.cc[1] = pfs->t1; /* interrupts while an idle priocessing in gxshade.6.c .  */
    curve[2].vertex.cc[0] = curve[2].vertex.cc[1] = pfs->t1;
    curve[3].vertex.cc[0] = curve[3].vertex.cc[1] = pfs->t0;
    make_other_poles(curve);
    return patch_fill(pfs1, curve, NULL, NULL);
}

private inline int
gs_shading_A_fill_rectangle_aux(const gs_shading_t * psh0, const gs_rect * rect,
			    const gs_fixed_rect *clip_rect,
			    gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_A_t *const psh = (const gs_shading_A_t *)psh0;
    gs_function_t * const pfn = psh->params.Function;
    gs_matrix cmat;
    gs_rect t_rect;
    A_fill_state_t state;
    patch_fill_state_t pfs1;
    float d0 = psh->params.Domain[0], d1 = psh->params.Domain[1];
    float dd = d1 - d0;
    double t0, t1;
    gs_point dist;
    int code;

    state.psh = psh;
    shade_init_fill_state((shading_fill_state_t *)&pfs1, psh0, dev, pis);
    pfs1.Function = pfn;
    pfs1.rect = *clip_rect;
    code = init_patch_fill_state(&pfs1);
    if (code < 0)
	return code;
    pfs1.maybe_self_intersecting = false;
    /*
     * Compute the parameter range.  We construct a matrix in which
     * (0,0) corresponds to t = 0 and (0,1) corresponds to t = 1,
     * and use it to inverse-map the rectangle to be filled.
     */
    cmat.tx = psh->params.Coords[0];
    cmat.ty = psh->params.Coords[1];
    state.delta.x = psh->params.Coords[2] - psh->params.Coords[0];
    state.delta.y = psh->params.Coords[3] - psh->params.Coords[1];
    cmat.yx = state.delta.x;
    cmat.yy = state.delta.y;
    cmat.xx = cmat.yy;
    cmat.xy = -cmat.yx;
    gs_bbox_transform_inverse(rect, &cmat, &t_rect);
    t0 = min(max(t_rect.p.y, 0), 1);
    t1 = max(min(t_rect.q.y, 1), 0);
    state.v0 = t0;
    state.v1 = t1;
    state.u0 = t_rect.p.x;
    state.u1 = t_rect.q.x;
    state.t0 = t0 * dd + d0;
    state.t1 = t1 * dd + d0;
    gs_distance_transform(state.delta.x, state.delta.y, &ctm_only(pis),
			  &dist);
    state.length = hypot(dist.x, dist.y);	/* device space line length */
    code = A_fill_region(&state, &pfs1);
    if (psh->params.Extend[0] && t0 > t_rect.p.y) {
	if (code < 0)
	    return code;
	/* Use the general algorithm, because we need the trapping. */
	state.v0 = t_rect.p.y;
	state.v1 = t0;
	state.t0 = state.t1 = t0 * dd + d0;
	code = A_fill_region(&state, &pfs1);
    }
    if (psh->params.Extend[1] && t1 < t_rect.q.y) {
	if (code < 0)
	    return code;
	/* Use the general algorithm, because we need the trapping. */
	state.v0 = t1;
	state.v1 = t_rect.q.y;
	state.t0 = state.t1 = t1 * dd + d0;
	code = A_fill_region(&state, &pfs1);
    }
    term_patch_fill_state(&pfs1);
    return code;
}

int
gs_shading_A_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    const gs_fixed_rect * rect_clip,
			    gx_device * dev, gs_imager_state * pis)
{
    int code;

    if (VD_TRACE_AXIAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    code = gs_shading_A_fill_rectangle_aux(psh0, rect, rect_clip, dev, pis);
    if (VD_TRACE_AXIAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}

/* ---------------- Radial shading ---------------- */

typedef struct R_frame_s {	/* A rudiment of old code. */
    double t0, t1;
    gs_client_color cc[2];	/* color at t0, t1 */
} R_frame_t;

typedef struct R_fill_state_s {
    shading_fill_state_common;
    const gs_shading_R_t *psh;
    gs_rect rect;
    gs_point delta;
    double dr, dd;
    R_frame_t frame;
} R_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

private int 
R_tensor_annulus(patch_fill_state_t *pfs, const gs_rect *rect,
    double x0, double y0, double r0, double t0,
    double x1, double y1, double r1, double t1)
{   
    double dx = x1 - x0, dy = y1 - y0;
    double d = hypot(dx, dy);
    gs_point p0, p1, pc0, pc1;
    int k, j, code;
    bool inside = 0;

    pc0.x = x0, pc0.y = y0; 
    pc1.x = x1, pc1.y = y1;
    if (r0 + d <= r1 || r1 + d <= r0) {
	/* One circle is inside another one. 
	   Use any subdivision, 
	   but don't depend on dx, dy, which may be too small. */
	p0.x = 0, p0.y = -1;
	/* Align stripes along radii for faster triangulation : */
	inside = 1;
    } else {
        /* Must generate canonic quadrangle arcs,
	   because we approximate them with curves. */
	if(any_abs(dx) >= any_abs(dy)) {
	    if (dx > 0)
		p0.x = 0, p0.y = -1;
	    else
		p0.x = 0, p0.y = 1;
	} else {
	    if (dy > 0)
		p0.x = 1, p0.y = 0;
	    else
		p0.x = -1, p0.y = 0;
	}
    }
    /* fixme: wish: cut invisible parts off. 
       Note : when r0 != r1 the invisible part is not a half circle. */
    for (k = 0; k < 4; k++, p0 = p1) {
	gs_point p[12];
	patch_curve_t curve[4];

	p1.x = -p0.y; p1.y = p0.x;
	if ((k & 1) == k >> 1) {
	    make_quadrant_arc(p + 0, &pc0, &p1, &p0, r0);
	    make_quadrant_arc(p + 6, &pc1, &p0, &p1, r1);
	} else {
	    make_quadrant_arc(p + 0, &pc0, &p0, &p1, r0);
	    make_quadrant_arc(p + 6, &pc1, &p1, &p0, r1);
	}
	p[4].x = (p[3].x * 2 + p[6].x) / 3;
	p[4].y = (p[3].y * 2 + p[6].y) / 3;
	p[5].x = (p[3].x + p[6].x * 2) / 3;
	p[5].y = (p[3].y + p[6].y * 2) / 3;
	p[10].x = (p[9].x * 2 + p[0].x) / 3;
	p[10].y = (p[9].y * 2 + p[0].y) / 3;
	p[11].x = (p[9].x + p[0].x * 2) / 3;
	p[11].y = (p[9].y + p[0].y * 2) / 3;
	for (j = 0; j < 4; j++) {
	    int jj = (j + inside) % 4;

	    code = gs_point_transform2fixed(&pfs->pis->ctm, 
			p[j * 3 + 0].x, p[j * 3 + 0].y, &curve[jj].vertex.p);
	    if (code < 0)
		return code;
	    code = gs_point_transform2fixed(&pfs->pis->ctm, 
			p[j * 3 + 1].x, p[j * 3 + 1].y, &curve[jj].control[0]);
	    if (code < 0)
		return code;
	    code = gs_point_transform2fixed(&pfs->pis->ctm, 
			p[j * 3 + 2].x, p[j * 3 + 2].y, &curve[jj].control[1]);
	    if (code < 0)
		return code;
	    curve[j].straight = (((j + inside) & 1) != 0);
	}
	curve[(0 + inside) % 4].vertex.cc[0] = t0;
	curve[(1 + inside) % 4].vertex.cc[0] = t0;
	curve[(2 + inside) % 4].vertex.cc[0] = t1;
	curve[(3 + inside) % 4].vertex.cc[0] = t1;
	curve[0].vertex.cc[1] = curve[1].vertex.cc[1] = 0; /* Initialize against FPE. */
	curve[2].vertex.cc[1] = curve[3].vertex.cc[1] = 0; /* Initialize against FPE. */
	code = patch_fill(pfs, curve, NULL, NULL);
	if (code < 0)
	    return code;
    }
    return 0;
}


private int
R_outer_circle(patch_fill_state_t *pfs, const gs_rect *rect, 
	double x0, double y0, double r0, 
	double x1, double y1, double r1, 
	double *x2, double *y2, double *r2)
{
    double dx = x1 - x0, dy = y1 - y0;
    double sp, sq, s;

    /* Compute a cone circle, which contacts the rect externally. */
    /* Don't bother with all 4 sides of the rect, 
       just do with the X or Y span only,
       so it's not an exact contact, sorry. */
    if (any_abs(dx) > any_abs(dy)) {
	/* Solving :
	    x0 + (x1 - x0) * sq + r0 + (r1 - r0) * sq == bbox_px
	    (x1 - x0) * sp + (r1 - r0) * sp == bbox_px - x0 - r0
	    sp = (bbox_px - x0 - r0) / (x1 - x0 + r1 - r0)

	    x0 + (x1 - x0) * sq - r0 - (r1 - r0) * sq == bbox_qx
	    (x1 - x0) * sq - (r1 - r0) * sq == bbox_x - x0 + r0
	    sq = (bbox_x - x0 + r0) / (x1 - x0 - r1 + r0)
	 */
	if (x1 - x0 + r1 - r0 ==  0) /* We checked for obtuse cone. */
	    return_error(gs_error_unregistered); /* Must not happen. */
	if (x1 - x0 - r1 + r0 ==  0) /* We checked for obtuse cone. */
	    return_error(gs_error_unregistered); /* Must not happen. */
	sp = (rect->p.x - x0 - r0) / (x1 - x0 + r1 - r0);
	sq = (rect->q.x - x0 + r0) / (x1 - x0 - r1 + r0);
    } else {
	/* Same by Y. */
	if (y1 - y0 + r1 - r0 ==  0) /* We checked for obtuse cone. */
	    return_error(gs_error_unregistered); /* Must not happen. */
	if (y1 - y0 - r1 + r0 ==  0) /* We checked for obtuse cone. */
	    return_error(gs_error_unregistered); /* Must not happen. */
	sp = (rect->p.y - y0 - r0) / (y1 - y0 + r1 - r0);
	sq = (rect->q.y - y0 + r0) / (y1 - y0 - r1 + r0);
    }
    if (sp >= 1 && sq >= 1)
	s = min(sp, sq);
    else if(sp >= 1)
	s = sp;
    else if (sq >= 1)
	s = sq;
    else {
	/* The circle 1 is outside the rect, use it. */
        s = 1;
    }
    if (r0 + (r1 - r0) * s < 0) {
	/* Passed the cone apex, use the apex. */
	s = r0 / (r0 - r1);
	*r2 = 0;
    } else
	*r2 = r0 + (r1 - r0) * s;
    *x2 = x0 + (x1 - x0) * s;
    *y2 = y0 + (y1 - y0) * s;
    return 0;
}

private double 
R_rect_radius(const gs_rect *rect, double x0, double y0)
{
    double d, dd;

    dd = hypot(rect->p.x - x0, rect->p.y - y0);
    d = hypot(rect->p.x - x0, rect->q.y - y0);
    dd = max(dd, d);
    d = hypot(rect->q.x - x0, rect->q.y - y0);
    dd = max(dd, d);
    d = hypot(rect->q.x - x0, rect->p.y - y0);
    dd = max(dd, d);
    return dd;
}

private int
R_fill_triangle_new(patch_fill_state_t *pfs, const gs_rect *rect, 
    double x0, double y0, double x1, double y1, double x2, double y2, double t)
{
    shading_vertex_t p0, p1, p2;
    patch_color_t *c;
    int code;
    byte *color_stack_ptr1 = reserve_colors(pfs, pfs->color_stack, &c, 1); /* Can't fail */

    p0.c = c;
    p1.c = c;
    p2.c = c;
    code = gs_point_transform2fixed(&pfs->pis->ctm, x0, y0, &p0.p);
    if (code >= 0)
	code = gs_point_transform2fixed(&pfs->pis->ctm, x1, y1, &p1.p);
    if (code >= 0)
	code = gs_point_transform2fixed(&pfs->pis->ctm, x2, y2, &p2.p);
    if (code >= 0) {
	c->t[0] = c->t[1] = t;
	patch_resolve_color(c, pfs);
	code = mesh_triangle(pfs, &p0, &p1, &p2, color_stack_ptr1);
    }
    release_colors(pfs, pfs->color_stack, 1);
    return code;
}

private int
R_obtuse_cone(patch_fill_state_t *pfs, const gs_rect *rect,
	double x0, double y0, double r0, 
	double x1, double y1, double r1, double t0, double r_rect)
{
    double dx = x1 - x0, dy = y1 - y0, dr = any_abs(r1 - r0);
    double d = hypot(dx, dy);
    /* Assuming dr > d / 3 && d > dr + 1e-7 * (d + dr), see the caller. */
    double r = r_rect * 1.4143; /* A few bigger than sqrt(2). */
    double ax, ay, as; /* Cone apex. */
    double g0; /* The distance from apex to the tangent point of the 0th circle. */
    int code;

    as = r0 / (r0 - r1);
    ax = x0 + (x1 - x0) * as;
    ay = y0 + (y1 - y0) * as;
    g0 = sqrt(dx * dx + dy * dy - dr * dr) * as;
    if (g0 < 1e-7 * r0) {
	/* Nearly degenerate, replace with half-plane. */
	/* Restrict the half plane with triangle, which covers the rect. */
	gs_point p0, p1, p2; /* Right tangent limit, apex limit, left tangent linit,
				(right, left == when looking from the apex). */

	p0.x = ax - dy * r / d;
	p0.y = ay + dx * r / d;
	p1.x = ax - dx * r / d;
	p1.y = ay - dy * r / d;
	p2.x = ax + dy * r / d;
	p2.y = ay - dx * r / d;
	/* Split into 2 triangles at the apex,
	   so that the apex is preciselly covered.
	   Especially important when it is not exactly degenerate. */
	code = R_fill_triangle_new(pfs, rect, ax, ay, p0.x, p0.y, p1.x, p1.y, t0);
	if (code < 0)
	    return code;
	return R_fill_triangle_new(pfs, rect, ax, ay, p1.x, p1.y, p2.x, p2.y, t0);
    } else {
	/* Compute the "limit" circle so that its
	   tangent points are outside the rect. */
	/* Note: this branch is executed when the condition above is false :
	   g0 >= 1e-7 * r0 .
	   We believe that computing this branch with doubles
	   provides enough precision after converting coordinates into 'fixed',
	   and that the limit circle radius is not dramatically big.
	 */
	double es, er; /* The limit circle parameter, radius. */
	double ex, ey; /* The limit circle centrum. */

	es = as - as * r / g0; /* Always negative. */
	er = r * r0 / g0 ;
	ex = x0 + dx * es;
	ey = y0 + dy * es;
	/* Fill the annulus: */
	code = R_tensor_annulus(pfs, rect, x0, y0, r0, t0, ex, ey, er, t0);
	if (code < 0)
	    return code;
	/* Fill entire ending circle to ewnsure antire rect is covered : */
	return R_tensor_annulus(pfs, rect, ex, ey, er, t0, ex, ey, 0, t0);
    }
}

private int
R_tensor_cone_apex(patch_fill_state_t *pfs, const gs_rect *rect,
	double x0, double y0, double r0, 
	double x1, double y1, double r1, double t)
{
    double as = r0 / (r0 - r1);
    double ax = x0 + (x1 - x0) * as;
    double ay = y0 + (y1 - y0) * as;

    return R_tensor_annulus(pfs, rect, x1, y1, r1, t, ax, ay, 0, t);
}


private int
R_extensions(patch_fill_state_t *pfs, const gs_shading_R_t *psh, const gs_rect *rect, 
	double t0, double t1, bool Extend0, bool Extend1)
{
    float x0 = psh->params.Coords[0], y0 = psh->params.Coords[1];
    floatp r0 = psh->params.Coords[2];
    float x1 = psh->params.Coords[3], y1 = psh->params.Coords[4];
    floatp r1 = psh->params.Coords[5];
    double dx = x1 - x0, dy = y1 - y0, dr = any_abs(r1 - r0);
    double d = hypot(dx, dy), r;
    int code;

    if (dr >= d - 1e-7 * (d + dr)) {
	/* Nested circles, or degenerate. */
	if (r0 > r1) {
	    if (Extend0) {
		r = R_rect_radius(rect, x0, y0);
		if (r > r0) {
		    code = R_tensor_annulus(pfs, rect, x0, y0, r, t0, x0, y0, r0, t0);
		    if (code < 0)
			return code;
		}
	    }
	    if (Extend1 && r1 > 0)
		return R_tensor_annulus(pfs, rect, x1, y1, r1, t1, x1, y1, 0, t1);
	} else {
	    if (Extend1) {
		r = R_rect_radius(rect, x1, y1);
		if (r > r1) {
		    code = R_tensor_annulus(pfs, rect, x1, y1, r, t1, x1, y1, r1, t1);
		    if (code < 0)
			return code;
		}
	    }
	    if (Extend0 && r0 > 0)
		return R_tensor_annulus(pfs, rect, x0, y0, r0, t0, x0, y0, 0, t0);
	}
    } else if (dr > d / 3) {
	/* Obtuse cone. */
	if (r0 > r1) {
	    if (Extend0) {
		r = R_rect_radius(rect, x0, y0);
		code = R_obtuse_cone(pfs, rect, x0, y0, r0, x1, y1, r1, t0, r);
		if (code < 0)
		    return code;
	    }
	    if (Extend1 && r1 != 0)
		return R_tensor_cone_apex(pfs, rect, x0, y0, r0, x1, y1, r1, t1);
	    return 0;
	} else {
	    if (Extend1) {
		r = R_rect_radius(rect, x1, y1);
		code = R_obtuse_cone(pfs, rect, x1, y1, r1, x0, y0, r0, t1, r);
		if (code < 0)
		    return code;
	    }
	    if (Extend0 && r0 != 0)
		return R_tensor_cone_apex(pfs, rect, x1, y1, r1, x0, y0, r0, t0);
	}
    } else {
	/* Acute cone or cylinder. */
	double x2, y2, r2, x3, y3, r3;

	if (Extend0) {
	    code = R_outer_circle(pfs, rect, x1, y1, r1, x0, y0, r0, &x3, &y3, &r3);
	    if (code < 0)
		return code;
	    if (x3 != x1 || y3 != y1) {
		code = R_tensor_annulus(pfs, rect, x0, y0, r0, t0, x3, y3, r3, t0);
		if (code < 0)
		    return code;
	    }
	}
	if (Extend1) {
	    code = R_outer_circle(pfs, rect, x0, y0, r0, x1, y1, r1, &x2, &y2, &r2);
	    if (code < 0)
		return code;
	    if (x2 != x0 || y2 != y0) {
		code = R_tensor_annulus(pfs, rect, x1, y1, r1, t1, x2, y2, r2, t1);
		if (code < 0)
		    return code;
	    }
	}
    }
    return 0;
}

private int
gs_shading_R_fill_rectangle_aux(const gs_shading_t * psh0, const gs_rect * rect,
			    const gs_fixed_rect *clip_rect,
			    gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_R_t *const psh = (const gs_shading_R_t *)psh0;
    R_fill_state_t state;
    float d0 = psh->params.Domain[0], d1 = psh->params.Domain[1];
    float dd = d1 - d0;
    float x0 = psh->params.Coords[0], y0 = psh->params.Coords[1];
    floatp r0 = psh->params.Coords[2];
    float x1 = psh->params.Coords[3], y1 = psh->params.Coords[4];
    floatp r1 = psh->params.Coords[5];
    int code;
    float dist_between_circles;
    gs_point dev_dpt;
    gs_point dev_dr;
    patch_fill_state_t pfs1;

    if (r0 == 0 && r1 == 0)
	return 0; /* PLRM requires to paint nothing. */
    shade_init_fill_state((shading_fill_state_t *)&state, psh0, dev, pis);
    state.psh = psh;
    state.rect = *rect;
    /* Compute the parameter range. */
    code = gs_function_evaluate(psh->params.Function, &d0,
			     state.frame.cc[0].paint.values);
    if (code < 0)
	return code;
    code = gs_function_evaluate(psh->params.Function, &d1,
			     state.frame.cc[1].paint.values);
    if (code < 0)
	return code;
    state.delta.x = x1 - x0;
    state.delta.y = y1 - y0;
    state.dr = r1 - r0;

    gs_distance_transform(state.delta.x, state.delta.y, &ctm_only(pis), &dev_dpt);
    gs_distance_transform(state.dr, 0, &ctm_only(pis), &dev_dr);
    
    dist_between_circles = hypot(x1-x0, y1-y0);

    state.dd = dd;
    memcpy(&pfs1, (shading_fill_state_t *)&state , sizeof(shading_fill_state_t));
    pfs1.Function = psh->params.Function;
    code = init_patch_fill_state(&pfs1);
    if (code < 0)
	return code;
    pfs1.rect = *clip_rect;
    pfs1.maybe_self_intersecting = false;
    code = R_extensions(&pfs1, psh, rect, d0, d1, psh->params.Extend[0], false);
    if (code >= 0) {
	float x0 = psh->params.Coords[0], y0 = psh->params.Coords[1];
	floatp r0 = psh->params.Coords[2];
	float x1 = psh->params.Coords[3], y1 = psh->params.Coords[4];
	floatp r1 = psh->params.Coords[5];
	
	code = R_tensor_annulus(&pfs1, rect, x0, y0, r0, d0, x1, y1, r1, d1);
    }
    if (code >= 0)
	code = R_extensions(&pfs1, psh, rect, d0, d1, false, psh->params.Extend[1]);
    term_patch_fill_state(&pfs1);
    return code;
}

int
gs_shading_R_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    const gs_fixed_rect * rect_clip,
			    gx_device * dev, gs_imager_state * pis)
{   
    int code;

    if (VD_TRACE_RADIAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    code = gs_shading_R_fill_rectangle_aux(psh0, rect, rect_clip, dev, pis);
    if (VD_TRACE_FUNCTIONAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}
