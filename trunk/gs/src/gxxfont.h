/* Copyright (C) 1992, 1993, 1994, 1996, 2002 Aladdin Enterprises.  All rights reserved.
  
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
/* External font interface for Ghostscript library */

#ifndef gxxfont_INCLUDED
#  define gxxfont_INCLUDED

#include "gsccode.h"
#include "gsmatrix.h"
#include "gsuid.h"
#include "gsxfont.h"

/*
 *                    Design issues for external fonts
 *
 * 1. Where do xfonts come from: a device or a font service?
 *
 * 2. Is a given xfont associated with a particular device, or with a
 *    class of devices, which may have different output media?
 *    (Specifically, Windows displays vs. printers.)
 *
 * 3. Is an xfont a handle that must be interpreted by its originator,
 *    or an object with its own set of operations?
 *
 * 4. Are xfonts always transformation-specific, or is there such a thing
 *    as a scalable xfont?
 *
 * 5. What is the meaning of the transformation matrix supplied when
 *    asking for an xfont?
 *
 *                      Answers (for the current design)
 *
 * 1. Devices supply xfonts.  Internal devices (image, null, clipping,
 *    command list, tracing) forward font requests to a real underlying
 *    device.  File format devices should do the same, but right now
 *    they don't.
 *
 * 2. An xfont is not associated with anything: it just provides bitmaps.
 *    Since xfonts are only used at small sizes and low resolutions,
 *    tuning differences for different output media aren't likely to be
 *    an issue.
 *
 * 3. Xfonts are objects.  They are allocated by their originator, and
 *    (currently) only freed by `restore'.
 *
 * 4. Xfonts are always transformation-specific.  This may lead to some
 *    clutter, but it's very unlikely that a document will have enough
 *    different transformed versions of a single font for this to be a
 *    problem in practice.
 *
 * 5. The transformation matrix is the CTM within the BuildChar or BuildGlyph
 *    procedure.  This maps a 1000x1000 square to the intended character size
 *    (assuming the base font uses the usual 1000-unit scaling).
 */

/* The definitions for xfonts are very similar to those for devices. */

/* Structure for generic xfonts. */
typedef struct gx_xfont_common_s {
    const gx_xfont_procs *procs;
} gx_xfont_common;

/* A generic xfont. */
struct gx_xfont_s {
    gx_xfont_common common;
};

/* Definition of xfont procedures. */

struct gx_xfont_procs_s {

    /* Look up a font name, UniqueID, and matrix, and return */
    /* an xfont. */

    /* NOTE: even though this is defined as an xfont_proc, */
    /* it is actually a `factory' procedure, the only one that */
    /* does not take an xfont * as its first argument. */

#define xfont_proc_lookup_font(proc)\
  gx_xfont *proc(P7(gx_device *dev, const byte *fname, uint len,\
    int encoding_index, const gs_uid *puid, const gs_matrix *pmat,\
    gs_memory_t *mem))
    xfont_proc_lookup_font((*lookup_font));

    /*
     * Convert a character name to an xglyph code.  encoding_index is
     * actually a gs_encoding_index_t.  Either chr or glyph may be absent
     * (gs_no_char/glyph), but not both.  glyph_name is the glyph's
     * (string) name if the glyph is not GS_NO_GLYPH and is not a CID.
     */
    /*
     * This procedure was deprecated as of release 3.43, but still
     * supported.  In release 7.21, the argument list was changed, and the
     * procedure is no longer deprecated.
     */

#define xfont_proc_char_xglyph(proc)\
  gx_xglyph proc(P5(gx_xfont *xf, gs_char chr, int encoding_index,\
    gs_glyph glyph, const gs_const_string *glyph_name))
    xfont_proc_char_xglyph((*char_xglyph));

    /* Get the metrics for a character. */
    /* Note: pwidth changed in release 2.9.7. */

#define xfont_proc_char_metrics(proc)\
  int proc(P5(gx_xfont *xf, gx_xglyph xg, int wmode,\
    gs_point *pwidth, gs_int_rect *pbbox))
    xfont_proc_char_metrics((*char_metrics));

    /* Render a character. */
    /* (x,y) corresponds to the character origin. */
    /* The target may be any Ghostscript device. */

#define xfont_proc_render_char(proc)\
  int proc(P7(gx_xfont *xf, gx_xglyph xg, gx_device *target,\
    int x, int y, gx_color_index color, int required))
    xfont_proc_render_char((*render_char));

    /* Release any external resources associated with an xfont. */
    /* If mprocs is not NULL, also free any storage */
    /* allocated by lookup_font (including the xfont itself). */

#define xfont_proc_release(proc)\
  int proc(P2(gx_xfont *xf, gs_memory_t *mem))
    xfont_proc_release((*release));

    /* (There was a char_xglyph2 procedure here, added in release 3.43, */
    /* removed in 7.21.) */

};

/*
 * Since xfonts are garbage-collectable, they need structure descriptors.
 * Fortunately, the common part of an xfont contains no pointers to
 * GC-managed space, so simple xfonts can use gs_private_st_simple.
 * The following macro will serve for an xfont with only one pointer,
 * to its device:
 */
#define gs__st_dev_ptrs1(scope_st, stname, stype, sname, penum, preloc, de)\
  private ENUM_PTRS_WITH(penum, stype *xfptr) return 0;\
    case 0: ENUM_RETURN(gx_device_enum_ptr((gx_device *)(xfptr->de)));\
  ENUM_PTRS_END\
  private RELOC_PTRS_WITH(preloc, stype *xfptr) ;\
    xfptr->de = (void *)gx_device_reloc_ptr((gx_device *)(xfptr->de), gcst);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
/*
 * We probably don't ever want xfont descriptors to be public....
#define gs_public_st_dev_ptrs1(stname, stype, sname, penum, preloc, de)\
  gs__st_dev_ptrs1(public_st, stname, stype, sname, penum, preloc, de)
 */
#define gs_private_st_dev_ptrs1(stname, stype, sname, penum, preloc, de)\
  gs__st_dev_ptrs1(private_st, stname, stype, sname, penum, preloc, de)

#endif /* gxxfont_INCLUDED */
