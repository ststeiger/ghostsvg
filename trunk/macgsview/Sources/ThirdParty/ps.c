/*
 * ps.c -- Postscript scanning and copying routines.
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
 *
 *   Author: Tim Theisen           Systems Programmer
 * Internet: tim@cs.wisc.edu       Department of Computer Sciences
 *     UUCP: uwvax!tim             University of Wisconsin-Madison
 *    Phone: (608)262-0438         1210 West Dayton Street
 *      FAX: (608)262-9777         Madison, WI   53706
 */
/* modified by Russell Lang, rjl@aladdin.com to use with GSview
 * 1995-01-11
 * supports DOS EPS files (binary header)
 * supports the following incorrect DSC usage
 *   %%BeginData lines > 254 char                 (Microsoft Windows)
 *   EPS inclusions surrounded by %MSEPS Preamble/%MSEPS Trailer
 *    instead of %%BeginDocument/%%EndDocument    (Microsoft Windows).
 *   %!PS-Adobe instead of %!PS-Adobe-            (OS/2)
 *   atend instead of (atend)
 * rjl 1995-04-01
 *   Commented out two lines to deal with documents that include
 *   code after %%EndSetup and before %%Page:     (Microsoft Windows)
 * rjl 1996-08-08
 *   Modified gettext() to skip over trailing ')' of (text)
 *   Modified psscan() to skip over HP LaserJet PJL prologue
 * rjl 1998-07-02
 *   Modified %MSEPS Premable kludge to only work when not inside
 *   %%Begin/%%EndDocument or similar.
 */

/* modified by Bernd Heller, mac-gs@aladdin.com to use with MacGSView
 * 11/07/1999
 */


/*********************************************************/
/* inserted by Bernd Heller								 */
#include "CGSApplication.h"
/*********************************************************/


extern "C" {
int stricmp(const char *s1, const char *s2);
}

#include <stdio.h>
#if defined(__TURBOC__) || defined(OS2) || defined(_MSC_VER) || defined(__MWERKS__)
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define strcasecmp(s,t) stricmp(s,t)
extern void pserror(char *str);
#else
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef BUFSIZ
#define BUFSIZ 1024
#endif
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <ctype.h>
#include <X11/Xos.h>		// #includes the appropriate <string.h>
#define pserror(str) fprintf(stderr,str)
#endif
#include "ps.h"

#define pserror(str) fprintf(/*stderr*/stdout,str)

#ifdef DEBUG_MALLOC
#if defined(__WIN32__) || defined(OS2)
#define FAR
#endif
void FAR * debug_malloc(size_t size);
void FAR * debug_calloc(size_t nitems, size_t size);
void  FAR * debug_realloc(void FAR *block, size_t size);
void debug_free(void FAR *block);
#define malloc(size) debug_malloc(size)
#define calloc(nitems, size) debug_calloc(nitems, size)
#define realloc(block, size) debug_realloc(block, size)
#define free(block) debug_free(block)
extern long allocated_memory;
#endif

#ifdef BSD4_2
#define memset(a,b,c) bzero(a,c)
#endif

/* redefine fgets to use a version that handles Unix, PC, or Mac EOL characters */
#define fgets psfgets
#if NeedFunctionPrototypes
char *psfgets(char *s, int n, FILE *stream);
#else
char *psfgets();
#endif

char *psfgetslen(char *s, int n, FILE *stream, unsigned int *read);


/* length calculates string length at compile time */
/* can only be used with character constants */
#define length(a) (sizeof(a)-1)
#define iscomment(a, b)	(strncmp(a, b, length(b)) == 0)
#define DSCcomment(a) (a[0] == '%' && a[1] == '%')

	/* list of standard paper sizes from Adobe's PPD. */

struct documentmedia papersizes[] = {
    {"Letter",		612,  792},
    {"LetterSmall",	612,  792},
    {"Tabloid",		792, 1224},
    {"Ledger",	   1224,  792},
    {"Legal",		612, 1008},
    {"Statement",	396,  612},
    {"Executive",	540,  720},
    {"A3",			842, 1190},
    {"A4",			595,  842},
    {"A4Small",		595,  842},
    {"A5",			420,  595},
    {"B4",			729, 1032},
    {"B5",			516,  729},
    {"Folio",		612,  936},
    {"Quarto",		610,  780},
    {"10x14",		720, 1008},
    {NULL,			  0,    0}
};


#if NeedFunctionPrototypes
static char *readline(char *line, int size, FILE *fp, unsigned long enddoseps, 
  long *position, unsigned int *line_len, unsigned int *line_count, 
  int imported);
#else
static char *readline();
#endif

#if NeedFunctionPrototypes
static char *gettextline(char *line);
#else
static char *gettextline();
#endif

#if NeedFunctionPrototypes
static char *gettext(char *line, char **next_char);
#else
static char *gettext();
#endif

#if NeedFunctionPrototypes
static int  blank(char *line);
#else
static int  blank();
#endif


#include <Threads.h>


/*
 *	psscan -- scan the PostScript file for document structuring comments.
 *
 *	This scanner is designed to retrieve the information necessary for
 *	the ghostview previewer.  It will scan files that conform to any
 *	version (1.0, 2.0, 2.1, or 3.0) of the document structuring conventions.
 *	It does not really care which version of comments the file contains.
 *	(The comments are largely upward compatible.)  It will scan a number
 *	of non-conforming documents.  (You could have part of the document
 *	conform to V2.0 and the rest conform to V3.0.  It would be similar
 *	to the DC-2 1/2+, it would look funny but it can still fly.)
 *
 *	This routine returns a pointer to the document structure.
 *	The structure contains the information relevant to previewing.
 *      These include EPSF flag (to tell if the file is a encapsulated figure),
 *      Page Media (for the Page Size), Bounding Box (to minimize backing
 *      pixmap size or determine window size for encapsulated PostScript), 
 *      Orientation of Paper (for default transformation matrix), and
 *      Page Order.  The title and CreationDate are also retrieved to
 *      help identify the document.
 *
 *      The following comments are examined:
 *
 *      Header section: 
 *      Must start with %!PS-Adobe.  Version numbers ignored.
 *
 *      %!PS-Adobe* [EPSF-*]
 *      %%BoundingBox: <int> <int> <int> <int>|(atend)
 *      %%CreationDate: <textline>
 *      %%Orientation: Portrait|Landscape|(atend)
 *      %%Pages: <uint> [<int>]|(atend)
 *      %%PageOrder: Ascend|Descend|Special|(atend)
 *      %%Title: <textline>
 *      %%DocumentMedia: <text> <real> <real> <real> <text> <text>
 *      %%DocumentPaperSizes: <text>
 *      %%EndComments
 *
 *      Note: Either the 3.0 or 2.0 syntax for %%Pages is accepted.
 *            Also either the 2.0 %%DocumentPaperSizes or the 3.0
 *            %%DocumentMedia comments are accepted as well.
 *
 *      The header section ends either explicitly with %%EndComments or
 *      implicitly with any line that does not begin with %X where X is
 *      a not whitespace character.
 *
 *      If the file is encapsulated PostScript the optional Preview section
 *      is next:
 *
 *      %%BeginPreview
 *      %%EndPreview
 *
 *      This section explicitly begins and ends with the above comments.
 *
 *      Next the Defaults section for version 3 page defaults:
 *
 *      %%BeginDefaults
 *      %%PageBoundingBox: <int> <int> <int> <int>
 *      %%PageOrientation: Portrait|Landscape
 *      %%PageMedia: <text>
 *      %%EndDefaults
 *
 *      This section explicitly begins and ends with the above comments.
 *
 *      The prolog section either explicitly starts with %%BeginProlog or
 *      implicitly with any nonblank line.
 *
 *      %%BeginProlog
 *      %%EndProlog
 *
 *      The Prolog should end with %%EndProlog, however the proglog implicitly
 *      ends when %%BeginSetup, %%Page, %%Trailer or %%EOF are encountered.
 *
 *      The Setup section is where the version 2 page defaults are found.
 *      This section either explicitly begins with %%BeginSetup or implicitly
 *      with any nonblank line after the Prolog.
 *
 *      %%BeginSetup
 *      %%PageBoundingBox: <int> <int> <int> <int>
 *      %%PageOrientation: Portrait|Landscape
 *      %%PaperSize: <text>
 *      %%EndSetup
 *
 *      The Setup should end with %%EndSetup, however the setup implicitly
 *      ends when %%Page, %%Trailer or %%EOF are encountered.
 *
 *      Next each page starts explicitly with %%Page and ends implicitly with
 *      %%Page or %%Trailer or %%EOF.  The following comments are recognized:
 *
 *      %%Page: <text> <uint>
 *      %%PageBoundingBox: <int> <int> <int> <int>|(atend)
 *      %%PageOrientation: Portrait|Landscape
 *      %%PageMedia: <text>
 *      %%PaperSize: <text>
 *
 *      The tralier section start explicitly with %%Trailer and end with %%EOF.
 *      The following comment are examined with the proper (atend) notation
 *      was used in the header:
 *
 *      %%Trailer
 *      %%BoundingBox: <int> <int> <int> <int>|(atend)
 *      %%Orientation: Portrait|Landscape|(atend)
 *      %%Pages: <uint> [<int>]|(atend)
 *      %%PageOrder: Ascend|Descend|Special|(atend)
 *      %%EOF
 *
 *
 *  + A DC-3 received severe damage to one of its wings.  The wing was a total
 *    loss.  There was no replacement readily available, so the mechanic
 *    installed a wing from a DC-2.
 */


