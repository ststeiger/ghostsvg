/* 
   carbonviewer

   a simple viewer example for the Ghostscript shared library
   
   $Id$
   
   this is the document class. it remembers the rendered pages
   and parsing state and associates them with a display winow.

*/

/* implementation of page linked list */
/* each element saves the bitmap rendered by the ghostscript
   library in a QuickDraw offscreen gworld structure. this is
   hardly memory efficient, but simple. */

#include <Carbon.h>
#include <stdlib.h>
#include <string.h>

#include "document.h"

/* create and initialize a new page structure */ 
cv_page_t *new_page(int width, int height, int format)
{
	QDErr err;
	Rect bounds;
	cv_page_t *page = malloc(sizeof(cv_page_t));

	page->next = NULL;
	
	page->width = width;
	page->height = height;
	page->format = format;	// NB: we assume this is native 32-bit for now
	
	SetRect(&bounds, 0, 0, width, height);
	err = NewGWorld(&(page->image), 32, &bounds, NULL, NULL, 0);
	
	return page;
}

/* add a page at the head of a pagelist (which may be NULL) */
cv_page_t *add_page(cv_page_t *pages, int width, int height, int format)
{
	cv_page_t *page;
	
	page = new_page(width, height, format);
	page->next = pages;
	
	return page;
}

/* free a page structure */
void free_page(cv_page_t *page)
{
	if (page) {
		DisposeGWorld(page->image);
		free(page);
	}
}

/* free a linked list of pages */
void free_pages(cv_page_t *pages)
{
	cv_page_t *next, *page = pages;
	
	while (page) {
		next = page->next;
		free_page(page);
		page = next;
	}
}

/* copy a page image from the ghostscript buffer
   to our offscreen pixmap */
int cv_copy_page(cv_doc_t *doc)
{
	cv_page_t	*page = doc->pages;
	GWorldPtr	offworld;
	CGrafPtr	onport, saveport;
	GDHandle	savedevice;
	Rect		bounds, pagebounds;
	PixMapHandle	offpix, onpix;
	int			offstride;
	QDErr		err;
	int			i;
	
	GetGWorld(&saveport, &savedevice);
	
	SetRect(&pagebounds, 0, 0, page->width, page->height);
	err = NewGWorld(&offworld, 32, &pagebounds, NULL, NULL, 0);
	SetGWorld(page->image, NULL);
	
	offpix = GetGWorldPixMap(offworld);
	LockPixels(offpix);
	offstride = GetPixRowBytes(offpix);
	for (i = 0; i < page->height; i++) {
		memcpy((*offpix)->baseAddr + i*offstride,
				doc->gs_image + i*doc->gs_stride, offstride);
	}
	UnlockPixels(offpix);
	SetGWorld(saveport, savedevice);
	
	GetWindowBounds(doc->window, kWindowContentRgn, &bounds);
	onport = GetWindowPort(doc->window);	
	onpix = GetPortPixMap(onport);
	
	SetPort(onport);
	LockPixels(offpix);
	CopyBits((BitMap*)*offpix, (BitMap*)*onpix,
			&pagebounds, &bounds, srcCopy, NULL);
	UnlockPixels(offpix);
	
	//InvalWindowRect(page->window, &bounds);
		
	DisposeGWorld(offworld);
	SetGWorld(saveport, savedevice);

	return 0;
}

/* implementation of document objects. */
/*  we allow the Carbon event manager to keep track of these 
   via the window list */

/* allocate a new document structure */
cv_doc_t *new_document(void)
{
	cv_doc_t *doc = malloc(sizeof(cv_doc_t));
	
	doc->current_page = 0;
	doc->num_pages = 0;
	doc->pages = NULL;
	doc->filename = NULL;
	
	doc->window = NULL;
	
	doc->gs_stride = 0;
	doc->gs_image = NULL;
	
	return doc;
}

/* free a document structure */
void free_document(cv_doc_t *doc)
{
	if (doc) {
		free_pages(doc->pages);
		free(doc);
	}
}
