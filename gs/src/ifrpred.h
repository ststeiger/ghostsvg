/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.

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
/* filter_read_predictor prototype */

#ifndef ifrpred_INCLUDED
#  define ifrpred_INCLUDED

/* Exported by zfdecode.c for zfzlib.c */
int filter_read_predictor(P4(i_ctx_t *i_ctx_p, int npop,
			     const stream_template * template,
			     stream_state * st));

#endif /* ifrpred_INCLUDED */
