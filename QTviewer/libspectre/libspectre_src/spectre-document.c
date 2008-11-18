/* This file is part of Libspectre.
 * 
 * Copyright (C) 2007 Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Libspectre is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * Libspectre is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* For stat */
#include <sys/types.h>
#include <sys/stat.h>
/*#include <unistd.h> */

#include "spectre-document.h"
#include "spectre-private.h"
#include "spectre-exporter.h"
#include "spectre-utils.h"

struct SpectreDocument
{
	struct document *doc;
	
	SpectreStatus    status;

	int              structured;
};

SpectreDocument *
spectre_document_new (void)
{
	SpectreDocument *doc;

	doc = calloc (1, sizeof (SpectreDocument));
	return doc;
}

void
spectre_document_load (SpectreDocument *document,
		       const char      *filename)
{
	_spectre_return_if_fail (document != NULL);
	_spectre_return_if_fail (filename != NULL);
	
	if (document->doc && strcmp (filename, document->doc->filename) == 0) {
		document->status = SPECTRE_STATUS_SUCCESS;
		return;
	}

	if (document->doc) {
		psdocdestroy (document->doc);
		document->doc = NULL;
	}
	
	document->doc = psscan (filename, SCANSTYLE_NORMAL);
	if (!document->doc) {
		/* FIXME: OOM | INVALID_PS */
		document->status = SPECTRE_STATUS_LOAD_ERROR;
		return;
	}

	if (document->doc->numpages == 0 && document->doc->lenprolog == 0) {
		document->status = SPECTRE_STATUS_LOAD_ERROR;
		psdocdestroy (document->doc);
		document->doc = NULL;
		
		return;
	}

	document->structured = ((!document->doc->epsf && document->doc->numpages > 0) ||
				(document->doc->epsf && document->doc->numpages > 1));

	if (document->status != SPECTRE_STATUS_SUCCESS)
		document->status = SPECTRE_STATUS_SUCCESS;
}

void
spectre_document_free (SpectreDocument *document)
{
	if (!document)
		return;

	if (document->doc) {
		psdocdestroy (document->doc);
		document->doc = NULL;
	}

	free (document);
}

SpectreStatus
spectre_document_status (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, SPECTRE_STATUS_DOCUMENT_NOT_LOADED);
	
	return document->status;
}

unsigned int
spectre_document_get_n_pages (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, 0);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return 0;
	}
	
	return document->structured ? document->doc->numpages : 1;
}

SpectreOrientation
spectre_document_get_orientation (SpectreDocument *document)
{
	int doc_orientation;

	_spectre_return_val_if_fail (document != NULL, SPECTRE_ORIENTATION_PORTRAIT);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return SPECTRE_ORIENTATION_PORTRAIT;
	}

	doc_orientation = document->doc->orientation != NONE ?
		document->doc->orientation : document->doc->default_page_orientation;
	
	switch (doc_orientation) {
	default:
	case PORTRAIT:
		return SPECTRE_ORIENTATION_PORTRAIT;
	case LANDSCAPE:
		return SPECTRE_ORIENTATION_LANDSCAPE;
	case SEASCAPE:
		return SPECTRE_ORIENTATION_REVERSE_LANDSCAPE;
	case UPSIDEDOWN:
		return SPECTRE_ORIENTATION_REVERSE_PORTRAIT;
	}
}

const char *
spectre_document_get_title (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, NULL);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return NULL;
	}

	return document->doc->title;
}

const char *
spectre_document_get_creator (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, NULL);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return NULL;
	}

	return document->doc->creator;
}

const char *
spectre_document_get_for (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, NULL);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return NULL;
	}

	return document->doc->fortext;
}

const char *
spectre_document_get_creation_date (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, NULL);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return NULL;
	}
	
	return document->doc->date;
}

const char *
spectre_document_get_format (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, NULL);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return NULL;
	}

	return document->doc->format;
}

int
spectre_document_is_eps (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, FALSE);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return FALSE;
	}

	return document->doc->epsf;
}

unsigned int 
spectre_document_get_language_level (SpectreDocument *document)
{
	_spectre_return_val_if_fail (document != NULL, 0);
	
	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return 0;
	}

	return document->doc->languagelevel ? atoi (document->doc->languagelevel) : 0;
}

SpectrePage *
spectre_document_get_page (SpectreDocument *document,
			   unsigned int     page_index)
{
	SpectrePage *page;
	unsigned int index;

	_spectre_return_val_if_fail (document != NULL, NULL);

	index = (document->doc->pageorder == DESCEND) ?
		(document->doc->numpages - 1) - page_index :
		page_index;

	if (index >= spectre_document_get_n_pages (document)) {
		document->status = SPECTRE_STATUS_INVALID_PAGE;
		return NULL;
	}

	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return NULL;
	}
	
	page = _spectre_page_new (index, document->doc);
	if (!page) {
		document->status = SPECTRE_STATUS_NO_MEMORY;
		return NULL;
	}

	if (document->status != SPECTRE_STATUS_SUCCESS)
		document->status = SPECTRE_STATUS_SUCCESS;
	
	return page;
}

