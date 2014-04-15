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
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CGR_BLENDER_UTILS_H
#define CGR_BLENDER_UTILS_H

extern "C" {
#  include "DNA_mesh_types.h"
}

#include "BKE_main.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include <string>

enum {
	OBJECT_PERSISTENT_ID_SIZE = 8
};


Mesh* GetRenderMesh(Scene *sce, Main *bmain, Object *ob);
void  FreeRenderMesh(Main *main, Mesh *mesh);
void  FreeDupliList(Object *ob);

int   IsObjectHasActions(Object *ob);
int   IsMeshAnimated(Object *ob);
int   IsMeshValid(Scene *sce, Main *main, Object *ob);

int   IsParentUpdated(Object *ob);

std::string GetIDName(ID *id, const std::string prefix="");

#endif // CGR_BLENDER_UTILS_H
