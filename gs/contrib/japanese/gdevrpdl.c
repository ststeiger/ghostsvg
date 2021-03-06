/* Copyright (C) 1999, 2000 Norihito Ohmori.

   Ghostscript driver for Ricoh RPDL printer.

   This software is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the GNU General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   this software, but only under the conditions described in the GNU
   General Public License.  A copy of this license is supposed to have been
   given to you along with this software so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.
 */

/*$Id: gdevrpdl.c,v 1.1 2002/10/12 23:24:34 tillkamppeter Exp $ */
/* Ricoh RPDL driver for Ghostscript */

#include "gdevlprn.h"
#include "gdevlips.h"

#define DPI 240

/* The device descriptors */
static dev_proc_open_device(rpdl_open);
static dev_proc_close_device(rpdl_close);
static dev_proc_print_page_copies(rpdl_print_page_copies);
static dev_proc_image_out(rpdl_image_out);
static void rpdl_printer_initialize(gx_device_printer * pdev, FILE * prn_stream, int num_copies);
static void rpdl_paper_set(gx_device_printer * pdev, FILE * prn_stream);

static gx_device_procs rpdl_prn_procs =
lprn_procs(rpdl_open, gdev_prn_output_page, rpdl_close);

gx_device_lprn far_data gs_rpdl_device =
lprn_device(gx_device_lprn, rpdl_prn_procs, "rpdl",
	    DPI, DPI, 0.0, 0.0, 0.0, 0.0, 1,
	    rpdl_print_page_copies, rpdl_image_out);

#define ppdev ((gx_device_printer *)pdev)

/* Open the printer. */
static int
rpdl_open(gx_device * pdev)
{
    int xdpi = pdev->x_pixels_per_inch;
    int ydpi = pdev->y_pixels_per_inch;

    /* Resolution Check */
    if (xdpi != ydpi)
	return_error(gs_error_rangecheck);
    if (xdpi != 240 && xdpi != 400 && xdpi != 600)
	return_error(gs_error_rangecheck);

    return gdev_prn_open(pdev);
}

static int
rpdl_close(gx_device * pdev)
{
    gdev_prn_open_printer(pdev, 1);
    if (ppdev->Duplex && (pdev->PageCount & 1)) {
	fprintf(ppdev->file, "\014"); /* Form Feed */
    }
    return gdev_prn_close(pdev);
}

static int
rpdl_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_coipes)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int code = 0;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    /* printer initialize */
    if (pdev->PageCount == 0)
	rpdl_printer_initialize(pdev, prn_stream, num_coipes);

    if (!(lprn->CompBuf = gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), bpl * 3 / 2 + 1, maxY, "rpdl_print_page_copies(CompBuf)")))
	return_error(gs_error_VMerror);

    lprn->NegativePrint = false; /* Not Support */

    code = lprn_print_image(pdev, prn_stream);
    if (code < 0)
	return code;

    gs_free(gs_lib_ctx_get_non_gc_memory_t(), lprn->CompBuf, bpl * 3 / 2 + 1, maxY, "rpdl_print_page_copies(CompBuf)");

    fprintf(prn_stream, "\014");	/* Form  Feed */

    return code;
}

/* Output data */
static void
rpdl_image_out(gx_device_printer * pdev, FILE * prn_stream, int x, int y, int width, int height)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int Len;

    Len = lips_mode3format_encode(lprn->TmpBuf, lprn->CompBuf, width / 8 * height);

    if (Len < width / 8 * height) {
      if (pdev->x_pixels_per_inch == 240) {
	/* Unit Size is 1/720 inch */
	fprintf(prn_stream, "\033\022G3,%d,%d,,4,%d,%d,%d@",
		width, height, x * 3, y * 3, Len);
      } else {
	fprintf(prn_stream, "\033\022G3,%d,%d,,4,%d,%d,%d@",
		width, height, x, y, Len);
      }
      fwrite(lprn->CompBuf, 1, Len, prn_stream);
    } else { /* compression result is bad. So, raw data is used. */
      if (pdev->x_pixels_per_inch == 240) {
	/* Unit Size is 1/720 inch */
	fprintf(prn_stream, "\033\022G3,%d,%d,,,%d,%d@",
		width, height, x * 3, y * 3);
	fwrite(lprn->TmpBuf, 1, width / 8 * height, prn_stream);
      } else {
	fprintf(prn_stream, "\033\022G3,%d,%d,,,%d,%d@",
		width, height, x, y);
	fwrite(lprn->TmpBuf, 1, width / 8 * height, prn_stream);
      }
    }
}

/* output printer initialize code */

/* ------ Internal routines ------ */

