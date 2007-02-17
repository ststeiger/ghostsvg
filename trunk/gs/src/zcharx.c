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
/* Level 2 character operators */
#include "ghost.h"
#include "oper.h"
#include "gsmatrix.h"		/* for gxfont.h */
#include "gstext.h"
#include "gxfixed.h"		/* for gxfont.h */
#include "gxfont.h"
#include "gxtext.h"
#include "ialloc.h"
#include "ichar.h"
#include "igstate.h"
#include "iname.h"
#include "ibnum.h"
#include "memory_.h"

/* Common setup for glyphshow and .glyphwidth. */
private int
glyph_show_setup(i_ctx_t *i_ctx_p, gs_glyph *pglyph)
{
    os_ptr op = osp;

    switch (gs_currentfont(igs)->FontType) {
	case ft_CID_encrypted:
	case ft_CID_user_defined:
	case ft_CID_TrueType:
	case ft_CID_bitmap:
	    check_int_leu(*op, gs_max_glyph - gs_min_cid_glyph);
	    *pglyph = (gs_glyph) op->value.intval + gs_min_cid_glyph;
	    break;
	default:
	    check_type(*op, t_name);
	    *pglyph = name_index(imemory, op);
    }
    return op_show_enum_setup(i_ctx_p);
}

/* <charname> glyphshow - */
private int
zglyphshow(i_ctx_t *i_ctx_p)
{
    gs_glyph glyph;
    gs_text_enum_t *penum;
    int code;

    if ((code = glyph_show_setup(i_ctx_p, &glyph)) != 0 ||
	(code = gs_glyphshow_begin(igs, glyph, imemory, &penum)) < 0)
	return code;
    *(op_proc_t *)&penum->enum_client_data = zglyphshow;
    if ((code = op_show_finish_setup(i_ctx_p, penum, 1, NULL)) < 0) {
	ifree_object(penum, "zglyphshow");
	return code;
    }
    return op_show_continue_pop(i_ctx_p, 1);
}

/* <charname> .glyphwidth <wx> <wy> */
private int
zglyphwidth(i_ctx_t *i_ctx_p)
{
    gs_glyph glyph;
    gs_text_enum_t *penum;
    int code;

    if ((code = glyph_show_setup(i_ctx_p, &glyph)) != 0 ||
	(code = gs_glyphwidth_begin(igs, glyph, imemory, &penum)) < 0)
	return code;
    if ((code = op_show_finish_setup(i_ctx_p, penum, 1, finish_stringwidth)) < 0) {
	ifree_object(penum, "zglyphwidth");
	return code;
    }
    return op_show_continue_pop(i_ctx_p, 1);
}

/* <string> <numarray|numstring> xshow - */
/* <string> <numarray|numstring> yshow - */
/* <string> <numarray|numstring> xyshow - */
private int
moveshow(i_ctx_t *i_ctx_p, bool have_x, bool have_y)
{
    os_ptr op = osp;
    gs_text_enum_t *penum;
    int code = op_show_setup(i_ctx_p, op - 1);
    int format;
    uint i, size, widths_needed;
    float *values;
    extern bool CPSI_mode;

    if (code != 0)
	return code;
    format = num_array_format(op);
    if (format < 0)
	return format;
    size = num_array_size(op, format);
    values = (float *)ialloc_byte_array(size, sizeof(float), "moveshow");
    if (values == 0)
	return_error(e_VMerror);
    if (CPSI_mode)
	memset(values, 0, size * sizeof(values[0])); /* Safety. */
    if ((code = gs_xyshow_begin(igs, op[-1].value.bytes, r_size(op - 1),
				(have_x ? values : (float *)0),
				(have_y ? values : (float *)0),
				size, imemory, &penum)) < 0) {
	ifree_object(values, "moveshow");
	return code;
    }
    if (CPSI_mode) {
	/* CET 13-29.PS page 2 defines a longer width array
	   then the text requires, and CPSI silently ignores extra elements.
	   So we need to compute exact number of characters 
	   to know how many elements to load and type check. */
	code = gs_text_count_chars(igs, gs_get_text_params(penum), imemory);
	if (code < 0)
	    return code;
	widths_needed = code;
	if (have_x && have_y)
	    widths_needed <<= 1;
    } else
	widths_needed = size;
    for (i = 0; i < widths_needed; ++i) {
	ref value;

	switch (code = num_array_get(imemory, op, format, i, &value)) {
	case t_integer:
	    values[i] = (float)value.value.intval; break;
	case t_real:
	    values[i] = value.value.realval; break;
	case t_null:
	    code = gs_note_error(e_rangecheck);
	    /* falls through */
	default:
	    ifree_object(values, "moveshow");
	    return code;
	}
    }
    if ((code = op_show_finish_setup(i_ctx_p, penum, 2, NULL)) < 0) {
	ifree_object(values, "moveshow");
	return code;
    }
    pop(2);
    return op_show_continue(i_ctx_p);
}
private int
zxshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, true, false);
}
private int
zyshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, false, true);
}
private int
zxyshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, true, true);
}

/* ------ Initialization procedure ------ */

const op_def zcharx_op_defs[] =
{
    op_def_begin_level2(),
    {"1glyphshow", zglyphshow},
    {"1.glyphwidth", zglyphwidth},
    {"2xshow", zxshow},
    {"2xyshow", zxyshow},
    {"2yshow", zyshow},
    op_def_end(0)
};
