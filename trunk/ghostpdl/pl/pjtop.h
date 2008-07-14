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

/* pjtop.h */
/* Interface to PJL parser */

#ifndef pjtop_INCLUDED
#  define pjtop_INCLUDED

#include "pltop.h"

/*
 * Generic PJL data types which may be subclassed by specific implementations
 */
struct pjl_implementation_s;	/* fwd decl */

#ifndef PJL_ENVAR_T
#define PJL_ENVAR_T
typedef char pjl_envvar_t;	/* opaque decl */
#endif /* PJL_ENVVAR_T */

/*
 * Define PJL-specific procedures
 */
/* return the current setting of a pjl environment variable.  The
   input parameter should be the exact string used in PJLTRM.
   Sample Usage: 
         char *formlines = pjl_get_envvar(pst, "formlines");
	 if (formlines) {
	     int fl = atoi(formlines);
	     .
	     .
	 }
   Both variables and values are case insensitive.
*/
typedef pjl_envvar_t * (*pjl_proc_get_envvar_t)(pl_interp_instance_t *pli, const char *pjl_var);
pjl_envvar_t * pjl_proc_get_envvar(pl_interp_instance_t *pli, const char *pjl_var);

/* compare a pjl environment variable to a string values. */
typedef int (*pjl_proc_compare_t)(pl_interp_instance_t *pli, const pjl_envvar_t *s1, const char *s2);
int pjl_proc_compare(pl_interp_instance_t *pli, const pjl_envvar_t *s1, const char *s2);

/* map a pjl symbol set name to a pcl integer */
typedef int (*pjl_proc_map_pjl_sym_to_pcl_sym_t)(pl_interp_instance_t *pli, const pjl_envvar_t *symname);
int pjl_proc_map_pjl_sym_to_pcl_sym(pl_interp_instance_t *pli, const pjl_envvar_t *symname);

/* pjl environment variable to integer. */
typedef int (*pjl_proc_vartoi_t)(pl_interp_instance_t *pli, const pjl_envvar_t *s);
int pjl_proc_vartoi(pl_interp_instance_t *pli, const pjl_envvar_t *s);

/* pjl envioronment variable to float. */
typedef floatp (*pjl_proc_vartof_t)(pl_interp_instance_t *pli, const pjl_envvar_t *s);
floatp pjl_proc_vartof(pl_interp_instance_t *pli, const pjl_envvar_t *s);

/* convert a pjl designated fontsource to a subdirectory pathname. */
typedef char * (*pjl_proc_fontsource_to_path_t)(pl_interp_instance_t *pli,
  const pjl_envvar_t *fontsource);
char * pjl_proc_fontsource_to_path(pl_interp_instance_t *pli,
  const pjl_envvar_t *fontsource);

/* Change to next highest priority font source.  The following events
   automatically change the value of the FONTSOURCE variable to the
   next highest priority font source containing a default-marked font:
   if the currently set font source is C, C1, or C2, and the cartridge
   is removed from the printer; if the currently set font source is S
   and all soft fonts are deleted; if the currently set font source is
   S, while the currently set font number is the highest-numbered soft
   font, and any soft font is deleted.  Ideally this function would be
   solely responsible for these activities, with the current
   architecture we depend in part on pcl to keep up with font resource
   bookkeeping.  PJLTRM is not careful to define distinguish between
   default font source vs environment font source.  Both are set when
   the font source is changed. */
typedef void (*pjl_proc_set_next_fontsource_t)(pl_interp_instance_t *pli);
void pjl_proc_set_next_fontsource(pl_interp_instance_t *pli);

/* tell pjl that a soft font is being deleted.  We return 0 if no
   state change is required and 1 if the pdl should update its font
   state.  (see discussion above) */
typedef int (*pjl_proc_register_permanent_soft_font_deletion_t)(pl_interp_instance_t *pli,
 int font_number);
int pjl_proc_register_permanent_soft_font_deletion(pl_interp_instance_t *pli,
 int font_number);

/* request that pjl add a soft font and return a pjl font number for
   the font.   */
typedef int (*pjl_proc_register_permanent_soft_font_addition_t)(pl_interp_instance_t *pli);
int pjl_proc_register_permanent_soft_font_addition(pl_interp_instance_t *pli);

typedef long int (*pjl_proc_get_named_resource_size_t)(pl_interp_instance_t *pli, char *name);
long int pjl_proc_get_named_resource_size(pl_interp_instance_t *pli, char *name);

typedef int (*pjl_proc_get_named_resource_t)(pl_interp_instance_t *pli, char *name, byte *data);
int pjl_proc_get_named_resource(pl_interp_instance_t *pli, char *name, byte *data);

/*
 * Define a generic interpreter implementation
 */
typedef struct pjl_implementation_s {
  pl_interp_implementation_t               pl;   /* MUST BE FIRST generic impl */ 

  /* PJL-specific procedure vector */
  pjl_proc_get_envvar_t                    proc_get_envvar;
  pjl_proc_compare_t                       proc_compare;
  pjl_proc_map_pjl_sym_to_pcl_sym_t        proc_map_pjl_sym_to_pcl_sym;
  pjl_proc_vartoi_t                        proc_vartoi;
  pjl_proc_vartof_t                        proc_vartof;
  pjl_proc_fontsource_to_path_t            proc_fontsource_to_path;
  pjl_proc_set_next_fontsource_t           proc_set_next_fontsource;
  pjl_proc_register_permanent_soft_font_deletion_t proc_register_permanent_soft_font_deletion;
  pjl_proc_register_permanent_soft_font_addition_t proc_register_permanent_soft_font_addition;
  pjl_proc_get_named_resource_size_t       proc_get_named_resource_size;
  pjl_proc_get_named_resource_t            proc_get_named_resource;


} pjl_implementation_t;


#endif				/* pjtop_INCLUDED */

