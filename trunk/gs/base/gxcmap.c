/* Copyright (C) 2001-2006 Artifex Software, Inc.
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
/* Color mapping for Ghostscript */
#include "gx.h"
#include "gserrors.h"
#include "gsccolor.h"
#include "gxalpha.h"
#include "gxcspace.h"
#include "gxfarith.h"
#include "gxfrac.h"
#include "gxdcconv.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxlum.h"
#include "gzstate.h"
#include "gxdither.h"
#include "gxcdevn.h"
#include "string_.h"
#include "gsicc_manage.h"
#include "gdevdevn.h"
#include "gsicc_cache.h"
#include "gscms.h"

/* Structure descriptor */
public_st_device_color();
static 
ENUM_PTRS_WITH(device_color_enum_ptrs, gx_device_color *cptr)
{
	return ENUM_USING(*cptr->type->stype, vptr, size, index);
}
ENUM_PTRS_END
static RELOC_PTRS_WITH(device_color_reloc_ptrs, gx_device_color *cptr)
{
    RELOC_USING(*cptr->type->stype, vptr, size);
}
RELOC_PTRS_END

gx_color_index
gx_default_encode_color(gx_device * dev, const gx_color_value cv[])
{
    int             ncomps = dev->color_info.num_components;
    int             i;
    const byte *    comp_shift = dev->color_info.comp_shift;
    const byte *    comp_bits = dev->color_info.comp_bits;
    gx_color_index  color = 0;

#ifdef DEBUG
    if ( dev->color_info.separable_and_linear != GX_CINFO_SEP_LIN ) {
        dprintf( "gx_default_encode_color() requires separable and linear\n" );
        return gx_no_color_index;
    }
#endif
    for (i = 0; i < ncomps; i++) {
	color |= (gx_color_index)(cv[i] >> (gx_color_value_bits - comp_bits[i]))
		<< comp_shift[i];

    }
    return color;
}

/* 
 * This routine is only used if the device is 'separable'.  See
 * separable_and_linear in gxdevcli.h for more information.
 */
int
gx_default_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[])
{
    int                     ncomps = dev->color_info.num_components;
    int                     i;
    const byte *            comp_shift = dev->color_info.comp_shift;
    const byte *            comp_bits = dev->color_info.comp_bits;
    const gx_color_index *  comp_mask = dev->color_info.comp_mask;
    uint shift, ivalue, nbits, scale;

#ifdef DEBUG
    if ( dev->color_info.separable_and_linear != GX_CINFO_SEP_LIN ) {
        dprintf( "gx_default_decode_color() requires separable and linear\n" );
        return gs_error_rangecheck;
    }
#endif

    for (i = 0; i < ncomps; i++) {
	/*
	 * Convert from the gx_color_index bits to a gx_color_value.
	 * Split the conversion into an integer and a fraction calculation
	 * so we can do integer arthmetic.  The calculation is equivalent
	 * to floor(0xffff.fffff * ivalue / ((1 << nbits) - 1))
	 */
	nbits = comp_bits[i];
	scale = gx_max_color_value / ((1 << nbits) - 1);
	ivalue = (color & comp_mask[i]) >> comp_shift[i];
	cv[i] = ivalue * scale;
	/*
	 * Since our scaling factor is an integer, we lost the fraction.
	 * Determine what part of the ivalue that the faction would have 
	 * added into the result.
	 */
	shift = nbits - (gx_color_value_bits % nbits);
	cv[i] += ivalue >> shift;
    }
    return 0;
}

gx_color_index
gx_error_encode_color(gx_device * dev, const gx_color_value colors[])
{
#ifdef DEBUG
    /* The "null" device is expected to be missing encode_color */
    if (strcmp(dev->dname, "null") != 0)
	dprintf("No encode_color proc defined for device.\n");
#endif
    return gx_no_color_index;
}

int
gx_error_decode_color(gx_device * dev, gx_color_index cindex, gx_color_value colors[])
{
     int i=dev->color_info.num_components;
 
#ifdef DEBUG
     dprintf("No decode_color proc defined for device.\n");
#endif
     for(; i>=0; i--)
 	colors[i] = 0;
     return gs_error_rangecheck;
}

/*
 * The "back-stop" default encode_color method. This will be used only
 * if no applicable color encoding procedure is provided, and the number
 * of color model components is 1. The encoding is presumed to induce an
 * additive color model (DeviceGray).
 *
 * The particular method employed is a trivial generalization of the
 * default map_rgb_color method used in the pre-DeviceN code (this was
 * known as gx_default_w_b_map_rgb_color). Since the DeviceRGB color
 * model is assumed additive, any of the procedures used as a default
 * map_rgb_color method are assumed to induce an additive color model.
 * gx_default_w_b_map_rgb_color mapped white to 1 and black to 0, so
 * the new procedure is set up with zero-base and positive slope as well.
 * The generalization is the use of depth; the earlier procedure assumed
 * a bi-level device.
 *
 * Two versions of this procedure are provided, the first of which
 * applies if max_gray == 2^depth - 1 and is faster, while the second
 * applies to the general situation. Note that, as with the encoding
 * procedures used in the pre-DeviceN code, both of these methods induce
 * a small rounding error if 1 < depth < gx_color_value_bits.
 */
gx_color_index
gx_default_gray_fast_encode(gx_device * dev, const gx_color_value cv[])
{
    return cv[0] >> (gx_color_value_bits - dev->color_info.depth);
}

gx_color_index
gx_default_gray_encode(gx_device * dev, const gx_color_value cv[])
{
    return cv[0] * (dev->color_info.max_gray + 1) / (gx_max_color_value + 1);
}

/**
 * This routine is provided for old devices which provide a
 * map_rgb_color routine but not encode_color. New devices are
 * encouraged either to use the defaults or to set encode_color rather
 * than map_rgb_color.
 **/
gx_color_index
gx_backwards_compatible_gray_encode(gx_device *dev,
				    const gx_color_value cv[])
{
    gx_color_value gray_val = cv[0];
    gx_color_value rgb_cv[3];

    rgb_cv[0] = gray_val;
    rgb_cv[1] = gray_val;
    rgb_cv[2] = gray_val;
    return (*dev_proc(dev, map_rgb_color))(dev, rgb_cv);
}

/* -------- Default color space to color model conversion routines -------- */

void
gray_cs_to_gray_cm(gx_device * dev, frac gray, frac out[])
{
    out[0] = gray;
}

