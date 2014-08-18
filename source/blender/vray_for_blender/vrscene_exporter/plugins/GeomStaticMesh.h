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

#ifndef GEOM_STATIC_MESH_H
#define GEOM_STATIC_MESH_H

#include "exp_types.h"


namespace VRayScene {

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

	MHash        hash;
	MHash        hashUvVertices;
	MHash        hashUvFaces;
};

typedef std::vector<MChan*> MChans;


class GeomStaticMesh : public VRayExportable {
public:
	GeomStaticMesh(Scene *scene, Main *main, Object *ob);

	virtual      ~GeomStaticMesh() { freeData(); }
	virtual void  initHash();
	virtual void  initName(const std::string &name="");
	virtual void  writeData(PyObject *output, VRayExportable *prevState, bool keyFrame=false);

	virtual void  preInit();
	virtual void  init();
	void          freeData();

	void          initAttributes();
	void          initAttributes(PointerRNA *ptr);

	char*         getVertices() const       { return m_vertices; }
	char*         getFaces() const          { return m_faces; }
	char*         getNormals() const        { return m_normals; }
	char*         getFaceNormals() const    { return m_faceNormals; }
	char*         getFaceMtlIDs() const     { return m_faceMtlIDs; }
	char*         getEdgeVisibility() const { return m_edge_visibility; }

	MHash         getVerticesHash() const       { return m_hashVertices; }
	MHash         getFacesHash() const          { return m_hashFaces; }
	MHash         getNormalsHash() const        { return m_hashNormals; }
	MHash         getFaceNormalsHash() const    { return m_hashFaceNormals; }
	MHash         getFaceMtlIDsHash() const     { return m_hashFaceMtlIDs; }
	MHash         getEdgeVisibilityHash() const { return m_hashEdgeVis; }

	int           getMapChannelCount() const { return map_channels.size(); }
	const MChan*  getMapChannel(const int i) const;

private:
	void          initVertices();
	void          initFaces();
	void          initMapChannels();

	int           mapChannelsUpdated(GeomStaticMesh *prevMesh);

	MHash         hashArray(void *data, int dataLen);
	void          freeArrays();

	BL::BlendData b_data;
	BL::Scene     b_scene;
	BL::Object    b_object;
	BL::Mesh      b_mesh;

	// Char buffers with ZIP'ed data
	char         *m_vertices;
	char         *m_faces;
	char         *m_normals;
	char         *m_faceNormals;
	char         *m_faceMtlIDs;
	char         *m_edge_visibility;

	MChans        map_channels;

	// Data arrays and hashes
	float        *m_vertsArray;
	int          *m_facesArray;
	int          *m_faceNormalsArray;
	float        *m_normalsArray;
	int          *m_mtlIDsArray;
	int          *m_edgeVisArray;

	int           m_vertsArraySize;
	int           m_facesArraySize;
	int           m_normalsArraySize;
	int           m_faceNormalsArraySize;
	int           m_mtlIDArraySize;
	int           m_edgeVisArraySize;

	MHash         m_hashVertices;
	MHash         m_hashFaces;
	MHash         m_hashNormals;
	MHash         m_hashFaceNormals;
	MHash         m_hashFaceMtlIDs;
	MHash         m_hashEdgeVis;

	// GeomStaticMesh properties
	int           dynamic_geometry;
	int           environment_geometry;
	int           osd_subdiv_level;
	int           osd_subdiv_type;
	int           osd_subdiv_uvs;
	float         weld_threshold;
	int           primary_visibility;
	int           smooth_uv;
	int           smooth_uv_borders;

};

}

#endif // GEOM_STATIC_MESH_H
