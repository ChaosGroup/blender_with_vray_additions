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

#include "cgr_config.h"

#include "GeomStaticMesh.h"

#include <boost/unordered_set.hpp>
#include <boost/lexical_cast.hpp>
#include <locale>
#include <algorithm>

#include "cgr_blender_data.h"
#include "cgr_string.h"
#include "cgr_vrscene.h"
#include "cgr_rna.h"

#include "cgr_hash.h"

#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_customdata.h"
#include "MEM_guardedalloc.h"
#include "BLI_math.h"
#include "BLI_sys_types.h"
#include "BLI_string.h"
#include "BLI_path_util.h"

extern "C" {
#  include "DNA_meshdata_types.h"
#  include "DNA_material_types.h"
}


using namespace VRayScene;


typedef BL::Array<int, 4>   FaceVerts;
typedef BL::Array<float, 2> UvVert;
typedef BL::Array<float, 3> ColVert;


struct MapVertex {
	MapVertex() {
		index = 0;
	}

	MapVertex(const UvVert &uv) {
		MapVertex();

		v[0] = uv.data[0];
		v[1] = uv.data[1];
		v[2] = 0.0f;
	}

	MapVertex(const ColVert &col) {
		MapVertex();

		v[0] = col.data[0];
		v[1] = col.data[1];
		v[2] = col.data[2];
	}

	bool operator == (const MapVertex &_v) const {
		return (v[0] == _v.v[0]) && (v[1] == _v.v[1]) && (v[2] == _v.v[2]);
	}

	float        v[3];
	mutable int  index;

};

struct MapVertexHash
{
	std::size_t operator () (const MapVertex &_v) const {
		MHash hash;
		MurmurHash3_x86_32(_v.v, 3 * sizeof(float), 42, &hash);
		return (std::size_t)hash;
	}
};

typedef boost::unordered_set<MapVertex, MapVertexHash> UvSet;


MChan::MChan()
{
	name = "";
	index = 0;
	cloned = false;
	uv_vertices = NULL;
	uv_faces = NULL;

	hash = 1;
	hashUvVertices = 1;
	hashUvFaces = 1;
}


void MChan::freeData()
{
	FREE_ARRAY(uv_vertices);

	if (NOT(cloned)) {
		FREE_ARRAY(uv_faces);
	}
}


GeomStaticMesh::GeomStaticMesh(Scene *scene, Main *main, Object *ob):
	VRayExportable(scene, main, ob),
	b_data(PointerRNA_NULL),
	b_scene(PointerRNA_NULL),
	b_object(PointerRNA_NULL),
	b_mesh(PointerRNA_NULL)
{
	m_vertices = NULL;
	m_faces = NULL;
	m_normals = NULL;
	m_faceNormals = NULL;
	m_faceMtlIDs = NULL;
	m_edge_visibility = NULL;

	m_vertsArray = NULL;
	m_facesArray = NULL;
	m_faceNormalsArray = NULL;
	m_normalsArray = NULL;
	m_mtlIDsArray = NULL;
	m_edgeVisArray = NULL;

	m_vertsArraySize = 0;
	m_facesArraySize = 0;
	m_normalsArraySize = 0;
	m_faceNormalsArraySize = 0;
	m_mtlIDArraySize = 0;
	m_edgeVisArraySize = 0;

	map_channels.clear();

	dynamic_geometry = 0;
	environment_geometry = 0;
	primary_visibility = 1;
	osd_subdiv_level = 0;
	osd_subdiv_type = 0;
	osd_subdiv_uvs = 0;
	weld_threshold = -1.0f;
	smooth_uv         = true;
	smooth_uv_borders = true;

	m_force_osd = false;

	m_hash = 1;
	m_hashVertices = 1;
	m_hashFaces = 1;
	m_hashNormals = 1;
	m_hashFaceNormals = 1;
	m_hashFaceMtlIDs = 1;
	m_hashEdgeVis = 1;

	m_propGroup = Py_None;

	// Prepare RNA pointers
	PointerRNA dataPtr;
	RNA_id_pointer_create((ID*)m_main, &dataPtr);
	b_data = BL::BlendData(dataPtr);

	PointerRNA scenePtr;
	RNA_id_pointer_create((ID*)m_sce, &scenePtr);
	b_scene = BL::Scene(scenePtr);

	PointerRNA objectPtr;
	RNA_id_pointer_create((ID*)m_ob, &objectPtr);
	b_object = BL::Object(objectPtr);
}


