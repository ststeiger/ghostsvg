/* Copyright (C) 2000-2004 artofcode LLC.  All rights reserved.
  
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

/*$Id$ */
/* The dither object abstraction within the Rinkj driver. */

#include "rinkj-dither.h"

void
rinkj_dither_line (RinkjDither *self, unsigned char *dst, const unsigned char *src)
{
  self->dither_line (self, dst, src);
}

void
rinkj_dither_close (RinkjDither *self)
{
  self->close (self);
}