struct document *
psscan( FILE *file )
{
    struct document *doc;
    int bb_set = NONE;
    int pages_set = NONE;
    int page_order_set = NONE;
    int orientation_set = NONE;
    int page_bb_set = NONE;
    int page_media_set = NONE;
    int preread;		/* flag which tells the readline isn't needed */
    int i;
    unsigned int maxpages = 0;
    unsigned int nextpage = 1;	/* Next expected page */
    unsigned int thispage;
    int ignore = 0;		/* whether to ignore page ordinals */
    char *label;
    char line[PSLINELENGTH];	/* 255 characters + 1 newline + 1 NULL */
    char text[PSLINELENGTH];	/* Temporary storage for text */
    long position;		/* Position of the current line */
    long beginsection;		/* Position of the beginning of the section */
    unsigned int line_count;	/* line count for error messages */
    unsigned int line_len; 	/* Length of the current line */
    unsigned int section_len;	/* Place to accumulate the section length */
    char *next_char;		/* 1st char after text returned by gettext() */
    char *cp;
    struct documentmedia *dmp;
    unsigned long enddoseps;	/* zero of not DOS EPS, otherwise position of end of ps section */
    DOSEPS doseps;


    line_count = 0;
    rewind(file);

    /* rjl: check for DOS EPS files and almost DSC files that start with ^D */
    enddoseps = ps_read_doseps(file, &doseps);
    position = ftell(file);
    if (fgetc(file) != '\004')
	fseek(file, position, SEEK_SET);

    if (readline(line, sizeof line, file, enddoseps, &position, &line_len, 
	&line_count, 0) == NULL) {
	pserror("Warning: empty file.\n");
	return(NULL);
    }

    /* rjl: check for HP LaserJet prologue */
    if (iscomment(line, "\033%-12345X")) {
	/* found prolog, read until first DSC comment */
        while (readline(line, sizeof line, file, enddoseps, &position, 
		&line_len, &line_count, 0)) {
	    if (line[0] == '%')
		break;
	}
        if (line[0] != '%') {
	    pserror("Warning: error skipping PJL prologue.\n");
	    return(NULL);
	}
    }

    /* Header comments */

    if (iscomment(line,"%!PS-Adobe")) {  /* rjl: some almost DSC code doesn't include the hyphen or version number */
	doc = (struct document *) malloc(sizeof(struct document));
	if (doc == NULL) {
	    pserror("Fatal Error: Dynamic memory exhausted.\n");
	    exit(-1);
	}
	memset(doc, 0, sizeof(struct document));
	sscanf(line, "%*s %s", text);
	doc->epsf = iscomment(text, "EPSF-");
	doc->beginheader = position;
	section_len = line_len;
	if (enddoseps) { /* rjl: add doseps header */
	    doc->doseps = (DOSEPS *) malloc(sizeof(DOSEPS));
	    if (doc == NULL) {
		pserror("Fatal Error: Dynamic memory exhausted.\n");
		exit(-1);
	    }
	    *(doc->doseps) = doseps;
	}
    } else {
	return(NULL);
    }

    preread = 0;
    while (preread || readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) {
	if (!preread) section_len += line_len;
	preread = 0;
	if (line[0] != '%' ||
	    iscomment(line+1, "%EndComments") ||
	    line[1] == ' ' || line[1] == '\t' || line[1] == '\n' ||
	    !isprint(line[1])) {
	    break;
	} else if (line[1] != '%') {
	    /* Do nothing */
	} else if (iscomment(line+2, "Begin") || iscomment(line+2, "End") ||
		   iscomment(line+2, "Page:") || iscomment(line+2, "Trailer") ||
		   iscomment(line+2, "EOF")) {
	    break;
	} else if (doc->title == NULL && iscomment(line+2, "Title:")) {
	    doc->title = gettextline(line+length("%%Title:"));
	} else if (doc->date == NULL && iscomment(line+2, "CreationDate:")) {
	    doc->date = gettextline(line+length("%%CreationDate:"));
	} else if (bb_set == NONE && iscomment(line+2, "BoundingBox:")) {
	    sscanf(line+length("%%BoundingBox:"), "%s", text);
	    if (strcmp(text, "(atend)") == 0) {
		bb_set = ATEND;
	    }
	    else if (strcmp(text, "atend") == 0) { /* rjl: some bad DSC omits the () */
		char buf[64];
		sprintf(buf, "Invalid %%%%BoundingBox on line %d\n", line_count);
		pserror(buf);
		bb_set = ATEND;
	    } else {
		if (sscanf(line+length("%%BoundingBox:"), "%d %d %d %d",
			   &(doc->boundingbox[LLX]),
			   &(doc->boundingbox[LLY]),
			   &(doc->boundingbox[URX]),
			   &(doc->boundingbox[URY])) == 4)
		    bb_set = 1;
		else {
		    float fllx, flly, furx, fury;
		    if (sscanf(line+length("%%BoundingBox:"), "%f %f %f %f",
			       &fllx, &flly, &furx, &fury) == 4) {
			bb_set = 1;
			doc->boundingbox[LLX] = fllx;
			doc->boundingbox[LLY] = flly;
			doc->boundingbox[URX] = furx;
			doc->boundingbox[URY] = fury;
			if (fllx < doc->boundingbox[LLX])
			    doc->boundingbox[LLX]--;
			if (flly < doc->boundingbox[LLY])
			    doc->boundingbox[LLY]--;
			if (furx > doc->boundingbox[URX])
			    doc->boundingbox[URX]++;
			if (fury > doc->boundingbox[URY])
			    doc->boundingbox[URY]++;
		    }
		}
	    }
	} else if (orientation_set == NONE &&
		   iscomment(line+2, "Orientation:")) {
	    sscanf(line+length("%%Orientation:"), "%s", text);
	    if (strcmp(text, "(atend)") == 0) {
		orientation_set = ATEND;
	    }
	    else if (strcmp(text, "atend") == 0) { /* rjl: some bad DSC omits the () */
		char buf[64];
		sprintf(buf, "Invalid %%%%Orientation on line %d\n", line_count);
		pserror(buf);
		orientation_set = ATEND;
	    } else if (strcmp(text, "Portrait") == 0) {
		doc->orientation = PORTRAIT;
		orientation_set = 1;
	    } else if (strcmp(text, "Landscape") == 0) {
		doc->orientation = LANDSCAPE;
		orientation_set = 1;
	    } else if (strcmp(text, "Seascape") == 0) {
		doc->orientation = SEASCAPE;
		orientation_set = 1;
	    } else if (strcmp(text, "UpsideDown") == 0) {
		doc->orientation = UPSIDEDOWN;
		orientation_set = 1;
	    }
	} else if (page_order_set == NONE && iscomment(line+2, "PageOrder:")) {
	    sscanf(line+length("%%PageOrder:"), "%s", text);
	    if (strcmp(text, "(atend)") == 0) {
		page_order_set = ATEND;
	    }
	    else if (strcmp(text, "atend") == 0) { /* rjl: some bad DSC omits the () */
		char buf[64];
		sprintf(buf, "Invalid %%%%PageOrder on line %d\n", line_count);
		pserror(buf);
		page_order_set = ATEND;
	    } else if (strcmp(text, "Ascend") == 0) {
		doc->pageorder = ASCEND;
		page_order_set = 1;
	    } else if (strcmp(text, "Descend") == 0) {
		doc->pageorder = DESCEND;
		page_order_set = 1;
	    } else if (strcmp(text, "Special") == 0) {
		doc->pageorder = SPECIAL;
		page_order_set = 1;
	    }
	} else if (pages_set == NONE && iscomment(line+2, "Pages:")) {
	    sscanf(line+length("%%Pages:"), "%s", text);
	    if (strcmp(text, "(atend)") == 0) {
		pages_set = ATEND;
	    }
	    else if (strcmp(text, "atend") == 0) { /* rjl: some bad DSC omits the () */
		char buf[64];
		sprintf(buf, "Invalid %%%%Pages on line %d\n", line_count);
		pserror(buf);
		pages_set = ATEND;
	    } else {
		switch (sscanf(line+length("%%Pages:"), "%d %d",
			       &maxpages, &i)) {
		    case 2:
			if (page_order_set == NONE) {
			    if (i == -1) {
				doc->pageorder = DESCEND;
				page_order_set = 1;
			    } else if (i == 0) {
				doc->pageorder = SPECIAL;
				page_order_set = 1;
			    } else if (i == 1) {
				doc->pageorder = ASCEND;
				page_order_set = 1;
			    }
			}
		    case 1:
			if (maxpages > 0) {
			    doc->pages = (struct page *) calloc(maxpages,
							   sizeof(struct page));
			    if (doc->pages == NULL) {
				pserror("Fatal Error: Dynamic memory exhausted.\n");
				exit(-1);
			    }
			}
		}
	    }
	} else if (doc->nummedia == NONE &&
		   iscomment(line+2, "DocumentMedia:")) {
	    float w, h;
	    doc->media = (struct documentmedia *)
			 malloc(sizeof (struct documentmedia));
	    if (doc->media == NULL) {
		pserror("Fatal Error: Dynamic memory exhausted.\n");
		exit(-1);
	    }
	    doc->media[0].name = gettext(line+length("%%DocumentMedia:"),
					 &next_char);
	    if (doc->media[0].name != NULL) {
		if (sscanf(next_char, "%f %f", &w, &h) == 2) {
		    doc->media[0].width = w + 0.5;
		    doc->media[0].height = h + 0.5;
		}
		if (doc->media[0].width != 0 && doc->media[0].height != 0)
		    doc->nummedia = 1;
		else
		    free(doc->media[0].name);
	    }
	    preread=1;
	    while (readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0) &&
		   DSCcomment(line) && iscomment(line+2, "+")) {
		section_len += line_len;
		doc->media = (struct documentmedia *)
			     realloc(doc->media,
				     (doc->nummedia+1)*
				     sizeof (struct documentmedia));
		if (doc->media == NULL) {
		    pserror("Fatal Error: Dynamic memory exhausted.\n");
		    exit(-1);
		}
		doc->media[doc->nummedia].name = gettext(line+length("%%+"),
							 &next_char);
		if (doc->media[doc->nummedia].name != NULL) {
		    if (sscanf(next_char, "%f %f", &w, &h) == 2) {
			doc->media[doc->nummedia].width = w + 0.5;
			doc->media[doc->nummedia].height = h + 0.5;
		    }
		    if (doc->media[doc->nummedia].width != 0 &&
			doc->media[doc->nummedia].height != 0) doc->nummedia++;
		    else
			free(doc->media[doc->nummedia].name);
		}
	    }
	    section_len += line_len;
	    if (doc->nummedia != 0) doc->default_page_media = doc->media;
	} else if (doc->nummedia == NONE &&
		   iscomment(line+2, "DocumentPaperSizes:")) {

	    doc->media = (struct documentmedia *)
			 malloc(sizeof (struct documentmedia));
	    if (doc->media == NULL) {
		pserror("Fatal Error: Dynamic memory exhausted.\n");
		exit(-1);
	    }
	    doc->media[0].name = gettext(line+length("%%DocumentPaperSizes:"),
					 &next_char);
	    if (doc->media[0].name != NULL) {
		doc->media[0].width = 0;
		doc->media[0].height = 0;
		for (dmp=papersizes; dmp->name != NULL; dmp++) {
		    /* Note: Paper size comment uses down cased paper size
		     * name.  Case insensitive compares are only used for
		     * PaperSize comments.
		     */
		    if (strcasecmp(doc->media[0].name, dmp->name) == 0) {
			free(doc->media[0].name);
			doc->media[0].name =
				(char *)malloc(strlen(dmp->name)+1);
			if (doc->media[0].name == NULL) {
			    pserror("Fatal Error: Dynamic memory exhausted.\n");
			    exit(-1);
			}
			strcpy(doc->media[0].name, dmp->name);
			doc->media[0].width = dmp->width;
			doc->media[0].height = dmp->height;
			break;
		    }
		}
		if (doc->media[0].width != 0 && doc->media[0].height != 0)
		    doc->nummedia = 1;
		else
		    free(doc->media[0].name);
	    }
	    while ( (cp = gettext(next_char, &next_char)) != NULL ) {
		doc->media = (struct documentmedia *)
			     realloc(doc->media,
				     (doc->nummedia+1)*
				     sizeof (struct documentmedia));
		if (doc->media == NULL) {
		    pserror("Fatal Error: Dynamic memory exhausted.\n");
		    exit(-1);
		}
		doc->media[doc->nummedia].name = cp;
		doc->media[doc->nummedia].width = 0;
		doc->media[doc->nummedia].height = 0;
		for (dmp=papersizes; dmp->name != NULL; dmp++) {
		    /* Note: Paper size comment uses down cased paper size
		     * name.  Case insensitive compares are only used for
		     * PaperSize comments.
		     */
		    if (strcasecmp(doc->media[doc->nummedia].name,
			       dmp->name) == 0) {
			free(doc->media[doc->nummedia].name);
			doc->media[doc->nummedia].name =
				(char *)malloc(strlen(dmp->name)+1);
			if (doc->media[doc->nummedia].name == NULL) {
			    pserror("Fatal Error: Dynamic memory exhausted.\n");
			    exit(-1);
			}
			strcpy(doc->media[doc->nummedia].name, dmp->name);
			doc->media[doc->nummedia].name = dmp->name;
			doc->media[doc->nummedia].width = dmp->width;
			doc->media[doc->nummedia].height = dmp->height;
			break;
		    }
		}
		if (doc->media[doc->nummedia].width != 0 &&
		    doc->media[doc->nummedia].height != 0) doc->nummedia++;
		else
		    free(doc->media[doc->nummedia].name);
	    }
	    preread=1;
	    while (readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0) &&
		   DSCcomment(line) && iscomment(line+2, "+")) {
		section_len += line_len;
		next_char = line + length("%%+");
		while ( (cp = gettext(next_char, &next_char)) != NULL ) {
		    doc->media = (struct documentmedia *)
				 realloc(doc->media,
					 (doc->nummedia+1)*
					 sizeof (struct documentmedia));
		    if (doc->media == NULL) {
			pserror("Fatal Error: Dynamic memory exhausted.\n");
			exit(-1);
		    }
		    doc->media[doc->nummedia].name = cp;
		    doc->media[doc->nummedia].width = 0;
		    doc->media[doc->nummedia].height = 0;
		    for (dmp=papersizes; dmp->name != NULL; dmp++) {
			/* Note: Paper size comment uses down cased paper size
			 * name.  Case insensitive compares are only used for
			 * PaperSize comments.
			 */
			if (strcasecmp(doc->media[doc->nummedia].name,
				   dmp->name) == 0) {
			    doc->media[doc->nummedia].width = dmp->width;
			    doc->media[doc->nummedia].height = dmp->height;
			    break;
			}
		    }
		    if (doc->media[doc->nummedia].width != 0 &&
			doc->media[doc->nummedia].height != 0) doc->nummedia++;
		    else
			free(doc->media[doc->nummedia].name);
		}
	    }
	    section_len += line_len;
	    if (doc->nummedia != 0) doc->default_page_media = doc->media;
	}
    }

    if (DSCcomment(line) && iscomment(line+2, "EndComments")) {
	readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0);
	section_len += line_len;
    }
    doc->endheader = position;
    doc->lenheader = section_len - line_len;

    /* Optional Preview comments for encapsulated PostScript files */ 

    beginsection = position;
    section_len = line_len;
    while (blank(line) &&
	   readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) {
	section_len += line_len;
    }

    if (doc->epsf && DSCcomment(line) && iscomment(line+2, "BeginPreview")) {
	doc->beginpreview = beginsection;
	beginsection = 0;
	while (readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndPreview"))) {
	    section_len += line_len;
	}
	section_len += line_len;
	readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0);
	section_len += line_len;
	doc->endpreview = position;
	doc->lenpreview = section_len - line_len;
    }

    /* Page Defaults for Version 3.0 files */

    if (beginsection == 0) {
	beginsection = position;
	section_len = line_len;
    }
    while (blank(line) &&
	   readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) {
	section_len += line_len;
    }

    if (DSCcomment(line) && iscomment(line+2, "BeginDefaults")) {
	doc->begindefaults = beginsection;
	beginsection = 0;
	while (readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndDefaults"))) {
	    section_len += line_len;
	    if (!DSCcomment(line)) {
		/* Do nothing */
	    } else if (doc->default_page_orientation == NONE &&
		iscomment(line+2, "PageOrientation:")) {
		sscanf(line+length("%%PageOrientation:"), "%s", text);
		if (strcmp(text, "Portrait") == 0) {
		    doc->default_page_orientation = PORTRAIT;
		} else if (strcmp(text, "Landscape") == 0) {
		    doc->default_page_orientation = LANDSCAPE;
		} else if (strcmp(text, "Seascape") == 0) {
			doc->default_page_orientation = SEASCAPE;
		} else if (strcmp(text, "UpsideDown") == 0) {
			doc->default_page_orientation = UPSIDEDOWN;
		}
	    } else if (page_media_set == NONE &&
		       iscomment(line+2, "PageMedia:")) {
		cp = gettext(line+length("%%PageMedia:"), NULL);
		for (dmp = doc->media, i=0; i<doc->nummedia; i++, dmp++) {
		    if (strcmp(cp, dmp->name) == 0) {
			doc->default_page_media = dmp;
			page_media_set = 1;
			break;
		    }
		}
		free(cp);
	    } else if (page_bb_set == NONE &&
		       iscomment(line+2, "PageBoundingBox:")) {
		if (sscanf(line+length("%%PageBoundingBox:"), "%d %d %d %d",
			   &(doc->default_page_boundingbox[LLX]),
			   &(doc->default_page_boundingbox[LLY]),
			   &(doc->default_page_boundingbox[URX]),
			   &(doc->default_page_boundingbox[URY])) == 4)
		    page_bb_set = 1;
		else {
		    float fllx, flly, furx, fury;
		    if (sscanf(line+length("%%PageBoundingBox:"), "%f %f %f %f",
			       &fllx, &flly, &furx, &fury) == 4) {
			page_bb_set = 1;
			doc->default_page_boundingbox[LLX] = fllx;
			doc->default_page_boundingbox[LLY] = flly;
			doc->default_page_boundingbox[URX] = furx;
			doc->default_page_boundingbox[URY] = fury;
			if (fllx < doc->default_page_boundingbox[LLX])
			    doc->default_page_boundingbox[LLX]--;
			if (flly < doc->default_page_boundingbox[LLY])
			    doc->default_page_boundingbox[LLY]--;
			if (furx > doc->default_page_boundingbox[URX])
			    doc->default_page_boundingbox[URX]++;
			if (fury > doc->default_page_boundingbox[URY])
			    doc->default_page_boundingbox[URY]++;
		    }
		}
	    }
	}
	section_len += line_len;
	readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0);
	section_len += line_len;
	doc->enddefaults = position;
	doc->lendefaults = section_len - line_len;
    }

    /* Document Prolog */

    if (beginsection == 0) {
	beginsection = position;
	section_len = line_len;
    }
    while (blank(line) &&
	   readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) {
	section_len += line_len;
    }

    if (!(DSCcomment(line) &&
	  (iscomment(line+2, "BeginSetup") ||
	   iscomment(line+2, "Page:") ||
	   iscomment(line+2, "Trailer") ||
	   iscomment(line+2, "EOF")))) {
	doc->beginprolog = beginsection;
	beginsection = 0;
	preread = 1;

	while ((preread ||
		readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) &&
	       !(DSCcomment(line) &&
	         (iscomment(line+2, "EndProlog") ||
	          iscomment(line+2, "BeginSetup") ||
	          iscomment(line+2, "Page:") ||
	          iscomment(line+2, "Trailer") ||
	          iscomment(line+2, "EOF")))) {
	    if (!preread) section_len += line_len;
	    preread = 0;
	}
	section_len += line_len;
	if (DSCcomment(line) && iscomment(line+2, "EndProlog")) {
	    readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0);
	    section_len += line_len;
	}
	doc->endprolog = position;
	doc->lenprolog = section_len - line_len;
    }

    /* Document Setup,  Page Defaults found here for Version 2 files */

    if (beginsection == 0) {
	beginsection = position;
	section_len = line_len;
    }
    while (blank(line) &&
	   readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) {
	section_len += line_len;
    }

    if (!(DSCcomment(line) &&
	  (iscomment(line+2, "Page:") ||
	   iscomment(line+2, "Trailer") ||
	   iscomment(line+2, "EOF")))) {
	doc->beginsetup = beginsection;
	beginsection = 0;
	preread = 1;
	while ((preread ||
		readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) &&
	       !(DSCcomment(line) &&
	         (iscomment(line+2, "EndSetup") ||
	          iscomment(line+2, "Page:") ||
	          iscomment(line+2, "Trailer") ||
	          iscomment(line+2, "EOF")))) {
	    if (!preread) section_len += line_len;
	    preread = 0;
	    if (!DSCcomment(line)) {
		/* Do nothing */
	    } else if (doc->default_page_orientation == NONE &&
		iscomment(line+2, "PageOrientation:")) {
		sscanf(line+length("%%PageOrientation:"), "%s", text);
		if (strcmp(text, "Portrait") == 0) {
		    doc->default_page_orientation = PORTRAIT;
		} else if (strcmp(text, "Landscape") == 0) {
		    doc->default_page_orientation = LANDSCAPE;
		} else if (strcmp(text, "Seascape") == 0) {
			doc->default_page_orientation = SEASCAPE;
		} else if (strcmp(text, "UpsideDown") == 0) {
			doc->default_page_orientation = UPSIDEDOWN;
		}
	    } else if (page_media_set == NONE &&
		       iscomment(line+2, "PaperSize:")) {
		cp = gettext(line+length("%%PaperSize:"), NULL);
		for (dmp = doc->media, i=0; i<doc->nummedia; i++, dmp++) {
		    /* Note: Paper size comment uses down cased paper size
		     * name.  Case insensitive compares are only used for
		     * PaperSize comments.
		     */
		    if (strcasecmp(cp, dmp->name) == 0) {
			doc->default_page_media = dmp;
			page_media_set = 1;
			break;
		    }
		}
		free(cp);
	    } else if (page_bb_set == NONE &&
		       iscomment(line+2, "PageBoundingBox:")) {
		if (sscanf(line+length("%%PageBoundingBox:"), "%d %d %d %d",
			   &(doc->default_page_boundingbox[LLX]),
			   &(doc->default_page_boundingbox[LLY]),
			   &(doc->default_page_boundingbox[URX]),
			   &(doc->default_page_boundingbox[URY])) == 4)
		    page_bb_set = 1;
		else {
		    float fllx, flly, furx, fury;
		    if (sscanf(line+length("%%PageBoundingBox:"), "%f %f %f %f",
			       &fllx, &flly, &furx, &fury) == 4) {
			page_bb_set = 1;
			doc->default_page_boundingbox[LLX] = fllx;
			doc->default_page_boundingbox[LLY] = flly;
			doc->default_page_boundingbox[URX] = furx;
			doc->default_page_boundingbox[URY] = fury;
			if (fllx < doc->default_page_boundingbox[LLX])
			    doc->default_page_boundingbox[LLX]--;
			if (flly < doc->default_page_boundingbox[LLY])
			    doc->default_page_boundingbox[LLY]--;
			if (furx > doc->default_page_boundingbox[URX])
			    doc->default_page_boundingbox[URX]++;
			if (fury > doc->default_page_boundingbox[URY])
			    doc->default_page_boundingbox[URY]++;
		    }
		}
	    }
	}
	section_len += line_len;
	if (DSCcomment(line) && iscomment(line+2, "EndSetup")) {
	    readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0);
	    section_len += line_len;
	}
	doc->endsetup = position;
	doc->lensetup = section_len - line_len;
    }

    /* Individual Pages */

    if (beginsection == 0) {
	beginsection = position;
	section_len = line_len;
    }
    while (blank(line) &&
	   readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) {
	section_len += line_len;
    }