void GeomStaticMesh::init()
{
	BL::SubsurfModifier b_sbs(PointerRNA_NULL);
	int b_sbs_show_render = true;

	if (ExporterSettings::gSet.m_subsurfToOSD) {
		if (b_object.modifiers.length()) {
			BL::Modifier b_mod = b_object.modifiers[b_object.modifiers.length()-1];
			if (b_mod && b_mod.show_render() && b_mod.type() == BL::Modifier::type_SUBSURF) {
				b_sbs = BL::SubsurfModifier(b_mod);
				b_sbs_show_render = b_sbs.show_render();
				b_sbs.show_render(false);

				m_force_osd = true;
				osd_subdiv_type  = b_sbs.subdivision_type() == BL::SubsurfModifier::subdivision_type_CATMULL_CLARK ? 0 : 1;
				osd_subdiv_level = b_sbs.render_levels();
				osd_subdiv_uvs   = b_sbs.use_subsurf_uv();
			}
		}
	}

	b_mesh = b_data.meshes.new_from_object(b_scene, b_object, true, 2, false, false);
	if (!b_mesh) {
		EMPTY_HEX_DATA(m_vertices);
		EMPTY_HEX_DATA(m_faces);
	}
	else {
		if (b_sbs) {
			b_sbs.show_render(b_sbs_show_render);
		}
		if (b_mesh.use_auto_smooth()) {
			b_mesh.calc_normals_split();
		}
		b_mesh.calc_tessface(true);

		// NOTE: Mesh could actually have no data.
		// This could be fine for mesh animated with "Build" mod, for example.

		initVertices();
		initFaces();
		initMapChannels();
		initAttributes();

		initHash();

		b_data.meshes.remove(b_mesh, false);
		b_mesh = BL::Mesh(PointerRNA_NULL);
	}
}


void GeomStaticMesh::preInit()
{
	initName();
}


void GeomStaticMesh::freeData()
{
	DEBUG_PRINT(CGR_USE_DESTR_DEBUG, COLOR_RED "GeomStaticMesh::freeData(" COLOR_YELLOW "%s" COLOR_RED ")" COLOR_DEFAULT, m_name.c_str());

	FREE_ARRAY(m_vertices);
	FREE_ARRAY(m_normals);
	FREE_ARRAY(m_faces);
	FREE_ARRAY(m_faceNormals);
	FREE_ARRAY(m_faceMtlIDs);
	FREE_ARRAY(m_edge_visibility);

	for(int i = 0; i < map_channels.size(); ++i) {
		map_channels[i]->freeData();
	}
}


void GeomStaticMesh::initName(const std::string &name)
{
	if(NOT(name.empty()))
		m_name = name;
	else {
		const bool could_instance = CouldInstance(b_scene, b_object);

		m_name = GetIDName(could_instance
		                   ? (ID*)m_ob->data
		                   : (ID*)m_ob) + "@Geom";
	}
}


int GeomStaticMesh::mapChannelsUpdated(GeomStaticMesh *prevMesh)
{
	if(NOT(prevMesh))
		return 1;

	if(NOT(map_channels.size()))
		return 0;

	size_t mapChannelCount = map_channels.size();
	for(size_t i = 0; i < mapChannelCount; ++i) {
		const MChan *curChan = this->getMapChannel(i);
		const MChan *prevChan = prevMesh->getMapChannel(i);

		if(prevChan == NULL)
			continue;

		if(curChan->hash != prevChan->hash)
			return 1;
	}

	return 0;
}


const MChan* GeomStaticMesh::getMapChannel(const int i) const
{
	if(i >= map_channels.size())
		return NULL;
	return map_channels[i];
}


