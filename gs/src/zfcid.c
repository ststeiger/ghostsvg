/* Copyright (C) 1996, 1997, 1998, 1999, 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* CID-keyed font operators */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsmatrix.h"
#include "gsccode.h"
#include "gsstruct.h"
#include "gxfcid.h"
#include "gxfont1.h"
#include "stream.h"		/* for files.h */
#include "bfont.h"
#include "files.h"
#include "icid.h"
#include "idict.h"
#include "idparam.h"
#include "ifont1.h"
#include "ifont2.h"
#include "ifont42.h"
#include "store.h"

/* ---------------- Shared utilities ---------------- */

/* Get the CIDSystemInfo of a CIDFont. */
private int
cid_font_system_info_param(gs_cid_system_info_t *pcidsi, const ref *prfont)
{
    ref *prcidsi;

    if (dict_find_string(prfont, "CIDSystemInfo", &prcidsi) <= 0)
	return_error(e_rangecheck);
    return cid_system_info_param(pcidsi, prcidsi);
}

/* Get the additional information for a CIDFontType 0 or 2 CIDFont. */
private int
cid_font_data_param(os_ptr op, gs_font_cid_data *pdata, ref *pGlyphDirectory)
{
    int code;

    check_type(*op, t_dictionary);
    code = cid_font_system_info_param(&pdata->CIDSystemInfo, op);
    if (code < 0 ||
	(code = dict_int_param(op, "CIDCount", 0, max_int, -1,
			       &pdata->CIDCount)) < 0 ||
	(code = font_GlyphDirectory_param(op, pGlyphDirectory)) < 0
	)
	return code;
    if (r_has_type(pGlyphDirectory, t_null)) {
	/* Standard CIDFont, require GDBytes. */
	return dict_int_param(op, "GDBytes", 1, MAX_GDBytes, 0,
			      &pdata->GDBytes);
    } else {
	pdata->GDBytes = 0;
	return 0;
    }
}

/* ---------------- CIDFontType 0 (FontType 9) ---------------- */

/* ------ Accessing ------ */

/* Parse a multi-byte integer from a string. */
private int
get_index(gs_const_string *pgstr, int count, ulong *pval)
{
    int i;

    if (pgstr->size < count)
	return_error(e_rangecheck);
    *pval = 0;
    for (i = 0; i < count; ++i)
	*pval = (*pval << 8) + pgstr->data[i];
    pgstr->data += count;
    pgstr->size -= count;
    return 0;
}

/* Get bytes from GlyphData or DataSource. */
private int
cid0_read_bytes(gs_font_cid0 *pfont, ulong base, uint count, byte *buf,
		gs_const_string *pstr)
{
    const font_data *pfdata = pfont_data(pfont);
    byte *data = buf;
    int code = 0;

    /* Check for overflow. */
    if (base != (long)base || base > base + count)
	return_error(e_rangecheck);
    if (r_has_type(&pfdata->u.cid0.DataSource, t_null)) {
	/* Get the bytes from GlyphData (a string or array of strings). */
	const ref *pgdata = &pfdata->u.cid0.GlyphData;

	if (r_has_type(pgdata, t_string)) {  /* single string */
	    uint size = r_size(pgdata);

	    if (base >= size || count > size - base)
		return_error(e_rangecheck);
	    data = pgdata->value.bytes + base;
	} else {		/* array of strings */
	    /*
	     * The algorithm is similar to the one in
	     * string_array_access_proc in zfont42.c, but it also has to
	     * deal with the case where the requested string crosses array
	     * elements.
	     */
	    ulong skip = base;
	    uint copied = 0;
	    uint index = 0;
	    ref rstr;
	    uint size;

	    for (;; skip -= size, ++index) {
		int code = array_get(pgdata, index, &rstr);

		if (code < 0)
		    return code;
		if (!r_has_type(&rstr, t_string))
		    return_error(e_typecheck);
		size = r_size(&rstr);
		if (skip < size)
		    break;
		skip -= size;
	    }
	    size -= skip;
	    if (count <= size) {
		data = rstr.value.bytes + skip;
	    } else {		/* multiple strings needed */
		if (data == 0) {  /* no buffer provided */
		    data = gs_alloc_string(pfont->memory, count,
					   "cid0_read_bytes");
		    if (data == 0)
			return_error(e_VMerror);
		    code = 1;
		}
		memcpy(data, rstr.value.bytes + skip, size);
		copied = size;
		while (copied < count) {
		    int code = array_get(pgdata, ++index, &rstr);

		    if (code < 0)
			goto err;
		    if (!r_has_type(&rstr, t_string)) {
			code = gs_note_error(e_typecheck);
			goto err;
		    }
		    size = r_size(&rstr);
		    if (size > count - copied)
			size = count - copied;
		    memcpy(data + copied, rstr.value.bytes, size);
		    copied += size;
		}
	    }
	}
    } else {
	/* Get the bytes from DataSource (a stream). */
	stream *s;
	uint nread;

	check_read_known_file(s, &pfdata->u.cid0.DataSource, return_error);
	if (sseek(s, base) < 0)
	    return_error(e_ioerror);
	if (data == 0) {	/* no buffer provided */
	    data = gs_alloc_string(pfont->memory, count, "cid0_read_bytes");
	    if (data == 0)
		return_error(e_VMerror);
	    code = 1;
	}
	if (sgets(s, data, count, &nread) < 0 || nread != count) {
	    code = gs_note_error(e_ioerror);
	    goto err;
	}
    }
    pstr->data = data;
    pstr->size = count;
    return code;
 err:
    if (data != buf)
	gs_free_string(pfont->memory, data, count, "cid0_read_bytes");
    return code;
}