static void
rgb_cs_to_gray_cm(gx_device * dev, const gs_imager_state *pis,
				   frac r, frac g, frac b, frac out[])
{
    out[0] = color_rgb_to_gray(r, g, b, NULL);
}

static void
cmyk_cs_to_gray_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    out[0] = color_cmyk_to_gray(c, m, y, k, NULL);
}

static void
gray_cs_to_rgb_cm(gx_device * dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = gray;
}

void
rgb_cs_to_rgb_cm(gx_device * dev, const gs_imager_state *pis,
				  frac r, frac g, frac b, frac out[])
{
    out[0] = r;
    out[1] = g;
    out[2] = b;
}

static void
cmyk_cs_to_rgb_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    color_cmyk_to_rgb(c, m, y, k, NULL, out, dev->memory);
}

static void
gray_cs_to_rgbk_cm(gx_device * dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = frac_0;
    out[3] = gray;
}

static void
rgb_cs_to_rgbk_cm(gx_device * dev, const gs_imager_state *pis,
				  frac r, frac g, frac b, frac out[])
{
    if ((r == g) && (g == b)) {
	out[0] = out[1] = out[2] = frac_0;
	out[3] = r;
    }
    else {
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = frac_0;
    }
}

static void
cmyk_cs_to_rgbk_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    frac rgb[3];
    if ((c == frac_0) && (m == frac_0) && (y == frac_0)) {
	out[0] = out[1] = out[2] = frac_0;
	out[3] = frac_1 - k;
    }
    else {
	color_cmyk_to_rgb(c, m, y, k, NULL, rgb, dev->memory);
	rgb_cs_to_rgbk_cm(dev, NULL, rgb[0], rgb[1], rgb[2], out);
    }
}

static void
gray_cs_to_cmyk_cm(gx_device * dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = frac_0;
    out[3] = frac_1 - gray;
}

/*
 * Default map from DeviceRGB color space to DeviceCMYK color
 * model. Since this mapping is defined by the PostScript language
 * it is unlikely that any device with a DeviceCMYK color model
 * would define this mapping on its own.
 *
 * If the imager state is not available, map as though the black
 * generation and undercolor removal functions are identity
 * transformations. This mode is used primarily to support the
 * raster operation (rop) feature of PCL, which requires that
 * the raster operation be performed in an RGB color space.
 * Note that default black generation and undercolor removal
 * functions in PostScript need NOT be identity transformations:
 * often they are { pop 0 }.
 */
static void
rgb_cs_to_cmyk_cm(gx_device * dev, const gs_imager_state *pis,
  			   frac r, frac g, frac b, frac out[])
{
    if (pis != 0)
        color_rgb_to_cmyk(r, g, b, pis, out, dev->memory);
    else {
        frac    c = frac_1 - r, m = frac_1 - g, y = frac_1 - b;
        frac    k = min(c, min(m, y));

        out[0] = c - k;
        out[1] = m - k;
        out[2] = y - k;
        out[3] = k;
    }
}

void
cmyk_cs_to_cmyk_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
}


/* The list of default color space to color model conversion routines. */

static const gx_cm_color_map_procs DeviceGray_procs = {
    gray_cs_to_gray_cm, rgb_cs_to_gray_cm, cmyk_cs_to_gray_cm
};

static const gx_cm_color_map_procs DeviceRGB_procs = {
    gray_cs_to_rgb_cm, rgb_cs_to_rgb_cm, cmyk_cs_to_rgb_cm
};

static const gx_cm_color_map_procs DeviceCMYK_procs = {
    gray_cs_to_cmyk_cm, rgb_cs_to_cmyk_cm, cmyk_cs_to_cmyk_cm
};

static const gx_cm_color_map_procs DeviceRGBK_procs = {
    gray_cs_to_rgbk_cm, rgb_cs_to_rgbk_cm, cmyk_cs_to_rgbk_cm
};

/*
 * These are the default handlers for returning the list of color space
 * to color model conversion routines.
 */
const gx_cm_color_map_procs *
gx_default_DevGray_get_color_mapping_procs(const gx_device * dev)
{
    return &DeviceGray_procs;
}

const gx_cm_color_map_procs *
gx_default_DevRGB_get_color_mapping_procs(const gx_device * dev)
{
    return &DeviceRGB_procs;
}

const gx_cm_color_map_procs *
gx_default_DevCMYK_get_color_mapping_procs(const gx_device * dev)
{
    return &DeviceCMYK_procs;
}

const gx_cm_color_map_procs *
gx_default_DevRGBK_get_color_mapping_procs(const gx_device * dev)
{
    return &DeviceRGBK_procs;
}

const gx_cm_color_map_procs *
gx_error_get_color_mapping_procs(const gx_device * dev)
{
    /*
     * We should never get here.  If we do then we do not have a "get_color_mapping_procs"
     * routine for the device. This will be noisy, but better than returning NULL which
     * would lead to SEGV (Segmentation Fault) errors when this is used.
     */
    emprintf1(dev->memory,
              "No get_color_mapping_procs proc defined for device '%s'\n",
              dev->dname);
    switch (dev->color_info.num_components) {
      case 1:     /* DeviceGray or DeviceInvertGray */
	return gx_default_DevGray_get_color_mapping_procs(dev);

      case 3:
	return gx_default_DevRGB_get_color_mapping_procs(dev);

      case 4:
      default:		/* Unknown color model - punt with CMYK */
        return gx_default_DevCMYK_get_color_mapping_procs(dev);
    }
}
    
/* ----- Default color component name to colorant index conversion routines ------ */

#define compare_color_names(pname, name_size, name_str) \
    (name_size == (int)strlen(name_str) && strncmp(pname, name_str, name_size) == 0)

/* Default color component to index for a DeviceGray color model */
int
gx_default_DevGray_get_color_comp_index(gx_device * dev, const char * pname,
					  int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Gray") ||
	compare_color_names(pname, name_size, "Grey")) 
        return 0;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for a DeviceRGB color model */
