/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
  
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
/* Generic operand stack API */

#ifndef iosdata_INCLUDED
#  define iosdata_INCLUDED

#include "isdata.h"

/* Define the operand stack structure. */
/* Currently this is just a generic ref stack. */
typedef struct op_stack_s {

    ref_stack_t stack;		/* the actual operand stack */

} op_stack_t;

#define public_st_op_stack()	/* in interp.c */\
  gs_public_st_suffix_add0(st_op_stack, op_stack_t, "op_stack_t",\
    op_stack_enum_ptrs, op_stack_reloc_ptrs, st_ref_stack)
#define st_op_stack_num_ptrs st_ref_stack_num_ptrs

#endif /* iosdata_INCLUDED */
