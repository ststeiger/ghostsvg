/* Copyright (C) 1989, 1995, 1996, 1997, 1998, 1999, 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* Font creation utilities */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "oper.h"
#include "gxfixed.h"
#include "gscencs.h"
#include "gsmatrix.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "bfont.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "ilevel.h"
#include "iname.h"
#include "inamedef.h"		/* for inlining name_index */
#include "interp.h"		/* for initial_enter_name */
#include "ipacked.h"
#include "istruct.h"
#include "store.h"

/* Structure descriptor */
public_st_font_data();

/* <string|name> <font_dict> .buildfont3 <string|name> <font> */
/* Build a type 3 (user-defined) font. */
private int
zbuildfont3(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;
    build_proc_refs build;
    gs_font_base *pfont;

    check_type(*op, t_dictionary);
    code = build_gs_font_procs(op, &build);
    if (code < 0)
	return code;
    code = build_gs_simple_font(i_ctx_p, op, &pfont, ft_user_defined,
				&st_gs_font_base, &build, bf_options_none);
    if (code < 0)
	return code;
    return define_gs_font((gs_font *) pfont);
}

/* Encode a character. */
gs_glyph
zfont_encode_char(gs_font *pfont, gs_char chr, gs_glyph_space_t ignored)
{
    const ref *pencoding = &pfont_data(pfont)->Encoding;
    ulong index = chr;	/* work around VAX widening bug */
    ref cname;
    int code = array_get(pencoding, (long)index, &cname);

    if (code < 0 || !r_has_type(&cname, t_name))
	return gs_no_glyph;
    return (gs_glyph)name_index(&cname);
}

/* Get the name of a glyph. */
private int
zfont_glyph_name(gs_font *font, gs_glyph index, gs_const_string *pstr)
{
    ref nref, sref;

    if (index >= gs_min_cid_glyph) {	/* Fabricate a numeric name. */
	char cid_name[sizeof(gs_glyph) * 3 + 1];
	int code;

	sprintf(cid_name, "%lu", (ulong) index);
	code = name_ref((const byte *)cid_name, strlen(cid_name),
			&nref, 1);
	if (code < 0)
	    return code;
    } else
	name_index_ref(index, &nref);
    name_string_ref(&nref, &sref);
    pstr->data = sref.value.const_bytes;
    pstr->size = r_size(&sref);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zbfont_op_defs[] =
{
    {"2.buildfont3", zbuildfont3},
    op_def_end(0)
};

/* ------ Subroutines ------ */

/* Convert strings to executable names for build_proc_refs. */
int
build_proc_name_refs(build_proc_refs * pbuild,
		     const char *bcstr, const char *bgstr)
{
    int code;

    if (!bcstr)
	make_null(&pbuild->BuildChar);
    else {
	if ((code = name_ref((const byte *)bcstr, strlen(bcstr), &pbuild->BuildChar, 0)) < 0)
	    return code;
	r_set_attrs(&pbuild->BuildChar, a_executable);
    }
    if (!bgstr)
	make_null(&pbuild->BuildGlyph);
    else {
	if ((code = name_ref((const byte *)bgstr, strlen(bgstr), &pbuild->BuildGlyph, 0)) < 0)
	    return code;
	r_set_attrs(&pbuild->BuildGlyph, a_executable);
    }
    return 0;
}

/* Get the BuildChar and/or BuildGlyph routines from a (base) font. */
int
build_gs_font_procs(os_ptr op, build_proc_refs * pbuild)
{
    int ccode, gcode;
    ref *pBuildChar;
    ref *pBuildGlyph;

    check_type(*op, t_dictionary);
    ccode = dict_find_string(op, "BuildChar", &pBuildChar);
    gcode = dict_find_string(op, "BuildGlyph", &pBuildGlyph);
    if (ccode <= 0) {
	if (gcode <= 0)
	    return_error(e_invalidfont);
	make_null(&pbuild->BuildChar);
    } else {
	check_proc(*pBuildChar);
	pbuild->BuildChar = *pBuildChar;
    }
    if (gcode <= 0)
	make_null(&pbuild->BuildGlyph);
    else {
	check_proc(*pBuildGlyph);
	pbuild->BuildGlyph = *pBuildGlyph;
    }
    return 0;
}

/* Do the common work for building a primitive font -- one whose execution */
/* algorithm is implemented in C (Type 1, Type 2, Type 4, or Type 42). */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_primitive_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base ** ppfont,
			font_type ftype, gs_memory_type_ptr_t pstype,
			const build_proc_refs * pbuild,
			build_font_options_t options)
{
    ref *pcharstrings = 0;
    ref CharStrings;
    gs_font_base *pfont;
    font_data *pdata;
    int code;

    if (dict_find_string(op, "CharStrings", &pcharstrings) <= 0) {
	if (!(options & bf_CharStrings_optional))
	    return_error(e_invalidfont);
    } else {
	ref *ignore;

	if (!r_has_type(pcharstrings, t_dictionary))
	    return_error(e_invalidfont);
	if ((options & bf_notdef_required) != 0 &&
	    dict_find_string(pcharstrings, ".notdef", &ignore) <= 0
	    )
	    return_error(e_invalidfont);
	/*
	 * Since build_gs_simple_font may resize the dictionary and cause
	 * pointers to become invalid, save CharStrings.
	 */
	CharStrings = *pcharstrings;
    }
    code = build_gs_outline_font(i_ctx_p, op, ppfont, ftype, pstype, pbuild,
				 options, build_gs_simple_font);
    if (code != 0)
	return code;
    pfont = *ppfont;
    pdata = pfont_data(pfont);
    if (pcharstrings)
	ref_assign(&pdata->CharStrings, &CharStrings);
    else
	make_null(&pdata->CharStrings);
    /* Check that the UniqueIDs match.  This is part of the */
    /* Adobe protection scheme, but we may as well emulate it. */
    if (uid_is_valid(&pfont->UID) &&
	!dict_check_uid_param(op, &pfont->UID)
	)
	uid_set_invalid(&pfont->UID);
    return 0;
}