void GeomStaticMesh::initVertices()
{
	if(NOT(b_mesh && b_mesh.vertices.length())) {
		EMPTY_HEX_DATA(m_vertices);
		return;
	}

	m_vertsArraySize = 3 * b_mesh.vertices.length();
	m_vertsArray     = new float[m_vertsArraySize];

	BL::Mesh::vertices_iterator vertIt;
	int coordIndex = 0;
	for(b_mesh.vertices.begin(vertIt); vertIt != b_mesh.vertices.end(); ++vertIt) {
		m_vertsArray[coordIndex+0] = vertIt->co()[0];
		m_vertsArray[coordIndex+1] = vertIt->co()[1];
		m_vertsArray[coordIndex+2] = vertIt->co()[2];
		coordIndex += 3;
	}

	m_vertices = GetExportString((u_int8_t*)m_vertsArray, m_vertsArraySize * sizeof(float));
}


void GeomStaticMesh::initFaces()
{
	if(NOT(b_mesh && b_mesh.tessfaces.length())) {
		EMPTY_HEX_DATA(m_faces);
		EMPTY_HEX_DATA(m_normals);
		EMPTY_HEX_DATA(m_faceNormals);
		EMPTY_HEX_DATA(m_faceMtlIDs);
		EMPTY_HEX_DATA(m_edge_visibility);
		return;
	}

	const bool useAutoSmooth = b_mesh.use_auto_smooth();

	// Prepass faces
	BL::Mesh::tessfaces_iterator faceIt;
	m_totTriFaces = 0;
	for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt) {
		FaceVerts faceVerts = faceIt->vertices_raw();

		// If face is quad we split it into 2 tris
		m_totTriFaces += faceVerts[3] ? 2 : 1;
	}

	m_facesArraySize        = 3 *     m_totTriFaces;
	m_faceNormalsArraySize  = 3 *     m_totTriFaces;
	m_normalsArraySize      = 3 * 3 * m_totTriFaces;
	m_mtlIDArraySize        =         m_totTriFaces;
	m_edgeVisArraySize      =         m_totTriFaces / 10 + ((m_totTriFaces % 10 > 0) ? 1 : 0);

	m_facesArray       = new int[m_facesArraySize];
	m_faceNormalsArray = new int[m_faceNormalsArraySize];
	m_normalsArray     = new float[m_normalsArraySize];
	m_mtlIDsArray      = new int[m_mtlIDArraySize];
	m_edgeVisArray     = new int[m_edgeVisArraySize];

	// Reset some arrays
	memset(m_edgeVisArray, 0, m_edgeVisArraySize * sizeof(int));

	int normArrIndex   = 0;
	int faceVertIndex  = 0;
	int faceNormIndex  = 0;
	int faceMtlIDIndex = 0;
	int currentTr = 0;

	for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt) {
		FaceVerts faceVerts = faceIt->vertices_raw();

		// Normals
		float  n0[3];
		float  n1[3];
		float  n2[3];
		float  n3[3];

		if(useAutoSmooth) {
			BL::Array<float, 12> autoNo = faceIt->split_normals();

			copy_v3_v3(n0, &autoNo.data[0 * 3]);
			copy_v3_v3(n1, &autoNo.data[1 * 3]);
			copy_v3_v3(n2, &autoNo.data[2 * 3]);
			if(faceVerts[3])
				copy_v3_v3(n3, &autoNo.data[3 * 3]);
		}
		else {
			if(faceIt->use_smooth()) {
				copy_v3_v3(n0, &b_mesh.vertices[faceVerts[0]].normal().data[0]);
				copy_v3_v3(n1, &b_mesh.vertices[faceVerts[1]].normal().data[0]);
				copy_v3_v3(n2, &b_mesh.vertices[faceVerts[2]].normal().data[0]);
				if(faceVerts[3])
					copy_v3_v3(n3, &b_mesh.vertices[faceVerts[3]].normal().data[0]);
			}
			else {
				float fno[3];
				copy_v3_v3(fno, &faceIt->normal().data[0]);

				copy_v3_v3(n0, fno);
				copy_v3_v3(n1, fno);
				copy_v3_v3(n2, fno);

				if(faceVerts[3])
					copy_v3_v3(n3, fno);
			}
		}

		// Material ID
		int matID = faceIt->material_index() + 1;

		// Store face vertices
		m_facesArray[faceVertIndex++] = faceVerts[0];
		m_facesArray[faceVertIndex++] = faceVerts[1];
		m_facesArray[faceVertIndex++] = faceVerts[2];
		if(faceVerts[3]) {
			m_facesArray[faceVertIndex++] = faceVerts[0];
			m_facesArray[faceVertIndex++] = faceVerts[2];
			m_facesArray[faceVertIndex++] = faceVerts[3];
		}

		// Store normals
		COPY_VECTOR(m_normalsArray, normArrIndex, n0);
		COPY_VECTOR(m_normalsArray, normArrIndex, n1);
		COPY_VECTOR(m_normalsArray, normArrIndex, n2);
		if(faceVerts[3]) {
			COPY_VECTOR(m_normalsArray, normArrIndex, n0);
			COPY_VECTOR(m_normalsArray, normArrIndex, n2);
			COPY_VECTOR(m_normalsArray, normArrIndex, n3);
		}

		// Store face normals
		int verts = faceVerts[3] ? 6 : 3;
		for(int v = 0; v < verts; ++v) {
			m_faceNormalsArray[faceNormIndex] = faceNormIndex;
			faceNormIndex++;
		}

		// Store material ID
		m_mtlIDsArray[faceMtlIDIndex++] = matID;
		if(faceVerts[3])
			m_mtlIDsArray[faceMtlIDIndex++] = matID;

		// Store edge visibility
		if(faceVerts[3]) {
			m_edgeVisArray[currentTr/10] |= (3 << ((currentTr%10)*3));
			currentTr++;
			m_edgeVisArray[currentTr/10] |= (6 << ((currentTr%10)*3));
			currentTr++;
		}
		else {
			m_edgeVisArray[currentTr/10] |= (7 << ((currentTr%10)*3));
			currentTr++;
		}
	}

	m_faces           = GetExportString((u_int8_t*)m_facesArray,       faceVertIndex  * sizeof(int));
	m_normals         = GetExportString((u_int8_t*)m_normalsArray,     normArrIndex   * sizeof(float));
	m_faceNormals     = GetExportString((u_int8_t*)m_faceNormalsArray, faceNormIndex  * sizeof(int));
	m_faceMtlIDs      = GetExportString((u_int8_t*)m_mtlIDsArray,      faceMtlIDIndex * sizeof(int));
	m_edge_visibility = GetExportString((u_int8_t*)m_edgeVisArray,     m_edgeVisArraySize * sizeof(int));
}


