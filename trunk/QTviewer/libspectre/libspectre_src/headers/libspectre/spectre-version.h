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

#ifndef SPECTRE_VERSION_H
#define SPECTRE_VERSION_H

#include <libspectre/spectre-macros.h>

SPECTRE_BEGIN_DECLS

#define SPECTRE_MAJOR_VERSION @SPECTRE_MAJOR_VERSION@
#define SPECTRE_MINOR_VERSION @SPECTRE_MINOR_VERSION@
#define SPECTRE_MICRO_VERSION @SPECTRE_MICRO_VERSION@

#define SPECTRE_VERSION_STRING "@SPECTRE_VERSION@"

#define SPECTRE_VERSION ((@SPECTRE_MAJOR_VERSION@ << 16) | (@SPECTRE_MINOR_VERSION@ << 8) | (@SPECTRE_MICRO_VERSION@))

SPECTRE_END_DECLS

#endif /* SPECTRE_VERSION_H */