int
gx_default_DevRGB_get_color_comp_index(gx_device * dev, const char * pname,
					   int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Red"))
        return 0;
    if (compare_color_names(pname, name_size, "Green"))
        return 1;
    if (compare_color_names(pname, name_size, "Blue"))
        return 2;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for a DeviceCMYK color model */
int
gx_default_DevCMYK_get_color_comp_index(gx_device * dev, const char * pname,
					    int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Cyan"))
        return 0;
    if (compare_color_names(pname, name_size, "Magenta"))
        return 1;
    if (compare_color_names(pname, name_size, "Yellow"))
        return 2;
    if (compare_color_names(pname, name_size, "Black"))
        return 3;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for a DeviceRGBK color model */
int
gx_default_DevRGBK_get_color_comp_index(gx_device * dev, const char * pname,
					    int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Red"))
        return 0;
    if (compare_color_names(pname, name_size, "Green"))
        return 1;
    if (compare_color_names(pname, name_size, "Blue"))
        return 2;
    if (compare_color_names(pname, name_size, "Black"))
        return 3;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for an unknown color model */
int
gx_error_get_color_comp_index(gx_device * dev, const char * pname,
					int name_size, int component_type)
{
    /*
     * We should never get here.  If we do then we do not have a "get_color_comp_index"
     * routine for the device.
     */
#ifdef DEBUG
    dprintf("No get_color_comp_index proc defined for device.\n");
#endif
    return -1;			    /* Always return "unknown" component name */
}

#undef compare_color_names

/* ---------------- Device color rendering ---------------- */

static cmap_proc_gray(cmap_gray_halftoned);
static cmap_proc_gray(cmap_gray_direct);

static cmap_proc_rgb(cmap_rgb_halftoned);
static cmap_proc_rgb(cmap_rgb_direct);

#define cmap_cmyk_halftoned cmap_cmyk_direct
static cmap_proc_cmyk(cmap_cmyk_direct);

static cmap_proc_rgb_alpha(cmap_rgb_alpha_halftoned);
static cmap_proc_rgb_alpha(cmap_rgb_alpha_direct);

/* Procedure names are only guaranteed unique to 23 characters.... */
static cmap_proc_rgb_alpha(cmap_rgb_alpha_halftoned);
static cmap_proc_rgb_alpha(cmap_rgb_alpha_direct);

static cmap_proc_separation(cmap_separation_halftoned);
static cmap_proc_separation(cmap_separation_direct);

static cmap_proc_devicen(cmap_devicen_halftoned);
static cmap_proc_devicen(cmap_devicen_direct);

static cmap_proc_is_halftoned(cmap_halftoned_is_halftoned);
static cmap_proc_is_halftoned(cmap_direct_is_halftoned);

static const gx_color_map_procs cmap_few = {
     cmap_gray_halftoned, 
     cmap_rgb_halftoned, 
     cmap_cmyk_halftoned,
     cmap_rgb_alpha_halftoned,
     cmap_separation_halftoned,
     cmap_devicen_halftoned,
     cmap_halftoned_is_halftoned
    };
static const gx_color_map_procs cmap_many = {
     cmap_gray_direct,
     cmap_rgb_direct,
     cmap_cmyk_direct,
     cmap_rgb_alpha_direct,
     cmap_separation_direct,
     cmap_devicen_direct,
     cmap_direct_is_halftoned
    };

const gx_color_map_procs *const cmap_procs_default = &cmap_many;


/* Determine the color mapping procedures for a device. */
/* Note that the default procedure doesn't consult the imager state. */
const gx_color_map_procs *
gx_get_cmap_procs(const gs_imager_state *pis, const gx_device * dev)
{
    return (pis->get_cmap_procs)(pis, dev);
}

const gx_color_map_procs *
gx_default_get_cmap_procs(const gs_imager_state *pis, const gx_device * dev)
{
    return (gx_device_must_halftone(dev) ? &cmap_few : &cmap_many);
}

/* Set the color mapping procedures in the graphics state. */
void
gx_set_cmap_procs(gs_imager_state * pis, const gx_device * dev)
{
    pis->cmap_procs = gx_get_cmap_procs(pis, dev);
}

/* Remap the color in the graphics state. */
int
gx_remap_color(gs_state * pgs)
{
    const gs_color_space *pcs = gs_currentcolorspace_inline(pgs);
    int                   code;

    /* The current color in the graphics state is always used for */
    /* the texture, never for the source. */
    code = (*pcs->type->remap_color) (gs_currentcolor_inline(pgs),
				      pcs, gs_currentdevicecolor_inline(pgs),
				      (gs_imager_state *) pgs, pgs->device,
				      gs_color_select_texture);
    /* if overprint mode is in effect, update the overprint information */
    if (code >= 0 && pgs->effective_overprint_mode == 1)
	code = gs_do_set_overprint(pgs);
    return code;
}

/* Indicate that a color space has no underlying concrete space. */
const gs_color_space *
gx_no_concrete_space(const gs_color_space * pcs, const gs_imager_state * pis)
{
    return NULL;
}

/* Indicate that a color space is concrete. */
const gs_color_space *
gx_same_concrete_space(const gs_color_space * pcs, const gs_imager_state * pis)
{
    return pcs;
}

/* Indicate that a color cannot be concretized. */
int
gx_no_concretize_color(const gs_client_color * pcc, const gs_color_space * pcs,
		       frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    return_error(gs_error_rangecheck);
}

/* By default, remap a color by concretizing it and then */
/* remapping the concrete color. */
int
gx_default_remap_color(const gs_client_color * pcc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		       gs_color_select_t select)
{
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    const gs_color_space *pconcs;
    int i = pcs->type->num_components(pcs);
    int code = (*pcs->type->concretize_color)(pcc, pcs, conc, pis, dev);

    if (code < 0)
	return code;
    pconcs = cs_concrete_space(pcs, pis);
    code = (*pconcs->type->remap_concrete_color)(conc, pconcs, pdc, pis, dev, select);

    /* Save original color space and color info into dev color */
    i = any_abs(i);
    for (i--; i >= 0; i--)
	pdc->ccolor.paint.values[i] = pcc->paint.values[i];
    pdc->ccolor_valid = true;
    return code;
}

/* Color remappers for the standard color spaces. */
/* Note that we use D... instead of Device... in some places because */
/* gcc under VMS only retains 23 characters of procedure names. */


/* DeviceGray */
int
gx_concretize_DeviceGray(const gs_client_color * pc, const gs_color_space * pcs,
			 frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    pconc[0] = gx_unit_frac(pc->paint.values[0]);
    return 0;
}
int
gx_remap_concrete_DGray(const frac * pconc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
			gs_color_select_t select)
{
    if (pis->alpha == gx_max_color_value)
	(*pis->cmap_procs->map_gray)
	    (pconc[0], pdc, pis, dev, select);
    else
	(*pis->cmap_procs->map_rgb_alpha)
	    (pconc[0], pconc[0], pconc[0], cv2frac(pis->alpha),
	     pdc, pis, dev, select);
    return 0;
}
int
gx_remap_DeviceGray(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		    gs_color_select_t select)
{
    frac fgray = gx_unit_frac(pc->paint.values[0]);

    /* Save original color space and color info into dev color */
    pdc->ccolor.paint.values[0] = pc->paint.values[0];
    pdc->ccolor_valid = true;
    if (pis->alpha == gx_max_color_value)
	(*pis->cmap_procs->map_gray)
	    (fgray, pdc, pis, dev, select);
    else
	(*pis->cmap_procs->map_rgb_alpha)
	    (fgray, fgray, fgray, cv2frac(pis->alpha), pdc, pis, dev, select);
    return 0;
}

/* DeviceRGB */
int
gx_concretize_DeviceRGB(const gs_client_color * pc, const gs_color_space * pcs,
			frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    pconc[0] = gx_unit_frac(pc->paint.values[0]);
    pconc[1] = gx_unit_frac(pc->paint.values[1]);
    pconc[2] = gx_unit_frac(pc->paint.values[2]);
    return 0;
}
int
gx_remap_concrete_DRGB(const frac * pconc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		       gs_color_select_t select)
{
    if (pis->alpha == gx_max_color_value)
	gx_remap_concrete_rgb(pconc[0], pconc[1], pconc[2],
			      pdc, pis, dev, select);
    else
	gx_remap_concrete_rgb_alpha(pconc[0], pconc[1], pconc[2],
				    cv2frac(pis->alpha),
				    pdc, pis, dev, select);
    return 0;
}
int
gx_remap_DeviceRGB(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		   gs_color_select_t select)
{
    frac fred = gx_unit_frac(pc->paint.values[0]), fgreen = gx_unit_frac(pc->paint.values[1]),
         fblue = gx_unit_frac(pc->paint.values[2]);

    /* Save original color space and color info into dev color */
    pdc->ccolor.paint.values[0] = pc->paint.values[0];
    pdc->ccolor.paint.values[1] = pc->paint.values[1];
    pdc->ccolor.paint.values[2] = pc->paint.values[2];
    pdc->ccolor_valid = true;
    if (pis->alpha == gx_max_color_value)
	gx_remap_concrete_rgb(fred, fgreen, fblue,
			      pdc, pis, dev, select);
    else
	gx_remap_concrete_rgb_alpha(fred, fgreen, fblue, cv2frac(pis->alpha),
				    pdc, pis, dev, select);
    return 0;
}

/* DeviceCMYK */
int
gx_concretize_DeviceCMYK(const gs_client_color * pc, const gs_color_space * pcs,
			 frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    pconc[0] = gx_unit_frac(pc->paint.values[0]);
    pconc[1] = gx_unit_frac(pc->paint.values[1]);
    pconc[2] = gx_unit_frac(pc->paint.values[2]);
    pconc[3] = gx_unit_frac(pc->paint.values[3]);
    return 0;
}
int
gx_remap_concrete_DCMYK(const frac * pconc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
			gs_color_select_t select)
{
/****** IGNORE alpha ******/
    gx_remap_concrete_cmyk(pconc[0], pconc[1], pconc[2], pconc[3], pdc,
			   pis, dev, select);
    return 0;
}
int
gx_remap_DeviceCMYK(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		    gs_color_select_t select)
{
/****** IGNORE alpha ******/
    /* Save original color space and color info into dev color */
    pdc->ccolor.paint.values[0] = pc->paint.values[0];
    pdc->ccolor.paint.values[1] = pc->paint.values[1];
    pdc->ccolor.paint.values[2] = pc->paint.values[2];
    pdc->ccolor.paint.values[3] = pc->paint.values[3];
    pdc->ccolor_valid = true;
    gx_remap_concrete_cmyk(gx_unit_frac(pc->paint.values[0]),
			   gx_unit_frac(pc->paint.values[1]),
			   gx_unit_frac(pc->paint.values[2]),
			   gx_unit_frac(pc->paint.values[3]),
			   pdc, pis, dev, select);
    return 0;
}


/* ------ Render Gray color. ------ */

static void
cmap_gray_halftoned(frac gray, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    dev_proc(dev, get_color_mapping_procs)(dev)->map_gray(dev, gray, cm_comps);

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]);
    else {
        if (dev->color_info.opmode == GX_CINFO_OPMODE_UNKNOWN)
            check_cmyk_color_model_comps(dev);  
        if (dev->color_info.opmode == GX_CINFO_OPMODE) {  /* CMYK-like color space */
            int k = dev->color_info.black_component;

            for (i = 0; i < ncomps; i++) {
                if (i == k)
                    cm_comps[i] = frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
                 else
                    cm_comps[i] = cm_comps[i]; /* Ignore transfer, see PLRM3 p. 494 */
            }
        } else {
            for (i = 0; i < ncomps; i++)
                cm_comps[i] = frac_1 - gx_map_color_frac(pis,
	    		    (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
        }
    }
    if (gx_render_device_DeviceN(cm_comps, pdc, dev, pis->dev_ht,
	    				&pis->screen_phase[select]) == 1)
	gx_color_load_select(pdc, pis, dev, select);
}

static void
cmap_gray_direct(frac gray, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    dev_proc(dev, get_color_mapping_procs)(dev)->map_gray(dev, gray, cm_comps);

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]));
    else {
        if (dev->color_info.opmode == GX_CINFO_OPMODE_UNKNOWN)
            check_cmyk_color_model_comps(dev);  
        if (dev->color_info.opmode == GX_CINFO_OPMODE) {  /* CMYK-like color space */
            int k = dev->color_info.black_component;

            for (i = 0; i < ncomps; i++) {
                if (i == k)
                    cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));
                else
                    cv[i] = frac2cv(cm_comps[i]); /* Ignore transfer, see PLRM3 p. 494 */
            }
        } else {
            for (i = 0; i < ncomps; i++)
                cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
	    		    (frac)(frac_1 - cm_comps[i]), effective_transfer[i]));
        }
    }
    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
    else
        cmap_gray_halftoned(gray, pdc, pis, dev, select);
}