newpage:
    while (DSCcomment(line) && iscomment(line+2, "Page:")) {
	if (maxpages == 0) {
	    maxpages = 1;
	    doc->pages = (struct page *) calloc(maxpages, sizeof(struct page));
	    if (doc->pages == NULL) {
		pserror("Fatal Error: Dynamic memory exhausted.\n");
		exit(-1);
	    }
	}
	label = gettext(line+length("%%Page:"), &next_char);
	if (sscanf(next_char, "%d", &thispage) != 1) thispage = 0;
	if (nextpage == 1) {
	    ignore = thispage != 1;
	}
	if (!ignore && thispage != nextpage) {
	    free(label);
	    doc->numpages--;
	    goto continuepage;
	}
	nextpage++;
	if (doc->numpages == maxpages) {
	    maxpages++;
	    doc->pages = (struct page *)
			 realloc(doc->pages, maxpages*sizeof (struct page));
	    if (doc->pages == NULL) {
		pserror("Fatal Error: Dynamic memory exhausted.\n");
		exit(-1);
	    }
	}
	memset(&(doc->pages[doc->numpages]), 0, sizeof(struct page));
	page_bb_set = NONE;
	doc->pages[doc->numpages].label = label;
	if (beginsection) {
	    doc->pages[doc->numpages].begin = beginsection;
	    beginsection = 0;
	} else {
	    doc->pages[doc->numpages].begin = position;
	    section_len = line_len;
	}
continuepage:
	while (readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0) &&
	       !(DSCcomment(line) &&
	         (iscomment(line+2, "Page:") ||
	          iscomment(line+2, "Trailer") ||
	          iscomment(line+2, "EOF")))) {
	    section_len += line_len;
	    if (!DSCcomment(line)) {
		/* Do nothing */
	    } else if (doc->pages[doc->numpages].orientation == NONE &&
		iscomment(line+2, "PageOrientation:")) {
		sscanf(line+length("%%PageOrientation:"), "%s", text);
		if (strcmp(text, "Portrait") == 0) {
		    doc->pages[doc->numpages].orientation = PORTRAIT;
		} else if (strcmp(text, "Landscape") == 0) {
		    doc->pages[doc->numpages].orientation = LANDSCAPE;
		} else if (strcmp(text, "Seascape") == 0) {
			doc->pages[doc->numpages].orientation = SEASCAPE;
		} else if (strcmp(text, "UpsideDown") == 0) {
			doc->pages[doc->numpages].orientation = UPSIDEDOWN;
		}
	    } else if (doc->pages[doc->numpages].media == NULL &&
		       iscomment(line+2, "PageMedia:")) {
		cp = gettext(line+length("%%PageMedia:"), NULL);
		for (dmp = doc->media, i=0; i<doc->nummedia; i++, dmp++) {
		    if (strcmp(cp, dmp->name) == 0) {
			doc->pages[doc->numpages].media = dmp;
			break;
		    }
		}
		free(cp);
	    } else if (doc->pages[doc->numpages].media == NULL &&
		       iscomment(line+2, "PaperSize:")) {
		cp = gettext(line+length("%%PaperSize:"), NULL);
		for (dmp = doc->media, i=0; i<doc->nummedia; i++, dmp++) {
		    /* Note: Paper size comment uses down cased paper size
		     * name.  Case insensitive compares are only used for
		     * PaperSize comments.
		     */
		    if (strcasecmp(cp, dmp->name) == 0) {
			doc->pages[doc->numpages].media = dmp;
			break;
		    }
		}
		free(cp);
	    } else if ((page_bb_set == NONE || page_bb_set == ATEND) &&
		       iscomment(line+2, "PageBoundingBox:")) {
		sscanf(line+length("%%PageBoundingBox:"), "%s", text);
		if (strcmp(text, "(atend)") == 0) {
		    page_bb_set = ATEND;
		}
		else if (strcmp(text, "atend") == 0) { /* rjl: some bad DSC omits the () */
		    char buf[64];
		    sprintf(buf, "Invalid %%%%PageBoundingBox on line %d\n", line_count);
		    pserror(buf);
		    page_bb_set = ATEND;
		} else {
		    if (sscanf(line+length("%%PageBoundingBox:"), "%d %d %d %d",
			    &(doc->pages[doc->numpages].boundingbox[LLX]),
			    &(doc->pages[doc->numpages].boundingbox[LLY]),
			    &(doc->pages[doc->numpages].boundingbox[URX]),
			    &(doc->pages[doc->numpages].boundingbox[URY])) == 4)
			if (page_bb_set == NONE) page_bb_set = 1;
		    else {
			float fllx, flly, furx, fury;
			if (sscanf(line+length("%%PageBoundingBox:"),
				   "%f %f %f %f",
				   &fllx, &flly, &furx, &fury) == 4) {
			    if (page_bb_set == NONE) page_bb_set = 1;
			    doc->pages[doc->numpages].boundingbox[LLX] = fllx;
			    doc->pages[doc->numpages].boundingbox[LLY] = flly;
			    doc->pages[doc->numpages].boundingbox[URX] = furx;
			    doc->pages[doc->numpages].boundingbox[URY] = fury;
			    if (fllx <
				    doc->pages[doc->numpages].boundingbox[LLX])
				doc->pages[doc->numpages].boundingbox[LLX]--;
			    if (flly <
				    doc->pages[doc->numpages].boundingbox[LLY])
				doc->pages[doc->numpages].boundingbox[LLY]--;
			    if (furx >
				    doc->pages[doc->numpages].boundingbox[URX])
				doc->pages[doc->numpages].boundingbox[URX]++;
			    if (fury >
				    doc->pages[doc->numpages].boundingbox[URY])
				doc->pages[doc->numpages].boundingbox[URY]++;
			}
		    }
		}
	    }
	}
	section_len += line_len;
	doc->pages[doc->numpages].end = position;
	doc->pages[doc->numpages].len = section_len - line_len;
	doc->numpages++;
    }

    /* Document Trailer */

    if (beginsection) {
	doc->begintrailer = beginsection;
	beginsection = 0;
    } else {
	doc->begintrailer = position;
	section_len = line_len;
    }

    preread = 1;
    while ((preread ||
	    readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) &&
	   !(DSCcomment(line) && iscomment(line+2, "EOF"))) {
	if (!preread) section_len += line_len;
	preread = 0;
	if (!DSCcomment(line)) {
	    /* Do nothing */
	} else if (iscomment(line+2, "Page:")) {
	    free(gettext(line+length("%%Page:"), &next_char));
	    if (sscanf(next_char, "%d", &thispage) != 1) thispage = 0;
	    if (!ignore && thispage == nextpage) {
		if (doc->numpages > 0) {
		    doc->pages[doc->numpages-1].end = position;
		    doc->pages[doc->numpages-1].len += section_len - line_len;
		} else {
		    if (doc->endsetup) {
			doc->endsetup = position;
/* following line removed rjl 1995-04-01
			doc->endsetup += section_len - line_len;
*/
		    } else if (doc->endprolog) {
			doc->endprolog = position;
/* following line removed rjl 1995-04-01
			doc->endprolog += section_len - line_len;
*/
		    }
		}
		goto newpage;
	    }
	} else if (bb_set == ATEND && iscomment(line+2, "BoundingBox:")) {
	    if (sscanf(line+length("%%BoundingBox:"), "%d %d %d %d",
		       &(doc->boundingbox[LLX]),
		       &(doc->boundingbox[LLY]),
		       &(doc->boundingbox[URX]),
		       &(doc->boundingbox[URY])) != 4) {
		float fllx, flly, furx, fury;
		if (sscanf(line+length("%%BoundingBox:"), "%f %f %f %f",
			   &fllx, &flly, &furx, &fury) == 4) {
		    doc->boundingbox[LLX] = fllx;
		    doc->boundingbox[LLY] = flly;
		    doc->boundingbox[URX] = furx;
		    doc->boundingbox[URY] = fury;
		    if (fllx < doc->boundingbox[LLX])
			doc->boundingbox[LLX]--;
		    if (flly < doc->boundingbox[LLY])
			doc->boundingbox[LLY]--;
		    if (furx > doc->boundingbox[URX])
			doc->boundingbox[URX]++;
		    if (fury > doc->boundingbox[URY])
			doc->boundingbox[URY]++;
		}
	    }
	} else if (orientation_set == ATEND &&
		   iscomment(line+2, "Orientation:")) {
	    sscanf(line+length("%%Orientation:"), "%s", text);
	    if (strcmp(text, "Portrait") == 0) {
		doc->orientation = PORTRAIT;
	    } else if (strcmp(text, "Landscape") == 0) {
		doc->orientation = LANDSCAPE;
	    } else if (strcmp(text, "Seascape") == 0) {
		doc->orientation = SEASCAPE;
	    } else if (strcmp(text, "UpsideDown") == 0) {
		doc->orientation = UPSIDEDOWN;
	    }
	} else if (page_order_set == ATEND && iscomment(line+2, "PageOrder:")) {
	    sscanf(line+length("%%PageOrder:"), "%s", text);
	    if (strcmp(text, "Ascend") == 0) {
		doc->pageorder = ASCEND;
	    } else if (strcmp(text, "Descend") == 0) {
		doc->pageorder = DESCEND;
	    } else if (strcmp(text, "Special") == 0) {
		doc->pageorder = SPECIAL;
	    }
	} else if (pages_set == ATEND && iscomment(line+2, "Pages:")) {
	    if (sscanf(line+length("%%Pages:"), "%*u %d", &i) == 1) {
		if (page_order_set == NONE) {
		    if (i == -1) doc->pageorder = DESCEND;
		    else if (i == 0) doc->pageorder = SPECIAL;
		    else if (i == 1) doc->pageorder = ASCEND;
		}
	    }
	}
    }
    section_len += line_len;
    if (DSCcomment(line) && iscomment(line+2, "EOF")) {
	readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0);
	section_len += line_len;
    }
    doc->endtrailer = position;
    doc->lentrailer = section_len - line_len;

