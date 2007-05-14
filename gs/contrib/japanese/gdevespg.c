/* Copyright (C) 1999 Norihito Ohmori.

   Ghostscript driver for EPSON ESC/Page printer.

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

/*$Id: gdevespg.c $ */
/* EPSON ESC/Page driver for Ghostscript */

/*
   Main part of this driver is taked from Hiroshi Narimatsu's
   Ghostscript driver, epag-3.08.
   <ftp://ftp.humblesoft.com/pub/epag-3.08.tar.gz>
 */

#include "gdevlprn.h"
#include "gdevlips.h"

#define DPI 300

/* The device descriptors */
private dev_proc_open_device(escpage_open);
private dev_proc_open_device(lp2000_open);
private dev_proc_close_device(escpage_close);
private dev_proc_print_page_copies(escpage_print_page_copies);
private dev_proc_print_page_copies(lp2000_print_page_copies);
private dev_proc_image_out(escpage_image_out);

private void escpage_printer_initialize(gx_device_printer * pdev, FILE * fp, int);
private void escpage_paper_set(gx_device_printer * pdev, FILE * fp);

private gx_device_procs lp2000_prn_procs =
lprn_procs(lp2000_open, gdev_prn_output_page, gdev_prn_close);

private gx_device_procs escpage_prn_procs =
lprn_procs(escpage_open, gdev_prn_output_page, escpage_close);

gx_device_lprn far_data gs_lp2000_device =
lprn_device(gx_device_lprn, lp2000_prn_procs, "lp2000",
	    DPI, DPI, 0.0, 0.0, 0.0, 0.0, 1,
	    lp2000_print_page_copies, escpage_image_out);

gx_device_lprn far_data gs_escpage_device =
lprn_duplex_device(gx_device_lprn, escpage_prn_procs, "escpage",
		   DPI, DPI, 0.0, 0.0, 0.0, 0.0, 1,
		   escpage_print_page_copies, escpage_image_out);

#define ppdev ((gx_device_printer *)pdev)

static char *epson_remote_start = "\033\001@EJL \r\n";

/* Open the printer. */
private int
escpage_open(gx_device * pdev)
{
    int xdpi = pdev->x_pixels_per_inch;
    int ydpi = pdev->y_pixels_per_inch;

    /* Resolution Check */
    if (xdpi != ydpi)
	return_error(gs_error_rangecheck);
    if (xdpi < 60 || xdpi > 600)
	return_error(gs_error_rangecheck);

    return gdev_prn_open(pdev);
}

private int
lp2000_open(gx_device * pdev)
{
    int xdpi = pdev->x_pixels_per_inch;
    int ydpi = pdev->y_pixels_per_inch;

    /* Resolution Check */
    if (xdpi != ydpi)
	return_error(gs_error_rangecheck);
    if (xdpi < 60 || xdpi > 300)
	return_error(gs_error_rangecheck);

    return gdev_prn_open(pdev);
}

private int
escpage_close(gx_device * pdev)
{
    gdev_prn_open_printer(pdev, 1);
    if (ppdev->Duplex && (pdev->PageCount & 1)) {
	fprintf(ppdev->file, "%c0dpsE", GS);
    }
    fputs(epson_remote_start, ppdev->file);
    fputs(epson_remote_start, ppdev->file);
    return gdev_prn_close(pdev);
}

private int
escpage_print_page_copies(gx_device_printer * pdev, FILE * fp, int num_coipes)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    if (pdev->PageCount == 0) {
	double xDpi = pdev->x_pixels_per_inch;

	/* Goto REMOTE MODE */
	fputs(epson_remote_start, fp);
	fprintf(fp, "@EJL SELECT LANGUAGE=ESC/PAGE\r\n");

	/* RIT (Resolution Improvement Technology) Setting */
	if (lprn->RITOff)
	    fprintf(fp, "@EJL SET RI=OFF\r\n");
	else
	    fprintf(fp, "@EJL SET RI=ON\r\n");

	/* Resolution Setting */
	fprintf(fp, "@EJL SET RS=%s\r\n", xDpi > 300 ? "FN" : "QK");
	fprintf(fp, "@EJL ENTER LANGUAGE=ESC/PAGE\r\n");
    }
    return lp2000_print_page_copies(pdev, fp, num_coipes);
}

private int
lp2000_print_page_copies(gx_device_printer * pdev, FILE * fp, int num_coipes)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int code = 0;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    /* printer initialize */
    if (pdev->PageCount == 0)
	escpage_printer_initialize(pdev, fp, num_coipes);

    if (!(lprn->CompBuf = gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), bpl * 3 / 2 + 1, maxY, "lp2000_print_page_copies(CompBuf)")))
	return_error(gs_error_VMerror);

    if (lprn->NegativePrint) {
	fprintf(fp, "%c1dmG", GS);
	fprintf(fp, "%c0;0;%d;%d;0rG", GS, pdev->width, pdev->height);
	fprintf(fp, "%c2owE", GS);
    }
    code = lprn_print_image(pdev, fp);
    if (code < 0)
	return code;

    gs_free(gs_lib_ctx_get_non_gc_memory_t(), lprn->CompBuf, bpl * 3 / 2 + 1, maxY, "lp2000_print_page_copies(CompBuf)");

    if (pdev->Duplex)
	fprintf(fp, "%c0dpsE", GS);
    else
	fprintf(fp, "\014");	/* eject page */
    return code;
}

