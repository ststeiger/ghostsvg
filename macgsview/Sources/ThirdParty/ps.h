/*
 * ps.h -- Include file for PostScript routines.
 * Copyright (C) 1992, 1994  Timothy O. Theisen.  All rights reserved.
 *
 * This file is part of Ghostview.
 *
 * Ghostview is distributed with NO WARRANTY OF ANY KIND.  No author or
 * distributor accepts any responsibility for the consequences of using
 * it, or for whether it serves any particular purpose or works at all,
 * unless he or she says so in writing.  Refer to the Ghostview Free
 * Public License (the "License") for full details.
 *
 * Every copy of Ghostview must include a copy of the License, normally
 * in a plain ASCII text file named PUBLIC.  The License grants you the
 * right to copy, modify and redistribute Ghostview, but only under
 * certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved
 * on all copies.
 *
 *   Author: Tim Theisen           Systems Programmer
 * Internet: tim@cs.wisc.edu       Department of Computer Sciences
 *     UUCP: uwvax!tim             University of Wisconsin-Madison
 *    Phone: (608)262-0438         1210 West Dayton Street
 *      FAX: (608)262-9777         Madison, WI   53706
 */

#ifndef PS_H__HEADER_
#define PS_H__HEADER_

extern "C" {

#ifndef NeedFunctionPrototypes
#if defined(FUNCPROTO) || defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus) || defined(__TURBOC__) || defined(OS2) || defined(_MSC_VER)
#define NeedFunctionPrototypes 1
#else
#define NeedFunctionPrototypes 0
#endif /* __STDC__ */
#endif /* NeedFunctionPrototypes */

	/* Constants used to index into the bounding box array. */

#define LLX 0
#define LLY 1
#define URX 2
#define URY 3

	/* Constants used to store keywords that are scanned. */
	/* NONE is not a keyword, it tells when a field was not set */

enum {ATEND = -1, NONE = 0, PORTRAIT, LANDSCAPE, SEASCAPE, UPSIDEDOWN,
	  ASCEND, DESCEND, SPECIAL};

#define PSLINELENGTH 257	/* 255 characters + 1 newline + 1 NULL */

/* rjl: DOS binary EPS header */
#define PS_DWORD unsigned long	/* must be 32 bits unsigned */
#define PS_WORD unsigned short	/* must be 16 bits unsigned */
typedef struct tagDOSEPS {
   unsigned char id[4];
   PS_DWORD ps_begin;
   PS_DWORD ps_length;
   PS_DWORD mf_begin;
   PS_DWORD mf_length;
   PS_DWORD tiff_begin;
   PS_DWORD tiff_length;
   PS_WORD checksum;
} DOSEPS;

struct document {
    int  epsf;				/* Encapsulated PostScript flag. */
    char *title;			/* Title of document. */
    char *date;				/* Creation date. */
    int  pageorder;			/* ASCEND, DESCEND, SPECIAL */
    long beginheader, endheader;	/* offsets into file */
    unsigned int lenheader;
    long beginpreview, endpreview;
    unsigned int lenpreview;
    long begindefaults, enddefaults;
    unsigned int lendefaults;
    long beginprolog, endprolog;
    unsigned int lenprolog;
    long beginsetup, endsetup;
    unsigned int lensetup;
    long begintrailer, endtrailer;
    unsigned int lentrailer;
    int  boundingbox[4];
    int  default_page_boundingbox[4];
    int  orientation;			/* PORTRAIT, LANDSCAPE */
    int  default_page_orientation;	/* PORTRAIT, LANDSCAPE */
    unsigned int nummedia;
    struct documentmedia *media;
    struct documentmedia *default_page_media;
    DOSEPS *doseps;
    unsigned int numpages;
    struct page *pages;
    int linecount;		/* rjl: line count when parsing DSC comments */
};

struct page {
    char *label;
    int  boundingbox[4];
    struct documentmedia *media;
    int  orientation;			/* PORTRAIT, LANDSCAPE */
    long begin, end;			/* offsets into file */
    unsigned int len;
};

struct documentmedia {
    char *name;
    int width, height;
};

	/* list of standard paper sizes from Adobe's PPD. */

extern struct documentmedia papersizes[];

	/* scans a PostScript file and return a pointer to the document
	   structure.  Returns NULL if file does not Conform to commenting
	   conventions . */

#if NeedFunctionPrototypes
struct document *psscan(FILE *file);
#else
struct document *psscan();
#endif

	/* free data structure malloc'ed by psscan */

#if NeedFunctionPrototypes
void psfree(struct document *);
#else
void psfree();
#endif

	/* Copy a portion of the PostScript file */

#if NeedFunctionPrototypes
void pscopy(FILE *from, FILE *to, long begin, long end);
#else
void pscopy();
#endif

	/* Copy a portion of the PostScript file upto a comment */

#if NeedFunctionPrototypes
char *pscopyuntil(FILE *from, FILE *to, long begin, long end,
		  const char *comment);
#else
char *pscopyuntil();
#endif

	/* redefine fgets to use a version that handles 
	 * Unix, PC, or Mac EOL characters
	 */
#if NeedFunctionPrototypes
char *psfgets(char *s, int n, FILE *stream);
#else
char *psfgets();
#endif
	/* DOS EPS header reading */

#if NeedFunctionPrototypes
unsigned long ps_read_doseps(FILE *file, DOSEPS *doseps);
#else
unsigned long ps_read_doseps();
#endif

#if NeedFunctionPrototypes
PS_DWORD reorder_dword(PS_DWORD val);
#else
PS_DWORD reorder_dword();
#endif

#if NeedFunctionPrototypes
PS_WORD reorder_word(PS_WORD val);
#else
PS_WORD reorder_word();
#endif

}

#endif