/* Build a FDArray entry for a CIDFontType 0 font. */
/* Note that as of Adobe PostScript version 3011, this may be either */
/* a Type 1 or Type 2 font. */
private int
build_FDArray_sub_font(i_ctx_t *i_ctx_p, ref *op,
		       gs_font_base **ppfont,
		       font_type ftype, gs_memory_type_ptr_t pstype,
		       const build_proc_refs * pbuild,
		       build_font_options_t ignore_options)
{
    gs_font *pfont;
    int code = build_gs_sub_font(i_ctx_p, op, &pfont, ftype, pstype, pbuild,
				 NULL, op);

    if (code >= 0)
	*ppfont = (gs_font_base *)pfont;
    return code;
}
int
build_gs_FDArray_font(i_ctx_t *i_ctx_p, ref *op,
		      gs_font_base **ppfont,
		      font_type ftype, gs_memory_type_ptr_t pstype,
		      const build_proc_refs * pbuild)
{
    gs_font_base *pfont;
    font_data *pdata;
    int code = build_gs_outline_font(i_ctx_p, op, &pfont, ftype, pstype,
				     pbuild, bf_options_none,
				     build_FDArray_sub_font);
    static const double bbox[4] = { 0, 0, 0, 0 };
    gs_uid uid;

    if (code < 0)
	return code;
    pdata = pfont_data(pfont);
    /* Fill in members normally set by build_gs_primitive_font. */
    make_null(&pdata->CharStrings);
    /* Fill in members normally set by build_gs_simple_font. */
    uid_set_invalid(&uid);
    init_gs_simple_font(pfont, bbox, &uid);
    pfont->encoding_index = ENCODING_INDEX_UNKNOWN;
    pfont->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;
    /* Fill in members normally set by build_gs_font. */
    pfont->key_name = pfont->font_name;
    *ppfont = pfont;
    return 0;
}

