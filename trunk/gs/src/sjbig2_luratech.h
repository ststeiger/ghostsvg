/* Copyright (C) 2005-2006 artofcode LLC.  All rights reserved.
  
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

/* $Id: sjbig2.h,v 1.6 2005/06/09 07:15:07 giles Exp $ */
/* Definitions for jbig2decode filter - Luratech version */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef sjbig2_luratech_INCLUDED
#  define sjbig2_luratech_INCLUDED

#include "scommon.h"
#include <ldf_jb2.h>

/* JBIG2Decode internal stream state */
typedef struct stream_jbig2decode_state_s
{
    stream_state_common;	/* inherit base object from scommon.h */
    JB2_Handle_Document doc;	/* Luratech JBIG2 codec context */
    unsigned char *global_data;
    unsigned long global_size;
    unsigned char *inbuf;  /* compressed image data */
    unsigned long insize, infill;
    unsigned char *image;  /* decoded image data */
    unsigned long width, height;
    unsigned long row, stride;
    unsigned long offset;  /* next output byte to be returned */
    JB2_Error error;
}
stream_jbig2decode_state;

#define private_st_jbig2decode_state()	\
  gs_private_st_simple(st_jbig2decode_state, stream_jbig2decode_state,\
    "jbig2decode filter state")
extern const stream_template s_jbig2decode_template;

/* call in to process the JBIG2Globals parameter */
public int
s_jbig2decode_make_global_data(byte *data, uint size, void **result);
public int
s_jbig2decode_set_global_data(stream_state *ss, void *data);
public void
s_jbig2decode_free_global_data(void *data);

#endif /* sjbig2_luratech_INCLUDED */
