/* 
   carbonviewer

   a simple viewer example for the Ghostscript shared library
   
   $Id$
   
   this is the document class. it remembers the rendered pages
   and parsing state and associates them with a display winow.

*/

/* linked-list structure for holding rendered pages */
typedef struct cv_page_s {
	struct cv_page_s *next;
	int width, height;
	int format;
	GWorldPtr image;
} cv_page_t;

cv_page_t *new_page(int width, int height, int format);
cv_page_t *add_page(cv_page_t *pages, int width, int height, int format);
void free_page(cv_page_t *page);
void free_pages(cv_page_t *pages);

/* collective structure associating a page list with a window */
/* we allow the Carbon event manager to keep track of these 
   via the window list */
typedef struct cv_doc_s {
	int current_page, num_pages;
	cv_page_t *pages;
	char *filename;
	WindowRef window;
	int gs_stride;
	unsigned char *gs_image;
} cv_doc_t;

cv_doc_t *new_document(void);
void free_document(cv_doc_t *doc);

int cv_copy_page(cv_doc_t *doc);