/* Do the common work for building an outline font -- a non-composite font */
/* for which PaintType and StrokeWidth are meaningful. */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_outline_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base ** ppfont,
		      font_type ftype, gs_memory_type_ptr_t pstype,
		      const build_proc_refs * pbuild,
		      build_font_options_t options,
		      build_base_font_proc_t build_base_font)
{
    int painttype;
    float strokewidth;
    gs_font_base *pfont;
    int code = dict_int_param(op, "PaintType", 0, 3, 0, &painttype);

    if (code < 0)
	return code;
    code = dict_float_param(op, "StrokeWidth", 0.0, &strokewidth);
    if (code < 0)
	return code;
    code = build_base_font(i_ctx_p, op, ppfont, ftype, pstype, pbuild,
			   options);
    if (code != 0)
	return code;
    pfont = *ppfont;
    pfont->PaintType = painttype;
    pfont->StrokeWidth = strokewidth;
    return 0;
}

/* Do the common work for building a font of any non-composite FontType. */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_simple_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base ** ppfont,
		     font_type ftype, gs_memory_type_ptr_t pstype,
		     const build_proc_refs * pbuild,
		     build_font_options_t options)
{
    double bbox[4];
    gs_uid uid;
    int code;
    gs_font_base *pfont;

    code = font_bbox_param(op, bbox);
    if (code < 0)
	return code;
    code = dict_uid_param(op, &uid, 0, imemory, i_ctx_p);
    if (code < 0)
	return code;
    if ((options & bf_UniqueID_ignored) && uid_is_UniqueID(&uid))
	uid_set_invalid(&uid);
    code = build_gs_font(i_ctx_p, op, (gs_font **) ppfont, ftype, pstype,
			 pbuild, options);
    if (code != 0)		/* invalid or scaled font */
	return code;
    pfont = *ppfont;
    pfont->procs.init_fstack = gs_default_init_fstack;
    pfont->procs.define_font = gs_no_define_font;
    pfont->procs.make_font = zbase_make_font;
    pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pfont->FAPI = 0;
    pfont->FAPI_font_data = 0;
    init_gs_simple_font(pfont, bbox, &uid);
    lookup_gs_simple_font_encoding(pfont);
    return 0;
}

/* Initialize the FontBBox and UID of a non-composite font. */
void
init_gs_simple_font(gs_font_base *pfont, const double bbox[4],
		    const gs_uid *puid)
{
    pfont->FontBBox.p.x = bbox[0];
    pfont->FontBBox.p.y = bbox[1];
    pfont->FontBBox.q.x = bbox[2];
    pfont->FontBBox.q.y = bbox[3];
    pfont->UID = *puid;
}

/* Compare the encoding of a simple font with the registered encodings. */
void
lookup_gs_simple_font_encoding(gs_font_base * pfont)
{
    const ref *pfe = &pfont_data(pfont)->Encoding;
    uint esize = r_size(pfe);
    int index = -1;

    pfont->encoding_index = index;
    if (esize <= 256) {
	/* Look for an encoding that's "close". */
	int near_index = -1;
	uint best = esize / 3;	/* must match at least this many */
	gs_const_string fstrs[256];
	int i;

	/* Get the string names of the glyphs in the font's Encoding. */
	for (i = 0; i < esize; ++i) {
	    ref fchar;

	    if (array_get(pfe, (long)i, &fchar) < 0 ||
		!r_has_type(&fchar, t_name)
		)
		fstrs[i].data = 0, fstrs[i].size = 0;
	    else {
		ref nsref;

		name_string_ref(&fchar, &nsref);
		fstrs[i].data = nsref.value.const_bytes;
		fstrs[i].size = r_size(&nsref);
	    }
	}
	/* Compare them against the known encodings. */
	for (index = 0; index < NUM_KNOWN_REAL_ENCODINGS; ++index) {
	    uint match = esize;

	    for (i = esize; --i >= 0;) {
		gs_const_string rstr;

		gs_c_glyph_name(gs_c_known_encode((gs_char)i, index), &rstr);
		if (rstr.size == fstrs[i].size &&
		    !memcmp(rstr.data, fstrs[i].data, rstr.size)
		    )
		    continue;
		if (--match <= best)
		    break;
	    }
	    if (match > best) {
		best = match;
		near_index = index;
		/* If we have a perfect match, stop now. */
		if (best == esize)
		    break;
	    }
	}
	index = near_index;
	if (best == esize)
	    pfont->encoding_index = index;
    }
    pfont->nearest_encoding_index = index;
}

