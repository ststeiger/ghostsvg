/* Copyright (C) 1994, 2000 Aladdin Enterprises.  All rights reserved.

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
/* TIFF and fax devices */
#include "gdevprn.h"
#include "gdevtifs.h"		/* for TIFF output only */
#include "strimpl.h"
#include "scfx.h"
#include "gdevtfax.h"		/* prototypes */

/* Define the device parameters. */
#define X_DPI 204
#define Y_DPI 196
#define LINE_SIZE ((X_DPI * 101 / 10 + 7) / 8)	/* max bytes per line */

/* The device descriptors */

private dev_proc_get_params(tfax_get_params);
private dev_proc_put_params(tfax_put_params);
private dev_proc_print_page(faxg3_print_page);
private dev_proc_print_page(faxg32d_print_page);
private dev_proc_print_page(faxg4_print_page);
private dev_proc_print_page(tiffcrle_print_page);
private dev_proc_print_page(tiffg3_print_page);
private dev_proc_print_page(tiffg32d_print_page);
private dev_proc_print_page(tiffg4_print_page);

struct gx_device_tfax_s {
    gx_device_common;
    gx_prn_device_common;
    int adjust_width;		/* 0 = no adjust, 1 = adjust to fax values */
    long MaxStripSize;		/* 0 = no limit, other is UNCOMPRESSED limit */
    gdev_tiff_state tiff;	/* for TIFF output only */
};
typedef struct gx_device_tfax_s gx_device_tfax;

/* Define procedures that adjust the paper size. */
private const gx_device_procs gdev_fax_std_procs =
    prn_params_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		     tfax_get_params, tfax_put_params);

#define TFAX_DEVICE(dname, print_page)\
{\
    prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, dname,\
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,\
			X_DPI, Y_DPI,\
			0, 0, 0, 0,	/* margins */\
			1, print_page),\
    1,				/* adjust_width */\
    0				/* unlimited strip size byte count */\
}

const gx_device_tfax gs_faxg3_device =
    TFAX_DEVICE("faxg3", faxg3_print_page);

const gx_device_tfax gs_faxg32d_device =
    TFAX_DEVICE("faxg32d", faxg32d_print_page);

const gx_device_tfax gs_faxg4_device =
    TFAX_DEVICE("faxg4", faxg4_print_page);

const gx_device_tfax gs_tiffcrle_device =
    TFAX_DEVICE("tiffcrle", tiffcrle_print_page);

const gx_device_tfax gs_tiffg3_device =
    TFAX_DEVICE("tiffg3", tiffg3_print_page);

const gx_device_tfax gs_tiffg32d_device =
    TFAX_DEVICE("tiffg32d", tiffg32d_print_page);

const gx_device_tfax gs_tiffg4_device =
    TFAX_DEVICE("tiffg4", tiffg4_print_page);

/* Open the device. */
/* This is no longer needed: we retain it for client backward compatibility. */
int
gdev_fax_open(gx_device * dev)
{
    return gdev_prn_open(dev);
}

/* Get/put the AdjustWidth  and the MaxStripSize (for TIFF) parameters. */
private int
tfax_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int code = gdev_prn_get_params(dev, plist);
    int ecode = code;

    if ((code = param_write_int(plist, "AdjustWidth", &tfdev->adjust_width)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "MaxStripSize", &tfdev->MaxStripSize)) < 0)
        ecode = code;

    return ecode;
}
private int
tfax_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int ecode = 0;
    int code;
    int aw = tfdev->adjust_width;
    long mss = tfdev->MaxStripSize;
    const char *param_name;

    switch (code = param_read_int(plist, (param_name = "AdjustWidth"), &aw)) {
        case 0:
	    if (aw >= 0 && aw <= 1)
		break;
	    code = gs_error_rangecheck;
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }
    switch (code = param_read_long(plist, (param_name = "MaxStripSize"), &mss)) {
        case 0:
	    /*
	     * Strip must be large enough to accommodate a raster line.
	     * If the max strip size is too small, we still write a single
	     * line per strip rather than giving an error.
	     */
	    if (mss >= 0)
	        break;
	    code = gs_error_rangecheck;
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }

    if (ecode < 0)
	return ecode;
    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
	return code;

    tfdev->adjust_width = aw;
    tfdev->MaxStripSize = mss;
    return code;
}