/* ------ Render RGB color. ------ */

static void
cmap_rgb_halftoned(frac r, frac g, frac b, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]);
    else
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]);

    if (gx_render_device_DeviceN(cm_comps, pdc, dev, pis->dev_ht,
	    				&pis->screen_phase[select]) == 1)
	gx_color_load_select(pdc, pis, dev, select);
}

static void
cmap_rgb_direct(frac r, frac g, frac b, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]));
    else
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
    else
        cmap_rgb_halftoned(r, g, b, pdc, pis, dev, select);
}


/* ------ Render CMYK color. ------ */

static void
cmap_cmyk_direct(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    dev_proc(dev, get_color_mapping_procs)(dev)->map_cmyk(dev, c, m, y, k, cm_comps);

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]);
    else
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]);

    /* We make a test for direct vs. halftoned, rather than */
    /* duplicating most of the code of this procedure. */
    if (gx_device_must_halftone(dev)) {
	if (gx_render_device_DeviceN(cm_comps, pdc, dev,
		    pis->dev_ht, &pis->screen_phase[select]) == 1)
	    gx_color_load_select(pdc, pis, dev, select);
	return;
    }

    for (i = 0; i < ncomps; i++)
        cv[i] = frac2cv(cm_comps[i]);

    color = dev_proc(dev, encode_color)(dev, cv);
    if (color != gx_no_color_index) 
	color_set_pure(pdc, color);
    else {
	if (gx_render_device_DeviceN(cm_comps, pdc, dev,
		    pis->dev_ht, &pis->screen_phase[select]) == 1)
	    gx_color_load_select(pdc, pis, dev, select);
	return;
    }
}