void GeomStaticMesh::initMapChannels()
{
	if(NOT(b_mesh && b_mesh.tessfaces.length())) {
		return;
	}

	int channelCount = 0;
	channelCount += b_mesh.tessface_uv_textures.length();
	channelCount += b_mesh.tessface_vertex_colors.length();

	if(NOT(channelCount))
		return;

	int uv_layer_id = 0;

	// UV
	//
	BL::Mesh::tessface_uv_textures_iterator uvIt;
	for(b_mesh.tessface_uv_textures.begin(uvIt); uvIt != b_mesh.tessface_uv_textures.end(); ++uvIt) {
		MChan *mapChannel = new MChan();
		mapChannel->name  = uvIt->name();

		if(std::count_if(mapChannel->name.begin(), mapChannel->name.end(), ::isdigit) == mapChannel->name.size()) {
			mapChannel->index = boost::lexical_cast<int>(mapChannel->name);
		}
		else {
			mapChannel->index = uv_layer_id++;
		}

		if(m_totTriFaces) {
			UvSet uvSet;

			BL::Mesh::tessfaces_iterator faceIt;
			int f = 0;
			for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt, ++f) {
				FaceVerts faceVerts = faceIt->vertices_raw();

				uvSet.insert(MapVertex(uvIt->data[f].uv1()));
				uvSet.insert(MapVertex(uvIt->data[f].uv2()));
				uvSet.insert(MapVertex(uvIt->data[f].uv3()));

				if(faceVerts[3])
					uvSet.insert(MapVertex(uvIt->data[f].uv4()));
			}

			f = 0;
			for (UvSet::iterator uvVertIt = uvSet.begin(); uvVertIt != uvSet.end(); ++uvVertIt, ++f) {
				(*uvVertIt).index = f;
			}

			const int mapVertexArraySize = 3 * uvSet.size();
			float *mapVertex = new float[mapVertexArraySize];
			int c = 0;
			for (UvSet::const_iterator uvVertIt = uvSet.begin(); uvVertIt != uvSet.end(); ++uvVertIt) {
				const MapVertex &uv = *uvVertIt;
				mapVertex[c++] = uv.v[0];
				mapVertex[c++] = uv.v[1];
				mapVertex[c++] = uv.v[2];
			}

			int  mapFacesArraySize  = 3 * m_totTriFaces;
			int *mapFaces = new int[mapFacesArraySize];

			int vertIndex = 0;
			f = 0;
			for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt, ++f) {
				FaceVerts faceVerts = faceIt->vertices_raw();

				const int v0 = uvSet.find(MapVertex(uvIt->data[f].uv1()))->index;
				const int v1 = uvSet.find(MapVertex(uvIt->data[f].uv2()))->index;
				const int v2 = uvSet.find(MapVertex(uvIt->data[f].uv3()))->index;

				mapFaces[vertIndex++] = v0;
				mapFaces[vertIndex++] = v1;
				mapFaces[vertIndex++] = v2;

				if(faceVerts[3]) {
					const int v3 = uvSet.find(MapVertex(uvIt->data[f].uv4()))->index;

					mapFaces[vertIndex++] = v0;
					mapFaces[vertIndex++] = v2;
					mapFaces[vertIndex++] = v3;
				}
			}

			mapChannel->uv_faces    = GetExportString((u_int8_t*)mapFaces, vertIndex * sizeof(int));
			mapChannel->uv_vertices = GetExportString((u_int8_t*)mapVertex, mapVertexArraySize * sizeof(float));

			mapChannel->hashUvFaces = ExporterSettings::gSet.m_isAnimation
									  ? hashArray(mapFaces, vertIndex * sizeof(int))
									  : 1;
			mapChannel->hashUvVertices = ExporterSettings::gSet.m_isAnimation
									  ? hashArray(mapVertex, mapVertexArraySize * sizeof(float))
									  : 1;

			delete [] mapFaces;
			delete [] mapVertex;
		}
		else {
			EMPTY_HEX_DATA(mapChannel->uv_faces);
			EMPTY_HEX_DATA(mapChannel->uv_vertices);

			mapChannel->hashUvVertices = 1;
			mapChannel->hashUvFaces = 1;
		}

		mapChannel->hash = mapChannel->hashUvVertices ^ mapChannel->hashUvFaces;

		map_channels.push_back(mapChannel);
	}

	// Vertex paint
	//
	BL::Mesh::tessface_vertex_colors_iterator colIt;
	for(b_mesh.tessface_vertex_colors.begin(colIt); colIt != b_mesh.tessface_vertex_colors.end(); ++colIt) {
		MChan *mapChannel = new MChan();
		mapChannel->name  = colIt->name();

		if(std::count_if(mapChannel->name.begin(), mapChannel->name.end(), ::isdigit) == mapChannel->name.size()) {
			mapChannel->index = boost::lexical_cast<int>(mapChannel->name);
		}
		else {
			mapChannel->index = uv_layer_id++;
		}

		if(m_totTriFaces) {
			UvSet uvSet;

			BL::Mesh::tessfaces_iterator faceIt;
			int f = 0;
			for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt, ++f) {
				FaceVerts faceVerts = faceIt->vertices_raw();

				uvSet.insert(MapVertex(colIt->data[f].color1()));
				uvSet.insert(MapVertex(colIt->data[f].color2()));
				uvSet.insert(MapVertex(colIt->data[f].color3()));

				if(faceVerts[3])
					uvSet.insert(MapVertex(colIt->data[f].color4()));
			}

			f = 0;
			for (UvSet::iterator uvVertIt = uvSet.begin(); uvVertIt != uvSet.end(); ++uvVertIt, ++f) {
				(*uvVertIt).index = f;
			}

			const int mapVertexArraySize = 3 * uvSet.size();
			float *mapVertex = new float[mapVertexArraySize];
			int c = 0;
			for (UvSet::const_iterator uvVertIt = uvSet.begin(); uvVertIt != uvSet.end(); ++uvVertIt) {
				const MapVertex &uv = *uvVertIt;
				mapVertex[c++] = uv.v[0];
				mapVertex[c++] = uv.v[1];
				mapVertex[c++] = uv.v[2];
			}

			int  mapFacesArraySize  = 3 * m_totTriFaces;
			int *mapFaces = new int[mapFacesArraySize];

			int vertIndex = 0;
			f = 0;
			for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt, ++f) {
				FaceVerts faceVerts = faceIt->vertices_raw();

				const int v0 = uvSet.find(MapVertex(colIt->data[f].color1()))->index;
				const int v1 = uvSet.find(MapVertex(colIt->data[f].color2()))->index;
				const int v2 = uvSet.find(MapVertex(colIt->data[f].color3()))->index;

				mapFaces[vertIndex++] = v0;
				mapFaces[vertIndex++] = v1;
				mapFaces[vertIndex++] = v2;

				if(faceVerts[3]) {
					const int v3 = uvSet.find(MapVertex(colIt->data[f].color4()))->index;

					mapFaces[vertIndex++] = v0;
					mapFaces[vertIndex++] = v2;
					mapFaces[vertIndex++] = v3;
				}
			}

			mapChannel->uv_faces    = GetExportString((u_int8_t*)mapFaces, vertIndex * sizeof(int));
			mapChannel->uv_vertices = GetExportString((u_int8_t*)mapVertex, mapVertexArraySize * sizeof(float));

			mapChannel->hashUvFaces = ExporterSettings::gSet.m_isAnimation
									  ? hashArray(mapFaces, vertIndex * sizeof(int))
									  : 1;
			mapChannel->hashUvVertices = ExporterSettings::gSet.m_isAnimation
									  ? hashArray(mapVertex, mapVertexArraySize * sizeof(float))
									  : 1;

			delete [] mapFaces;
			delete [] mapVertex;
		}
		else {
			EMPTY_HEX_DATA(mapChannel->uv_faces);
			EMPTY_HEX_DATA(mapChannel->uv_vertices);

			mapChannel->hashUvVertices = 1;
			mapChannel->hashUvFaces = 1;
		}

		mapChannel->hash = mapChannel->hashUvVertices ^ mapChannel->hashUvFaces;

		map_channels.push_back(mapChannel);
	}
}


