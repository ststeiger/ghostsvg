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

/* pcjob.c -  PCL5 job control commands */
#include "std.h"
#include "gx.h"
#include "gsmemory.h"
#include "gsmatrix.h"		/* for gsdevice.h */
#include "gsdevice.h"
#include "pcommand.h"
#include "pcursor.h"            /* for pcl_home_cursor() */
#include "pcstate.h"
#include "pcparam.h"
#include "pcdraw.h"
#include "pcpage.h"
#include "pjtop.h"

/* Commands */

int
pcl_do_printer_reset(pcl_state_t *pcs)
{
    if ( pcs->macro_level )
	return e_Range;	/* not allowed inside macro */

    /* reset the other parser in case we have gotten the
       pcl_printer_reset while in gl/2 mode. */
    pcl_implicit_gl2_finish(pcs);
    /* Print any partial page if not pclxl snippet mode. */
    if (pcs->end_page == pcl_end_page_top) {
	int code = pcl_end_page_if_marked(pcs);
	if ( code < 0 )
	    return code;
	/* if duplex start on the front side of the paper */
	if ( pcs->duplex )
	    put_param1_bool(pcs, "FirstSide", true);
    }
    /* unload fonts */
    
    /* Reset to user default state. */
    return pcl_do_resets(pcs, pcl_reset_printer);
}
    
static int /* ESC E */
pcl_printer_reset(pcl_args_t *pargs, pcl_state_t *pcs)
{	
    return pcl_do_printer_reset(pcs);
}

static int /* ESC % -12345 X */
pcl_exit_language(pcl_args_t *pargs, pcl_state_t *pcs)
{	if ( int_arg(pargs) != -12345 )
	   return e_Range;
	{ int code = pcl_printer_reset(pargs, pcs);
	  return (code < 0 ? code : e_ExitLanguage);
	}
}

static int /* ESC & l <num_copies> X */
pcl_number_of_copies(pcl_args_t *pargs, pcl_state_t *pcs)
{	int i = int_arg(pargs);
	if ( i < 1 )
	  return 0;
	pcs->num_copies = i;
	return put_param1_int(pcs, "NumCopies", i);
}

static int /* ESC & l <sd_enum> S */
pcl_simplex_duplex_print(pcl_args_t *pargs, pcl_state_t *pcs)
{	int code;
	bool reopen = false;

	/* oddly the command goes to the next page irrespective of
           arguments */
	code = pcl_end_page_if_marked(pcs);
	if ( code < 0 )
	    return code;
	pcl_home_cursor(pcs);
	switch ( int_arg(pargs) )
	  {
	  case 0:
	    pcs->duplex = false;
	    break;
	  case 1:
	    pcs->duplex = true;
	    pcs->bind_short_edge = false;
	    break;
	  case 2:
	    pcs->duplex = true;
	    pcs->bind_short_edge = true;
	    break;
	  default:
	    return 0;
	  }
	code = put_param1_bool(pcs, "Duplex", pcs->duplex);
	switch ( code )
	  {
	  case 1:		/* reopen device */
	    reopen = true;
	  case 0:
	    break;
	  case gs_error_undefined:
	    return 0;
	  default:		/* error */
	    if ( code < 0 )
	      return code;
	  }
	code = put_param1_bool(pcs, "BindShortEdge", pcs->bind_short_edge);
	switch ( code )
	  {
	  case 1:		/* reopen device */
	    reopen = true;
	  case 0:
	  case gs_error_undefined:
	    break;
	  default:		/* error */
	    if ( code < 0 )
	      return code;
	  }
	return (reopen ? gs_setdevice_no_erase(pcs->pgs,
					       gs_currentdevice(pcs->pgs)) :
		0);
}

static int /* ESC & a <side_enum> G */
pcl_duplex_page_side_select(pcl_args_t *pargs, pcl_state_t *pcs)
{	uint i = uint_arg(pargs);
	int code;

	/* oddly the command goes to the next page irrespective of
           arguments */
	code = pcl_end_page_if_marked(pcs);
	if ( code < 0 )
	    return code;
	pcl_home_cursor(pcs);

	if ( i > 2 )
	  return 0;

	if ( i > 0 && pcs->duplex )
	  put_param1_bool(pcs, "FirstSide", i == 1);
	return 0;
}

