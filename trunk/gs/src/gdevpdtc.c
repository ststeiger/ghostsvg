/* Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
  
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
/* Composite and CID-based text processing for pdfwrite. */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfcmap.h"
#include "gxfont.h"
#include "gxfont0.h"
#include "gxfont0c.h"
#include "gdevpdfx.h"
#include "gdevpdtx.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdts.h"
#include "gdevpdtt.h"

/* ---------------- Non-CMap-based composite font ---------------- */

/*
 * Process a text string in a composite font with FMapType != 9 (CMap).
 */
int
process_composite_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		       uint size)
{
    byte *const buf = vbuf;
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    int index = 0, code = 0;
    gs_string str;
    pdf_text_process_state_t text_state;
    pdf_text_enum_t curr, prev;
    gs_point total_width;

    str.data = buf;
    if (pte->text.operation &
	(TEXT_FROM_ANY - (TEXT_FROM_STRING | TEXT_FROM_BYTES))
	)
	return_error(gs_error_rangecheck);
    if (pte->text.operation & TEXT_INTERVENE) {
	/* Not implemented. (PostScript doesn't even allow this case.) */
	return_error(gs_error_rangecheck);
    }
    total_width.x = total_width.y = 0;
    curr = *penum;
    prev = curr;
    prev.current_font = 0;
    /* Scan runs of characters in the same leaf font. */
    for ( ; ; ) {
	int font_code, buf_index;
	gs_font *new_font;

	for (buf_index = 0; ; ++buf_index) {
	    gs_char chr;
	    gs_glyph glyph;

	    gs_text_enum_copy_dynamic((gs_text_enum_t *)&prev,
				      (gs_text_enum_t *)&curr, false);
	    font_code = pte->orig_font->procs.next_char_glyph
		((gs_text_enum_t *)&curr, &chr, &glyph);
	    /*
	     * We check for a font change by comparing the current
	     * font, rather than testing the return code, because
	     * it makes the control structure a little simpler.
	     */
	    switch (font_code) {
	    case 0:		/* no font change */
	    case 1:		/* font change */
		new_font = curr.fstack.items[curr.fstack.depth].font;
		if (new_font != prev.current_font)
		    break;
		if (chr != (byte)chr)	/* probably can't happen */
		    return_error(gs_error_rangecheck);
		buf[buf_index] = (byte)chr;
		continue;
	    case 2:		/* end of string */
		break;
	    default:	/* error */
		return font_code;
	    }
	    break;
	}
	str.size = buf_index;
	gs_text_enum_copy_dynamic((gs_text_enum_t *)&curr,
				  (gs_text_enum_t *)&prev, false);
	if (buf_index) {
	    /* buf_index == 0 is only possible the very first time. */
	    /*
	     * The FontMatrix of leaf descendant fonts is not updated
	     * by scalefont.  Compute the effective FontMatrix now.
	     */
	    const gs_matrix *psmat =
		&curr.fstack.items[curr.fstack.depth - 1].font->FontMatrix;
	    gs_matrix fmat;

	    gs_matrix_multiply(&curr.current_font->FontMatrix, psmat,
			       &fmat);
	    code = pdf_encode_process_string(&curr, &str, &fmat, &text_state,
					     &index);
	    if (code < 0)
		return code;
	    gs_text_enum_copy_dynamic(pte, (gs_text_enum_t *)&curr, true);
	    if (pte->text.operation & TEXT_RETURN_WIDTH) {
		pte->returned.total_width.x = total_width.x +=
		    curr.returned.total_width.x;
		pte->returned.total_width.y = total_width.y +=
		    curr.returned.total_width.y;
	    }
	}
	if (font_code == 2)
	    break;
	/*
	 * Some compilers may complain that new_font might not be
	 * initialized at this point.  However, control can only reach
	 * here if the for (buf_index) loop terminated with font_code =
	 * 0 or 1, in which case new_font is definitely initialized.
	 */
	curr.current_font = new_font;
    }
    return code;
}

