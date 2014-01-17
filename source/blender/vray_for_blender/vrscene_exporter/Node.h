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

#ifndef CGR_NODE_H
#define CGR_NODE_H

extern "C" {
#  include "DNA_mesh_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_object_types.h"
#  include "BKE_main.h"
}

#include "utils/CGR_vrscene.h"
#include "utils/murmur3.h"

#include <string>
#include <vector>


namespace VRScene {

struct Node {
    Node();
    ~Node() { freeData(); }

    void          init(Scene *sce, Main *main, Object *ob, DupliObject *dOb=NULL);
    void          freeData();

    MHash         getHash() const { return hash; }

    char*         getTransform() const;

public:
    int           getObjectID() const;

private:
    MHash         hash;
    Object       *object;

    int           objectID;
    char          transform[TRANSFORM_HEX_SIZE];

};

}

#endif // CGR_NODE_H
