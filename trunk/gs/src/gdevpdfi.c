/* Copyright (C) 1996, 2000, 2002 Aladdin Enterprises.  All rights reserved.
  
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
/* Image handling for PDF-writing driver */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsdevice.h"
#include "gsflip.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"		/* for data stream */
#include "gxcspace.h"
#include "gximage3.h"
#include "gximag3x.h"
#include "gsiparm4.h"

/* Forward references */
private image_enum_proc_plane_data(pdf_image_plane_data);
private image_enum_proc_end_image(pdf_image_end_image);
private image_enum_proc_end_image(pdf_image_end_image_object);
private IMAGE3_MAKE_MID_PROC(pdf_image3_make_mid);
private IMAGE3_MAKE_MCDE_PROC(pdf_image3_make_mcde);
private IMAGE3X_MAKE_MID_PROC(pdf_image3x_make_mid);
private IMAGE3X_MAKE_MCDE_PROC(pdf_image3x_make_mcde);

private const gx_image_enum_procs_t pdf_image_enum_procs = {
    pdf_image_plane_data,
    pdf_image_end_image
};
private const gx_image_enum_procs_t pdf_image_object_enum_procs = {
    pdf_image_plane_data,
    pdf_image_end_image_object
};

/* ---------------- Driver procedures ---------------- */

/* Define the structure for keeping track of progress through an image. */
typedef struct pdf_image_enum_s {
    gx_image_enum_common;
    gs_memory_t *memory;
    int width;
    int bits_per_pixel;		/* bits per pixel (per plane) */
    int rows_left;
    pdf_image_writer writer;
    gs_matrix mat;
} pdf_image_enum;
gs_private_st_composite(st_pdf_image_enum, pdf_image_enum, "pdf_image_enum",
  pdf_image_enum_enum_ptrs, pdf_image_enum_reloc_ptrs);
/* GC procedures */
private ENUM_PTRS_WITH(pdf_image_enum_enum_ptrs, pdf_image_enum *pie)
    if (index < pdf_image_writer_max_ptrs) {
	gs_ptr_type_t ret =
	    ENUM_USING(st_pdf_image_writer, &pie->writer, sizeof(pie->writer),
		       index);

	if (ret == 0)		/* don't stop early */
	    ENUM_RETURN(0);
	return ret;
    }
    return ENUM_USING_PREFIX(st_gx_image_enum_common,
			     pdf_image_writer_max_ptrs);
ENUM_PTRS_END
private RELOC_PTRS_WITH(pdf_image_enum_reloc_ptrs, pdf_image_enum *pie)
{
    RELOC_USING(st_pdf_image_writer, &pie->writer, sizeof(pie->writer));
    RELOC_USING(st_gx_image_enum_common, vptr, size);
}
RELOC_PTRS_END

/*
 * Test whether we can write an image in-line.  This is always true,
 * because we only support PDF 1.2 and later.
 */
private bool
can_write_image_in_line(const gx_device_pdf *pdev, const gs_image_t *pim)
{
    return true;
}

/*
 * Convert a Type 4 image to a Type 1 masked image if possible.
 * Type 1 masked images are more compact, and are supported in all PDF
 * versions, whereas general masked images require PDF 1.3 or higher.
 * Also, Acrobat 5 for Windows has a bug that causes an error for images
 * with a color-key mask, at least for 1-bit-deep images using an Indexed
 * color space.
 */