/* ---------------- CMap-based composite font ---------------- */

/*
 * Process a text string in a composite font with FMapType == 9 (CMap).
 */
private const char *const standard_cmap_names[] = {
    /* The following were added in PDF 1.4. */
    "GBKp-EUC-H", "GBKp-EUC-V",
    "GBK2K-H", "GBK2K-V",
    "HKscs-B5-H", "HKscs-B5-V",
#define END_PDF14_CMAP_NAMES_INDEX 6

    "Identity-H", "Identity-V",

    "GB-EUC-H", "GB-EUC-V",
    "GBpc-EUC-H", "GBpc-EUC-V",
    "GBK-EUC-H", "GBK-EUC-V",
    "UniGB-UCS2-H", "UniGB-UCS2-V",

    "B5pc-H", "B5pc-V",
    "ETen-B5-H", "ETen-B5-V",
    "ETenms-B5-H", "ETenms-B5-V",
    "CNS-EUC-H", "CNS-EUC-V",
    "UniCNS-UCS2-H", "UniCNS-UCS2-V",

    "83pv-RKSJ-H",
    "90ms-RKSJ-H", "90ms-RKSJ-V",
    "90msp-RKSJ-H", "90msp-RKSJ-V",
    "90pv-RKSJ-H",
    "Add-RKSJ-H", "Add-RKSJ-V",
    "EUC-H", "EUC-V",
    "Ext-RKSJ-H", "Ext-RKSJ-V",
    "H", "V",
    "UniJIS-UCS2-H", "UniJIS-UCS2-V",
    "UniJIS-UCS2-HW-H", "UniJIS-UCS2-HW-V",

    "KSC-EUC-H", "KSC-EUC-V",
    "KSCms-UHC-H", "KSCms-UHC-V",
    "KSCms-UHC-HW-H", "KSCms-UHC-HW-V",
    "KSCpc-EUC-H",
    "UniKS-UCS2-H", "UniKS-UCS2-V",

    0
};

/* Record widths and CID => GID mappings. */
private int
scan_cmap_text(gs_text_enum_t *pte, gs_font_type0 *font /*fmap_CMap*/,
	       pdf_font_resource_t *pdsubf /*CIDFont*/,
	       gs_font_base *subfont /*CIDFont*/, gs_point *pwxy)
{
    pdf_font_descriptor_t *pfd = pdsubf->FontDescriptor;
    gs_text_enum_t scan = *pte;
    double w = 0;

    for ( ; ; ) {
	gs_char chr;
	gs_glyph glyph;
	int code = font->procs.next_char_glyph(&scan, &chr, &glyph);

	if (code == 2)		/* end of string */
	    break;
	if (code < 0)
	    return code;
	if (glyph >= GS_MIN_CID_GLYPH) {
	    uint cid = glyph - GS_MIN_CID_GLYPH;

	    code = pdf_font_used_glyph(pfd, glyph, subfont);
	    if (code < 0)
		return code;
	    if (code == 0 /* just copied */ || pdsubf->Widths[cid] == 0) {
		pdf_glyph_widths_t widths;

		code = pdf_glyph_widths(pdsubf, glyph, subfont, &widths);
		if (code < 0)
		    return code;
		if (code == 0) { /* OK to cache */
		    pdsubf->Widths[cid] = widths.Width;
		    pdsubf->real_widths[cid] = widths.real_width;
		}
		w += widths.real_width;
	    } else
		w += pdsubf->real_widths[cid];
	    if (cid < pdsubf->count)
		pdsubf->used[cid >> 3] |= 0x80 >> (cid & 7);
	}
    }
    if (font->WMode)
	pwxy->x = 0, pwxy->y = w;
    else
	pwxy->x = w, pwxy->y = 0;
    return 0;
}

