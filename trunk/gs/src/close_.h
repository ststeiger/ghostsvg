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
/* Declaration of close */

#ifndef close__INCLUDED
#  define close__INCLUDED

/*
 * This absurd little file is needed because even though open and creat
 * are guaranteed to be declared in <fcntl.h>, close is not: in MS
 * environments, it's declared in <io.h>, but everywhere else it's in
 * <unistd.h>.
 */

/*
 * We must include std.h before any file that includes (or might include)
 * sys/types.h.
 */
#include "std.h"

#ifdef __WIN32__
#  include <io.h>
#else  /* !__WIN32__ */
#  include <unistd.h>
#endif /* !__WIN32__ */

#endif /* close__INCLUDED */
