/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* Miscellaneous support for platform facilities */

#include "unistd_.h"
#include "fcntl_.h"
#include "stdio_.h"
#include "stat_.h"
#include "memory_.h"
#include "string_.h"
#include "gp.h"
#include "gpgetenv.h"
#include "gpmisc.h"

/*
 * Get the name of the directory for temporary files, if any.  Currently
 * this checks the TMPDIR and TEMP environment variables, in that order.
 * The return value and the setting of *ptr and *plen are as for gp_getenv.
 */
int
gp_gettmpdir(char *ptr, int *plen)
{
    int max_len = *plen;
    int code = gp_getenv("TMPDIR", ptr, plen);

    if (code != 1)
	return code;
    *plen = max_len;
    return gp_getenv("TEMP", ptr, plen);
}

/*
 * Open a temporary file, using O_EXCL and S_I*USR to prevent race
 * conditions and symlink attacks.
 */
FILE *
gp_fopentemp(const char *fname, const char *mode)
{
    int flags = O_EXCL;
    /* Scan the mode to construct the flags. */
    const char *p = mode;
    int fildes;
    FILE *file;

    while (*p)
	switch (*p++) {
	case 'a':
	    flags |= O_CREAT | O_APPEND;
	    break;
	case 'r':
	    flags |= O_RDONLY;
	    break;
	case 'w':
	    flags |= O_CREAT | O_WRONLY | O_TRUNC;
	    break;
#ifdef O_BINARY
	    /* Watcom C insists on this non-ANSI flag being set. */
	case 'b':
	    flags |= O_BINARY;
	    break;
#endif
	case '+':
	    flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
	    break;
	default:		/* e.g., 'b' */
	    break;
	}
    fildes = open(fname, flags, S_IRUSR | S_IWUSR);
    if (fildes < 0)
	return 0;
    /*
     * The DEC VMS C compiler incorrectly defines the second argument of
     * fdopen as (char *), rather than following the POSIX.1 standard,
     * which defines it as (const char *).  Patch this here.
     */
    file = fdopen(fildes, (char *)mode); /* still really const */
    if (file == 0)
	close(fildes);
    return file;
}

/* Append a string to buffer. */
private inline bool
append(char **bp, const char *bpe, const char **ip, uint len)
{
    if (bpe - *bp < len)
	return false;
    memcpy(*bp, *ip, len);
    *bp += len;
    *ip += len;
    return true;
}

/*
 * Combine a file name with a prefix.
 * Concatenates two paths and reduce parent references and current 
 * directory references from the concatenation when possible.
 * The trailing zero byte is being added.
 *
 * Examples : 
 *	"/gs/lib" + "../Resource/CMap/H" --> "/gs/Resource/CMap/H"
 *	"C:/gs/lib" + "../Resource/CMap/H" --> "C:/gs/Resource/CMap/H"
 *	"hard disk:gs:lib" + "::Resource:CMap:H" --> 
 *		"hard disk:gs:Resource:CMap:H"
 *	"DUA1:[GHOSTSCRIPT.LIB" + "..RESOURCE.CMAP]H" --> 
 *		"DUA1:[GHOSTSCRIPT.RESOURCE.CMAP]H"
 *      "\\server\share/a/b///c/../d.e/./" + "../x.e/././/../../y.z/v.v" --> 
 *		"\\server\share/a/y.z/v.v"
 */