private int
process_cmap_text_common(gs_text_enum_t *pte, const void *vdata, void *vbuf,
			 uint size,
			 pdf_font_resource_t *pdfont, /* Type 0, fmap_CMap */
			 pdf_font_resource_t *pdsubf /* CIDFont */)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)pte->dev;
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    gs_font *font = pte->current_font; /* Type 0, fmap_CMap */
    gs_font_type0 *const pfont = (gs_font_type0 *)font;
    gs_font *subfont = pfont->data.FDepVector[0];
    const gs_cmap_t *pcmap = pfont->data.CMap;
    const char *const *pcmn =
	standard_cmap_names +
	(pdev->CompatibilityLevel < 1.4 ? END_PDF14_CMAP_NAMES_INDEX : 0);
    pdf_resource_t *pcmres = 0;	/* CMap */
    pdf_text_process_state_t text_state;
    gs_point wxy;
    int code;

    if (pte->text.operation &
	(TEXT_FROM_ANY - (TEXT_FROM_STRING | TEXT_FROM_BYTES))
	)
	return_error(gs_error_rangecheck);
    if (pte->text.operation & TEXT_INTERVENE) {
	/* Not implemented.  (PostScript doesn't allow TEXT_INTERVENE.) */
	return_error(gs_error_rangecheck);
    }
    /* Require a single CID-keyed DescendantFont. */
    if (pfont->data.fdep_size != 1)
	return_error(gs_error_rangecheck);
    switch (subfont->FontType) {
    case ft_CID_encrypted:
    case ft_CID_TrueType:
	break;
    default:
	return_error(gs_error_rangecheck);
    }

    /*
     * If the CMap isn't standard, write it out if necessary.
     */

    for (; *pcmn != 0; ++pcmn)
	if (pcmap->CMapName.size == strlen(*pcmn) &&
	    !memcmp(*pcmn, pcmap->CMapName.data, pcmap->CMapName.size))
	    break;
    if (*pcmn == 0) {		/* not standard */
	pcmres = pdf_find_resource_by_gs_id(pdev, resourceCMap, pcmap->id);
	if (pcmres == 0) {
	    /* Create and write the CMap object. */
	    code = pdf_cmap_alloc(pdev, pcmap, &pcmres);
	    if (code < 0)
		return code;
	}
    }

    code = pdf_update_text_state(&text_state, penum, pdfont,
				 &font->FontMatrix);
    if (code < 0)
	return code;
    if (code > 0)		/* can't emulate ADD_TO_*_WIDTH */
	return_error(gs_error_rangecheck);
    /******
	   PATCH: We don't implement ADD_TO_*_WIDTH at all, because
	   it's too much trouble to calculate the width to pass to
	   pdf_append_chars.
    ******/
    if (((text_state.members & TEXT_ADD_TO_ALL_WIDTHS) &&
	 text_state.values.character_spacing != 0) ||
	((text_state.members & TEXT_ADD_TO_SPACE_WIDTH) &&
	 text_state.values.word_spacing != 0)
	)
	return_error(gs_error_rangecheck);
	    
    if (pcmap->from_Unicode) {
	gs_cmap_ranges_enum_t renum;

	gs_cmap_ranges_enum_init(pcmap, &renum);
	if (gs_cmap_enum_next_range(&renum) == 0 && renum.range.size == 2 &&
	    gs_cmap_enum_next_range(&renum) == 1) {
	    /*
	     * Exactly one code space range, of size 2.  Add an identity
	     * ToUnicode CMap.
	     */
	    if (!pdev->Identity_ToUnicode_CMaps[pcmap->WMode]) {
		/* Create and write an identity ToUnicode CMap now. */
		gs_cmap_t *pidcmap;

		code = gs_cmap_create_char_identity(&pidcmap, 2, pcmap->WMode,
						    pdev->memory);
		if (code < 0)
		    return code;
		pidcmap->CMapType = 2;	/* per PDF Reference */
		code = pdf_cmap_alloc(pdev, pidcmap,
				&pdev->Identity_ToUnicode_CMaps[pcmap->WMode]);
		if (code < 0)
		    return code;
	    }
	    pdfont->ToUnicode = pdev->Identity_ToUnicode_CMaps[pcmap->WMode];
	}
    }
    if (pcmres) {
	uint size = pcmap->CMapName.size;
	byte *chars = gs_alloc_string(pdev->pdf_memory, size,
				      "pdf_font_resource_t(CMapName)");

	if (chars == 0)
	    return_error(gs_error_VMerror);
	sprintf(pdfont->u.type0.Encoding_name, "%ld 0 R",
		pdf_resource_id(pcmres));
	pdfont->u.type0.CMapName.data = chars;
	pdfont->u.type0.CMapName.size = size;
    } else {
	sprintf(pdfont->u.type0.Encoding_name, "/%s", *pcmn);
	pdfont->u.type0.CMapName.data = (const byte *)*pcmn;
	pdfont->u.type0.CMapName.size = strlen(*pcmn);
	pdfont->u.type0.cmap_is_standard = true;
    }
    pdfont->u.type0.WMode = pcmap->WMode;

    code = scan_cmap_text(pte, pfont, pdsubf, (gs_font_base *)subfont, &wxy);
    if (code < 0)
	return code;

    /*
     * Let the default implementation do the rest of the work, but don't
     * allow it to draw anything in the output; instead, append the
     * characters to the output now.
     */

    {
	gs_text_params_t text;
	gs_text_enum_t *pte_default;
	gs_const_string str;
	int acode;
	gs_point dist;

	text = pte->text;
	if (text.operation & TEXT_DO_DRAW)
	    text.operation =
		(text.operation - TEXT_DO_DRAW) | TEXT_DO_CHARWIDTH;
	code = pdf_default_text_begin(pte, &text, &pte_default);
	if (code < 0)
	    return code;
	str.data = vdata;
	str.size = size;
	if ((acode = pdf_set_text_process_state(pdev, pte, &text_state, &str)) < 0 ||
	    (acode = pdf_text_distance_transform(wxy.x, wxy.y,
						 pdev->text->text_state,
						 &dist)) < 0 ||
	    (acode = pdf_append_chars(pdev, str.data, str.size, dist.x, dist.y)) < 0
	    ) {
	    gs_text_release(pte_default, "process_cmap_text");
	    return acode;
	}
	/* Let the caller (pdf_text_process) handle pte_default. */
	gs_text_enum_copy_dynamic(pte_default, pte, false);
	penum->pte_default = pte_default;
	return code;
    }	
}
int
process_cmap_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		  uint size)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)pte->dev;
    gs_font *font = pte->current_font; /* Type 0, fmap_CMap */
    gs_font_type0 *const pfont = (gs_font_type0 *)font;
    gs_font *subfont = pfont->data.FDepVector[0];
    pdf_font_resource_t *pdfont; /* Type 0, fmap_CMap */
    pdf_font_resource_t *pdsubf; /* CIDFont */
    int code;

    /* Create pdf font resources for the composite font and the CIDFont. */
    /****** SOME OF THIS IS REDUNDANT -- SEE pdf_make_font_resource ******/

    code = pdf_make_font_resource(pdev, subfont, &pdsubf);
    if (code < 0)
	return code;
    code = pdf_make_font_resource(pdev, font, &pdfont);
    if (code < 0)
	return code;
    pdfont->u.type0.DescendantFont = pdsubf;
    /* Copy the subfont name to the composite font. */
    /****** FIXME ******
	pdf_put_name(pdev, chars, size);
	if (pef->sub_font_type == ft_CID_encrypted &&
	    pef->cmapname[0] == '/'
	    ) {
	    stream_putc(s, '-');
	    pdf_put_name_chars(pdev, (const byte*) (pef->cmapname + 1),
			       strlen(pef->cmapname + 1));
	}
     ****** FIXME ******/
    return process_cmap_text_common(pte, vdata, vbuf, size, pdfont, pdsubf);
}