void GeomStaticMesh::initAttributes()
{
	if(m_propGroup && m_propGroup != Py_None) {
		dynamic_geometry     = GetPythonAttrInt(m_propGroup, "dynamic_geometry");
		environment_geometry = GetPythonAttrInt(m_propGroup, "environment_geometry");

		if (!m_force_osd) {
			osd_subdiv_level = GetPythonAttrInt(m_propGroup, "osd_subdiv_level");
			osd_subdiv_type  = GetPythonAttrInt(m_propGroup, "osd_subdiv_type");
			osd_subdiv_uvs   = GetPythonAttrInt(m_propGroup, "osd_subdiv_uvs");
		}

		weld_threshold = GetPythonAttrFloat(m_propGroup, "weld_threshold");
	}
}


void GeomStaticMesh::initAttributes(PointerRNA *ptr)
{
	PointerRNA geomStaticMesh = RNA_pointer_get(ptr, "GeomStaticMesh");

	dynamic_geometry     = RNA_boolean_get(&geomStaticMesh, "dynamic_geometry");
	environment_geometry = RNA_boolean_get(&geomStaticMesh, "environment_geometry");
	primary_visibility   = RNA_boolean_get(&geomStaticMesh, "primary_visibility");

	if (!m_force_osd) {
		osd_subdiv_type  = RNA_enum_get(&geomStaticMesh,    "osd_subdiv_type");
		osd_subdiv_level = RNA_int_get(&geomStaticMesh,     "osd_subdiv_level");
		osd_subdiv_uvs   = RNA_boolean_get(&geomStaticMesh, "osd_subdiv_uvs");
	}

	weld_threshold = RNA_float_get(&geomStaticMesh, "weld_threshold");

	smooth_uv         = RNA_boolean_get(&geomStaticMesh, "smooth_uv");
	smooth_uv_borders = RNA_boolean_get(&geomStaticMesh, "smooth_uv_borders");
}