#if 0
    section_len = line_len;
    preread = 1;
    while (preread ||
	   readline(line, sizeof line, file, enddoseps, &position, &line_len, &line_count, 0)) {
	if (!preread) section_len += line_len;
	preread = 0;
	if (DSCcomment(line) && iscomment(line+2, "Page:")) {
	    free(gettext(line+length("%%Page:"), &next_char));
	    if (sscanf(next_char, "%d", &thispage) != 1) thispage = 0;
	    if (!ignore && thispage == nextpage) {
		if (doc->numpages > 0) {
		    doc->pages[doc->numpages-1].end = position;
		    doc->pages[doc->numpages-1].len += doc->lentrailer +
						       section_len - line_len;
		} else {
		    if (doc->endsetup) {
			doc->endsetup = position;
			doc->endsetup += doc->lentrailer +
					 section_len - line_len;
		    } else if (doc->endprolog) {
			doc->endprolog = position;
			doc->endprolog += doc->lentrailer +
					  section_len - line_len;
		    }
		}
		goto newpage;
	    }
	}
    }
#endif
    return doc;
}

/*
 *	psfree -- free dynamic storage associated with document structure.
 */

void
psfree(struct document *doc)
{
    int i;

    if (doc) {
	for (i=0; i<doc->numpages; i++) {
	    if (doc->pages[i].label) free(doc->pages[i].label);
	}
	for (i=0; i<doc->nummedia; i++) {
	    if (doc->media[i].name) free(doc->media[i].name);
	}
	if (doc->title) free(doc->title);
	if (doc->date) free(doc->date);
	if (doc->pages) free(doc->pages);
	if (doc->media) free(doc->media);
	if (doc->doseps) free(doc->doseps); /* rjl: */
	free(doc);
    }
}

