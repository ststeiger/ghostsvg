/* Copyright (C) 2000-2004 artofcode LLC.  All rights reserved.
  
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

/*$Id$ */
/* Bytestream abstraction for Rinkj driver. */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "rinkj-byte-stream.h"

int
rinkj_byte_stream_write (RinkjByteStream *bs, const char *buf, int size)
{
  return bs->write (bs, buf, size);
}

int
rinkj_byte_stream_puts (RinkjByteStream *bs, const char *str)
{
  return bs->write (bs, str, strlen (str));
}

#define MAX_STRING 8192

/* Careful using this function! */
int
rinkj_byte_stream_printf (RinkjByteStream *bs, const char *fmt, ...)
{
  char str[MAX_STRING];
  int len;
  va_list ap;

  va_start (ap, fmt);
  len = vsprintf (str, fmt, ap);
  va_end (ap);
  return rinkj_byte_stream_write (bs, str, len);
}


int
rinkj_byte_stream_close (RinkjByteStream *bs)
{
  return bs->write (bs, NULL, 0);
}

/* This module just writes a byte stream to a file. */

typedef struct _RinkjByteStreamFile RinkjByteStreamFile;

struct _RinkjByteStreamFile {
  RinkjByteStream super;
  FILE *f;
};

static int
rinkj_byte_stream_file_write (RinkjByteStream *self, const char *buf, int size)
{
  RinkjByteStreamFile *z = (RinkjByteStreamFile *)self;
  int status;

  if (size == 0)
    {
#if 1
      status = 0; /* Ghostscript wants to close the file itself. */
#else
      status = fclose (z->f);
#endif
      free (self);
      return status;
    }
  else
    {
#ifdef DEBUG_OUT
      return 0;
#endif
      status = fwrite (buf, 1, size, z->f);
      if (status == size)
	return 0;
      else
	return -1;
    }
}

RinkjByteStream *
rinkj_byte_stream_file_new (FILE *f)
{
  RinkjByteStreamFile *result;

  result = (RinkjByteStreamFile *)malloc (sizeof (RinkjByteStreamFile));

  result->super.write = rinkj_byte_stream_file_write;
  result->f = f;

  return &result->super;
}