/* Get FontMatrix and FontName parameters. */
private void get_font_name(ref *, const ref *);
private void copy_font_name(gs_font_name *, const ref *);
private int
sub_font_params(const ref *op, gs_matrix *pmat, ref *pfname)
{
    ref *pmatrix;
    ref *pfontname;

    if (dict_find_string(op, "FontMatrix", &pmatrix) <= 0 ||
	read_matrix(pmatrix, pmat) < 0
	)
	return_error(e_invalidfont);
    if (dict_find_string(op, "FontName", &pfontname) > 0)
	get_font_name(pfname, pfontname);
    else
	make_empty_string(pfname, a_readonly);
    return 0;
}

/* Do the common work for building a font of any FontType. */
/* The caller guarantees that *op is a dictionary. */
/* op[-1] must be the key under which the font is being registered */
/* in FontDirectory, normally a name or string. */
/* Return 0 for a new font, 1 for a font made by makefont or scalefont, */
/* or a negative error code. */
int
build_gs_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font ** ppfont, font_type ftype,
	      gs_memory_type_ptr_t pstype, const build_proc_refs * pbuild,
	      build_font_options_t options)
{
    ref kname;			/* t_string */
    ref *pftype;
    ref *pencoding = 0;
    bool bitmapwidths;
    int exactsize, inbetweensize, transformedchar;
    int wmode;
    int code;
    gs_font *pfont;
    ref *pfid;
    ref *aop = dict_access_ref(op);

    get_font_name(&kname, op - 1);
    if (dict_find_string(op, "FontType", &pftype) <= 0 ||
	!r_has_type(pftype, t_integer) ||
	pftype->value.intval != (int)ftype
	)
	return_error(e_invalidfont);
    if (dict_find_string(op, "Encoding", &pencoding) <= 0) {
	if (!(options & bf_Encoding_optional))
	    return_error(e_invalidfont);
    } else {
	if (!r_is_array(pencoding))
	    return_error(e_invalidfont);
    }
    if ((code = dict_int_param(op, "WMode", 0, 1, 0, &wmode)) < 0 ||
	(code = dict_bool_param(op, "BitmapWidths", false, &bitmapwidths)) < 0 ||
	(code = dict_int_param(op, "ExactSize", 0, 2, fbit_use_bitmaps, &exactsize)) < 0 ||
	(code = dict_int_param(op, "InBetweenSize", 0, 2, fbit_use_outlines, &inbetweensize)) < 0 ||
	(code = dict_int_param(op, "TransformedChar", 0, 2, fbit_use_outlines, &transformedchar)) < 0
	)
	return code;
    code = dict_find_string(op, "FID", &pfid);
    if (code > 0) {
	if (!r_has_type(pfid, t_fontID))
	    return_error(e_invalidfont);
	/*
	 * If this font has a FID entry already, it might be a scaled font
	 * made by makefont or scalefont; in a Level 2 environment, it might
	 * be an existing font being registered under a second name, or a
	 * re-encoded font (which was invalid in Level 1, but dvips did it
	 * anyway).
	 */
	pfont = r_ptr(pfid, gs_font);
	if (pfont->base == pfont) {	/* original font */
	    if (!level2_enabled)
		return_error(e_invalidfont);
	    if (obj_eq(pfont_dict(pfont), op)) {
		*ppfont = pfont;
		return 1;
	    }
	    /*
	     * This is a re-encoded font, or some other
	     * questionable situation in which the FID
	     * was preserved.  Pretend the FID wasn't there.
	     */
	} else {		/* This was made by makefont or scalefont. */
	    /* Just insert the new name. */
	    gs_matrix mat;
	    ref fname;			/* t_string */

	    code = sub_font_params(op, &mat, &fname);
	    if (code < 0)
		return code;
	    code = 1;
	    copy_font_name(&pfont->font_name, &fname);
	    goto set_name;
	}
    }
    /* This is a new font. */
    if (!r_has_attr(aop, a_write))
	return_error(e_invalidaccess);
    {
	ref encoding;

	/*
	 * Since add_FID may resize the dictionary and cause
	 * pencoding to become invalid, save the Encoding.
	 */
	if (pencoding) {
	    encoding = *pencoding;
	    pencoding = &encoding;
	}
	code = build_gs_sub_font(i_ctx_p, op, &pfont, ftype, pstype,
				 pbuild, pencoding, op);
	if (code < 0)
	    return code;
    }
    pfont->BitmapWidths = bitmapwidths;
    pfont->ExactSize = (fbit_type)exactsize;
    pfont->InBetweenSize = (fbit_type)inbetweensize;
    pfont->TransformedChar = (fbit_type)transformedchar;
    pfont->WMode = wmode;
    pfont->procs.font_info = zfont_info;
    code = 0;
set_name:
    copy_font_name(&pfont->key_name, &kname);
    *ppfont = pfont;
    return code;
}