/*
 * gettextine -- skip over white space and return the rest of the line.
 *               If the text begins with '(' return the text string
 *		 using gettext().
 */

static char *
gettextline(char *line)
{
    char *cp;

    while (*line && (*line == ' ' || *line == '\t')) line++;
    if (*line == '(') {
	return gettext(line, NULL);
    } else {
	if (strlen(line) == 0) return NULL;
	cp = (char *) malloc(strlen(line));
	if (cp == NULL) {
	    pserror("Fatal Error: Dynamic memory exhausted.\n");
	    exit(-1);
	}
	strncpy(cp, line, strlen(line)-1);
	cp[strlen(line)-1] = '\0';
	return cp;
    }
}

/*
 *	gettext -- return the next text string on the line.
 *		   return NULL if nothing is present.
 */

static char *
gettext(char *line, char **next_char)
{
    char text[PSLINELENGTH];	/* Temporary storage for text */
    char *cp;
    int quoted=0;

    while (*line && (*line == ' ' || *line == '\t')) line++;
    cp = text;
    if (*line == '(') {
	int level = 0;
	quoted=1;
	line++;
	while (*line && !(*line == ')' && level == 0 )) {
	    if (*line == '\\') {
		if (*(line+1) == 'n') {
		    *cp++ = '\n';
		    line += 2;
		} else if (*(line+1) == 'r') {
		    *cp++ = '\r';
		    line += 2;
		} else if (*(line+1) == 't') {
		    *cp++ = '\t';
		    line += 2;
		} else if (*(line+1) == 'b') {
		    *cp++ = '\b';
		    line += 2;
		} else if (*(line+1) == 'f') {
		    *cp++ = '\f';
		    line += 2;
		} else if (*(line+1) == '\\') {
		    *cp++ = '\\';
		    line += 2;
		} else if (*(line+1) == '(') {
		    *cp++ = '(';
		    line += 2;
		} else if (*(line+1) == ')') {
		    *cp++ = ')';
		    line += 2;
		} else if (*(line+1) >= '0' && *(line+1) <= '9') {
		    if (*(line+2) >= '0' && *(line+2) <= '9') {
			if (*(line+3) >= '0' && *(line+3) <= '9') {
			    *cp++ = (unsigned char)(((*(line+1) - '0')*8 + *(line+2) - '0')*8 +
				    *(line+3) - '0');
			    line += 4;
			} else {
			    *cp++ = (unsigned char)((*(line+1) - '0')*8 + *(line+2) - '0');
			    line += 3;
			}
		    } else {
			*cp++ = (unsigned char)(*(line+1) - '0');
			line += 2;
		    }
		} else {
		    line++;
		    *cp++ = *line++;
		}
	    } else if (*line == '(') {
		level++;
		*cp++ = *line++;
	    } else if (*line == ')') {
		level--;
		*cp++ = *line++;
	    } else {
		*cp++ = *line++;
	    }
	}
	if (*line == ')')	/* rjl 1996-08-05 */
	    line++;		/* rjl 1996-08-05 */
    } else {
	while (*line && !(*line == ' ' || *line == '\t' || *line == '\n'))
	    *cp++ = *line++;
    }
    *cp = '\0';
    if (next_char) *next_char = line;
    if (!quoted && strlen(text) == 0) return NULL;
    cp = (char *) malloc(strlen(text)+1);
    if (cp == NULL) {
	pserror("Fatal Error: Dynamic memory exhausted.\n");
	exit(-1);
    }
    strcpy(cp, text);
    return cp;
}