static void
cmap_rgb_alpha_halftoned(frac r, frac g, frac b, frac alpha,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
			 gs_color_select_t select)
{
     int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

    /* pre-multiply to account for the alpha weighting */
    if (alpha != frac_1) {
#ifdef PREMULTIPLY_TOWARDS_WHITE
        frac alpha_bias = frac_1 - alpha;
#else
	frac alpha_bias = 0;
#endif

        for (i = 0; i < ncomps; i++)
            cm_comps[i] = (frac)((long)cm_comps[i] * alpha) / frac_1 + alpha_bias;
    }

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]);
    else
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]);

    if (gx_render_device_DeviceN(cm_comps, pdc, dev, pis->dev_ht,
	    				&pis->screen_phase[select]) == 1)
	gx_color_load_select(pdc, pis, dev, select);
}

static void
cmap_rgb_alpha_direct(frac r, frac g, frac b, frac alpha, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv_alpha, cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

    /* pre-multiply to account for the alpha weighting */
    if (alpha != frac_1) {
#ifdef PREMULTIPLY_TOWARDS_WHITE
        frac alpha_bias = frac_1 - alpha;
#else
	frac alpha_bias = 0;
#endif

        for (i = 0; i < ncomps; i++)
            cm_comps[i] = (frac)((long)cm_comps[i] * alpha) / frac_1 + alpha_bias;
    }

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]));
    else
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));

    /* encode as a color index */
    if (dev_proc(dev, map_rgb_alpha_color) != gx_default_map_rgb_alpha_color &&
         (cv_alpha = frac2cv(alpha)) != gx_max_color_value)
        color = dev_proc(dev, map_rgb_alpha_color)(dev, cv[0], cv[1], cv[2], cv_alpha);
    else
        color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
    else
        cmap_rgb_alpha_halftoned(r, g, b, alpha, pdc, pis, dev, select);
}


/* ------ Render Separation All color. ------ */

/*
 * This routine maps DeviceN components into the order of the device's
 * colorants.
 *
 * Parameters:
 *    pcc - Pointer to DeviceN components.
 *    pcolor_component_map - Map from DeviceN to the Devices colorants.
 *        A negative value indicates component is not to be mapped.
 *    plist - Pointer to list for mapped components
 *
 * Returns:
 *    Mapped components in plist.
 */
static inline void
map_components_to_colorants(const frac * pcc,
	const gs_devicen_color_map * pcolor_component_map, frac * plist)
{
    int i = pcolor_component_map->num_colorants - 1;
    int pos;

    /* Clear all output colorants first */
    for (; i >= 0; i--) {
	plist[i] = frac_0;
    }

    /* Map color components into output list */
    for (i = pcolor_component_map->num_components - 1; i >= 0; i--) {
	pos = pcolor_component_map->color_map[i];
	if (pos >= 0)
	    plist[pos] = pcc[i];
    }
}

static void
cmap_separation_halftoned(frac all, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    bool additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;
    frac comp_value = all;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];

    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    if (pis->color_component_map.sep_type == SEP_ALL) {
	/*
	 * Invert the photometric interpretation for additive
	 * color spaces because separations are always subtractive.
	 */
	if (additive)
	    comp_value = frac_1 - comp_value;

        /* Use the "all" value for all components */
	i = pis->color_component_map.num_colorants - 1;
        for (; i >= 0; i--)
            cm_comps[i] = comp_value;
    }
    else {
        /* map to the color model */
        map_components_to_colorants(&all, &(pis->color_component_map), cm_comps);
    }

    /* apply the transfer function(s); convert to color values */
    if (additive)
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]);
    else
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]);

    if (gx_render_device_DeviceN(cm_comps, pdc, dev, pis->dev_ht,
    					&pis->screen_phase[select]) == 1)
	gx_color_load_select(pdc, pis, dev, select);
}

static void
cmap_separation_direct(frac all, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    bool additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;
    frac comp_value = all;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    if (pis->color_component_map.sep_type == SEP_ALL) {
	/*
	 * Invert the photometric interpretation for additive
	 * color spaces because separations are always subtractive.
	 */
	if (additive)
	    comp_value = frac_1 - comp_value;

        /* Use the "all" value for all components */
        i = pis->color_component_map.num_colorants - 1;
        for (; i >= 0; i--)
            cm_comps[i] = comp_value;
    }
    else {
        /* map to the color model */
        map_components_to_colorants(&comp_value, &(pis->color_component_map), cm_comps);
    }

    /* apply the transfer function(s); convert to color values */
    if (additive)
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]));
    else
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
    else
        cmap_separation_halftoned(all, pdc, pis, dev, select);
}

/* Routines for handling CM of CMYK components of a DeviceN color space */
static bool
devicen_has_cmyk(gx_device * dev)
{
    gs_devn_params *devn_params;

    /* Device may not have ret_devn_params! */
    if (dev->procs.ret_devn_params != NULL) {
        devn_params = dev_proc(dev, ret_devn_params)(dev);
    } else {
        return false;
    }
    if (devn_params == NULL) {
        return false;
    }
    return(devn_params->num_std_colorant_names == 4);
}

