/* Copyright (C) 1994, 1995, 1999 Aladdin Enterprises.  All rights reserved.
  
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
/* Prototypes for debugging procedures in idebug.c */

#ifndef idebug_INCLUDED
#  define idebug_INCLUDED

/* Print individual values. */
void debug_print_name(P1(const ref *));
void debug_print_name_index(P1(uint /*name_index_t*/));
void debug_print_ref(P1(const ref *));
void debug_print_ref_packed(P1(const ref_packed *));

/* Dump regions of memory. */
void debug_dump_one_ref(P1(const ref *));
void debug_dump_refs(P3(const ref * from, uint size, const char *msg));
void debug_dump_array(P1(const ref * array));

/* Dump a stack.  Using this requires istack.h. */
#ifndef ref_stack_DEFINED
typedef struct ref_stack_s ref_stack_t;	/* also defined in isdata.h */
#  define ref_stack_DEFINED
#endif
void debug_dump_stack(P2(const ref_stack_t * pstack, const char *msg));

#endif /* idebug_INCLUDED */
