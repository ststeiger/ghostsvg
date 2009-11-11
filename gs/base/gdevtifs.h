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
/* Definitions for writing TIFF file formats. */

#ifndef gdevtifs_INCLUDED
#  define gdevtifs_INCLUDED

#include <tiffio.h>	/* must be first, because gdevio.h re-#defines "printf"
			   which is used in a function __attribute__ by
			   tiffio.h */
#include "gdevprn.h"

/* ================ Implementation ================ */

typedef struct gx_device_tiff_s {
    gx_device_common;
    gx_prn_device_common;
    bool  BigEndian;            /* true = big endian; false = little endian*/

    TIFF *tif;			/* TIFF file opened on gx_device_common.file */
} gx_device_tiff;

dev_proc_output_page(tiff_output_page);
dev_proc_open_device(tiff_open);
dev_proc_close_device(tiff_close);
dev_proc_get_params(tiff_get_params);
dev_proc_put_params(tiff_put_params);

/*
 * Open a TIFF file for writing from a file descriptor.
 */
TIFF * tiff_from_filep(const char *name, FILE *filep, int big_endian);

int tiff_print_page(gx_device_printer *dev, TIFF *tif);

int tiff_set_fields_for_printer(gx_device_printer *pdev,
				TIFF *tif,
				long max_strip_size);

int gdev_tiff_begin_page(gx_device_tiff *tfdev,
			 FILE *file,
			 long max_strip_size);


#endif /* gdevtifs_INCLUDED */