/*
 *	readline -- Read the next line in the postscript file.
 *                  Automatically skip over data (as indicated by
 *                  %%BeginBinary/%%EndBinary or %%BeginData/%%EndData
 *		    comments.)
 *		    Also, skip over included documents (as indicated by
 *		    %%BeginDocument/%%EndDocument comments.)
 */

static char *
readline(char *line, int size, FILE *fp, unsigned long enddoseps, long *position,
		 unsigned int *line_len, unsigned int *line_count, int imported)
{
    char text[PSLINELENGTH];	/* Temporary storage for text */
    char save[PSLINELENGTH];	/* Temporary storage for text */
    char *cp;
    unsigned int num;
    unsigned int nbytes;
    int i;
    char buf[BUFSIZ];
	
	/*********************************************************/
	/* inserted by Bernd Heller								 */
	{
#if 0
		// yield every 10000 lines
		static UInt32 lines = 0;
		if (lines++ > 10000) {
			::YieldToAnyThread();
			lines = 0;
		}
#else
		// yield every 6 ticks (1/10th second)
		static UInt32 lastticks = 0, newticks = 0;
		newticks = ::TickCount();
		if ((newticks - lastticks) > 20) {
			lastticks = newticks;
			::YieldToAnyThread();
		}
#endif
	}
	/*********************************************************/
	
    if (position) *position = ftell(fp);
    if (position && enddoseps) {
	if (*position >= enddoseps)
	    return NULL;    /* don't read any more, we have reached end of dos eps section */
	if (size > enddoseps - *position + 1)
	    size = (int)(enddoseps - *position + 1);
    }
    
	/*********************************************************/
	/* changed by Bernd Heller								 */
/*    cp = fgets(line, size, fp);
    *line_count += 1;
    if (cp == NULL) line[0] = '\0';
    *line_len = strlen(line);*/
	
	*line_count += 1;
	cp = psfgetslen(line, size, fp, line_len);
	if (cp == NULL) line[0] = '\0';
	
	/*********************************************************/
    
#if defined(__TURBOC__) || defined(OS2)
    /* remove MS-DOS carriage-return */
    if ((i = *line_len) >= 2) {
	if ((line[i-2] == '\r') && (line[i-1] == '\n')) {
	    line[i-2] = '\n';
	    line[i-1] = '\0';
	}
    }
#endif
    if (!(DSCcomment(line) && iscomment(line+2, "Begin"))) {
        /* rjl: skip over BUGGY EPS includes from Microsoft Word */
        /* Microsoft's EPSIMP.FLT needs to be fixed */
        /* One program inserts "%MSEPS Preamble [Softek v3.6]"
	 * without a finishing "%MSEPS Trailer".  This messed up
	 * the original kludge.
	 * We now ignore %MSEPS if inside a Begin/EndDocument or similar.
	 */
	if (!imported && (line[0] == '%') && 
	    iscomment(line+1, "MSEPS Preamble") ) {
	    strcpy(save, line);
	    while (readline(line, size, fp, enddoseps, NULL, &nbytes, 
		             line_count, 1) &&
		   !(line[0] == '%' && iscomment(line+1, "MSEPS Trailer"))) {
		*line_len += nbytes;
	    }
	    *line_len += nbytes;
	    strcpy(line, save);
	}
	else {
	    /* Do nothing */
	}
    } else if (iscomment(line+7, "Document:")) {
	strcpy(save, line+7);
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndDocument"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    } else if (iscomment(line+7, "Feature:")) {
	strcpy(save, line+7);
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndFeature"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    } else if (iscomment(line+7, "File:")) {
	strcpy(save, line+7);
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndFile"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    } else if (iscomment(line+7, "Font:")) {
	strcpy(save, line+7);
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndFont"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    } else if (iscomment(line+7, "ProcSet:")) {
	strcpy(save, line+7);
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndProcSet"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    } else if (iscomment(line+7, "Resource:")) {
	strcpy(save, line+7);
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndResource"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    } else if (iscomment(line+7, "Data:")) {
	text[0] = '\0';
	strcpy(save, line+7);
	if (sscanf(line+length("%%BeginData:"), "%d %*s %s", &num, text) >= 1) {
	    if (strcmp(text, "Lines") == 0) {
		for (i=0; i < num; i++) {
		  *line_count += 1;
		  do { /* rjl: handle antisocial PostScript with excessively long lines */
		    cp = fgets(line, size, fp);
		    *line_len += cp ? strlen(line) : 0;
		  } while ( (strlen(line) == size-1) && (line[size-2] != '\n') );
		}
	    } else {
		while (num > BUFSIZ) {
		    fread(buf, sizeof (char), BUFSIZ, fp);
		    *line_len += BUFSIZ;
		    num -= BUFSIZ;
		}
		fread(buf, sizeof (char), num, fp);
		*line_len += num;
	    }
	}
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndData"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    } else if (iscomment(line+7, "Binary:")) {
	strcpy(save, line+7);
	if(sscanf(line+length("%%BeginBinary:"), "%d", &num) == 1) {
	    while (num > BUFSIZ) {
		fread(buf, sizeof (char), BUFSIZ, fp);
		*line_len += BUFSIZ;
		num -= BUFSIZ;
	    }
	    fread(buf, sizeof (char), num, fp);
	    *line_len += num;
	}
	while (readline(line, size, fp, enddoseps, NULL, &nbytes, line_count, 1) &&
	       !(DSCcomment(line) && iscomment(line+2, "EndBinary"))) {
	    *line_len += nbytes;
	}
	*line_len += nbytes;
	strcpy(line, save);
    }
    return cp;
}