/* Output data */
private void
escpage_image_out(gx_device_printer * pdev, FILE * fp, int x, int y, int width, int height)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int Len;

    fprintf(fp, "%c%dY%c%dX", GS, y, GS, x);

    Len = lips_mode3format_encode(lprn->TmpBuf, lprn->CompBuf, width / 8 * height);

    fprintf(fp, "%c%d;%d;%d;0bi{I", GS, Len,
	    width, height);
    fwrite(lprn->CompBuf, 1, Len, fp);

    if (lprn->ShowBubble) {
	fprintf(fp, "%c0dmG", GS);
	fprintf(fp, "%c%d;%d;%d;%d;0rG", GS,
		x, y, x + width, y + height);
    }
}

/* output printer initialize code */

/* ------ Internal routines ------ */

static char can_inits[] =
{
    ESC, 'S', ESC, ESC, 'S', 034,
    ESC, 'z', 0, 0,		/* Go to ESC/Page  */
    GS, 'r', 'h', 'E',		/* hard reset         */
    GS, '1', 'm', 'm', 'E',	/* Page Memory Mode (for LP-3000/7000/7000G) */
    GS, '3', 'b', 'c', 'I',	/* Style of Compression  */
    GS, '0', ';', '0', 'i', 'u', 'E', /* Paper Feed Unit (Auto) */
};

private void
escpage_printer_initialize(gx_device_printer * pdev, FILE * fp, int copies)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    double xDpi, yDpi;

    xDpi = pdev->x_pixels_per_inch;
    yDpi = pdev->y_pixels_per_inch;

    /* Initialize */
    fwrite(can_inits, sizeof(can_inits), 1, fp);
    /* Duplex Setting */
    if (pdev->Duplex_set > 0) {
	if (pdev->Duplex) {
	    fprintf(fp, "%c1sdE", GS);
	    if (lprn->Tumble == 0)
		fprintf(fp, "%c0bdE", GS);
	    else
		fprintf(fp, "%c1bdE", GS);
	} else
	    fprintf(fp, "%c0sdE", GS);
    }
    /* Set the Size Unit  */
    fprintf(fp, "%c0;%4.2fmuE", GS, 72.0 / xDpi);
    /* Set the Resolution */
    fprintf(fp, "%c0;%d;%ddrE", GS, (int)(xDpi + 0.5), (int)(yDpi + 0.5));
    /* Set the Paper Size */
    escpage_paper_set(pdev, fp);
    /* Set the desired number of Copies */
    fprintf(fp, "%c%dcoO", GS, copies < 256 ? copies : 255);
    /* Set the Position to (0, 0) */
    fprintf(fp, "%c0;0loE", GS);
}

typedef struct {
    int width;			/* paper width (unit: point) */
    int height;			/* paper height (unit: point) */
    int escpage;		/* number of papersize in ESC/PAGE */
} EpagPaperTable;

static EpagPaperTable epagPaperTable[] =
{
    {842, 1190, 13},		/* A3 */
    {595, 842, 14},		/* A4 */
    {597, 842, 14},		/* A4 (8.3x11.7 inch) */
    {421, 595, 15},		/* A5 */
    {297, 421, 16},		/* A6 */
    {729, 1032, 24},		/* B4 JIS */
    {516, 729, 25},		/* B5 JIS */
    {612, 792, 30},		/* Letter */
    {396, 612, 31},		/* Half Letter */
    {612, 1008, 32},		/* Legal */
    {522, 756, 33},		/* Executive */
    {612, 936, 34},		/* Government Legal */
    {576, 756, 35},		/* Government Letter */
    {792, 1224, 36},		/* Ledger */
    {593, 935, 37},		/* F4 */
    {284, 419, 38},		/* PostCard */
    {933, 1369, 72},		/* A3 NOBI */
    {279, 540, 80},		/* Monarch */
    {297, 684, 81},		/* Commercial 10 */
    {312, 624, 90},		/* DL */
    {459, 649, 91},		/* C5 */
    {0, 0, -1},			/* Undefined */
};

private void
escpage_paper_set(gx_device_printer * pdev, FILE * fp)
{
    int width, height, w, h, wp, hp, bLandscape;
    EpagPaperTable *pt;

    width = pdev->MediaSize[0];
    height = pdev->MediaSize[1];

    if (width < height) {
	bLandscape = 0;
	w = width;
	h = height;
	wp = width / 72.0 * pdev->x_pixels_per_inch;
	hp = height / 72.0 * pdev->y_pixels_per_inch;
    } else {
	bLandscape = 1;
	w = height;
	h = width;
	wp = height / 72.0 * pdev->y_pixels_per_inch;
	hp = width / 72.0 * pdev->x_pixels_per_inch;
    }

    for (pt = epagPaperTable; pt->escpage > 0; pt++)
	if (pt->width == w && pt->height == h)
	    break;

    fprintf(fp, "%c%d", GS, pt->escpage);
    if (pt->escpage < 0)
	fprintf(fp, ";%d;%d", wp, hp);
    fprintf(fp, "psE");

    fprintf(fp, "%c%dpoE", GS, bLandscape);
}