/* Initialize the stream state with a set of default parameters. */
/* These select the same defaults as the CCITTFaxEncode filter, */
/* except we set BlackIs1 = true. */
private void
gdev_fax_init_state_adjust(stream_CFE_state * ss,
			   const gx_device_printer * pdev,
			   int adjust_width)
{
    (*s_CFE_template.set_defaults) ((stream_state *) ss);
    ss->Columns = pdev->width;
    ss->Rows = pdev->height;
    ss->BlackIs1 = true;
    if (adjust_width > 0) {
	/* Adjust the page width to a legal value for fax systems. */
	if (ss->Columns >= 1680 && ss->Columns <= 1736) {
	    /* Adjust width for A4 paper. */
	    ss->Columns = 1728;
	} else if (ss->Columns >= 2000 && ss->Columns <= 2056) {
	    /* Adjust width for B4 paper. */
	    ss->Columns = 2048;
	}
    }
}
void
gdev_fax_init_state(stream_CFE_state * ss, const gx_device_printer * pdev)
{
    gdev_fax_init_state_adjust(ss, pdev, 1);
}
private void
gdev_fax_init_fax_state(stream_CFE_state * ss, const gx_device_printer * pdev)
{
    gdev_fax_init_state_adjust(ss, pdev,
			       ((const gx_device_tfax *)pdev)->adjust_width);
}

/* Send the page to the printer. */
/* Print a page with a specified width, which may differ from	*/
/* the width stored in the device. The TIFF file may have	*/
/* multiple strips of height 'rows'.				*/
private int
gdev_stream_print_page_strips(gx_device_printer * pdev, FILE * prn_stream,
			      const stream_template * temp, stream_state * ss,
			      int width, long rows_per_strip)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)pdev;
    gs_memory_t *mem = pdev->memory;
    long end_strip = rows_per_strip;
    int code;
    stream_cursor_read r;
    stream_cursor_write w;
    int in_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    /*
     * Because of the width adjustment for fax systems, width may
     * be different from (either greater than or less than) pdev->width.
     * Allocate a large enough buffer to account for this.
     */
    int col_size = (width * pdev->color_info.depth + 7) >> 3;
    int max_size = max(in_size, col_size);
    int lnum;
    byte *in;
    byte *out;
    /* If the file is 'nul', don't even do the writes. */
    bool nul = !strcmp(pdev->fname, "nul");

    /* Initialize the common part of the encoder state. */
    ss->template = temp;
    ss->memory = mem;
    /* Now initialize the encoder. */
    code = (*temp->init) (ss);
    if (code < 0)
	return_error(gs_error_limitcheck);	/* bogus, but as good as any */

    /* Allocate the buffers. */
    in = gs_alloc_bytes(mem, temp->min_in_size + max_size + 1,
			"gdev_stream_print_page(in)");
#define OUT_SIZE 1000
    out = gs_alloc_bytes(mem, OUT_SIZE, "gdev_stream_print_page(out)");
    if (in == 0 || out == 0) {
	code = gs_note_error(gs_error_VMerror);
	goto done;
    }
    /* Set up the processing loop. */
    lnum = 0;
    r.ptr = r.limit = in - 1;
    w.ptr = out - 1;
    w.limit = w.ptr + OUT_SIZE;
#undef OUT_SIZE

    /* Process the image. */
    for (;;) {
	int status;

	if (end_strip > pdev->height)
	    end_strip = pdev->height;
	if_debug7('w', "[w]lnum=%d r=0x%lx,0x%lx,0x%lx w=0x%lx,0x%lx,0x%lx\n", lnum,
		  (ulong) in, (ulong) r.ptr, (ulong) r.limit,
		  (ulong) out, (ulong) w.ptr, (ulong) w.limit);
	status = (*temp->process) (ss, &r, &w, lnum == end_strip);
	if_debug7('w', "...%d, r=0x%lx,0x%lx,0x%lx w=0x%lx,0x%lx,0x%lx\n", status,
		  (ulong) in, (ulong) r.ptr, (ulong) r.limit,
		  (ulong) out, (ulong) w.ptr, (ulong) w.limit);
	switch (status) {
	    case 0:		/* need more input data */
		{
		    uint left;

		    if (lnum == pdev->height)
			goto ok;
		    if (lnum == end_strip) {
			if (!nul)
			    fwrite(out, 1, w.ptr + 1 - out, prn_stream);
			w.ptr = out - 1;
			gdev_tiff_end_strip(&tfdev->tiff, prn_stream);
		        end_strip += rows_per_strip;
			/* release and re-initialize the encoder for the next strip */
			if (temp->release != 0)
			    (*temp->release) (ss);
			(*temp->init) (ss);
		    }
		    left = r.limit - r.ptr;
		    memcpy(in, r.ptr + 1, left);
		    gdev_prn_copy_scan_lines(pdev, lnum++, in + left, in_size);
		    /* Note: we use col_size here, not in_size. */
		    if (col_size > in_size) {
			memset(in + left + in_size, 0, col_size - in_size);
		    }
		    r.limit = in + left + col_size - 1;
		    r.ptr = in - 1;
		}
		break;
	    case 1:		/* need to write output */
		if (!nul)
		    fwrite(out, 1, w.ptr + 1 - out, prn_stream);
		w.ptr = out - 1;
		break;
	}
    }

  ok:
    /* Write out any remaining output. */
    if (!nul)
	fwrite(out, 1, w.ptr + 1 - out, prn_stream);
    gdev_tiff_end_strip(&tfdev->tiff, prn_stream);

  done:
    gs_free_object(mem, out, "gdev_stream_print_page(out)");
    gs_free_object(mem, in, "gdev_stream_print_page(in)");
    if (temp->release != 0)
	(*temp->release) (ss);
    return code;
}

