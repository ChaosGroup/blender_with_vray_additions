/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CGR_EXPORT_GEOM_H
#define CGR_EXPORT_GEOM_H

#include "common/blender_includes.h"

#ifdef __cplusplus
extern "C" {
#endif


void ExportGeometry(FILE *outputFile, Scene *sce, Main *main, int activeLayers, int altDInstances);
void ExportGeometryAnimation(const char *outputFilepath, Scene *sce, Main *main, int activeLayers, int altDInstances, int checkAnimated);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CGR_EXPORT_GEOM_H