static void
rpdl_printer_initialize(gx_device_printer * pdev, FILE * prn_stream, int num_copies)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int xdpi = (int) pdev->x_pixels_per_inch;

    /* Initialize */
    fprintf(prn_stream, "\033\022!@R00\033 "); /* Change to RPDL Mode */
    fprintf(prn_stream, "\0334"); /* Graphic Mode kaijyo */
    fprintf(prn_stream, "\033\022YP,2 "); /* Select RPDL Mode */
    fprintf(prn_stream, "\033\022YB,2 "); /* Printable Area - Maximum */
    fprintf(prn_stream, "\033\022YK,1 "); /* Left Margin - 0 mm */
    fprintf(prn_stream, "\033\022YL,1 "); /* Top Margin - 0 mm */
    fprintf(prn_stream, "\033\022YM,1 "); /* 100 % */
    fprintf(prn_stream, "\033\022YQ,2 "); /* Page Length - Maximum */
    
    /* Paper Size Selection */
    rpdl_paper_set(pdev, prn_stream);

    /* Option Setting */
    /* Duplex Setting */
    if (pdev->Duplex_set > 0) {
	if (pdev->Duplex) {
	    fprintf(prn_stream, "\033\02261,");
	    if (lprn->Tumble == 0)
		fprintf(prn_stream, "\033\022YA01,2 ");
	    else
		fprintf(prn_stream, "\033\022YA01,1 ");
	} else
	    fprintf(prn_stream, "\033\02260,");
    }

    /* Resolution and Unit Setting */
    /* Resolution Seting */
    switch(xdpi) {
    case 600:
      fprintf(prn_stream, "\033\022YA04,3 ");
      break;
    case 400:
      fprintf(prn_stream, "\033\022YA04,1 ");
      break;
    default: /* 240 dpi */
      fprintf(prn_stream, "\033\022YA04,2 ");
      break;
    }

    /* Unit Setting */
    /* Graphics Unit */
    switch(xdpi) {
    case 600:
      fprintf(prn_stream, "\033\022YW,3 ");
      break;
    case 400:
      fprintf(prn_stream, "\033\022YW,1 ");
      break;
    default: /* 240 dpi */
      fprintf(prn_stream, "\033\022YW,2 ");
      break;
    }

    /* Spacing Unit */
    switch(xdpi) {
    case 600:
      fprintf(prn_stream, "\033\022Q5 ");
      break;
    case 400:
      fprintf(prn_stream, "\033\022Q4 ");
      break;
    default: /* 240 dpi */
      fprintf(prn_stream, "\033\022Q0 ");
      break;
    }

    /* Cartecian Unit */
    switch(xdpi) {
    case 600:
      fprintf(prn_stream, "\033\022#4 ");
      break;
    case 400:
      fprintf(prn_stream, "\033\022#2 ");
      break;
    }

    /* Paper Setting */
    if (pdev->MediaSize[0] > pdev->MediaSize[1])
      fprintf(prn_stream, "\033\022D2 "); /* landscape */
    else
      fprintf(prn_stream, "\033\022D1 "); /* portrait */

    /* Number of Copies */
    fprintf(prn_stream, "\033\022N%d ", num_copies);
}

static void
rpdl_paper_set(gx_device_printer * pdev, FILE * prn_stream)
{
    int width, height, w, h, wp, hp;

    width = pdev->MediaSize[0];
    height = pdev->MediaSize[1];

    if (width < height) {
	w = width;
	h = height;
	wp = width / 72.0 * pdev->x_pixels_per_inch;
	hp = height / 72.0 * pdev->y_pixels_per_inch;
    } else {
	w = height;
	h = width;
	wp = height / 72.0 * pdev->y_pixels_per_inch;
	hp = width / 72.0 * pdev->x_pixels_per_inch;
    }

    if (w == 1684 && h == 2380) /* A1 */
      fprintf(prn_stream, "\033\02251@A1R\033 ");
    else if (w == 1190 && h == 1684) { /* A2 */
      fprintf(prn_stream, "\033\02251@A2R\033 ");
      fprintf(prn_stream, "\033\02251@A2\033 ");
    } else if (w == 842 && h == 1190) { /* A3 */
      fprintf(prn_stream, "\033\02251@A3R\033 ");
      fprintf(prn_stream, "\033\02251@A3\033 ");
    } else if (w == 595 && h == 842) { /* A4 */
      fprintf(prn_stream, "\033\02251@A4R\033 ");
      fprintf(prn_stream, "\033\02251@A4\033 ");
    } else if (w == 597 && h == 842) { /* A4 */
      fprintf(prn_stream, "\033\02251@A4R\033 ");
      fprintf(prn_stream, "\033\02251@A4\033 ");
    } else if (w == 421 && h == 595) { /* A5 */
      fprintf(prn_stream, "\033\02251@A5R\033 ");
      fprintf(prn_stream, "\033\02251@A5\033 ");
    } else if (w == 297 && h == 421) { /* A6 */
      fprintf(prn_stream, "\033\02251@A6R\033 ");
      fprintf(prn_stream, "\033\02251@A6\033 ");
    } else if (w == 729 && h == 1032) { /* B4 */
      fprintf(prn_stream, "\033\02251@B4R\033 ");
      fprintf(prn_stream, "\033\02251@B4\033 ");
    } else if (w == 516 && h == 729) { /* B5 */
      fprintf(prn_stream, "\033\02251@B5R\033 ");
      fprintf(prn_stream, "\033\02251@B5\033 ");
    } else if (w == 363 && h == 516) { /* B6 */
      fprintf(prn_stream, "\033\02251@A6R\033 ");
      fprintf(prn_stream, "\033\02251@A6\033 ");
    } else if (w == 612 && h == 792) { /* Letter */
      fprintf(prn_stream, "\033\02251@LTR\033 ");
      fprintf(prn_stream, "\033\02251@LT\033 ");
    } else if (w == 612 && h == 1008) { /* Legal */
      fprintf(prn_stream, "\033\02251@LGR\033 ");
      fprintf(prn_stream, "\033\02251@LG\033 ");
    } else if (w == 396 && h == 612) { /* Half Letter */
      fprintf(prn_stream, "\033\02251@HLR\033 ");
      fprintf(prn_stream, "\033\02251@HLT\033 ");
    } else if (w == 792 && h == 1224) { /* Ledger */
      fprintf(prn_stream, "\033\02251@DLT\033 ");
      fprintf(prn_stream, "\033\02251@DLR\033 ");
    } else { /* Free Size (mm) */
      fprintf(prn_stream, "\033\022?5%d,%d\033 ",
	      (int)((w * 25.4) / 72),
	      (int)((h * 25.4) / 72));
    }
}