/* Print a page with a specified width, which may differ from the */
/* width stored in the device. */
private int
gdev_stream_print_page_width(gx_device_printer * pdev, FILE * prn_stream,
			     const stream_template * temp, stream_state * ss,
			     int width)
{
    return gdev_stream_print_page_strips(pdev, prn_stream, temp, ss,
					 width, pdev->height);
}

private int
gdev_stream_print_page(gx_device_printer * pdev, FILE * prn_stream,
		       const stream_template * temp, stream_state * ss)
{
    return gdev_stream_print_page_width(pdev, prn_stream, temp, ss,
					pdev->width);
}
/* Print a fax page.  Other fax drivers use this. */
int
gdev_fax_print_page(gx_device_printer * pdev, FILE * prn_stream,
		    stream_CFE_state * ss)
{
    return gdev_stream_print_page_width(pdev, prn_stream, &s_CFE_template,
					(stream_state *)ss, ss->Columns);
}

/* Print a fax page.  Other fax drivers use this. */
int
gdev_fax_print_page_stripped(gx_device_printer * pdev, FILE * prn_stream,
		    stream_CFE_state * ss, long rows)
{
    return gdev_stream_print_page_strips(pdev, prn_stream, &s_CFE_template,
					 (stream_state *)ss, ss->Columns,
					 rows);
}

/* Print a 1-D Group 3 page. */
private int
faxg3_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, pdev);
    state.EndOfLine = true;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* Print a 2-D Group 3 page. */
private int
faxg32d_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, pdev);
    state.K = (pdev->y_pixels_per_inch < 100 ? 2 : 4);
    state.EndOfLine = true;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* Print a Group 4 page. */
private int
faxg4_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, pdev);
    state.K = -1;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* ---------------- TIFF output ---------------- */

#include "slzwx.h"
#include "srlx.h"

/* Device descriptors for TIFF formats other than fax. */
private dev_proc_print_page(tifflzw_print_page);
private dev_proc_print_page(tiffpack_print_page);

const gx_device_tfax gs_tifflzw_device =
{prn_device_std_body(gx_device_tfax, prn_std_procs, "tifflzw",
		     DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		     X_DPI, Y_DPI,
		     0, 0, 0, 0,	/* margins */
		     1, tifflzw_print_page)
};

const gx_device_tfax gs_tiffpack_device =
{prn_device_std_body(gx_device_tfax, prn_std_procs, "tiffpack",
		     DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		     X_DPI, Y_DPI,
		     0, 0, 0, 0,	/* margins */
		     1, tiffpack_print_page)
};

/* Define the TIFF directory we use, beyond the standard entries. */
/* NB: this array is sorted by tag number (assumed below) */
typedef struct tiff_mono_directory_s {
    TIFF_dir_entry BitsPerSample;
    TIFF_dir_entry Compression;
    TIFF_dir_entry Photometric;
    TIFF_dir_entry FillOrder;
    TIFF_dir_entry SamplesPerPixel;
    TIFF_dir_entry T4T6Options;
    /* Don't use CleanFaxData. */
    /*  TIFF_dir_entry  CleanFaxData;   */
} tiff_mono_directory;
private const tiff_mono_directory dir_mono_template =
{
    {TIFFTAG_BitsPerSample, TIFF_SHORT, 1, 1},
    {TIFFTAG_Compression, TIFF_SHORT, 1, Compression_CCITT_T4},
    {TIFFTAG_Photometric, TIFF_SHORT, 1, Photometric_min_is_white},
    {TIFFTAG_FillOrder, TIFF_SHORT, 1, FillOrder_LSB2MSB},
    {TIFFTAG_SamplesPerPixel, TIFF_SHORT, 1, 1},
    {TIFFTAG_T4Options, TIFF_LONG, 1, 0},
	/* { TIFFTAG_CleanFaxData,      TIFF_SHORT, 1, CleanFaxData_clean }, */
};