private int
color_is_black_or_white(gx_device *dev, const gx_drawing_color *pdcolor)
{
    return (!color_is_pure(pdcolor) ? -1 :
	    gx_dc_pure_color(pdcolor) == gx_device_black(dev) ? 0 :
	    gx_dc_pure_color(pdcolor) == gx_device_white(dev) ? 1 : -1);
}
private int
pdf_convert_image4_to_image1(gx_device_pdf *pdev,
			     const gs_imager_state *pis,
			     const gx_drawing_color *pbcolor,
			     const gs_image4_t *pim4, gs_image_t *pim1,
			     gx_drawing_color *pdcolor)
{
    if (pim4->BitsPerComponent == 1 &&
	(pim4->MaskColor_is_range ?
	 pim4->MaskColor[0] | pim4->MaskColor[1] :
	 pim4->MaskColor[0]) <= 1
	) {
	gx_device *const dev = (gx_device *)pdev;
	const gs_color_space *pcs = pim4->ColorSpace;
	bool write_1s = !pim4->MaskColor[0];
	gs_client_color cc;
	int code;

	/*
	 * Prepare the drawing color.  (pdf_prepare_imagemask will set it.)
	 * This is the other color in the image (the one that isn't the
	 * mask key), taking Decode into account.
	 */

	cc.paint.values[0] = pim4->Decode[(int)write_1s];
	cc.pattern = 0;
	code = pcs->type->remap_color(&cc, pcs, pdcolor, pis, dev,
				      gs_color_select_texture);
	if (code < 0)
	    return code;

	/*
	 * The PDF imaging model doesn't support RasterOp.  We can convert a
	 * Type 4 image to a Type 1 imagemask only if the effective RasterOp
	 * passes through the source color unchanged.  "Effective" means we
	 * take into account CombineWithColor, and whether the source and/or
	 * texture are black, white, or neither.
	 */
	{
	    gs_logical_operation_t lop = pis->log_op;
	    int black_or_white = color_is_black_or_white(dev, pdcolor);

	    switch (black_or_white) {
	    case 0: lop = lop_know_S_0(lop); break;
	    case 1: lop = lop_know_S_1(lop); break;
	    default: DO_NOTHING;
	    }
	    if (pim4->CombineWithColor)
		switch (color_is_black_or_white(dev, pbcolor)) {
		case 0: lop = lop_know_T_0(lop); break;
		case 1: lop = lop_know_T_1(lop); break;
		default: DO_NOTHING;
		}
	    else
		lop = lop_know_T_0(lop);
	    switch (lop_rop(lop)) {
	    case rop3_0:
		if (black_or_white != 0)
		    return -1;
		break;
	    case rop3_1:
		if (black_or_white != 1)
		    return -1;
		break;
	    case rop3_S:
		break;
	    default:
		return -1;
	    }
	    if ((lop & lop_S_transparent) && black_or_white == 1)
		return -1;
	}

	/* All conditions are met.  Convert to a masked image. */

	gs_image_t_init_mask_adjust(pim1, write_1s, false);
#define COPY_ELEMENT(e) pim1->e = pim4->e
	COPY_ELEMENT(ImageMatrix);
	COPY_ELEMENT(Width);
	COPY_ELEMENT(Height);
	pim1->BitsPerComponent = 1;
	/* not Decode */
	COPY_ELEMENT(Interpolate);
	pim1->format = gs_image_format_chunky; /* BPC = 1, doesn't matter */
#undef COPY_ELEMENT
	return 0;
    }
    return -1;			/* arbitrary <0 */
}

/*
 * Start processing an image.  This procedure takes extra arguments because
 * it has to do something slightly different for the parts of an ImageType 3
 * image.
 */