static int
devicen_icc_cmyk(frac cm_comps[], const gs_imager_state * pis, gx_device *dev)
{
    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k;
    unsigned short *psrc_temp;

    /* Define the rendering intents. */
    rendering_params.black_point_comp = BP_ON;
    rendering_params.object_type = GS_PATH_TAG;
    rendering_params.rendering_intent = pis->renderingintent;
    /* Sigh, frac to full 16 bit.  Need to clean this up */
    for (k = 0; k < 4; k++){
        psrc[k] = frac2cv(cm_comps[k]);
    }
    icc_link = gsicc_get_link_profile(pis, dev, pis->icc_manager->default_cmyk, 
                dev->device_icc_profile, &rendering_params, pis->memory, false);
    /* Transform the color */
    if (icc_link->is_identity) {
        psrc_temp = &(psrc[0]);
    } else {
        /* Transform the color */
        psrc_temp = &(psrc_cm[0]);
        gscms_transform_color(icc_link, psrc, psrc_temp, 2, NULL);
    }
    /* This needs to be optimized */
    for (k = 0; k < 4; k++){
        cm_comps[k] = float2frac(((float) psrc_temp[k])/65535.0);
    }
    /* Release the link */
    gsicc_release_link(icc_link);
    return(0);
}

/* ------ DeviceN color mapping */

/*
 * This routine is called to map a DeviceN colorspace to a DeviceN
 * output device which requires halftoning.  T
 */
static void
cmap_devicen_halftoned(const frac * pcc, 
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int code;

    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    map_components_to_colorants(pcc, &(pis->color_component_map), cm_comps);
    /* See comments in cmap_devicen_direct for details on below operations */
    if (devicen_has_cmyk(dev) && 
        dev->device_icc_profile->data_cs == gsCMYK) {
        code = devicen_icc_cmyk(cm_comps, pis, dev);
    } 
    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]);
    else
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]);

    /* We need to finish halftoning */
    if (gx_render_device_DeviceN(cm_comps, pdc, dev, pis->dev_ht,
    					&pis->screen_phase[select]) == 1)
	gx_color_load_select(pdc, pis, dev, select);
}

/*
 * This routine is called to map a DeviceN colorspace to a DeviceN
 * output device which does not require halftoning.
 */
static void
cmap_devicen_direct(const frac * pcc, 
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    int code;

    /*   See the comment below */
    /* map to the color model */
    for (i=0; i < ncomps; i++)
	cm_comps[i] = 0;
    map_components_to_colorants(pcc, &(pis->color_component_map), cm_comps);;
    /*  Check if we have the standard colorants.  If yes, then we will apply
       ICC color management to those colorants. To understand why, consider
       the example where I have a Device with CMYK + O  and I have a 
       DeviceN color in the document that is specified for any set of 
       these colorants, and suppose that I let them pass through 
       witout any color management.  This is probably  not the 
       desired effect since I could have a DeviceN color fill that had 10% C, 
       20% M 0% Y 0% K and 0% O.  I would like this to look the same 
       as a CMYK color that will be color managed and specified with 10% C, 
       20% M 0% Y 0% K. Hence the CMYK values should go through the same
       color management as a stand alone CMYK value.  */
    if (devicen_has_cmyk(dev) && 
        dev->device_icc_profile->data_cs == gsCMYK) {
        /* We need to do a CMYK to CMYK conversion here.  This will always
           use the default CMYK profile and the device's output profile.
           We probably need to add some checking here
           and possibly permute the colorants, much as is done on the input
           side for the case when we add DeviceN icc source profiles for use
           in PDF and PS data. */
        code = devicen_icc_cmyk(cm_comps, pis, dev);
    } 
    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pis,
	    			cm_comps[i], effective_transfer[i]));
    else
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
	    		(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));
    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
    else
        cmap_devicen_halftoned(pcc, pdc, pis, dev, select);
}

/* ------ Halftoning check ----- */

static bool
cmap_halftoned_is_halftoned(const gs_imager_state * pis, gx_device * dev)
{
    return true;
}

static bool
cmap_direct_is_halftoned(const gs_imager_state * pis, gx_device * dev)
{
    return false;
}

/* ------ Transfer function mapping ------ */

/* Define an identity transfer function. */
float
gs_identity_transfer(floatp value, const gx_transfer_map * pmap)
{
    return (float) value;
}

/* Define the generic transfer function for the library layer. */
/* This just returns what's already in the map. */
float
gs_mapped_transfer(floatp value, const gx_transfer_map * pmap)
{
    return gx_map_color_float(pmap, value);
}

/* Set a transfer map to the identity map. */
void
gx_set_identity_transfer(gx_transfer_map *pmap)
{
    int i;

    pmap->proc = gs_identity_transfer;
    /* We still have to fill in the cached values. */
    for (i = 0; i < transfer_map_size; ++i)
	pmap->values[i] = bits2frac(i, log2_transfer_map_size);
}

#if FRAC_MAP_INTERPOLATE	/* NOTA BENE */

/* Map a color fraction through a transfer map. */
/* We only use this if we are interpolating. */
frac
gx_color_frac_map(frac cv, const frac * values)
{
#define cp_frac_bits (frac_bits - log2_transfer_map_size)
    int cmi = frac2bits_floor(cv, log2_transfer_map_size);
    frac mv = values[cmi];
    int rem, mdv;

    /* Interpolate between two adjacent values if needed. */
    rem = cv - bits2frac(cmi, log2_transfer_map_size);
    if (rem == 0)
	return mv;
    mdv = values[cmi + 1] - mv;
#if ARCH_INTS_ARE_SHORT
    /* Only use long multiplication if necessary. */
    if (mdv < -1 << (16 - cp_frac_bits) ||
	mdv > 1 << (16 - cp_frac_bits)
	)
	return mv + (uint) (((ulong) rem * mdv) >> cp_frac_bits);
#endif
    return mv + ((rem * mdv) >> cp_frac_bits);
#undef cp_frac_bits
}

#endif /* FRAC_MAP_INTERPOLATE */

/* ------ Default device color mapping ------ */
/* White-on-black */
gx_color_index
gx_default_w_b_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{				/* Map values >= 1/2 to 1, < 1/2 to 0. */
    int             i, ncomps = dev->color_info.num_components;
    gx_color_value  cv_all = 0;
     
    for (i = 0; i < ncomps; i++)
        cv_all |= cv[i];
    return cv_all > gx_max_color_value / 2 ? (gx_color_index)1
        : (gx_color_index)0;

}