void GeomStaticMesh::freeArrays()
{
	FREE_ARRAY(m_vertsArray);
	FREE_ARRAY(m_facesArray);
	FREE_ARRAY(m_normalsArray);
	FREE_ARRAY(m_faceNormalsArray);
	FREE_ARRAY(m_mtlIDsArray);
	FREE_ARRAY(m_edgeVisArray);
}


MHash GeomStaticMesh::hashArray(void *data, int dataLen)
{
	MHash hashArr[4] = {0, 0, 0, 0};
	MHash hash = 1;
	MHash seed = 42;

	MurmurHash3_x64_128(data, dataLen, seed, hashArr);

	for (int i = 0; i < 4; ++i)
		hash ^= hashArr[i];

	m_hash ^= hash;

	return hash;
}


void GeomStaticMesh::initHash()
{
	m_hash = 1;

	// If not animation don't waste time calculating hashes
	if(ExporterSettings::gSet.m_isAnimation) {
		m_hashVertices    = m_vertsArray       ? hashArray(m_vertsArray,       m_vertsArraySize       * sizeof(float)) : 1;
		m_hashNormals     = m_normalsArray     ? hashArray(m_normalsArray,     m_normalsArraySize     * sizeof(float)) : 1;
		m_hashFaces       = m_facesArray       ? hashArray(m_facesArray,       m_facesArraySize       * sizeof(int))   : 1;
		m_hashFaceNormals = m_faceNormalsArray ? hashArray(m_faceNormalsArray, m_faceNormalsArraySize * sizeof(int))   : 1;
		m_hashFaceMtlIDs  = m_mtlIDsArray      ? hashArray(m_mtlIDsArray,      m_mtlIDArraySize       * sizeof(int))   : 1;
		m_hashEdgeVis     = m_edgeVisArray     ? hashArray(m_edgeVisArray,     m_edgeVisArraySize     * sizeof(int))   : 1;
	}

	freeArrays();
}