typedef enum {
    PDF_IMAGE_DEFAULT,
    PDF_IMAGE_TYPE3_MASK,	/* no in-line, don't render */
    PDF_IMAGE_TYPE3_DATA	/* no in-line */
} pdf_typed_image_context_t;
private int
pdf_begin_typed_image(gx_device_pdf *pdev, const gs_imager_state * pis,
		      const gs_matrix *pmat, const gs_image_common_t *pic,
		      const gs_int_rect * prect,
		      const gx_drawing_color * pdcolor,
		      const gx_clip_path * pcpath, gs_memory_t * mem,
		      gx_image_enum_common_t ** pinfo,
		      pdf_typed_image_context_t context)
{
    cos_dict_t *pnamed = 0;
    const gs_pixel_image_t *pim;
    int code, i;
    pdf_image_enum *pie;
    gs_image_format_t format;
    const gs_color_space *pcs;
    gs_color_space cs_gray_temp;
    cos_value_t cs_value;
    int num_components;
    bool is_mask = false, in_line = false;
    gs_int_rect rect;
    /*
     * We define this union because psdf_setup_image_filters may alter the
     * gs_pixel_image_t part, but pdf_begin_image_data must also have access
     * to the type-specific parameters.
     */
    union iu_ {
	gs_pixel_image_t pixel;	/* we may change some components */
	gs_image1_t type1;
	gs_image3_t type3;
	gs_image3x_t type3x;
	gs_image4_t type4;
    } image[2];
    ulong nbytes;
    int width, height;
    const gs_range_t *pranges = 0;
    int alt_writer_count;

    /*
     * Pop the image name from the NI stack.  We must do this, to keep the
     * stack in sync, even if it turns out we can't handle the image.
     */
    {
	cos_value_t ni_value;

	if (cos_array_unadd(pdev->NI_stack, &ni_value) >= 0)
	    pnamed = (cos_dict_t *)ni_value.contents.object;
    }

    /* Check for the image types we can handle. */
    switch (pic->type->index) {
    case 1: {
	const gs_image_t *pim1 = (const gs_image_t *)pic;

	if (pim1->Alpha != gs_image_alpha_none)
	    goto nyi;
	is_mask = pim1->ImageMask;
	if (is_mask) {
	    /* If parameters are invalid, use the default implementation. */
	    if (pim1->BitsPerComponent != 1 ||
		!((pim1->Decode[0] == 0.0 && pim1->Decode[1] == 1.0) ||
		  (pim1->Decode[0] == 1.0 && pim1->Decode[1] == 0.0))
		)
		goto nyi;
	}
	in_line = context == PDF_IMAGE_DEFAULT &&
	    can_write_image_in_line(pdev, pim1);
	image[0].type1 = *pim1;
	break;
    }
    case 3: {
	const gs_image3_t *pim3 = (const gs_image3_t *)pic;

	if (pdev->CompatibilityLevel < 1.3)
	    goto nyi;
	if (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
		       prect->q.x == pim3->Width &&
		       prect->q.y == pim3->Height))
	    goto nyi;
	/*
	 * We handle ImageType 3 images in a completely different way:
	 * the default implementation sets up the enumerator.
	 */
	return gx_begin_image3_generic((gx_device *)pdev, pis, pmat, pic,
				       prect, pdcolor, pcpath, mem,
				       pdf_image3_make_mid,
				       pdf_image3_make_mcde, pinfo);
    }
    case IMAGE3X_IMAGETYPE: {
	/* See ImageType3 above for more information. */
	const gs_image3x_t *pim3x = (const gs_image3x_t *)pic;

	if (pdev->CompatibilityLevel < 1.4)
	    goto nyi;
	if (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
		       prect->q.x == pim3x->Width &&
		       prect->q.y == pim3x->Height))
	    goto nyi;
	return gx_begin_image3x_generic((gx_device *)pdev, pis, pmat, pic,
					prect, pdcolor, pcpath, mem,
					pdf_image3x_make_mid,
					pdf_image3x_make_mcde, pinfo);
    }
    case 4: {
	/* Try to convert the image to a plain masked image. */
	gx_drawing_color icolor;

	if (pdf_convert_image4_to_image1(pdev, pis, pdcolor,
					 (const gs_image4_t *)pic,
					 &image[0].type1, &icolor) >= 0) {
	    /* Undo the pop of the NI stack if necessary. */
	    if (pnamed)
		cos_array_add_object(pdev->NI_stack, COS_OBJECT(pnamed));
	    return pdf_begin_typed_image(pdev, pis, pmat,
					 (gs_image_common_t *)&image[0].type1,
					 prect, &icolor, pcpath, mem,
					 pinfo, context);
	}
	/* No luck.  Masked images require PDF 1.3 or higher. */
	if (pdev->CompatibilityLevel < 1.3)
	    goto nyi;
	image[0].type4 = *(const gs_image4_t *)pic;
	break;
    }
    default:
	goto nyi;
    }
    pim = (const gs_pixel_image_t *)pic;
    format = pim->format;
    switch (format) {
    case gs_image_format_chunky:
    case gs_image_format_component_planar:
	break;
    default:
	goto nyi;
    }
    /* AR5 on Windows doesn't support 0-size images. Skipping. */
    if (pim->Width == 0 || pim->Height == 0)
	goto nyi;
    /* PDF doesn't support images with more than 8 bits per component. */
    if (pim->BitsPerComponent > 8)
	goto nyi;
    pcs = pim->ColorSpace;
    num_components = (is_mask ? 1 : gs_color_space_num_components(pcs));

    code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
	return code;
    code = pdf_put_clip_path(pdev, pcpath);
    if (code < 0)
	return code;
    if (context == PDF_IMAGE_TYPE3_MASK) {
	/*
	 * The soft mask for an ImageType 3x image uses a DevicePixel
	 * color space, which pdf_color_space() can't handle.  Patch it
	 * to DeviceGray here.
	 */
	gs_cspace_init_DeviceGray(&cs_gray_temp);
	pcs = &cs_gray_temp;
    } else if (is_mask)
	code = pdf_prepare_imagemask(pdev, pis, pdcolor);
    else
	code = pdf_prepare_image(pdev, pis);
    if (code < 0)
	goto nyi;
    if (prect)
	rect = *prect;
    else {
	rect.p.x = rect.p.y = 0;
	rect.q.x = pim->Width, rect.q.y = pim->Height;
    }
    pie = gs_alloc_struct(mem, pdf_image_enum, &st_pdf_image_enum,
			  "pdf_begin_image");
    if (pie == 0)
	return_error(gs_error_VMerror);
    memset(pie, 0, sizeof(*pie)); /* cleanup entirely for GC to work in all cases. */
    *pinfo = (gx_image_enum_common_t *) pie;
    gx_image_enum_common_init(*pinfo, (const gs_data_image_t *) pim,
			      (context == PDF_IMAGE_TYPE3_MASK ?
			       &pdf_image_object_enum_procs :
			       &pdf_image_enum_procs),
			      (gx_device *)pdev, num_components, format);
    pie->memory = mem;
    width = rect.q.x - rect.p.x;
    pie->width = width;
    height = rect.q.y - rect.p.y;
    pie->bits_per_pixel =
	pim->BitsPerComponent * num_components / pie->num_planes;
    pie->rows_left = height;
    nbytes = (((ulong) pie->width * pie->bits_per_pixel + 7) >> 3) *
	pie->num_planes * pie->rows_left;
    /* Don't in-line the image if it is named. */
    in_line &= nbytes <= MAX_INLINE_IMAGE_BYTES && pnamed == 0;
    if (rect.p.x != 0 || rect.p.y != 0 ||
	rect.q.x != pim->Width || rect.q.y != pim->Height ||
	(is_mask && pim->CombineWithColor)
	/* Color space setup used to be done here: see SRZB comment below. */
	) {
	gs_free_object(mem, pie, "pdf_begin_image");
	goto nyi;
    }
    if (pmat == 0)
	pmat = &ctm_only(pis);
    {
	gs_matrix mat;
	gs_matrix bmat;
	int code;

	pdf_make_bitmap_matrix(&bmat, -rect.p.x, -rect.p.y,
			       pim->Width, pim->Height, height);
	if ((code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
	    (code = gs_matrix_multiply(&bmat, &mat, &mat)) < 0 ||
	    (code = gs_matrix_multiply(&mat, pmat, &pie->mat)) < 0
	    ) {
	    gs_free_object(mem, pie, "pdf_begin_image");
	    return code;
	}
	/* AR3,AR4 show no image when CTM is singular; AR5 reports an error */
	if (pie->mat.xx * pie->mat.yy == pie->mat.xy * pie->mat.yx) {
	    gs_free_object(mem, pie, "pdf_begin_image");
	    goto nyi;
	}
    }
    alt_writer_count = (in_line || 
				    (pim->Width <= 64 && pim->Height <= 64) ||
				    pdev->transfer_not_identity ? 1 : 2);
    image[1] = image[0];
    if ((code = pdf_begin_write_image(pdev, &pie->writer, gs_no_id, width,
				      height, pnamed, in_line, alt_writer_count)) < 0 ||
	/*
	 * Some regrettable PostScript code (such as LanguageLevel 1 output
	 * from Microsoft's PSCRIPT.DLL driver) misuses the transfer
	 * function to accomplish the equivalent of indexed color.
	 * Downsampling (well, only averaging) or JPEG compression are not
	 * compatible with this.  Play it safe by using only lossless
	 * filters if the transfer function(s) is/are other than the
	 * identity.
	 */
	(code = (pie->writer.alt_writer_count == 1 ?
		 psdf_setup_lossless_filters((gx_device_psdf *) pdev,
					     &pie->writer.binary[0],
					     &image[0].pixel) :
		 psdf_setup_image_filters((gx_device_psdf *) pdev,
					  &pie->writer.binary[0], &image[0].pixel,
					  pmat, pis, true))) < 0 ||
	/* SRZB 2001-04-25/Bl
	 * Since psdf_setup_image_filters may change the color space
	 * (in case of pdev->params.ConvertCMYKImagesToRGB == true),
	 * we postpone the selection of the PDF color space to here:
	 */
	(!is_mask &&
	 (code = pdf_color_space(pdev, &cs_value, &pranges,
				 image[0].pixel.ColorSpace,
				 (in_line ? &pdf_color_space_names_short :
				  &pdf_color_space_names), in_line)) < 0)
	)
	goto fail;
    if (pie->writer.alt_writer_count > 1) {
        code = pdf_make_alt_stream(pdev, &pie->writer.binary[1]);
        if (code)
            goto fail;
	code = psdf_setup_image_filters((gx_device_psdf *) pdev,
				  &pie->writer.binary[1], &image[1].pixel,
				  pmat, pis, false);
        if (code)
            goto fail;
	pie->writer.alt_writer_count = 2;
    }
    for (i = 0; i < pie->writer.alt_writer_count; i++) {
	if (pranges) {
	    /* Rescale the Decode values for the image data. */
	    const gs_range_t *pr = pranges;
	    float *decode = image[i].pixel.Decode;
	    int j;

	    for (j = 0; j < num_components; ++j, ++pr, decode += 2) {
		double vmin = decode[0], vmax = decode[1];
		double base = pr->rmin, factor = pr->rmax - base;

		decode[1] = (vmax - vmin) / factor + (vmin - base);
		decode[0] = vmin - base;
	    }
	}
        if ((code = pdf_begin_image_data(pdev, &pie->writer,
				         (const gs_pixel_image_t *)&image[i],
				         &cs_value, i)) < 0)
  	    goto fail;
    }
    if (pie->writer.alt_writer_count == 2) {
        psdf_setup_compression_chooser(&pie->writer.binary[2], 
	     (gx_device_psdf *)pdev, pim->Width, pim->Height, 
	     num_components, pim->BitsPerComponent);
	pie->writer.alt_writer_count = 3;
    }
    return 0;
 fail:
    /****** SHOULD FREE STRUCTURES AND CLEAN UP HERE ******/
    /* Fall back to the default implementation. */
 nyi:
    return gx_default_begin_typed_image
	((gx_device *)pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
	 pinfo);
}
int
gdev_pdf_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    return pdf_begin_typed_image((gx_device_pdf *)dev, pis, pmat, pic, prect,
				 pdcolor, pcpath, mem, pinfo,
				 PDF_IMAGE_DEFAULT);
}

