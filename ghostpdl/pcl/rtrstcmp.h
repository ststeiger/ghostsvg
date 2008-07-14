/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* rtrstcmp.h - interface to the raster decompression code */

#ifndef rtrstcmp_INCLUDED
#define rtrstcmp_INCLUDED

#include "gx.h"
#include "gsstruct.h"

/*
 * Types:
 *
 *     0 - compression mode 0 (no compression), param is size in bytes
 *     1 - compression mode 1 (run length compression), param is size in bytes
 *     2 - compression mode 2 ("Packbits" compression), param is size in bytes
 *     3 - compression mode 3 (delta row compression), param is size in bytes
 *     4 - not used
 *     5 - compression mode 5 (adaptive), param is size in bytes
 *     9 - compression mode 9 (modified delta row), param is size in bytes
 *
 * There is no separate format for repeated rows. The desired effect can be
 * achieve by create a buffer of type 3 with a size of 0 bytes.
 */
typedef enum {
    NO_COMPRESS = 0,
    RUN_LEN_COMPRESS = 1,
    PACKBITS_COMPRESS = 2,
    DELTA_ROW_COMPRESS = 3,
    /* 4 is not used, and indicated as reserved by HP */
    ADAPTIVE_COMPRESS = 5,
    /* 6 - 8 unused */
    MOD_DELTA_ROW_COMPRESS = 9
} pcl_rast_buff_type_t;

/*
 * A seed-row structure. These buffers are used both to pass data to the
 * graphic library image routines, and to retain information on the last row
 * sent to support "delta-row" compression.
 *
 * The is_blank flag is intended as a hint, not as an absolute indication. If
 * it is set, the seed row is known to be blank. However, even if it is not
 * set the seed row may still be blank.
 */
typedef struct  pcl_seed_row_s {
    ushort  size;
    bool    is_blank;
    byte *  pdata;
} pcl_seed_row_t;

/* in rtraster.c */
#define private_st_seed_row_t()                 \
    gs_private_st_ptrs1( st_seed_row_t,         \
                         pcl_seed_row_t,        \
                         "PCL raster seed row", \
                         seed_row_enum_ptrs,    \
                         seed_row_reloc_ptrs,   \
                         pdata                  \
                         )

#define private_st_seed_row_t_element()                 \
    gs_private_st_element( st_seed_row_t_element,       \
                           pcl_seed_row_t,              \
                           "PCL seed row array",        \
                           seed_row_element_enum_ptrs,  \
                           seed_row_element_reloc_ptrs, \
                           st_seed_row_t                \
                           )

/*
 * The array of decompression functions.
 */
extern void (*const pcl_decomp_proc[9 + 1])(pcl_seed_row_t *pout,
					const byte *pin,
					int in_size
                                                 );

#endif			/* rtrstcmp_INCLUDED */