SpectrePage *
spectre_document_get_page_by_label (SpectreDocument *document,
				    const char      *label)
{
	unsigned int i;
	int page_index = -1;

	_spectre_return_val_if_fail (document != NULL, NULL);
	
	if (!label) {
		document->status = SPECTRE_STATUS_INVALID_PAGE;
		return NULL;
	}

	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return NULL;
	}

	for (i = 0; i < document->doc->numpages; i++) {
		if (strcmp (document->doc->pages[i].label, label) == 0) {
			page_index = i;
			break;
		}
	}

	if (page_index == -1) {
		document->status = SPECTRE_STATUS_INVALID_PAGE;
		return NULL;
	}

	return spectre_document_get_page (document, page_index);
}

void
spectre_document_render_full (SpectreDocument      *document,
			      SpectreRenderContext *rc,
			      unsigned char       **page_data,
			      int                  *row_length)
{
	SpectrePage *page;

	_spectre_return_if_fail (document != NULL);
	_spectre_return_if_fail (rc != NULL);

	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return;
	}

	page = spectre_document_get_page (document, 0);
	if (!page || document->status != SPECTRE_STATUS_SUCCESS) {
		spectre_page_free (page);
		return;
	}
		
	spectre_page_render (page, rc, page_data, row_length);
	document->status = spectre_page_status (page);

	spectre_page_free (page);
}

void
spectre_document_render (SpectreDocument *document,
			 unsigned char  **page_data,
			 int             *row_length)
{
	SpectreRenderContext *rc;
	
	_spectre_return_if_fail (document != NULL);

	rc = spectre_render_context_new ();
	spectre_document_render_full (document, rc, page_data, row_length);
	spectre_render_context_free (rc);
}

void
spectre_document_get_page_size (SpectreDocument *document,
				int             *width,
				int             *height)
{
	SpectrePage *page;
	int          w, h;

	_spectre_return_if_fail (document != NULL);

	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return;
	}

	page = spectre_document_get_page (document, 0);
	if (!page || document->status != SPECTRE_STATUS_SUCCESS) {
		spectre_page_free (page);
		return;
	}
		
	spectre_page_get_size (page, &w, &h);
	if (width)
		*width = w;
	if (height)
		*height = h;
	
	spectre_page_free (page);
}

void
spectre_document_save (SpectreDocument *document,
		       const char      *filename)
{
	struct stat stat_buf;
	FILE *from;
	FILE *to;

	_spectre_return_if_fail (document != NULL);
	_spectre_return_if_fail (filename != NULL);

	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return;
	}
	
	if (stat (document->doc->filename, &stat_buf) != 0) {
		document->status = SPECTRE_STATUS_SAVE_ERROR;
		return;
	}

	from = fopen (document->doc->filename, "rb");
	if (!from) {
		document->status = SPECTRE_STATUS_SAVE_ERROR;
		return;
	}

	to = fopen (filename, "wb");
	if (!to) {
		document->status = SPECTRE_STATUS_SAVE_ERROR;
		fclose (from);
		return;
	}

	pscopy (from, to, document->doc, 0, stat_buf.st_size - 1);

	fclose (from);
	fclose (to);

	document->status = SPECTRE_STATUS_SUCCESS;
}

void
spectre_document_save_to_pdf (SpectreDocument *document,
			      const char      *filename)
{
	SpectreExporter *exporter;
	SpectreStatus    status;
	unsigned int     i;

	_spectre_return_if_fail (document != NULL);
	_spectre_return_if_fail (filename != NULL);

	if (!document->doc) {
		document->status = SPECTRE_STATUS_DOCUMENT_NOT_LOADED;
		return;
	}

	exporter = spectre_exporter_new (document, SPECTRE_EXPORTER_FORMAT_PDF);
	if (!exporter) {
		document->status = SPECTRE_STATUS_NO_MEMORY;
		return;
	}

	status = spectre_exporter_begin (exporter, filename);
	if (status) {
		document->status = status == SPECTRE_STATUS_NO_MEMORY ?
			SPECTRE_STATUS_NO_MEMORY : SPECTRE_STATUS_SAVE_ERROR;
		spectre_exporter_free (exporter);
		return;
	}

	for (i = 0; i < spectre_document_get_n_pages (document); i++) {
		status = spectre_exporter_do_page (exporter, i);
		if (status)
			break;
	}

	if (status) {
		document->status = status == SPECTRE_STATUS_NO_MEMORY ?
			SPECTRE_STATUS_NO_MEMORY : SPECTRE_STATUS_SAVE_ERROR;
		spectre_exporter_free (exporter);
		return;
	}
		
	status = spectre_exporter_end (exporter);
	spectre_exporter_free (exporter);

	if (status) {
		document->status = status == SPECTRE_STATUS_NO_MEMORY ?
			SPECTRE_STATUS_NO_MEMORY : SPECTRE_STATUS_SAVE_ERROR;
	} else {
		document->status = SPECTRE_STATUS_SUCCESS;
	}
}

struct document *
_spectre_document_get_doc (SpectreDocument *document)
{
	return document->doc;
}
