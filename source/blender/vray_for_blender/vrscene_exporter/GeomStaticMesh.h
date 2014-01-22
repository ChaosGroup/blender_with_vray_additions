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

#ifndef GEOM_STATIC_MESH_H
#define GEOM_STATIC_MESH_H

extern "C" {
#  include "DNA_mesh_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_object_types.h"
#  include "BKE_main.h"
}

#include "exp_types.h"

#include <string>
#include <vector>


class MChan {
public:
    MChan();
    ~MChan() { freeData(); }

    void         freeData();

    std::string  name;
    int          index;
    int          cloned;
    char        *uv_vertices;
    char        *uv_faces;
};

typedef std::vector<MChan*> MChans;


class GeomStaticMesh : public VRayExportable {
public:
    GeomStaticMesh();
	virtual      ~GeomStaticMesh() { freeData(); }

    void          init(Scene *sce, Main *main, Object *ob);
    void          freeData();

	virtual void  buildHash();

    char*         getVertices() const        { return vertices; }
    char*         getFaces() const           { return faces;}
    char*         getNormals() const         { return normals; }
    char*         getFaceNormals() const     { return faceNormals; }
    char*         getFace_mtlIDs() const     { return face_mtlIDs; }
    char*         getEdge_visibility() const { return edge_visibility; }

    size_t        getMapChannelCount() const { return map_channels.size(); }
    const MChan*  getMapChannel(const size_t i) const;

private:
    void          initVertices();
    void          initFaces();
    void          initMapChannels();

	void          initName();

    Mesh         *mesh;
    Object       *object;

    char         *vertices;
    size_t        coordIndex;

    char         *faces;
    size_t        vertIndex;

    char         *normals;
    char         *faceNormals;
    char         *face_mtlIDs;
    char         *edge_visibility;

    MChans        map_channels;

    int           dynamic_geometry;
    int           environment_geometry;

    int           osd_subdiv_level;
    int           osd_subdiv_type;
    int           osd_subdiv_uvs;

    float         weld_threshold;
};


#endif // GEOM_STATIC_MESH_H
