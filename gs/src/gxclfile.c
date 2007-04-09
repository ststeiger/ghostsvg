/* Copyright (C) 2001-2006 artofcode LLC.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* File-based command list implementation */
#include "stdio_.h"
#include "string_.h"
#include "unistd_.h"
#include "gserror.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gp.h"
#include "gxclio.h"

/* This is an implementation of the command list I/O interface */
/* that uses the file system for storage. */

extern clist_io_procs_t *clist_io_procs_file_global;

/* ------ Open/close/unlink ------ */

private int
clist_fopen(char fname[gp_file_name_sizeof], const char *fmode,
	    clist_file_ptr * pcf, gs_memory_t * mem, gs_memory_t *data_mem,
	    bool ok_to_compress)
{
    if (*fname == 0) {
	if (fmode[0] == 'r')
	    return_error(gs_error_invalidfileaccess);
	*pcf =
	    (clist_file_ptr) gp_open_scratch_file_64(gp_scratch_file_name_prefix,
						  fname, fmode);
    } else
	*pcf = gp_fopen(fname, fmode);
    if (*pcf == NULL) {
	eprintf1("Could not open the scratch file %s.\n", fname);
	return_error(gs_error_invalidfileaccess);
    }
    return 0;
}

private int
clist_unlink(const char *fname)
{
    return (unlink(fname) != 0 ? gs_note_error(gs_error_ioerror) : 0);
}

private int
clist_fclose(clist_file_ptr cf, const char *fname, bool delete)
{
    return (fclose((FILE *) cf) != 0 ? gs_note_error(gs_error_ioerror) :
	    delete ? clist_unlink(fname) :
	    0);
}

/* ------ Writing ------ */

private int
clist_fwrite_chars(const void *data, uint len, clist_file_ptr cf)
{
    return fwrite(data, 1, len, (FILE *) cf);
}

/* ------ Reading ------ */

private int
clist_fread_chars(void *data, uint len, clist_file_ptr cf)
{
    FILE *f = (FILE *) cf;
    byte *str = data;

    /* The typical implementation of fread */
    /* is extremely inefficient for small counts, */
    /* so we just use straight-line code instead. */
    switch (len) {
	default:
	    return fread(str, 1, len, f);
	case 8:
	    *str++ = (byte) getc(f);
	case 7:
	    *str++ = (byte) getc(f);
	case 6:
	    *str++ = (byte) getc(f);
	case 5:
	    *str++ = (byte) getc(f);
	case 4:
	    *str++ = (byte) getc(f);
	case 3:
	    *str++ = (byte) getc(f);
	case 2:
	    *str++ = (byte) getc(f);
	case 1:
	    *str = (byte) getc(f);
    }
    return len;
}

/* ------ Position/status ------ */

private int
clist_set_memory_warning(clist_file_ptr cf, int bytes_left)
{
    return 0;			/* no-op */
}

private int
clist_ferror_code(clist_file_ptr cf)
{
    return (ferror((FILE *) cf) ? gs_error_ioerror : 0);
}

private int64_t
clist_ftell(clist_file_ptr cf)
{
    return gp_ftell_64((FILE *) cf);
}

private void
clist_rewind(clist_file_ptr cf, bool discard_data, const char *fname)
{
    FILE *f = (FILE *) cf;

    if (discard_data) {
	/*
	 * The ANSI C stdio specification provides no operation for
	 * truncating a file at a given position, or even just for
	 * deleting its contents; we have to use a bizarre workaround to
	 * get the same effect.
	 */
	char fmode[4];

	/* Opening with "w" mode deletes the contents when closing. */
	freopen(fname, gp_fmode_wb, f);
	strcpy(fmode, "w+");
	strcat(fmode, gp_fmode_binary_suffix);
	freopen(fname, fmode, f);
    } else {
	rewind(f);
    }
}

private int
clist_fseek(clist_file_ptr cf, int64_t offset, int mode, const char *ignore_fname)
{
    return gp_fseek_64((FILE *) cf, offset, mode);
}

clist_io_procs_t clist_io_procs_file = {
    clist_fopen,
    clist_fclose,
    clist_unlink,
    clist_fwrite_chars,
    clist_fread_chars,
    clist_set_memory_warning,
    clist_ferror_code,
    clist_ftell,
    clist_rewind,
    clist_fseek,
};

init_proc(gs_clist_init);
int
gs_gxclfile_init(gs_memory_t *mem)
{
    clist_io_procs_file_global = &clist_io_procs_file;
    return 0;
}
