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

#ifndef SPECTRE_PAGE_H
#define SPECTRE_PAGE_H

#include <libspectre/spectre-macros.h>
#include <libspectre/spectre-status.h>
#include <libspectre/spectre-render-context.h>

SPECTRE_BEGIN_DECLS

typedef enum {
	SPECTRE_ORIENTATION_PORTRAIT /*! Vertical orientation */,
	SPECTRE_ORIENTATION_REVERSE_LANDSCAPE /*! Inverse horizontal orientation,
						also known as Seascape */,
	SPECTRE_ORIENTATION_REVERSE_PORTRAIT /*! Inverse vertical orientation */,
	SPECTRE_ORIENTATION_LANDSCAPE /*! Horizontal orientation */
} SpectreOrientation;

/*! This is the object that represents a page of a PostScript document.
    They can not be created directly and can only be obtained from
    ::spectre_document_get_page */
typedef struct SpectrePage SpectrePage;

/*! Returns the status of the given page
    @param page The page whose status will be returned
*/
SpectreStatus      spectre_page_status          (SpectrePage          *page);

/*! Frees the memory of the given page
    @param page The page whose memory will be freed
*/
void               spectre_page_free            (SpectrePage          *page);

/*! Returns the index of the page inside the document. First page has index 0
    @param page The page whose index will be returned
*/
unsigned int       spectre_page_get_index       (SpectrePage          *page);

/*! Returns the label of the page inside the document.
    @param page The page whose label will be returned
*/
const char        *spectre_page_get_label       (SpectrePage          *page);

/*! Returns the orientation of the page
    @param page The page whose orientation will be returned
*/
SpectreOrientation spectre_page_get_orientation (SpectrePage          *page);

/*! Returns the size of the page. It always returns the page size according to
    the page bounding box without taking into account the page orientation. 
    @param page The page whose size will be returned
    @param width The page width will be returned here, or NULL
    @param height The page height will be returned here, or NULL
    @see spectre_page_get_orientation
*/
void               spectre_page_get_size        (SpectrePage          *page,
						 int                  *width,
						 int                  *height);

/*! Renders the page to RGB32 format. This function can fail
    @param page The page to renderer
    @param rc The rendering context specifying how the page has to be rendered
    @param page_data A pointer that will point to the image data
                     if the call succeeds
    @param row_length The length of an image row will be returned here. It can
                      happen that row_length is different than width * 4
    @see spectre_page_status
*/
void               spectre_page_render          (SpectrePage          *page,
						 SpectreRenderContext *rc,
						 unsigned char       **page_data,
						 int                  *row_length);

/* ! Renders a rectangle of the page to RGB32 format. This function can fail
     @param page The page to renderer
     @param rc The rendering context specifying how the page has to be rendered
     @param x The X coordinate of the top left corner of the rectangle
     @param y The Y coordinate to the top left corner of the rectangle
     @param width The width of the rectangle
     @param height The height of the rectangle
     @param page_data A pointer that will point to the image data
                      if the call succeeds
     @param row_length The length of an image row will be returned here. It can
                       happen that row_length is different than width * 4
     @see spectre_page_status
*/
void               spectre_page_render_slice    (SpectrePage          *page,
						 SpectreRenderContext *rc,
						 int                   x,
						 int                   y,
						 int                   width,
						 int                   height,
						 unsigned char       **page_data,
						 int                  *row_length);

SPECTRE_END_DECLS

#endif /* SPECTRE_PAGE_H */