int
gx_default_w_b_map_color_rgb(gx_device * dev, gx_color_index color,
			     gx_color_value prgb[3])
{				/* Map 1 to max_value, 0 to 0. */
    prgb[0] = prgb[1] = prgb[2] = -(gx_color_value) color;
    return 0;
}

/* Black-on-white */
gx_color_index
gx_default_b_w_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    int             i, ncomps = dev->color_info.num_components;
    gx_color_value  cv_all = 0;
     
    for (i = 0; i < ncomps; i++)
        cv_all |= cv[i];
    return cv_all > gx_max_color_value / 2 ? (gx_color_index)0
        : (gx_color_index)1;
}

int
gx_default_b_w_map_color_rgb(gx_device * dev, gx_color_index color,
			     gx_color_value prgb[3])
{				/* Map 0 to max_value, 1 to 0. */
    prgb[0] = prgb[1] = prgb[2] = -((gx_color_value) color ^ 1);
    return 0;
}

/* RGB mapping for gray-scale devices */

gx_color_index
gx_default_gray_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{				/* We round the value rather than truncating it. */
    gx_color_value gray =
    (((cv[0] * (ulong) lum_red_weight) +
      (cv[1] * (ulong) lum_green_weight) +
      (cv[2] * (ulong) lum_blue_weight) +
      (lum_all_weights / 2)) / lum_all_weights
     * dev->color_info.max_gray +
     (gx_max_color_value / 2)) / gx_max_color_value;

    return gray;
}

int
gx_default_gray_map_color_rgb(gx_device * dev, gx_color_index color,
			      gx_color_value prgb[3])
{
    gx_color_value gray = (gx_color_value) 
	(color * gx_max_color_value / dev->color_info.max_gray);

    prgb[0] = gray;
    prgb[1] = gray;
    prgb[2] = gray;
    return 0;
}

gx_color_index
gx_default_8bit_map_gray_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index color = gx_color_value_to_byte(cv[0]);

    return (color == gx_no_color_index ? color ^ 1 : color);
}

int
gx_default_8bit_map_color_gray(gx_device * dev, gx_color_index color,
			      gx_color_value pgray[1])
{
    pgray[0] = (gx_color_value)(color * gx_max_color_value / 255);
    return 0;
}

/* RGB mapping for 24-bit true (RGB) color devices */

gx_color_index
gx_default_rgb_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    if (dev->color_info.depth == 24)
	return gx_color_value_to_byte(cv[2]) +
	    ((uint) gx_color_value_to_byte(cv[1]) << 8) +
	    ((ulong) gx_color_value_to_byte(cv[0]) << 16);
    else {
	int bpc = dev->color_info.depth / 3;
	int drop = sizeof(gx_color_value) * 8 - bpc;
	return ( ( (((gx_color_index)cv[0] >> drop) << bpc) +
		    ((gx_color_index)cv[1] >> drop)         ) << bpc) + 
	       ((gx_color_index)cv[2] >> drop);
    }
}

/* Map a color index to a r-g-b color. */
int
gx_default_rgb_map_color_rgb(gx_device * dev, gx_color_index color,
			     gx_color_value prgb[3])
{
    if (dev->color_info.depth == 24) {
	prgb[0] = gx_color_value_from_byte(color >> 16);
	prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
	prgb[2] = gx_color_value_from_byte(color & 0xff);
    } else {
	uint bits_per_color = dev->color_info.depth / 3;
	uint color_mask = (1 << bits_per_color) - 1;

	prgb[0] = ((color >> (bits_per_color * 2)) & color_mask) *
	    (ulong) gx_max_color_value / color_mask;
	prgb[1] = ((color >> (bits_per_color)) & color_mask) *
	    (ulong) gx_max_color_value / color_mask;
	prgb[2] = (color & color_mask) *
	    (ulong) gx_max_color_value / color_mask;
    }
    return 0;
}

/* CMYK mapping for RGB devices (should never be called!) */

gx_color_index
gx_default_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{				/* Convert to RGB */
    frac rgb[3];
    gx_color_value rgb_cv[3];
    color_cmyk_to_rgb(cv2frac(cv[0]), cv2frac(cv[1]), cv2frac(cv[2]), cv2frac(cv[3]),
		      NULL, rgb, dev->memory);
    rgb_cv[0] = frac2cv(rgb[0]);
    rgb_cv[1] = frac2cv(rgb[1]);
    rgb_cv[2] = frac2cv(rgb[2]);
    return (*dev_proc(dev, map_rgb_color)) (dev, rgb_cv);
}

/* Mapping for CMYK devices */

gx_color_index
cmyk_1bit_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
#define CV_BIT(v) ((v) >> (gx_color_value_bits - 1))
    return (gx_color_index)
	(CV_BIT(cv[3]) + (CV_BIT(cv[2]) << 1) + (CV_BIT(cv[1]) << 2) + (CV_BIT(cv[0]) << 3));
#undef CV_BIT
}

/* Shouldn't be called: decode_color should be cmyk_1bit_map_color_cmyk */
int
cmyk_1bit_map_color_rgb(gx_device * dev, gx_color_index color,
			gx_color_value prgb[3])
{
    if (color & 1)
	prgb[0] = prgb[1] = prgb[2] = 0;
    else {
	prgb[0] = (color & 8 ? 0 : gx_max_color_value);
	prgb[1] = (color & 4 ? 0 : gx_max_color_value);
	prgb[2] = (color & 2 ? 0 : gx_max_color_value);
    }
    return 0;
}

int
cmyk_1bit_map_color_cmyk(gx_device * dev, gx_color_index color,
			gx_color_value pcv[4])
{
    pcv[0] = (color & 8 ? 0 : gx_max_color_value);
    pcv[1] = (color & 4 ? 0 : gx_max_color_value);
    pcv[2] = (color & 2 ? 0 : gx_max_color_value);
    pcv[3] = (color & 1 ? 0 : gx_max_color_value);
    return 0;
}

gx_color_index
cmyk_8bit_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index color =
	gx_color_value_to_byte(cv[3]) +
	((uint)gx_color_value_to_byte(cv[2]) << 8) +
	((uint)gx_color_value_to_byte(cv[1]) << 16) +
	((uint)gx_color_value_to_byte(cv[0]) << 24);

    return (color == gx_no_color_index ? color ^ 1 : color);
}

