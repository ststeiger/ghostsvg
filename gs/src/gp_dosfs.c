/* Copyright (C) 1992, 1993, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/* $Id$ */
/* Common routines for MS-DOS (any compiler) and DesqView/X, */
/* which has a MS-DOS-like file system. */
#include "dos_.h"
#include "gx.h"
#include "gp.h"

/* ------ Printer accessing ------ */

/* Put a printer file (which might be stdout) into binary or text mode. */
/* This is not a standard gp procedure, */
/* but all MS-DOS configurations need it. */
void
gp_set_file_binary(int prnfno, bool binary)
{
    union REGS regs;

    regs.h.ah = 0x44;		/* ioctl */
    regs.h.al = 0;		/* get device info */
    regs.rshort.bx = prnfno;
    intdos(&regs, &regs);
    if (regs.rshort.cflag != 0 || !(regs.h.dl & 0x80))
	return;			/* error, or not a device */
    if (binary)
	regs.h.dl |= 0x20;	/* binary (no ^Z intervention) */
    else
	regs.h.dl &= ~0x20;	/* text */
    regs.h.dh = 0;
    regs.h.ah = 0x44;		/* ioctl */
    regs.h.al = 1;		/* set device info */
    intdos(&regs, &regs);
}

/* ------ File accessing ------ */

/* Set a file into binary or text mode. */
int
gp_setmode_binary(FILE * pfile, bool binary)
{
    gp_set_file_binary(fileno(pfile), binary);
    return 0;			/* Fake out dos return status */
}

/* ------ File names ------ */

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ';';

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char gp_fmode_binary_suffix[] = "b";

/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "rb";
const char gp_fmode_wb[] = "wb";

/* Answer whether a path_string can meaningfully have a prefix applied */
bool
gp_pathstring_not_bare(const char *fname, unsigned len)
{   /* A file name is not bare if it contains a drive specifications	*/
    /* (second character is a :) or if it starts with a '.', '/' or '\\'*/
    /* or it contains '/../' (parent reference)				*/
    if ((len > 0) && ((*fname == '/') || (*fname == '\\') ||
	  (*fname == '.') || ((len > 2) && (fname[1] == ':'))))
	return true;
    while (len-- > 3) {
        int c = *fname++;

	if (((c == '/') || (c == '\\')) &&
	    ((len >= 3) && (bytes_compare(fname, 2, "..", 2) == 0) &&
			((fname[2] == '/') || (fname[2] == '\\'))))
	    return true;
    }
    return false;
}

/* Answer whether the file_name references the directory	*/
/* containing the specified path (parent). 			*/
bool
gp_file_name_references_parent(const char *fname, unsigned len)
{
    int i = 0, last_sep_pos = -1;

    /* A file name references its parent directory if it starts */
    /* with ../ or ..\  or if one of these strings follows / or \ */
    while (i < len) {
	if (fname[i] == '/' || fname[i] == '\\') {
	    last_sep_pos = i++;
	    continue;
	}
	if (fname[i++] != '.')
	    continue;
        if (i > last_sep_pos + 2 || (i < len && fname[i] != '.'))
	    continue;
	i++;
	/* have separator followed by .. */
	if (i < len && (fname[i] == '/' || fname[i++] == '\\'))
	    return true;
    }
    return false;
}


/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name. The prefix directory/device is examined to	*/
/* determine if a separator is needed and may return an empty string	*/
const char *
gp_file_name_concat_string(const char *prefix, unsigned plen)
{
    if (plen > 0)
	switch (prefix[plen - 1]) {
	    case ':':
	    case '/':
	    case '\\':
		return "";
	};
    return "/";
}

/* -------------- Helpers for gp_file_name_combine_generic ------------- */

uint gp_file_name_root(const char *fname, uint len)
{   int i = 0;
    
    if (len == 0)
	return 0;
    if (len > 1 && fname[0] == '\\' && fname[1] == '\\') {
	/* A network path: "//server/share/" */
	int k = 0;

	for (i = 2; i < len; i++)
	    if (fname[i] == '\\')
		if (k++) {
		    i++;
		    break;
		}
    } else if (fname[0] == '/' || fname[0] == '\\') {
	/* Absolute with no drive. */
	i = 1;
    } else if (len > 1 && fname[1] == ':') {
	/* Absolute with a drive. */
	i = (len > 2 && (fname[2] == '/' || fname[2] == '\\') ? 3 : 2);
    }
    return i;
}

uint gs_file_name_check_separator(const char *fname, int len, const char *item)
{   if (len > 0) {
	if (fname[0] == '/' || fname[0] == '\\')
	    return 1;
    } else if (len < 0) {
	if (fname[-1] == '/' || fname[-1] == '\\')
	    return 1;
    }
    return 0;
}

bool gp_file_name_is_parent(const char *fname, uint len)
{   return len == 2 && fname[0] == '.' && fname[1] == '.';
}

bool gp_file_name_is_current(const char *fname, uint len)
{   return len == 1 && fname[0] == '.';
}

const char *gp_file_name_separator(void)
{   return "/";
}

const char *gp_file_name_directory_separator(void)
{   return "/";
}

const char *gp_file_name_parent(void)
{   return "..";
}

const char *gp_file_name_current(void)
{   return ".";
}

bool gp_file_name_is_partent_allowed(void)
{   return true;
}

bool gp_file_name_is_empty_item_meanful(void)
{   return false;
}

gp_file_name_combine_result
gp_file_name_combine(const char *prefix, uint plen, 
	    const char *fname, uint flen, char *buffer, uint *blen)
{
    return gp_file_name_combine_generic(prefix, plen, 
	    fname, flen, buffer, blen);
}