/*
 *	pscopy -- copy lines of Postscript from a section of one file
 *		  to another file.
 *                Automatically switch to binary copying whenever
 *                %%BeginBinary/%%EndBinary or %%BeginData/%%EndData
 *		  comments are encountered.
 */

void
pscopy(FILE *from, FILE *to, long begin, long end)
{
    char line[PSLINELENGTH];	/* 255 characters + 1 newline + 1 NULL */
    char text[PSLINELENGTH];	/* Temporary storage for text */
    unsigned int num;
    int i;
    char buf[BUFSIZ];

    if (begin >= 0) fseek(from, begin, SEEK_SET);
    while (ftell(from) < end) {

	fgets(line, sizeof line, from);
	fputs(line, to);

	if (!(DSCcomment(line) && iscomment(line+2, "Begin"))) {
	    /* Do nothing */
	} else if (iscomment(line+7, "Data:")) {
	    text[0] = '\0';
	    if (sscanf(line+length("%%BeginData:"),
		       "%d %*s %s", &num, text) >= 1) {
		if (strcmp(text, "Lines") == 0) {
		    for (i=0; i < num; i++) {
		      do { /* rjl: handle antisocial PostScript with excessively long lines */
			fgets(line, sizeof line, from);
			fputs(line, to);
		      } while ( (strlen(line) == sizeof(line)-1) && (line[sizeof(line)-2] != '\n') );
		    }
		} else {
		    while (num > BUFSIZ) {
			fread(buf, sizeof (char), BUFSIZ, from);
			fwrite(buf, sizeof (char), BUFSIZ, to);
			num -= BUFSIZ;
		    }
		    fread(buf, sizeof (char), num, from);
		    fwrite(buf, sizeof (char), num, to);
		}
	    }
	} else if (iscomment(line+7, "Binary:")) {
	    if(sscanf(line+length("%%BeginBinary:"), "%d", &num) == 1) {
		while (num > BUFSIZ) {
		    fread(buf, sizeof (char), BUFSIZ, from);
		    fwrite(buf, sizeof (char), BUFSIZ, to);
		    num -= BUFSIZ;
		}
		fread(buf, sizeof (char), num, from);
		fwrite(buf, sizeof (char), num, to);
	    }
	}
    }
}