void GeomStaticMesh::writeData(PyObject *output, VRayExportable *prevState, bool keyFrame)
{
	VRayExportable::initInterpolate(ExporterSettings::gSet.m_frameCurrent);

	GeomStaticMesh *prevMesh  = (GeomStaticMesh*)prevState;
	const float     prevFrame = ExporterSettings::gSet.m_frameCurrent - ExporterSettings::gSet.m_frameStep;

	PYTHON_PRINTF(output, "\nGeomStaticMesh %s {", m_name.c_str());

	PYTHON_PRINT_DATA(output, "vertices", "ListVectorHex",
					  m_vertices, m_hashVertices,
					  prevMesh,
					  prevMesh->getVertices(), prevMesh->getVerticesHash());

	PYTHON_PRINT_DATA(output, "faces", "ListIntHex",
					  m_faces, m_hashFaces,
					  prevMesh,
					  prevMesh->getFaces(), prevMesh->getFacesHash());

	if(m_normals) {
		PYTHON_PRINT_DATA(output, "normals", "ListVectorHex",
						  m_normals, m_hashNormals,
						  prevMesh,
						  prevMesh->getNormals(), prevMesh->getNormalsHash());
	}

	if(m_faceNormals) {
		PYTHON_PRINT_DATA(output, "faceNormals", "ListIntHex",
						  m_faceNormals, m_hashFaceNormals,
						  prevMesh,
						  prevMesh->getFaceNormals(), prevMesh->getFaceNormalsHash());
	}

	if(m_faceMtlIDs) {
		PYTHON_PRINT_DATA(output, "face_mtlIDs", "ListIntHex",
						  m_faceMtlIDs, m_hashFaceMtlIDs,
						  prevMesh,
						  prevMesh->getFaceMtlIDs(), prevMesh->getFaceMtlIDsHash());
	}

	if(m_edge_visibility) {
		PYTHON_PRINT_DATA(output, "edge_visibility", "ListIntHex",
						  m_edge_visibility, m_hashEdgeVis,
						  prevMesh,
						  prevMesh->getEdgeVisibility(), prevMesh->getEdgeVisibilityHash());
	}

	size_t mapChannelCount = getMapChannelCount();
	if(mapChannelCount) {
		// No need to export this every frame
		if(NOT(prevMesh)) {
			PYTHON_PRINT(output, "\n\tmap_channels_names=List(");
			for(size_t i = 0; i < mapChannelCount; ++i) {
				const MChan *mapChannel = this->getMapChannel(i);
				if(NOT(mapChannel))
					continue;

				std::string channelName = mapChannel->name;
				boost::erase_all(channelName, "\"");
				boost::erase_all(channelName, "'");

				PYTHON_PRINTF(output, "\"%s\"", channelName.c_str());
				if(i < mapChannelCount-1)
					PYTHON_PRINT(output, ",");
			}
			PYTHON_PRINT(output, ");");
		}

		// TODO: Keyframes
		if(mapChannelsUpdated(prevMesh)) {
			PYTHON_PRINTF(output, "\n\tmap_channels=%sList(", m_interpStart);
			for(size_t i = 0; i < mapChannelCount; ++i) {
				const MChan *mapChannel = this->getMapChannel(i);
				if(NOT(mapChannel))
					continue;

				PYTHON_PRINTF(output, "List(%i,ListVectorHex(\"", mapChannel->index);
				PYTHON_PRINT(output, mapChannel->uv_vertices);
				PYTHON_PRINT(output, "\"),ListIntHex(\"");
				PYTHON_PRINT(output, mapChannel->uv_faces);
				PYTHON_PRINT(output, "\"))");

				if(i < mapChannelCount-1)
					PYTHON_PRINT(output, ",");
			}
			PYTHON_PRINTF(output, ")%s;", m_interpEnd);
		}
	}
	else {
		PYTHON_PRINTF(output, "\n\tmap_channels=%sList(", m_interpStart);
		PYTHON_PRINTF(output, ")%s;", m_interpEnd);
	}

	if(ExporterSettings::gSet.IsFirstFrame()) {
		PYTHON_PRINTF(output, "\n\tprimary_visibility=%i;", primary_visibility);
		PYTHON_PRINTF(output, "\n\tenvironment_geometry=%i;", environment_geometry);
		PYTHON_PRINTF(output, "\n\tdynamic_geometry=%i;", dynamic_geometry);
		PYTHON_PRINTF(output, "\n\tosd_subdiv_level=%i;", osd_subdiv_level);
		PYTHON_PRINTF(output, "\n\tosd_subdiv_type=%i;",  osd_subdiv_type);
		PYTHON_PRINTF(output, "\n\tosd_subdiv_uvs=%i;",   osd_subdiv_uvs);
		PYTHON_PRINTF(output, "\n\tweld_threshold=%.3f;", weld_threshold);
		PYTHON_PRINTF(output, "\n\tsmooth_uv=%i;",         smooth_uv);
		PYTHON_PRINTF(output, "\n\tsmooth_uv_borders=%i;", smooth_uv_borders);
	}

	PYTHON_PRINT(output,  "\n}\n");
}