/* Get the CharString data for a CIDFontType 0 font. */
/* This is the glyph_data procedure in the font itself. */
/* Note that pgstr may be NULL. */
private int
z9_glyph_data(gs_font_base *pbfont, gs_glyph glyph, gs_const_string *pgstr,
	      int *pfidx)
{
    gs_font_cid0 *pfont = (gs_font_cid0 *)pbfont;
    const font_data *pfdata = pfont_data(pfont);
    long glyph_index = (long)(glyph - gs_min_cid_glyph);
    gs_const_string gstr;
    ulong fidx;
    int code;

    if (!r_has_type(&pfdata->u.cid0.GlyphDirectory, t_null)) {
	code = font_gdir_get_outline(&pfdata->u.cid0.GlyphDirectory,
				     glyph_index, &gstr);
	if (code < 0)
	    return code;
	/* Get the definition from GlyphDirectory. */
	if (gstr.data) {
	    code = get_index(&gstr, pfont->cidata.FDBytes, &fidx);
	    if (code < 0)
		return code;
	    if (fidx >= pfont->cidata.FDArray_size)
		return_error(e_rangecheck);
	    if (pgstr)
		*pgstr = gstr;
	    *pfidx = (int)fidx;
	    return code;
	}
    }
    /* Get the definition from the binary data (GlyphData or DataSource). */
    if (glyph_index < 0 || glyph_index >= pfont->cidata.common.CIDCount) {
	*pfidx = 0;
	if (pgstr)
	    pgstr->data = 0, pgstr->size = 0;
	return 0;
    }
    {
	byte fd_gd[(MAX_FDBytes + MAX_GDBytes) * 2];
	uint num_bytes = pfont->cidata.FDBytes + pfont->cidata.common.GDBytes;
	ulong base = pfont->cidata.CIDMapOffset + glyph_index * num_bytes;
	ulong gidx, gidx_next;

	code = cid0_read_bytes(pfont, base, (ulong)(num_bytes * 2), fd_gd,
			       &gstr);
	if (code < 0)
	    return code;
	get_index(&gstr, pfont->cidata.FDBytes, &fidx);
	get_index(&gstr, pfont->cidata.common.GDBytes, &gidx);
	if (fidx >= pfont->cidata.FDArray_size)
	    return_error(e_rangecheck);
	*pfidx = (int)fidx;
	if (pgstr == 0)
	    return 0;
	gstr.data += pfont->cidata.FDBytes;
	gstr.size -= pfont->cidata.FDBytes;
	get_index(&gstr, pfont->cidata.common.GDBytes, &gidx_next);
	return cid0_read_bytes(pfont, gidx, gidx_next - gidx, NULL, pgstr);
    }
}

/*
 * The "fonts" in the FDArray don't have access to their outlines -- the
 * outlines are always provided externally.  Replace the accessor procedures
 * with ones that will give an error if called.
 */