/*
 *	pscopyuntil -- copy lines of Postscript from a section of one file
 *		       to another file until a particular comment is reached.
 *                     Automatically switch to binary copying whenever
 *                     %%BeginBinary/%%EndBinary or %%BeginData/%%EndData
 *		       comments are encountered.
 */

char *
pscopyuntil(FILE *from, FILE *to, long begin, long end, const char *comment)
{
    char line[PSLINELENGTH];	/* 255 characters + 1 newline + 1 NULL */
    char text[PSLINELENGTH];	/* Temporary storage for text */
    unsigned int num;
    int comment_length = 0;
    int i;
    char buf[BUFSIZ];
    char *cp;

    if (comment)    /* rjl: allow NULL comment to degenerate to pscopy() */
        comment_length = strlen(comment);
    if (begin >= 0) fseek(from, begin, SEEK_SET);
    while (ftell(from) < end) {

	fgets(line, sizeof line, from);

	/* iscomment cannot be used here,
	 * because comment_length is not known at compile time. */
	if (comment && (strncmp(line, comment, comment_length) == 0)) {
	    cp = (char *) malloc(strlen(line)+1);
	    if (cp == NULL) {
		pserror("Fatal Error: Dynamic memory exhausted.\n");
		exit(-1);
	    }
	    strcpy(cp, line);
	    return cp;
	}
	fputs(line, to);
	if (!(DSCcomment(line) && iscomment(line+2, "Begin"))) {
	    /* Do nothing */
	} else if (iscomment(line+7, "Data:")) {
	    text[0] = '\0';
	    if (sscanf(line+length("%%BeginData:"),
		       "%d %*s %s", &num, text) >= 1) {
		if (strcmp(text, "Lines") == 0) {
		    for (i=0; i < num; i++) {
		      do { /* rjl: handle antisocial PostScript with excessively long lines */
			fgets(line, sizeof line, from);
			fputs(line, to);
		      } while ( (strlen(line) == sizeof(line)-1) && (line[sizeof(line)-2] != '\n') );
		    }
		} else {
		    while (num > BUFSIZ) {
			fread(buf, sizeof (char), BUFSIZ, from);
			fwrite(buf, sizeof (char), BUFSIZ, to);
			num -= BUFSIZ;
		    }
		    fread(buf, sizeof (char), num, from);
		    fwrite(buf, sizeof (char), num, to);
		}
	    }
	} else if (iscomment(line+7, "Binary:")) {
	    if(sscanf(line+length("%%BeginBinary:"), "%d", &num) == 1) {
		while (num > BUFSIZ) {
		    fread(buf, sizeof (char), BUFSIZ, from);
		    fwrite(buf, sizeof (char), BUFSIZ, to);
		    num -= BUFSIZ;
		}
		fread(buf, sizeof (char), num, from);
		fwrite(buf, sizeof (char), num, to);
	    }
	}
    }
    return NULL;
}

/*
 *	blank -- determine whether the line contains nothing but whitespace.
 */

static int
blank(char *line)
{
    char *cp = line;

    while (*cp == ' ' || *cp == '\t') cp++;
    return *cp == '\n' || (*cp == '%' && (line[0] != '%' || line[1] != '%'));
}

/* rjl: routines to handle reading DOS EPS files */
static unsigned long dsc_arch = 0x00000001;

/* change byte order if architecture is big-endian */
PS_DWORD
reorder_dword(PS_DWORD val)
{
    if (*((char *)(&dsc_arch)))
        return val;	/* little endian machine */
    else
	return ((val&0xff) << 24) | ((val&0xff00) << 8)
             | ((val&0xff0000L) >> 8) | ((val>>24)&0xff);
}

/* change byte order if architecture is big-endian */
PS_WORD
reorder_word(PS_WORD val)
{
    if (*((char *)(&dsc_arch)))
        return val;	/* little endian machine */
    else
	return (PS_WORD) ((PS_WORD)(val&0xff) << 8) | (PS_WORD)((val&0xff00) >> 8);
}

/* DOS EPS header reading */
unsigned long
ps_read_doseps(FILE *file, DOSEPS *doseps)
{
	fread(doseps->id, 1, 4, file);
	if (! ((doseps->id[0]==0xc5) && (doseps->id[1]==0xd0) 
		&& (doseps->id[2]==0xd3) && (doseps->id[3]==0xc6)) ) {
		/* id is "EPSF" with bit 7 set */
	    rewind(file);
	    return 0; 	/* OK */
	}
	fread(&doseps->ps_begin,    4, 1, file);	/* PS offset */
	doseps->ps_begin = (unsigned long)reorder_dword(doseps->ps_begin);
	fread(&doseps->ps_length,   4, 1, file);	/* PS length */
	doseps->ps_length = (unsigned long)reorder_dword(doseps->ps_length);
	fread(&doseps->mf_begin,    4, 1, file);	/* Metafile offset */
	doseps->mf_begin = (unsigned long)reorder_dword(doseps->mf_begin);
	fread(&doseps->mf_length,   4, 1, file);	/* Metafile length */
	doseps->mf_length = (unsigned long)reorder_dword(doseps->mf_length);
	fread(&doseps->tiff_begin,  4, 1, file);	/* TIFF offset */
	doseps->tiff_begin = (unsigned long)reorder_dword(doseps->tiff_begin);
	fread(&doseps->tiff_length, 4, 1, file);	/* TIFF length */
	doseps->tiff_length = (unsigned long)reorder_dword(doseps->tiff_length);
	fread(&doseps->checksum,    2, 1, file);
        doseps->checksum = (unsigned short)reorder_word(doseps->checksum);
	fseek(file, doseps->ps_begin, SEEK_SET);	/* seek to PS section */
	return doseps->ps_begin + doseps->ps_length;
}


#undef fgets
char *
psfgets(char *s, int n, FILE *stream)
{
/*
    return fgets(s, n, stream);
*/
    int ch = 0;
    char *p;
    p = s;
    while ( (--n > 0)  && ((ch = fgetc(stream)) != EOF) ) {
	*p++ = (char)ch;
	if (ch == '\n')
	    break;
	if (ch == '\r') {
	    /* cope with MS-DOS \r\n or Mac \r */
	    if (--n > 0) {
	        ch = fgetc(stream);
	        if (ch == EOF)
		    break;
	        if (ch == '\n')
	            *p++ = (char)ch;
		else
		    ungetc(ch, stream);
		break;
	    }
	}
    }
    if ((ch == EOF) && (p == s))
	return NULL;
    *p = '\0';
    return (ferror(stream)) ? NULL : s;
}



char *
psfgetslen(char *s, int n, FILE *stream, unsigned int *read)
{
/*
    return fgets(s, n, stream);
*/
    int ch = 0;
    char *p;
    p = s;
    while ( (--n > 0)  && ((ch = fgetc(stream)) != EOF) ) {
	*p++ = (char)ch;
	if (ch == '\n')
	    break;
	if (ch == '\r') {
	    /* cope with MS-DOS \r\n or Mac \r */
	    if (--n > 0) {
	        ch = fgetc(stream);
	        if (ch == EOF)
		    break;
	        if (ch == '\n')
	            *p++ = (char)ch;
		else
		    ungetc(ch, stream);
		break;
	    }
	}
    }
    if ((ch == EOF) && (p == s))
	return NULL;
    *p = '\0';
    
    *read = ((long) p - (long) s);
    
    return (ferror(stream)) ? NULL : s;
}


// Compare lexigraphically two strings

int stricmp(const char *s1, const char *s2)
{
    char c1, c2;
    while (1)
    {
    	c1 = tolower(*s1++);
    	c2 = tolower(*s2++);
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        if (c1 == 0) return 0;
    }
}