gp_file_name_combine_result
gp_file_name_combine_generic(const char *prefix, uint plen, 
	    const char *fname, uint flen, char *buffer, uint *blen)
{
    /*
     * THIS CODE IS SHARED FOR MULTIPLE PLATFORMS.
     * PLEASE DON'T CHANGE IT FOR A SPECIFIC PLATFORM.
     * Chenge gp_file_name_combine istead.
     */
    char *bp = buffer, *bpe = buffer + *blen;
    const char *ip, *ipe;
    uint slen;
    uint infix_type = 0; /* 0=none, 1=current, 2=parent. */
    uint infix_len = 0;
    uint rlen = gp_file_name_root(fname, flen);
    /* We need a special handling of infixes only immediately after a drive. */

    if (rlen != 0) {
        /* 'fname' is absolute, ignore the prefix. */
	ip = fname;
	ipe = fname + flen;
    } else {
        /* Concatenate with prefix. */
	ip = prefix;
	ipe = prefix + plen;
	rlen = gp_file_name_root(prefix, plen);
    }
    if (!append(&bp, bpe, &ip, rlen))
	return gp_combine_small_buffer;
    slen = gs_file_name_check_separator(bp, buffer - bp, bp); /* Backward search. */
    if (rlen != 0 && slen == 0) {
	/* Patch it against names like "c:dir" on Windows. */
	const char *sep = gp_file_name_separator();

	slen = strlen(sep);
	if (!append(&bp, bpe, &sep, slen))
	    return gp_combine_small_buffer;
	rlen += slen;
    }
    for (;;) {
	const char *item = ip;
	uint ilen;

	for (slen = 0; ip < ipe; ip++)
	    if((slen = gs_file_name_check_separator(ip, ipe - ip, item)) != 0)
		break;
	ilen = ip - item;
	if (ilen == 0 && !gp_file_name_is_empty_item_meanful()) {
	    ip += slen;
	    slen = 0;
	} else if (gp_file_name_is_current(item, ilen)) {
	    /* Skip the reference to 'current', except the starting one.
	     * We keep the starting 'current' for platforms, which
	     * require relative paths to start with it.
	     */
	    if (bp == buffer) {
		if (!append(&bp, bpe, &item, ilen))
		    return gp_combine_small_buffer;
		infix_type = 1;
		infix_len = ilen;
	    } else {
		ip += slen;
		slen = 0;
	    }
	} else if (!gp_file_name_is_parent(item, ilen)) {
	    if (!append(&bp, bpe, &item, ilen))
		return gp_combine_small_buffer;
	    /* The 'item' pointer is now broken; it may be restored using 'ilen'. */
	} else if (bp == buffer + rlen + infix_len) {
	    /* Input is a parent and the output only contains a root and an infix. */
	    if (rlen != 0)
		return gp_combine_cant_handle;
	    switch (infix_type) {
		case 1:
		    /* Replace the infix with parent. */
		    bp = buffer + rlen; /* Drop the old infix, if any. */
		    infix_len = 0;
		    /* Falls through. */
		case 0:
		    /* We have no infix, start with parent. */
		    if (!gp_file_name_is_partent_allowed())
			return gp_combine_cant_handle;
		    /* Falls through. */
		case 2:
		    /* Append one more parent - falls through. */
		    DO_NOTHING;
	    }
	    if (!append(&bp, bpe, &item, ilen))
		return gp_combine_small_buffer;
	    infix_type = 2;
	    infix_len += ilen;
	    /* Recompute the separator length. We cannot use the old slen on Mac OS. */
	    slen = gs_file_name_check_separator(ip, ipe - ip, ip);
	} else {
	    /* Input is a parent and the output continues after infix. */
	    /* Unappend the last separator and the last item. */
	    uint slen1 = gs_file_name_check_separator(bp, buffer + rlen - bp, bp); /* Backward search. */

	    for (bp -= slen1; bp > buffer + rlen; bp--)
		if (gs_file_name_check_separator(bp, buffer + rlen - bp, bp) != 0) /* Backward search. */
		    break;
	    /* Skip the input with separator. We cannot use slen on Mac OS. */
	    ip += gs_file_name_check_separator(ip, ipe - ip, ip);
	    slen = 0;
	}
	if (slen) {
	    if (bp == buffer + rlen + infix_len)
		infix_len += slen;
	    if (!append(&bp, bpe, &ip, slen))
		return gp_combine_small_buffer;
	}
	if (ip == ipe) {
	    if (ipe == fname + flen) {
		/* All done.
		 * Note that the case (prefix + plen == fname && flen == 0)
		 * falls here without appending a separator.
		 */
		const char *zero="";

		if (bp == buffer) {
		    /* Must not return empty path. */
		    const char *current = gp_file_name_current();
		    int clen = strlen(current);

		    if (!append(&bp, bpe, &current, clen))
			return gp_combine_small_buffer;
		}
		*blen = bp - buffer;
		if (!append(&bp, bpe, &zero, 1))
		    return gp_combine_small_buffer;
		return gp_combine_success;
	    } else {
	        /* ipe == prefix + plen */
		if (slen == 0) {
		    const char *sep = gp_file_name_separator();

		    slen = strlen(sep);
		    if (bp == buffer + rlen + infix_len)
			infix_len += slen;
		    if (!append(&bp, bpe, &sep, slen))
			return gp_combine_small_buffer;
		}
		/* Switch to fname. */
		ip = fname;
		ipe = fname + flen;
	    }
	}
    }
}

/*
 * Reduces parent references and current directory references when possible.
 * The trailing zero byte is being added.
 *
 * Examples : 
 *	"/gs/lib/../Resource/CMap/H" --> "/gs/Resource/CMap/H"
 *	"C:/gs/lib/../Resource/CMap/H" --> "C:/gs/Resource/CMap/H"
 *	"hard disk:gs:lib::Resource:CMap:H" --> 
 *		"hard disk:gs:Resource:CMap:H"
 *	"DUA1:[GHOSTSCRIPT.LIB..RESOURCE.CMAP]H" --> 
 *		"DUA1:[GHOSTSCRIPT.RESOURCE.CMAP]H"
 *      "\\server\share/a/b///c/../d.e/./../x.e/././/../../y.z/v.v" --> 
 *		"\\server\share/a/y.z/v.v"
 *
 */
gp_file_name_combine_result
gp_file_name_reduce(const char *fname, uint flen, char *buffer, uint *blen)
{
    return gp_file_name_combine(fname, flen, fname + flen, 0, buffer, blen);
}

/* 
 * Answers whether a file name is absolute (starts from a root). 
 */
bool gp_file_name_is_absolute(const char *fname, uint flen)
{
    return (gp_file_name_root(fname, flen) > 0);
}

/* 
 * Answers whether a reduced file name starts from parent. 
 * This won't work with non-reduced file names.
 */
bool gp_file_name_is_from_parent(const char *fname, uint flen)
{
    uint plen = gp_file_name_root(fname, flen), slen;
    const char *ip, *ipe;

    if (plen > 0 || plen == flen)
	return false;
    ipe = fname + flen;
    for (ip = fname; ip < ipe; ip++)
	if((slen = gs_file_name_check_separator(ip, ipe - ip, fname)) != 0)
	    break;
    return gp_file_name_is_parent(fname, ip - fname);
}