/* ---------------- All images ---------------- */

/* Process the next piece of an image. */
private int
pdf_image_plane_data_alt(gx_image_enum_common_t * info,
		     const gx_image_plane_t * planes, int height,
		     int *rows_used, int alt_writer_index)
{
    pdf_image_enum *pie = (pdf_image_enum *) info;
    int h = height;
    int y;
    /****** DOESN'T HANDLE IMAGES WITH VARYING WIDTH PER PLANE ******/
    uint width_bits = pie->width * pie->plane_depths[0];
    /****** DOESN'T HANDLE NON-ZERO data_x CORRECTLY ******/
    uint bcount = (width_bits + 7) >> 3;
    uint ignore;
    int nplanes = pie->num_planes;
    int status = 0;

    if (h > pie->rows_left)
	h = pie->rows_left;
    for (y = 0; y < h; ++y) {
	if (nplanes > 1) {
	    /*
	     * We flip images in blocks, and each block except the last one
	     * must contain an integral number of pixels.  The easiest way
	     * to meet this condition is for all blocks except the last to
	     * be a multiple of 3 source bytes (guaranteeing an integral
	     * number of 1/2/4/8/12-bit samples), i.e., 3*nplanes flipped
	     * bytes.  This requires a buffer of at least
	     * 3*GS_IMAGE_MAX_COMPONENTS bytes.
	     */
	    int pi;
	    uint count = bcount;
	    uint offset = 0;
#define ROW_BYTES max(200 /*arbitrary*/, 3 * GS_IMAGE_MAX_COMPONENTS)
	    const byte *bit_planes[GS_IMAGE_MAX_COMPONENTS];
	    int block_bytes = ROW_BYTES / (3 * nplanes) * 3;
	    byte row[ROW_BYTES];

	    for (pi = 0; pi < nplanes; ++pi)
		bit_planes[pi] = planes[pi].data + planes[pi].raster * y;
	    while (count) {
		uint flip_count;
		uint flipped_count;

		if (count >= block_bytes) {
		    flip_count = block_bytes;
		    flipped_count = block_bytes * nplanes;
		} else {
		    flip_count = count;
		    flipped_count =
			(width_bits % (block_bytes * 8) * nplanes + 7) >> 3;
		}
		image_flip_planes(row, bit_planes, offset, flip_count,
				  nplanes, pie->plane_depths[0]);
		status = sputs(pie->writer.binary[alt_writer_index].strm, row, 
			       flipped_count, &ignore);
		if (status < 0)
		    break;
		offset += flip_count;
		count -= flip_count;
	    }
	} else {
	    status = sputs(pie->writer.binary[alt_writer_index].strm,
			   planes[0].data + planes[0].raster * y, bcount,
			   &ignore);
	}
	if (status < 0)
	    break;
    }
    *rows_used = h;
    if (status < 0)
	return_error(gs_error_ioerror);
    return !pie->rows_left;
#undef ROW_BYTES
}

