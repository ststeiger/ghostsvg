/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

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
/* Interface to generic monochrome H-P DeskJet/LaserJet driver */

#ifndef gdevdljm_INCLUDED
#  define gdevdljm_INCLUDED

#include "gdevpcl.h"

/*
 * The notion that there is such a thing as a "PCL printer" is a fiction: no
 * two "PCL" printers, even at the same PCL level, have identical command
 * sets.  (The H-P documentation isn't fully accurate either; for example,
 * it doesn't reveal that the DeskJet printers implement anything beyond PCL
 * 3.)
 *
 * This file contains feature definitions for a generic monochrome PCL
 * driver (gdevdljm.c), and the specific feature values for all such
 * printers that Ghostscript currently supports.
 */

/* Printer spacing capabilities.  Include at most one of these. */
#define PCL_NO_SPACING   0	/* no vertical spacing capability, must be 0 */
#define PCL3_SPACING     1	/* <ESC>*p+<n>Y (PCL 3) */
#define PCL4_SPACING     2	/* <ESC>*b<n>Y (PCL 4) */
#define PCL5_SPACING     4	/* <ESC>*b<n>Y and clear seed row (PCL 5) */
/* The following is only used internally. */
#define PCL_ANY_SPACING\
  (PCL3_SPACING | PCL4_SPACING | PCL5_SPACING)

/* Individual printer properties.  Any subset of these may be included. */
#define PCL_MODE_2_COMPRESSION       8	/* compression mode 2 supported */
					/* (PCL 4) */
#define PCL_MODE_3_COMPRESSION      16	/* compression modes 2 & 3 supported */
					/* (PCL 5) */
#define PCL_END_GRAPHICS_DOES_RESET 32	/* <esc>*rB resets all parameters */
#define PCL_HAS_DUPLEX              64	/* <esc>&l<duplex>S supported */
#define PCL_CAN_SET_PAPER_SIZE     128	/* <esc>&l<sizecode>A supported */

/* Shorthands for the most common spacing/compression combinations. */
#define PCL_MODE0 PCL3_SPACING
#define PCL_MODE0NS PCL_NO_SPACING
#define PCL_MODE2 (PCL4_SPACING | PCL_MODE_2_COMPRESSION)
#define PCL_MODE2P (PCL_NO_SPACING | PCL_MODE_2_COMPRESSION)
#define PCL_MODE3 (PCL5_SPACING | PCL_MODE_3_COMPRESSION)
#define PCL_MODE3NS (PCL_NO_SPACING | PCL_MODE_3_COMPRESSION)

/* Parameters for the printers we know about. */

     /* H-P DeskJet */
#define PCL_DJ_FEATURES\
  (PCL_MODE2 |\
   PCL_END_GRAPHICS_DOES_RESET | PCL_CAN_SET_PAPER_SIZE)

     /* H-P DeskJet 500 */
#define PCL_DJ500_FEATURES\
  (PCL_MODE3 |\
   PCL_END_GRAPHICS_DOES_RESET | PCL_CAN_SET_PAPER_SIZE)

     /* Kyocera FS-600 */
#define PCL_FS600_FEATURES\
  (PCL_MODE3 |\
   PCL_CAN_SET_PAPER_SIZE)

     /* H-P original LaserJet */
#define PCL_LJ_FEATURES\
  (PCL_MODE0)

     /* H-P LaserJet Plus */
#define PCL_LJplus_FEATURES\
  (PCL_MODE0)

     /* H-P LaserJet IIp, IId */
#define PCL_LJ2p_FEATURES\
  (PCL_MODE2P |\
   PCL_CAN_SET_PAPER_SIZE)

     /* H-P LaserJet III* */
#define PCL_LJ3_FEATURES\
  (PCL_MODE3 |\
   PCL_CAN_SET_PAPER_SIZE)

     /* H-P LaserJet IIId */
#define PCL_LJ3D_FEATURES\
  (PCL_MODE3 |\
   PCL_HAS_DUPLEX | PCL_CAN_SET_PAPER_SIZE)

     /* H-P LaserJet 4 */
#define PCL_LJ4_FEATURES\
  (PCL_MODE3 |\
   PCL_CAN_SET_PAPER_SIZE)

     /* H-P LaserJet 4d */
#define PCL_LJ4D_FEATURES\
  (PCL_MODE3 |\
   PCL_HAS_DUPLEX | PCL_CAN_SET_PAPER_SIZE)

     /* H-P 2563B line printer */
#define PCL_LP2563B_FEATURES\
  (PCL_MODE0NS |\
   PCL_CAN_SET_PAPER_SIZE)

     /* OCE 9050 line printer */
#define	PCL_OCE9050_FEATURES\
  (PCL_MODE3NS |\
   PCL_CAN_SET_PAPER_SIZE)

/* ---------------- Procedures ---------------- */

/* Send a page to the printer. */
int dljet_mono_print_page(P5(
	gx_device_printer * pdev,	/* from device-specific _print_page */
	FILE * prn_stream,		/* ibid. */
	int dots_per_inch,		/* may be a multiple of y resolution */
	int features,			/* as defined above */
	const char *page_init		/* page initialization string */
			     ));

#endif /* gdevdljm_INCLUDED */