private int
z9_FDArray_glyph_data(gs_font_type1 * pfont, gs_glyph glyph,
		      gs_const_string * pgdata)
{
    return_error(e_invalidfont);
}
private int
z9_FDArray_seac_data(gs_font_type1 *pfont, int ccode, gs_glyph *pglyph,
		     gs_const_string *pstr)
{
    return_error(e_invalidfont);
}

/* ------ Defining ------ */

/* Get one element of a FDArray. */
private int
fd_array_element(i_ctx_t *i_ctx_p, gs_font_type1 **ppfont, ref *prfd)
{
    charstring_font_refs_t refs;
    gs_type1_data data1;
    build_proc_refs build;
    gs_font_base *pbfont;
    gs_font_type1 *pfont;
    /*
     * Standard CIDFontType 0 fonts have Type 1 fonts in the FDArray, but
     * CFF CIDFontType 0 fonts have Type 2 fonts there.
     */
    int fonttype = 1;		/* default */
    int code = charstring_font_get_refs(prfd, &refs);

    if (code < 0 ||
	(code = dict_int_param(prfd, "FontType", 1, 2, 1, &fonttype)) < 0
	)
	return code;
    /*
     * We don't handle the alternate Subr representation (SubrCount,
     * SDBytes, SubrMapOffset) here: currently that is handled in
     * PostScript code (lib/gs_cidfn.ps).
     */
    switch (fonttype) {
    case 1:
	data1.interpret = gs_type1_interpret;
	data1.subroutineNumberBias = 0;
	data1.lenIV = DEFAULT_LENIV_1;
	code = charstring_font_params(prfd, &refs, &data1);
	if (code < 0)
	    return code;
	code = build_proc_name_refs(&build,
				    "%Type1BuildChar", "%Type1BuildGlyph");
	break;
    case 2:
	code = type2_font_params(prfd, &refs, &data1);
	if (code < 0)
	    return code;
	code = charstring_font_params(prfd, &refs, &data1);
	if (code < 0)
	    return code;
	code = build_proc_name_refs(&build,
				    "%Type2BuildChar", "%Type2BuildGlyph");
	break;
    default:			/* can't happen */
	return_error(e_Fatal);
    }
    if (code < 0)
	return code;
    code = build_gs_FDArray_font(i_ctx_p, prfd, &pbfont, fonttype,
				 &st_gs_font_type1, &build);
    if (code < 0)
	return code;
    pfont = (gs_font_type1 *)pbfont;
    charstring_font_init(pfont, &refs, &data1);
    pfont->data.procs.glyph_data = z9_FDArray_glyph_data;
    pfont->data.procs.seac_data = z9_FDArray_seac_data;
    *ppfont = pfont;
    return 0;
}

/* <string|name> <font_dict> .buildfont9 <string|name> <font> */
gs_private_st_ptr(st_gs_font_type1_ptr, gs_font_type1 *, "gs_font_type1 *",
  font1_ptr_enum_ptrs, font1_ptr_reloc_ptrs);
gs_private_st_element(st_gs_font_type1_ptr_element, gs_font_type1 *,
  "gs_font_type1 *[]", font1_ptr_element_enum_ptrs,
  font1_ptr_element_reloc_ptrs, st_gs_font_type1_ptr);