/* Create a sub-font -- a font or an entry in the FDArray of a CIDFontType 0 */
/* font.  Default BitmapWidths, ExactSize, InBetweenSize, TransformedChar, */
/* and WMode; do not fill in key_name. */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_sub_font(i_ctx_t *i_ctx_p, const ref *op, gs_font **ppfont,
		  font_type ftype, gs_memory_type_ptr_t pstype,
		  const build_proc_refs * pbuild, const ref *pencoding,
		  ref *fid_op)
{
    gs_matrix mat;
    ref fname;			/* t_string */
    gs_font *pfont;
    font_data *pdata;
    /*
     * Make sure that we allocate the font data
     * in the same VM as the font dictionary.
     */
    uint space = ialloc_space(idmemory);
    int code = sub_font_params(op, &mat, &fname);

    if (code < 0)
	return code;
    ialloc_set_space(idmemory, r_space(op));
    pfont = gs_font_alloc(imemory, pstype, &gs_font_procs_default, NULL,
			  "buildfont(font)");
    pdata = ialloc_struct(font_data, &st_font_data,
			  "buildfont(data)");
    if (pfont == 0 || pdata == 0)
	code = gs_note_error(e_VMerror);
    else if (fid_op)
	code = add_FID(i_ctx_p, fid_op, pfont, iimemory);
    if (code < 0) {
	ifree_object(pdata, "buildfont(data)");
	ifree_object(pfont, "buildfont(font)");
	ialloc_set_space(idmemory, space);
	return code;
    }
    refset_null((ref *) pdata, sizeof(font_data) / sizeof(ref));
    ref_assign_new(&pdata->dict, op);
    ref_assign_new(&pdata->BuildChar, &pbuild->BuildChar);
    ref_assign_new(&pdata->BuildGlyph, &pbuild->BuildGlyph);
    if (pencoding)
	ref_assign_new(&pdata->Encoding, pencoding);
    pfont->client_data = pdata;
    pfont->FontType = ftype;
    pfont->FontMatrix = mat;
    pfont->BitmapWidths = false;
    pfont->ExactSize = fbit_use_bitmaps;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    pfont->WMode = 0;
    pfont->procs.encode_char = zfont_encode_char;
    pfont->procs.glyph_name = zfont_glyph_name;
    ialloc_set_space(idmemory, space);
    copy_font_name(&pfont->font_name, &fname);
    *ppfont = pfont;
    return 0;
}

/* Get the string corresponding to a font name. */
/* If the font name isn't a name or a string, return an empty string. */
private void
get_font_name(ref * pfname, const ref * op)
{
    switch (r_type(op)) {
	case t_string:
	    *pfname = *op;
	    break;
	case t_name:
	    name_string_ref(op, pfname);
	    break;
	default:
	    /* This is weird, but legal.... */
	    make_empty_string(pfname, a_readonly);
    }
}

/* Copy a font name into the gs_font structure. */
private void
copy_font_name(gs_font_name * pfstr, const ref * pfname)
{
    uint size = r_size(pfname);

    if (size > gs_font_name_max)
	size = gs_font_name_max;
    memcpy(&pfstr->chars[0], pfname->value.const_bytes, size);
    /* Following is only for debugging printout. */
    pfstr->chars[size] = 0;
    pfstr->size = size;
}

/* Finish building a font, by calling gs_definefont if needed. */
int
define_gs_font(gs_font * pfont)
{
    return (pfont->base == pfont && pfont->dir == 0 ?
	    /* i.e., unregistered original font */
	    gs_definefont(ifont_dir, pfont) :
	    0);
}
