/* $RCSfile$ $Revision$ */
/*
 * This file is maintained by a user: if you have any questions about it,
 * please contact Mark Hale (mark.hale@physics.org).
 */

/* Convert PostScript to PDF 1.3 (Acrobat 4-and-later compatible). */

parse arg params

call 'ps2pdf' '-dCompatibilityLevel=1.3' params