private int
pdf_image_plane_data(gx_image_enum_common_t * info,
		     const gx_image_plane_t * planes, int height,
		     int *rows_used)
{
    pdf_image_enum *pie = (pdf_image_enum *) info;
    int i;
    for (i = 0; i < pie->writer.alt_writer_count; i++) {
        int code = pdf_image_plane_data_alt(info, planes, height, rows_used, i);
        if (code)
            return code;
    }
    pie->rows_left -= *rows_used;
    if (pie->writer.alt_writer_count > 1)
        pdf_choose_compression(&pie->writer, false);
    return !pie->rows_left;
}

/* Clean up by releasing the buffers. */
private int
pdf_image_end_image_data(gx_image_enum_common_t * info, bool draw_last,
			 bool do_image)
{
    gx_device_pdf *pdev = (gx_device_pdf *)info->dev;
    pdf_image_enum *pie = (pdf_image_enum *)info;
    int height = pie->writer.height;
    int data_height = height - pie->rows_left;
    int code;

    if (pie->writer.pres)
	((pdf_x_object_t *)pie->writer.pres)->data_height = data_height;
    else if (data_height > 0)
	pdf_put_image_matrix(pdev, &pie->mat, (double)data_height / height);
    if (data_height > 0) {
	code = pdf_complete_image_data(pdev, &pie->writer, data_height,
			pie->width, pie->bits_per_pixel);
	if (code < 0)
	    return code;
	code = pdf_end_image_binary(pdev, &pie->writer, data_height);
	if (code < 0)
 	    return code;
	code = pdf_end_write_image(pdev, &pie->writer);
	switch (code) {
	default:
	    return code;	/* error */
	case 1:
	    code = 0;
	    break;
	case 0:
	    if (do_image)
		code = pdf_do_image(pdev, pie->writer.pres, &pie->mat, true);
	}
    }
    gs_free_object(pie->memory, pie, "pdf_end_image");
    return code;
}