private int
zbuildfont9(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    build_proc_refs build;
    int code = build_proc_name_refs(&build, NULL, "%Type9BuildGlyph");
    gs_font_cid_data common;
    uint CIDMapOffset;
    ref *prfda;
    gs_font_type1 **FDArray;
    uint FDArray_size;
    int FDBytes;
    gs_font_base *pfont;
    gs_font_cid0 *pfcid;
    ref GlyphDirectory, GlyphData, DataSource;
    ref *pGlyphData;
    uint i;

    /*
     * If the CIDFont's data have been loaded into VM, GlyphData will be
     * a string or an array of strings; if they are loaded incrementally
     * from a file, GlyphData will be an integer, and DataSource will be
     * a (reusable) stream.
     */
    if (code < 0 ||
	(code = cid_font_data_param(op, &common, &GlyphDirectory)) < 0 ||
	(code = dict_find_string(op, "GlyphData", &pGlyphData)) < 0 ||
	(code = dict_find_string(op, "FDArray", &prfda)) < 0 ||
	(code = dict_int_param(op, "FDBytes", 0, MAX_FDBytes, -1, &FDBytes)) < 0
	)
	return code;
    GlyphData = *pGlyphData;
    if (r_has_type(&GlyphData, t_integer)) {
	ref *pds;
	stream *discard;

	if ((code = dict_find_string(op, "DataSource", &pds)) < 0)
	    return code;
	check_read_file(discard, pds);
	DataSource = *pds;
    } else {
	if (!r_has_type(&GlyphData, t_string) && !r_is_array(&GlyphData))
	    return_error(e_typecheck);
	make_null(&DataSource);
    }
    if (!r_is_array(prfda))
	return_error(e_invalidfont);
    FDArray_size = r_size(prfda);
    if (FDArray_size == 0)
	return_error(e_invalidfont);
    if (r_has_type(&GlyphDirectory, t_null)) {
	/* Standard CIDFont, require CIDMapOffset. */
	if ((code = dict_uint_param(op, "CIDMapOffset", 0, max_uint - 1,
				    max_uint, &CIDMapOffset)) < 0)
	    return code;
    } else {
	CIDMapOffset = 0;
    }
    FDArray = ialloc_struct_array(FDArray_size, gs_font_type1 *,
				  &st_gs_font_type1_ptr_element,
				  "buildfont9(FDarray)");
    if (FDArray == 0)
	return_error(e_VMerror);
    memset(FDArray, 0, sizeof(gs_font_type1 *) * FDArray_size);
    for (i = 0; i < FDArray_size; ++i) {
	ref rfd;

	array_get(prfda, (long)i, &rfd);
	code = fd_array_element(i_ctx_p, &FDArray[i], &rfd);
	if (code < 0)
	    goto fail;
    }
    code = build_gs_simple_font(i_ctx_p, op, &pfont, ft_CID_encrypted,
				&st_gs_font_cid0, &build,
				bf_Encoding_optional |
				bf_FontBBox_required |
				bf_UniqueID_ignored);
    if (code < 0)
	goto fail;
    pfcid = (gs_font_cid0 *)pfont;
    pfcid->cidata.common = common;
    pfcid->cidata.CIDMapOffset = CIDMapOffset;
    pfcid->cidata.FDArray = FDArray;
    pfcid->cidata.FDArray_size = FDArray_size;
    pfcid->cidata.FDBytes = FDBytes;
    pfcid->cidata.glyph_data = z9_glyph_data;
    pfcid->cidata.proc_data = 0;	/* for GC */
    ref_assign(&pfont_data(pfont)->u.cid0.GlyphDirectory, &GlyphDirectory);
    ref_assign(&pfont_data(pfont)->u.cid0.GlyphData, &GlyphData);
    ref_assign(&pfont_data(pfont)->u.cid0.DataSource, &DataSource);
    code = define_gs_font((gs_font *)pfont);
    if (code >= 0)
	return code;
 fail:
    ifree_object(FDArray, "buildfont9(FDarray)");
    return code;
}

/* <cid9font> <cid> .type9mapcid <charstring> <font_index> */
private int
ztype9mapcid(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font *pfont;
    gs_font_cid0 *pfcid;
    int code = font_param(op - 1, &pfont);
    gs_const_string gstr;
    int fidx;

    if (code < 0)
	return code;
    if (pfont->FontType != ft_CID_encrypted)
	return_error(e_invalidfont);
    check_type(*op, t_integer);
    pfcid = (gs_font_cid0 *)pfont;
    code = pfcid->cidata.glyph_data((gs_font_base *)pfcid,
			(gs_glyph)(gs_min_cid_glyph + op->value.intval),
				    &gstr, &fidx);
    if (code < 0)
	return code;
    make_const_string(op - 1, a_readonly, gstr.size, gstr.data);
    make_int(op, fidx);
    return 0;
}

#ifdef DEBUG

#include "gdevpsf.h"