/* ---------------- CIDFont ---------------- */

/*
 * Process a text string in a CIDFont.  (Only glyphshow is supported.)
 */
int
process_cid_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		 uint size)
{
    uint operation = pte->text.operation;
    gs_text_enum_t save;
    gs_font *scaled_font = pte->current_font; /* CIDFont */
    gs_font *font;		/* unscaled font (CIDFont) */
    gx_device_pdf *pdev = (gx_device_pdf *)pte->dev;
    const gs_glyph *glyphs = (const gs_glyph *)vdata;
    gs_matrix scale_matrix;
    pdf_font_resource_t *pdfont; /* CIDFont */
    gs_font_type0 *font0;
    pdf_font_resource_t *pdgsf; /* Type 0 */
    int code;

    if (!(operation & (TEXT_FROM_GLYPHS | TEXT_FROM_SINGLE_GLYPH)))
	return_error(gs_error_rangecheck);

    /*
     * PDF doesn't support glyphshow directly: we need to create a Type 0
     * font with an Identity CMap.  Make sure all the glyph numbers fit
     * into 16 bits.  (Eventually we should support wider glyphs too.)
     */
    {
	int i;
	byte *pchars = vbuf;

	for (i = 0; i * sizeof(gs_glyph) < size; ++i) {
	    ulong gnum = glyphs[i] - GS_MIN_CID_GLYPH;

	    if (gnum & ~0xffffL)
		return_error(gs_error_rangecheck);
	    *pchars++ = (byte)(gnum >> 8);
	    *pchars++ = (byte)gnum;
	}
    }

    /* Find the original (unscaled) version of this font. */

    for (font = scaled_font; font->base != font; )
	font = font->base;
    /* Compute the scaling matrix. */
    gs_matrix_invert(&font->FontMatrix, &scale_matrix);
    gs_matrix_multiply(&scale_matrix, &scaled_font->FontMatrix, &scale_matrix);

    /* Find or create the CIDFont resource. */

    code = pdf_make_font_resource(pdev, font, &pdfont);
    if (code < 0)
	return code;

    /* Create the CMap and Type 0 font if they don't exist already. */

    font0 = pdfont->u.cidfont.glyphshow_font;
    if (font0 != 0) {
	/* We could store this instead of looking it up.... */
	pdgsf = (pdf_font_resource_t *)
	    pdf_find_resource_by_gs_id(pdev, resourceFont, font0->id);
	if (pdgsf == 0 || pdgsf->FontType != ft_composite)
	    return_error(gs_error_Fatal);
    } else {
	code = gs_font_type0_from_cidfont(&font0, font, font->WMode,
					  &scale_matrix, pdev->pdf_memory);
	if (code < 0)
	    return code;
	code = pdf_font_type0_alloc(pdev, &pdgsf, font0->id, pdfont);
	if (code < 0)
	    return code;
	pdfont->u.cidfont.glyphshow_font = font0;
    }
    font0->data.FDepVector[0] = scaled_font;

    /* Now handle the glyphshow as a show in the Type 0 font. */

    save = *pte;
    pte->current_font = (gs_font *)font0;
    /* Patch the operation temporarily for init_fstack. */
    pte->text.operation = (operation & ~TEXT_FROM_ANY) | TEXT_FROM_BYTES;
    /* Patch the data for process_cmap_text. */
    pte->text.data.bytes = vbuf;
    pte->text.size = size / sizeof(gs_glyph) * 2;
    pte->index = 0;
    gs_type0_init_fstack(pte, pte->current_font);
    code = process_cmap_text_common(pte, vbuf, vbuf, pte->text.size, pdgsf,
				    pdfont);
    /******
     * font0->data.FDepVector[0] references the uncopied CIDFont, which
     * may be freed any time after text processing completes.  How to
     * clear this at the right time?
     ******/
    pte->current_font = scaled_font;
    pte->text = save.text;
    pte->index = save.index + pte->index / 2;
    pte->fstack = save.fstack;
    return code;
}