/* End a normal image, drawing it. */
private int
pdf_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    return pdf_image_end_image_data(info, draw_last, true);
}

/* ---------------- Type 3/3x images ---------------- */

/*
 * For both types of masked images, we create temporary dummy (null) devices
 * that forward the begin_typed_image call to the implementation above.
 */
private int
pdf_make_mxd(gx_device **pmxdev, gx_device *tdev, gs_memory_t *mem)
{
    gx_device *fdev;
    int code = gs_copydevice(&fdev, (const gx_device *)&gs_null_device, mem);

    if (code < 0)
	return code;
    gx_device_set_target((gx_device_forward *)fdev, tdev);
    *pmxdev = fdev;
    return 0;
}

/* End the mask of an ImageType 3 image, not drawing it. */
private int
pdf_image_end_image_object(gx_image_enum_common_t * info, bool draw_last)
{
    return pdf_image_end_image_data(info, draw_last, false);
}

/* ---------------- Type 3 images ---------------- */

/* Implement the mask image device. */
private dev_proc_begin_typed_image(pdf_mid_begin_typed_image);
private int
pdf_image3_make_mid(gx_device **pmidev, gx_device *dev, int width, int height,
		    gs_memory_t *mem)
{
    int code = pdf_make_mxd(pmidev, dev, mem);

    if (code < 0)
	return code;
    set_dev_proc(*pmidev, begin_typed_image, pdf_mid_begin_typed_image);
    return 0;
}
private int
pdf_mid_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
			  const gs_matrix *pmat, const gs_image_common_t *pic,
			  const gs_int_rect * prect,
			  const gx_drawing_color * pdcolor,
			  const gx_clip_path * pcpath, gs_memory_t * mem,
			  gx_image_enum_common_t ** pinfo)
{
    /* The target of the null device is the pdfwrite device. */
    gx_device_pdf *const pdev = (gx_device_pdf *)
	((gx_device_null *)dev)->target;
    int code = pdf_begin_typed_image
	(pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem, pinfo,
	 PDF_IMAGE_TYPE3_MASK);

    if (code < 0)
	return code;
    if ((*pinfo)->procs != &pdf_image_object_enum_procs) {
	/* We couldn't handle the mask image.  Bail out. */
	/* (This is never supposed to happen.) */
	return_error(gs_error_rangecheck);
    }
    return code;
}