/* <file> <cid9font> .writefont9 - */
private int
zwritefont9(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font *pfont;
    gs_font_cid0 *pfcid;
    int code = font_param(op, &pfont);
    stream *s;

    if (code < 0)
	return code;
    if (pfont->FontType != ft_CID_encrypted)
	return_error(e_invalidfont);
    check_write_file(s, op - 1);
    pfcid = (gs_font_cid0 *)pfont;
    code = psf_write_cid0_font(s, pfcid,
			       WRITE_TYPE2_NO_LENIV | WRITE_TYPE2_CHARSTRINGS,
			       NULL, 0, NULL);
    if (code >= 0)
	pop(2);
    return code;
}

#endif

/* ---------------- CIDFontType 1 (FontType 10) ---------------- */

/* <string|name> <font_dict> .buildfont10 <string|name> <font> */
private int
zbuildfont10(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    build_proc_refs build;
    int code = build_gs_font_procs(op, &build);
    gs_cid_system_info_t cidsi;
    gs_font_base *pfont;

    if (code < 0)
	return code;
    code = cid_font_system_info_param(&cidsi, op);
    if (code < 0)
	return code;
    make_null(&build.BuildChar);	/* only BuildGlyph */
    code = build_gs_simple_font(i_ctx_p, op, &pfont, ft_CID_user_defined,
				&st_gs_font_cid1, &build,
				bf_Encoding_optional |
				bf_FontBBox_required |
				bf_UniqueID_ignored);
    if (code < 0)
	return code;
    ((gs_font_cid1 *)pfont)->cidata.CIDSystemInfo = cidsi;
    return define_gs_font((gs_font *)pfont);
}

/* ---------------- CIDFontType 2 (FontType 11) ---------------- */

/* ------ Accessing ------ */

/* Map a glyph CID to a TrueType glyph number using the CIDMap. */
private int
z11_CIDMap_proc(gs_font_cid2 *pfont, gs_glyph glyph)
{
    const ref *pcidmap = &pfont_data(pfont)->u.type42.CIDMap;
    ulong cid = glyph - gs_min_cid_glyph;
    int gdbytes = pfont->cidata.common.GDBytes;
    int gnum = 0;
    const byte *data;
    int i, code;
    ref rcid;
    ref *prgnum;

    switch (r_type(pcidmap)) {
    case t_string:
	if (cid >= r_size(pcidmap) / gdbytes)
	    return_error(e_rangecheck);
	data = pcidmap->value.const_bytes + cid * gdbytes;
	break;
    case t_integer:
	return cid + pcidmap->value.intval;
    case t_dictionary:
	make_int(&rcid, cid);
	code = dict_find(pcidmap, &rcid, &prgnum);
	if (code <= 0)
	    return (code < 0 ? code : gs_note_error(e_undefined));
	if (!r_has_type(prgnum, t_integer))
	    return_error(e_typecheck);
	return prgnum->value.intval;
    default:			/* array type */
	code = string_array_access_proc(pcidmap, 1, cid * gdbytes,
					gdbytes, &data);

	if (code < 0)
	    return code;
    }
    for (i = 0; i < gdbytes; ++i)
	gnum = (gnum << 8) + data[i];
    return gnum;
}

/* Handle MetricsCount when accessing outline or metrics information. */
private int
z11_get_outline(gs_font_type42 * pfont, uint glyph_index,
		gs_const_string * pgstr)
{
    gs_font_cid2 *const pfcid = (gs_font_cid2 *)pfont;
    int skip = pfcid->cidata.MetricsCount << 1;
    int code = pfcid->cidata.orig_procs.get_outline(pfont, glyph_index, pgstr);

    if (code >= 0) {
	byte *data = (byte *)pgstr->data;  /* break const */
	uint size = pgstr->size;

	if (size <= skip) {
	    if (code > 0 && size != 0)
		gs_free_string(pfont->memory, data, size, "z11_get_outline");
	    pgstr->data = 0, pgstr->size = 0;
	} else {
	    if (code > 0) {
		/* Newly allocated, freeble string. */
		memmove(data, data + skip, size - skip);
		pgstr->data = gs_resize_string(pfont->memory, data,
					       size, size - skip,
					       "z11_get_outline");
	    } else {
		pgstr->data += skip;
	    }
	    pgstr->size = size - skip;
	}
    }
    return code;
}
private int
z11_get_metrics(gs_font_type42 * pfont, uint glyph_index, int wmode,
		float sbw[4])
{
    gs_font_cid2 *const pfcid = (gs_font_cid2 *)pfont;
    int skip = pfcid->cidata.MetricsCount << 1;
    gs_const_string gstr;
    const byte *pmetrics;
    int lsb, width;

    if (wmode > skip >> 2 ||
	pfcid->cidata.orig_procs.get_outline(pfont, glyph_index, &gstr) < 0 ||
	gstr.size < skip
	)
	return pfcid->cidata.orig_procs.get_metrics(pfont, glyph_index, wmode,
						    sbw);
    pmetrics = gstr.data + skip - (wmode << 2);
    lsb = (pmetrics[2] << 8) + pmetrics[3];
    width = (pmetrics[0] << 8) + pmetrics[1];
    {
	double factor = 1.0 / pfont->data.unitsPerEm;

	if (wmode) {
	    sbw[0] = 0, sbw[1] = -lsb * factor;
	    sbw[2] = 0, sbw[3] = -width * factor;
	} else {
	    sbw[0] = lsb * factor, sbw[1] = 0;
	    sbw[2] = width * factor, sbw[3] = 0;
	}
    }
    return 0;
}

