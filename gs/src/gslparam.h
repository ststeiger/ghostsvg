/* Copyright (C) 1995, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$RCSfile$ $Revision$ */
/* Line parameter definitions */

#ifndef gslparam_INCLUDED
#  define gslparam_INCLUDED

/* Line cap values */
typedef enum {
    gs_cap_butt = 0,
    gs_cap_round = 1,
    gs_cap_square = 2,
    gs_cap_triangle = 3		/* not supported by PostScript */
} gs_line_cap;

#define gs_line_cap_max 3

/* Line join values */
typedef enum {
    gs_join_miter = 0,
    gs_join_round = 1,
    gs_join_bevel = 2,
    gs_join_none = 3,		/* not supported by PostScript */
    gs_join_triangle = 4	/* not supported by PostScript */
} gs_line_join;

#define gs_line_join_max 4

#endif /* gslparam_INCLUDED */