/* Implement the mask clip device. */
private int
pdf_image3_make_mcde(gx_device *dev, const gs_imager_state *pis,
		     const gs_matrix *pmat, const gs_image_common_t *pic,
		     const gs_int_rect *prect, const gx_drawing_color *pdcolor,
		     const gx_clip_path *pcpath, gs_memory_t *mem,
		     gx_image_enum_common_t **pinfo,
		     gx_device **pmcdev, gx_device *midev,
		     gx_image_enum_common_t *pminfo,
		     const gs_int_point *origin)
{
    int code = pdf_make_mxd(pmcdev, midev, mem);
    pdf_image_enum *pmie;
    pdf_image_enum *pmce;
    cos_stream_t *pmcs;

    if (code < 0)
	return code;
    code = pdf_begin_typed_image
	((gx_device_pdf *)dev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
	 pinfo, PDF_IMAGE_TYPE3_DATA);
    if (code < 0)
	return code;
    /* Add the /Mask entry to the image dictionary. */
    if ((*pinfo)->procs != &pdf_image_enum_procs) {
	/* We couldn't handle the image.  Bail out. */
	gx_image_end(*pinfo, false);
	gs_free_object(mem, *pmcdev, "pdf_image3_make_mcde");
	return_error(gs_error_rangecheck);
    }
    pmie = (pdf_image_enum *)pminfo;
    pmce = (pdf_image_enum *)(*pinfo);
    pmcs = (cos_stream_t *)pmce->writer.pres->object;
    return cos_dict_put_c_key_object(cos_stream_dict(pmcs), "/Mask",
				     pmie->writer.pres->object);
}