/* ------ Defining ------ */

/* <string|name> <font_dict> .buildfont11 <string|name> <font> */
private int
zbuildfont11(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font_cid_data common;
    gs_font_type42 *pfont;
    gs_font_cid2 *pfcid;
    int MetricsCount;
    ref rcidmap, ignore_gdir;
    int code = cid_font_data_param(op, &common, &ignore_gdir);

    if (code < 0 ||
	(code = dict_int_param(op, "MetricsCount", 0, 4, 0, &MetricsCount)) < 0
	)
	return code;
    if (MetricsCount & 1)	/* only allowable values are 0, 2, 4 */
	return_error(e_rangecheck);
    code = font_string_array_param(op, "CIDMap", &rcidmap);
    switch (code) {
    case 0:
	break;
    default:
	return code;
    case e_typecheck:
	switch (r_type(&rcidmap)) {
	case t_string:		/* in PLRM3 */
	case t_dictionary:	/* added in 3011 */
	case t_integer:		/* added in 3011 */
	    break;
	default:
	    return code;
	}
	break;
    }
    code = build_gs_TrueType_font(i_ctx_p, op, &pfont, ft_CID_TrueType,
				  &st_gs_font_cid2,
				  (const char *)0, "%Type11BuildGlyph",
				  bf_Encoding_optional |
				  bf_FontBBox_required |
				  bf_UniqueID_ignored |
				  bf_CharStrings_optional);
    if (code < 0)
	return code;
    pfcid = (gs_font_cid2 *)pfont;
    pfcid->cidata.common = common;
    pfcid->cidata.MetricsCount = MetricsCount;
    ref_assign(&pfont_data(pfont)->u.type42.CIDMap, &rcidmap);
    pfcid->cidata.CIDMap_proc = z11_CIDMap_proc;
    if (MetricsCount) {
	/* "Wrap" the glyph accessor procedures. */
	pfcid->cidata.orig_procs.get_outline = pfont->data.get_outline;
	pfont->data.get_outline = z11_get_outline;
	pfcid->cidata.orig_procs.get_metrics = pfont->data.get_metrics;
	pfont->data.get_metrics = z11_get_metrics;
    }
    return define_gs_font((gs_font *)pfont);
}

/* <cid11font> <cid> .type11mapcid <glyph_index> */
private int
ztype11mapcid(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font *pfont;
    int code = font_param(op - 1, &pfont);

    if (code < 0)
	return code;
    if (pfont->FontType != ft_CID_TrueType)
	return_error(e_invalidfont);
    check_type(*op, t_integer);
    code = z11_CIDMap_proc((gs_font_cid2 *)pfont,
			   (gs_glyph)(gs_min_cid_glyph + op->value.intval));
    if (code < 0)
	return code;
    make_int(op - 1, code);
    pop(1);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zfcid_op_defs[] =
{
    {"2.buildfont9", zbuildfont9},
    {"2.type9mapcid", ztype9mapcid},
#ifdef DEBUG
    {"2.writefont9", zwritefont9},
#endif
    {"2.buildfont10", zbuildfont10},
    {"2.buildfont11", zbuildfont11},
    {"2.type11mapcid", ztype11mapcid},
    op_def_end(0)
};
