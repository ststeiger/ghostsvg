/* Copyright (C) 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
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
/* Type 42 character display operator */
#include "ghost.h"
#include "oper.h"
#include "gsmatrix.h"
#include "gspaint.h"		/* for gs_fill, gs_stroke */
#include "gspath.h"
#include "gxfixed.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxtext.h"
#include "gzstate.h"		/* only for ->path */
#include "dstack.h"		/* only for systemdict */
#include "estack.h"
#include "ichar.h"
#include "icharout.h"
#include "ifont.h"		/* for font_param */
#include "igstate.h"
#include "store.h"

/* <font> <code|name> <name> <glyph_index> .type42execchar - */
private int type42_fill(P1(i_ctx_t *));
private int type42_stroke(P1(i_ctx_t *));
private int
ztype42execchar(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font *pfont;
    int code = font_param(op - 3, &pfont);
    gs_font_base *const pbfont = (gs_font_base *) pfont;
    gs_text_enum_t *penum = op_show_find(i_ctx_p);
    int present;
    double sbw[4];
    double w[2];

    if (code < 0)
	return code;
    if (penum == 0 ||
	(pfont->FontType != ft_TrueType &&
	 pfont->FontType != ft_CID_TrueType)
	)
	return_error(e_undefined);
    /*
     * Any reasonable implementation would execute something like
     *  1 setmiterlimit 0 setlinejoin 0 setlinecap
     * here, but apparently the Adobe implementations aren't reasonable.
     *
     * If this is a stroked font, set the stroke width.
     */
    if (pfont->PaintType)
	gs_setlinewidth(igs, pfont->StrokeWidth);
    check_estack(3);		/* for continuations */
    /*
     * Execute the definition of the character.
     */
    if (r_is_proc(op))
	return zchar_exec_char_proc(i_ctx_p);
    /*
     * The definition must be a Type 42 glyph index.
     * Note that we do not require read access: this is deliberate.
     */
    check_type(*op, t_integer);
    check_ostack(3);		/* for lsb values */
    present = zchar_get_metrics(pbfont, op - 1, sbw);
    if (present < 0)
	return present;
    /* Establish a current point. */
    code = gs_moveto(igs, 0.0, 0.0);
    if (code < 0)
	return code;
    /* Get the metrics and set the cache device. */
    if (present == metricsNone) {
	float sbw42[4];
	int i;

	code = gs_type42_wmode_metrics((gs_font_type42 *) pfont,
				       (uint) op->value.intval, false, sbw42);
	if (code < 0)
	    return code;
	for (i = 0; i < 4; ++i)
	    sbw[i] = sbw42[i];
	w[0] = sbw[2];
	w[1] = sbw[3];
	if (gs_rootfont(igs)->WMode) { /* for vertically-oriented metrics */
	    code = gs_type42_wmode_metrics((gs_font_type42 *) pfont,
					   (uint) op->value.intval,
					   true, sbw42);
	    if (code < 0) { /* no vertical metrics */
		if (pfont->FontType == ft_CID_TrueType) {
		    sbw[0] = sbw[2] / 2;
		    sbw[1] = pbfont->FontBBox.q.y;
		    sbw[2] = 0;
		    sbw[3] = pbfont->FontBBox.p.y - pbfont->FontBBox.q.y;
		}
	    } else {
		sbw[0] = sbw[2] / 2;
		sbw[1] = (pbfont->FontBBox.q.y + pbfont->FontBBox.p.y - sbw42[3]) / 2;
		sbw[2] = sbw42[2];
		sbw[3] = sbw42[3];
	    }
	}
    } else {
        w[0] = sbw[2];
        w[1] = sbw[3];
    }
    return zchar_set_cache(i_ctx_p, pbfont, op - 1,
			   (present == metricsSideBearingAndWidth ?
			    sbw : NULL),
			   w, &pbfont->FontBBox,
			   type42_fill, type42_stroke,
			   gs_rootfont(igs)->WMode ? sbw : NULL);
}

/* Continue after a CDevProc callout. */
private int type42_finish(P2(i_ctx_t *i_ctx_p,
			     int (*cont)(P1(gs_state *))));
private int
type42_fill(i_ctx_t *i_ctx_p)
{
    return type42_finish(i_ctx_p, gs_fill);
}
private int
type42_stroke(i_ctx_t *i_ctx_p)
{
    return type42_finish(i_ctx_p, gs_stroke);
}
/* <font> <code|name> <name> <glyph_index> <sbx> <sby> %type42_{fill|stroke} - */
/* <font> <code|name> <name> <glyph_index> %type42_{fill|stroke} - */
private int
type42_finish(i_ctx_t *i_ctx_p, int (*cont) (P1(gs_state *)))
{
    os_ptr op = osp;
    gs_font *pfont;
    int code;
    gs_text_enum_t *penum = op_show_find(i_ctx_p);
    double sbxy[2];
    gs_point sbpt;
    gs_point *psbpt = 0;
    os_ptr opc = op;

    if (!r_has_type(op - 3, t_dictionary)) {
	check_op(6);
	code = num_params(op, 2, sbxy);
	if (code < 0)
	    return code;
	sbpt.x = sbxy[0];
	sbpt.y = sbxy[1];
	psbpt = &sbpt;
	opc -= 2;
    }
    check_type(*opc, t_integer);
    code = font_param(opc - 3, &pfont);
    if (code < 0)
	return code;
    if (penum == 0 || (pfont->FontType != ft_TrueType &&
		       pfont->FontType != ft_CID_TrueType)
	)
	return_error(e_undefined);
    /*
     * We have to disregard penum->pis and penum->path, and render to
     * the current gstate and path.  This is a design bug that we will
     * have to address someday!
     */
    code = gs_type42_append((uint)opc->value.intval, (gs_imager_state *)igs,
			    igs->path, &penum->log2_scale,
			    (penum->text.operation & TEXT_DO_ANY_CHARPATH) != 0,
			    pfont->PaintType, (gs_font_type42 *)pfont);
    if (code < 0)
	return code;
    pop((psbpt == 0 ? 4 : 6));
    return (*cont)(igs);
}

/* ------ Initialization procedure ------ */

const op_def zchar42_op_defs[] =
{
    {"4.type42execchar", ztype42execchar},
    op_def_end(0)
};
