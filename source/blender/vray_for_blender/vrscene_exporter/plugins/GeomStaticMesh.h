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

extern "C" {
#  include "DNA_mesh_types.h"
}

#include "exp_types.h"

#include <string>
#include <vector>


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
	GeomStaticMesh(Scene *scene, Main *main, Object *ob, int checkComponents=true);

	virtual      ~GeomStaticMesh() { freeData(); }
	virtual void  initHash();
	virtual void  initName(const std::string &name="");
	virtual void  writeData(PyObject *output, VRayExportable *prevState, bool keyFrame=false);

	virtual void  preInit();
	virtual void  init();
	void          freeData();

	void          initAttributes();

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
	MHash         getEdgeVisibilityHash() const { return m_hashEdgeVisibility; }

	size_t        getMapChannelCount() const { return map_channels.size(); }
	const MChan*  getMapChannel(const size_t i) const;

private:
	void          writeGeomDisplacedMesh(PyObject *output);
	void          writeGeomStaticSmoothedMesh(PyObject *output);

	int           hasDisplace();

	void          initVertices();
	void          initFaces();
	void          initMapChannels();

	void          initDisplace();
	void          initSmooth();

	int           mapChannelsUpdated(GeomStaticMesh *prevMesh);

	Mesh         *mesh;
	StrVector     meshComponentNames;

	char         *m_vertices;
	size_t        coordIndex;

	char         *m_faces;
	size_t        vertIndex;

	char         *m_normals;
	char         *m_faceNormals;
	char         *m_faceMtlIDs;
	char         *m_edge_visibility;

	MChans        map_channels;

	// Additional hashes
	// Allows exporting only the changed
	// mesh parts
	MHash         m_hashVertices;
	MHash         m_hashFaces;
	MHash         m_hashNormals;
	MHash         m_hashFaceNormals;
	MHash         m_hashFaceMtlIDs;
	MHash         m_hashEdgeVisibility;

	// Export options
	int           m_useZip;

	// Options
	int           m_checkComponents;
	sstream       m_pluginDisplace;
	sstream       m_pluginSmooth;

	int           useSmooth;
	std::string   smoothName;

	int           useDisplace;
	std::string   displaceName;
	int           useDisplaceOverride;
	Tex          *displaceTexture;
	std::string   displaceTextureName;

	// GeomStaticMesh properties
	int           dynamic_geometry;
	int           environment_geometry;

	int           osd_subdiv_level;
	int           osd_subdiv_type;
	int           osd_subdiv_uvs;

	float         weld_threshold;

};

}

#endif // GEOM_STATIC_MESH_H