static int /* ESC & l 1 T */
pcl_job_separation(pcl_args_t *pargs, pcl_state_t *pcs)
{	int i = int_arg(pargs);
	if ( i != 1 )
	  return 0;
	/**** NEED A DRIVER PROCEDURE FOR END-OF-JOB ****/
	return 0;
}

static int /* ESC & l <bin_enum> G */
pcl_output_bin_selection(pcl_args_t *pargs, pcl_state_t *pcs)
{	uint i = uint_arg(pargs);
	if ( i < 1 || i > 2 )
	  return e_Range;
	return put_param1_int(pcs, "OutputBin", i);
}

static int /* ESC & u <upi> B */
pcl_set_unit_of_measure(pcl_args_t *pargs, pcl_state_t *pcs)
{	int num = int_arg(pargs);

	if ( num <= 96 )
	  num = 96;
	else if ( num >= 7200 )
	  num = 7200;
	else if ( 7200 % num != 0 )
	{	/* Pick the exact divisor of 7200 with the smallest */
		/* relative error. */
		static const int values[] = {
		  96, 100, 120, 144, 150, 160, 180, 200, 225, 240, 288,
		  300, 360, 400, 450, 480, 600, 720, 800, 900,
		  1200, 1440, 1800, 2400, 3600, 7200
		};
		const int *p = values;

		while ( num > p[1] ) p++;
		/* Now *p < num < p[1]. */
		if ( (p[1] - (float)num) / p[1] < ((float)num - *p) / *p )
		  p++;
		num = *p;
	}
	pcs->uom_cp = pcl_coord_scale / num;
	return 0;
}

/* Initialization */
static int
pcjob_do_registration(pcl_parser_state_t *pcl_parser_state, gs_memory_t *mem)
{		/* Register commands */
	DEFINE_ESCAPE_ARGS('E', "Printer Reset", pcl_printer_reset, pca_in_rtl)
	DEFINE_CLASS('%')
	  {0, 'X', {pcl_exit_language, pca_neg_ok|pca_big_error|pca_in_rtl}},
	END_CLASS
	DEFINE_CLASS('&')
	  {'l', 'X',
	     PCL_COMMAND("Number of Copies", pcl_number_of_copies,
			 pca_neg_ignore|pca_big_clamp)},
	  {'l', 'S',
	     PCL_COMMAND("Simplex/Duplex Print", pcl_simplex_duplex_print,
			 pca_neg_ignore|pca_big_ignore)},
	  {'a', 'G',
	     PCL_COMMAND("Duplex Page Side Select",
			 pcl_duplex_page_side_select,
			 pca_neg_ignore|pca_big_ignore)},
	  {'l', 'T',
	     PCL_COMMAND("Job Separation", pcl_job_separation,
			 pca_neg_error|pca_big_error)},
	  {'l', 'G',
	     PCL_COMMAND("Output Bin Selection", pcl_output_bin_selection,
			 pca_neg_error|pca_big_error)},
	  {'u', 'D',
	     PCL_COMMAND("Set Unit of Measure", pcl_set_unit_of_measure,
			 pca_neg_error|pca_big_error)},
	END_CLASS	  
	return 0;
}
static void
pcjob_do_reset(pcl_state_t *pcs, pcl_reset_type_t type)
{	
    if ( type & (pcl_reset_initial | pcl_reset_printer) ) { 
        pcs->num_copies = pjl_proc_vartoi(pcs->pjls,
            pjl_proc_get_envvar(pcs->pjls, "copies"));
        pcs->duplex = !pjl_proc_compare(pcs->pjls,
            pjl_proc_get_envvar(pcs->pjls, "duplex"), "off") ? false : true;
        pcs->bind_short_edge = !pjl_proc_compare(pcs->pjls,
            pjl_proc_get_envvar(pcs->pjls, "binding"), "longedge") ? false : true;
        pcs->back_side = false;
        pcs->output_bin = 1;
    }
    if ( type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay) ) {
        /* rtl always uses native units for user units.  The hp
           documentation does not say what to do if the resolution is
           assymetric... */
        pcl_args_t args;
        if ( pcs->personality == rtl )
            arg_set_uint(&args,
                         gs_currentdevice(pcs->pgs)->HWResolution[0]);
        else
            arg_set_uint(&args, 300);
        pcl_set_unit_of_measure(&args, pcs);
    }
}
const pcl_init_t pcjob_init = {
  pcjob_do_registration, pcjob_do_reset, 0
};