gx_color_index
cmyk_16bit_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index color =
	(uint64_t)cv[3] +
	((uint64_t)cv[2] << 16) +
	((uint64_t)cv[1] << 32) +
	((uint64_t)cv[0] << 48);

    return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Shouldn't be called: decode_color should be cmyk_8bit_map_color_cmyk */
int
cmyk_8bit_map_color_rgb(gx_device * dev, gx_color_index color,
			gx_color_value prgb[3])
{
    int
	not_k = (int) (~color & 0xff),
	r = not_k - (int) (color >> 24),
	g = not_k - (int) ((color >> 16) & 0xff),
	b = not_k - (int) ((color >> 8) & 0xff); 

    prgb[0] = (r < 0 ? 0 : gx_color_value_from_byte(r));
    prgb[1] = (g < 0 ? 0 : gx_color_value_from_byte(g));
    prgb[2] = (b < 0 ? 0 : gx_color_value_from_byte(b));
    return 0;
}

int
cmyk_8bit_map_color_cmyk(gx_device * dev, gx_color_index color,
			gx_color_value pcv[4])
{
    pcv[0] = gx_color_value_from_byte((color >> 24) & 0xff);
    pcv[1] = gx_color_value_from_byte((color >> 16) & 0xff);
    pcv[2] = gx_color_value_from_byte((color >> 8) & 0xff);
    pcv[3] = gx_color_value_from_byte(color & 0xff);
    return 0;
}

int
cmyk_16bit_map_color_cmyk(gx_device * dev, gx_color_index color,
			  gx_color_value pcv[4])
{
    pcv[0] = ((color >> 24) >> 24) & 0xffff;
    pcv[1] = ((color >> 16) >> 16) & 0xffff;
    pcv[2] = ( color        >> 16) & 0xffff;
    pcv[3] = ( color             ) & 0xffff;
    return 0;
}


/* Default mapping between RGB+alpha and RGB. */

gx_color_index
gx_default_map_rgb_alpha_color(gx_device * dev,
 gx_color_value r, gx_color_value g, gx_color_value b, gx_color_value alpha)
{				/* Colors have been premultiplied: we don't need to do it here. */
    gx_color_value cv[3];
    cv[0] = r; cv[1] = g; cv[2] = b;
    return (*dev_proc(dev, map_rgb_color))(dev, cv);
}

int
gx_default_map_color_rgb_alpha(gx_device * dev, gx_color_index color,
			       gx_color_value prgba[4])
{
    prgba[3] = gx_max_color_value;	/* alpha = 1 */
    return (*dev_proc(dev, map_color_rgb)) (dev, color, prgba);
}

frac
gx_unit_frac(float fvalue)
{
    frac f = frac_0;
    if (is_fneg(fvalue))
        f = frac_0;
    else if (is_fge1(fvalue))
        f = frac_1;
    else
        f = float2frac(fvalue);
    return f;
}

/* This is used by image color render to handle the cases where we need to
   perform either a transfer function or halftoning on the color values
   during an ICC color flow.  In this case, the color is already in the
   device color space but in 16bpp color values. */
void
cmap_transfer_halftone(gx_color_value *pconc, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, bool has_transfer,
     bool has_halftone, gs_color_select_t select)
{
    int ncomps = dev->color_info.num_components;
    frac frac_value;
    int i;
    frac cv_frac[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_color_value color_val[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* apply the transfer function(s) */
    if (has_transfer) {
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
            for (i = 0; i < ncomps; i++) {
                frac_value = cv2frac(pconc[i]);
                cv_frac[i] = gx_map_color_frac(pis,
	    			    frac_value, effective_transfer[i]);
            }
        } else {
            if (dev->color_info.opmode == GX_CINFO_OPMODE_UNKNOWN) {
                check_cmyk_color_model_comps(dev);
            }
            if (dev->color_info.opmode == GX_CINFO_OPMODE) {  /* CMYK-like color space */
                int k = dev->color_info.black_component;
                for (i = 0; i < ncomps; i++) {
                    frac_value = cv2frac(pconc[i]);
                    if (i == k) {
                        cv_frac[i] = frac_1 - gx_map_color_frac(pis,
	    		    (frac)(frac_1 - frac_value), effective_transfer[i]);
                    } else {
                        cv_frac[i] = cv2frac(pconc[i]);  /* Ignore transfer, see PLRM3 p. 494 */
                    }
                }
            } else {
                for (i = 0; i < ncomps; i++) {
                    frac_value = cv2frac(pconc[i]);
                    cv_frac[i] = frac_1 - gx_map_color_frac(pis,
	    		        (frac)(frac_1 - frac_value), effective_transfer[i]);
                }
            }
        }
    } else {
        if (has_halftone) {
            /* We need this to be in frac form */
            for (i = 0; i < ncomps; i++) {
                cv_frac[i] = cv2frac(pconc[i]);  
            }
        }   
    }
    /* Halftoning */
    if (has_halftone) {
        if (gx_render_device_DeviceN(&(cv_frac[0]), pdc, dev,
	            pis->dev_ht, &pis->screen_phase[select]) == 1)
            gx_color_load_select(pdc, pis, dev, select);
    } else {
        /* We have a frac value from the transfer function.  Do the encode.
           which does not take a frac value...  */
        for (i = 0; i < ncomps; i++) {
            color_val[i] = frac2cv(cv_frac[i]);  
        }
        color = dev_proc(dev, encode_color)(dev, &(color_val[0]));
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);

    }
}

bool
gx_device_uses_std_cmap_procs(gx_device * dev) 
{
    const gx_cm_color_map_procs *pprocs;

    if (dev->device_icc_profile != NULL) {
        pprocs = dev_proc(dev, get_color_mapping_procs)(dev);
        /* Check if they are forwarding procs */
        if (fwd_uses_fwd_cmap_procs(dev)) {
            pprocs = fwd_get_target_cmap_procs(dev);
        } 
        switch(dev->device_icc_profile->data_cs) {
            case gsGRAY:
                if (pprocs == &DeviceGray_procs) {
                    return true;
                }
                break;
            case gsRGB:	
                if (pprocs == &DeviceRGB_procs) {
                    return true;
                }
                break;
            case gsCMYK:
                if (pprocs == &DeviceCMYK_procs) {
                    return true;
                }
                break;
            default:
                break;
        }
    } 
    return false;
}