/* Forward references */
private int tfax_begin_page(P4(gx_device_tfax *, FILE *,
			       const tiff_mono_directory *, int));

/* Print a fax-encoded page. */
private int
tifff_print_page(gx_device_printer * dev, FILE * prn_stream,
		 stream_CFE_state * pstate, tiff_mono_directory * pdir)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int code;

    tfax_begin_page(tfdev, prn_stream, pdir, pstate->Columns);
    pstate->FirstBitLowOrder = true;	/* decoders prefer this */
    code = gdev_fax_print_page_stripped(dev, prn_stream, pstate, tfdev->tiff.rows);
    gdev_tiff_end_page(&tfdev->tiff, prn_stream);
    return code;
}
private int
tiffcrle_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_fax_state(&state, dev);
    state.EndOfLine = false;
    state.EncodedByteAlign = true;
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_RLE;
    dir.T4T6Options.tag = TIFFTAG_T4Options;
    dir.T4T6Options.value = T4Options_fill_bits;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}
private int
tiffg3_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_fax_state(&state, dev);
    state.EndOfLine = true;
    state.EncodedByteAlign = true;
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_T4;
    dir.T4T6Options.tag = TIFFTAG_T4Options;
    dir.T4T6Options.value = T4Options_fill_bits;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}
private int
tiffg32d_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_state(&state, dev);
    state.K = (dev->y_pixels_per_inch < 100 ? 2 : 4);
    state.EndOfLine = true;
    state.EncodedByteAlign = true;
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_T4;
    dir.T4T6Options.tag = TIFFTAG_T4Options;
    dir.T4T6Options.value = T4Options_2D_encoding | T4Options_fill_bits;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}
private int
tiffg4_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_state(&state, dev);
    state.K = -1;
    /*state.EncodedByteAlign = false; *//* no fill_bits option for T6 */
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_T6;
    dir.T4T6Options.tag = TIFFTAG_T6Options;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}

/* Print an LZW page. */
private int
tifflzw_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    tiff_mono_directory dir;
    stream_LZW_state state;
    int code;

    dir = dir_mono_template;
    dir.Compression.value = Compression_LZW;
    dir.FillOrder.value = FillOrder_MSB2LSB;
    tfax_begin_page(tfdev, prn_stream, &dir, dev->width);
    state.InitialCodeLength = 8;
    state.FirstBitLowOrder = false;
    state.BlockData = false;
    state.EarlyChange = 0;	/****** CHECK THIS ******/
    code = gdev_stream_print_page(dev, prn_stream, &s_LZWE_template,
				  (stream_state *) & state);
    gdev_tiff_end_page(&tfdev->tiff, prn_stream);
    return code;
}

/* Print a PackBits page. */
private int
tiffpack_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    tiff_mono_directory dir;
    stream_RLE_state state;
    int code;

    dir = dir_mono_template;
    dir.Compression.value = Compression_PackBits;
    dir.FillOrder.value = FillOrder_MSB2LSB;
    tfax_begin_page(tfdev, prn_stream, &dir, dev->width);
    state.EndOfData = false;
    state.record_size = gdev_mem_bytes_per_scan_line((gx_device *) dev);
    code = gdev_stream_print_page(dev, prn_stream, &s_RLE_template,
				  (stream_state *) & state);
    gdev_tiff_end_page(&tfdev->tiff, prn_stream);
    return code;
}

/* Begin a TIFF fax page. */
private int
tfax_begin_page(gx_device_tfax * tfdev, FILE * fp,
		const tiff_mono_directory * pdir, int width)
{
    /* Patch the width to reflect fax page width adjustment. */
    int save_width = tfdev->width;
    int code;

    tfdev->width = width;
    code = gdev_tiff_begin_page((gx_device_printer *) tfdev,
				&tfdev->tiff, fp,
				(const TIFF_dir_entry *)pdir,
				sizeof(*pdir) / sizeof(TIFF_dir_entry),
				NULL, 0, tfdev->MaxStripSize);
    tfdev->width = save_width;
    return code;
}
