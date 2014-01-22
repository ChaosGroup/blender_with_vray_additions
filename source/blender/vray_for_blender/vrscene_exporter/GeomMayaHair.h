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

#ifndef GEOM_MAYA_HAIR_H
#define GEOM_MAYA_HAIR_H

extern "C" {
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "BKE_main.h"
}

#include "exp_types.h"

#include <string>
#include <vector>


class GeomMayaHair : public VRayExportable {
public:
    GeomMayaHair();
	virtual      ~GeomMayaHair() { freeData(); }

    void          init(Scene *sce, Main *main, Object *ob, ParticleSystem *psys);
    void          freeData();

	virtual void  buildHash();

    char*         getHairVertices() const    { return hair_vertices; }
    char*         getNumHairVertices() const { return num_hair_vertices;}
    char*         getWidths() const          { return widths; }
    char*         getTransparency() const    { return transparency; }

private:

    char         *hair_vertices;
    char         *num_hair_vertices;
    char         *widths;
    char         *transparency;

    int           use_global_hair_tree;

    int           geom_splines;
    float         geom_tesselation_mult;

};

#endif // GEOM_MAYA_HAIR_H