/* ---------------- Type 3x images ---------------- */

/* Implement the mask image device. */
private int
pdf_image3x_make_mid(gx_device **pmidev, gx_device *dev, int width, int height,
		     int depth, gs_memory_t *mem)
{
    int code = pdf_make_mxd(pmidev, dev, mem);

    if (code < 0)
	return code;
    set_dev_proc(*pmidev, begin_typed_image, pdf_mid_begin_typed_image);
    return 0;
}

/* Implement the mask clip device. */
private int
pdf_image3x_make_mcde(gx_device *dev, const gs_imager_state *pis,
		      const gs_matrix *pmat, const gs_image_common_t *pic,
		      const gs_int_rect *prect,
		      const gx_drawing_color *pdcolor,
		      const gx_clip_path *pcpath, gs_memory_t *mem,
		      gx_image_enum_common_t **pinfo,
		      gx_device **pmcdev, gx_device *midev[2],
		      gx_image_enum_common_t *pminfo[2],
		      const gs_int_point origin[2],
		      const gs_image3x_t *pim)
{
    int code;
    pdf_image_enum *pmie;
    pdf_image_enum *pmce;
    cos_stream_t *pmcs;
    int i;
    const gs_image3x_mask_t *pixm;

    if (midev[0]) {
	if (midev[1])
	    return_error(gs_error_rangecheck);
	i = 0, pixm = &pim->Opacity;
    } else if (midev[1])
	i = 1, pixm = &pim->Shape;
    else
	return_error(gs_error_rangecheck);
    code = pdf_make_mxd(pmcdev, midev[i], mem);
    if (code < 0)
	return code;
    code = pdf_begin_typed_image
	((gx_device_pdf *)dev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
	 pinfo, PDF_IMAGE_TYPE3_DATA);
    if (code < 0)
	return code;
    if ((*pinfo)->procs != &pdf_image_enum_procs) {
	/* We couldn't handle the image.  Bail out. */
	gx_image_end(*pinfo, false);
	gs_free_object(mem, *pmcdev, "pdf_image3x_make_mcde");
	return_error(gs_error_rangecheck);
    }
    pmie = (pdf_image_enum *)pminfo[i];
    pmce = (pdf_image_enum *)(*pinfo);
    pmcs = (cos_stream_t *)pmce->writer.pres->object;
    /*
     * Add the SMask entry to the image dictionary, and, if needed,
     * the Matte entry to the mask dictionary.
     */
    if (pixm->has_Matte) {
	int num_components =
	    gs_color_space_num_components(pim->ColorSpace);

	code = cos_dict_put_c_key_floats(
				(cos_dict_t *)pmie->writer.pres->object,
				"/Matte", pixm->Matte,
				num_components);
	if (code < 0)
	    return code;
    }
    return cos_dict_put_c_key_object(cos_stream_dict(pmcs), "/SMask",
				     pmie->writer.pres->object);
}
