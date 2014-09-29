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


typedef BL::Array<int, 4>  FaceVerts;


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
	b_mesh = b_data.meshes.new_from_object(b_scene, b_object, true, 2, false, false);
	if (NOT(b_mesh))
		return;

	if (b_mesh.use_auto_smooth())
		b_mesh.calc_normals_split(b_mesh.auto_smooth_angle());

	b_mesh.calc_tessface();

	// NOTE: Mesh could actually have no data.
	// This could be fine for mesh animated with "Build" mod, for example.

	initVertices();
	initFaces();
	initMapChannels();
	initAttributes();

	preInit();
	initHash();

	b_data.meshes.remove(b_mesh);
}


void GeomStaticMesh::preInit()
{
	initName();
}


void GeomStaticMesh::freeData()
{
	DEBUG_PRINT(CGR_USE_DESTR_DEBUG, COLOR_RED"GeomStaticMesh::freeData("COLOR_YELLOW"%s"COLOR_RED")"COLOR_DEFAULT, m_name.c_str());

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
		if(ExporterSettings::gSet.m_useAltInstances) {
			m_name = GetIDName((ID*)m_ob->data, "Me");
		}
		else {
			m_name = GetIDName((ID*)m_ob, "Me");
		}
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
	const int totvert = b_mesh.vertices.length();
	if(NOT(totvert)) {
		EMPTY_HEX_DATA(m_vertices);
		return;
	}

	m_vertsArraySize = 3 * totvert;
	m_vertsArray     = new float[m_vertsArraySize];

	BL::Mesh::vertices_iterator vertIt;
	int coordIndex = 0;
	for(b_mesh.vertices.begin(vertIt); vertIt != b_mesh.vertices.end(); ++vertIt) {
		m_vertsArray[coordIndex+0] = vertIt->co()[0];
		m_vertsArray[coordIndex+1] = vertIt->co()[1];
		m_vertsArray[coordIndex+2] = vertIt->co()[2];
		coordIndex += 3;
	}

	m_vertices = GetStringZip((u_int8_t*)m_vertsArray, m_vertsArraySize * sizeof(float));
}


void GeomStaticMesh::initFaces()
{
	if(NOT(b_mesh.tessfaces.length())) {
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
			m_edgeVisArray[currentTr/10] |= (0b011 << ((currentTr%10)*3));
			currentTr++;
			m_edgeVisArray[currentTr/10] |= (0b110 << ((currentTr%10)*3));
			currentTr++;
		}
		else {
			m_edgeVisArray[currentTr/10] |= (0b111 << ((currentTr%10)*3));
			currentTr++;
		}
	}

	m_faces           = GetStringZip((u_int8_t*)m_facesArray,       faceVertIndex  * sizeof(int));
	m_normals         = GetStringZip((u_int8_t*)m_normalsArray,     normArrIndex   * sizeof(float));
	m_faceNormals     = GetStringZip((u_int8_t*)m_faceNormalsArray, faceNormIndex  * sizeof(int));
	m_faceMtlIDs      = GetStringZip((u_int8_t*)m_mtlIDsArray,      faceMtlIDIndex * sizeof(int));

	m_edge_visibility = GetStringZip((u_int8_t*)m_edgeVisArray,     m_edgeVisArraySize * sizeof(int));
}


#define GET_FACE_VERTEX_COLOR(it, index) \
	mapVertex[coordIndex++] = it->data[f].color##index()[0]; \
	mapVertex[coordIndex++] = it->data[f].color##index()[1]; \
	mapVertex[coordIndex++] = it->data[f].color##index()[2];


#define NEW_MAP_CHANNEL_BEGIN(uvIt) \
	MChan *mapChannel = new MChan(); \
	mapChannel->name  = uvIt->name(); \
	\
	if(std::count_if(mapChannel->name.begin(), mapChannel->name.end(), ::isdigit) == mapChannel->name.size()) { \
	   mapChannel->index = boost::lexical_cast<int>(mapChannel->name); \
	} \
	else { \
	   mapChannel->index = uv_layer_id++; \
	} \
	\
	if(m_totTriFaces) { \
	   int    mapVertexArraySize = 3 * 3 * m_totTriFaces; \
	   float *mapVertex = new float[mapVertexArraySize]; \
	\
	   int coordIndex = 0; \
	   int f          = 0; \
	   for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt, ++f) { \
		   FaceVerts faceVerts = faceIt->vertices_raw();


#define NEW_MAP_CHANNEL_END \
	} \
	   if(map_channels.size()) { \
		   mapChannel->uv_faces    = map_channels[0]->uv_faces; \
		   mapChannel->hashUvFaces = map_channels[0]->hashUvFaces; \
		   mapChannel->cloned = 1; \
	   } \
	   else { \
		   int  mapFacesArraySize  = 3 * m_totTriFaces; \
		   int *mapFaces = new int[mapFacesArraySize]; \
	\
		   int vertIndex = 0; \
		   int k = 0; \
		   int u = 0; \
		   int f = 0; \
		   for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt, ++f) { \
			   FaceVerts faceVerts = faceIt->vertices_raw(); \
			   if(faceVerts[3]) { \
				   mapFaces[vertIndex++] = u; k = u+1; \
				   mapFaces[vertIndex++] = k; k = u+2; \
				   mapFaces[vertIndex++] = k; \
				   mapFaces[vertIndex++] = k; k = u+3; \
				   mapFaces[vertIndex++] = k; \
				   mapFaces[vertIndex++] = u; \
				   u += 4; \
			   } else { \
				   mapFaces[vertIndex++] = u; k = u+1; \
				   mapFaces[vertIndex++] = k; k = u+2; \
				   mapFaces[vertIndex++] = k; \
				   u += 3; \
			   } \
		   } \
	\
		   mapChannel->uv_faces = GetStringZip((u_int8_t*)mapFaces, vertIndex * sizeof(int)); \
		   mapChannel->hashUvFaces = HashCode(mapChannel->uv_faces); \
	\
		   delete [] mapFaces; \
	   } \
	\
	   mapChannel->uv_vertices = GetStringZip((u_int8_t*)mapVertex, coordIndex * sizeof(float)); \
	   mapChannel->hashUvVertices = HashCode(mapChannel->uv_vertices); \
	\
	   delete [] mapVertex; \
	} \
	else { \
	   EMPTY_HEX_DATA(mapChannel->uv_faces); \
	   EMPTY_HEX_DATA(mapChannel->uv_vertices); \
	\
	   mapChannel->hashUvVertices = 1; \
	   mapChannel->hashUvFaces = 1; \
	} \
	\
	mapChannel->hash = mapChannel->hashUvVertices ^ mapChannel->hashUvFaces; \
	\
	map_channels.push_back(mapChannel);


void GeomStaticMesh::initMapChannels()
{
	int channelCount = 0;
	channelCount += b_mesh.tessface_uv_textures.length();
	channelCount += b_mesh.tessface_vertex_colors.length();

	if(NOT(channelCount))
		return;

	int uv_layer_id = 0;
	BL::Mesh::tessfaces_iterator              faceIt;
	BL::Mesh::tessface_uv_textures_iterator   uvIt;
	for(b_mesh.tessface_uv_textures.begin(uvIt); uvIt != b_mesh.tessface_uv_textures.end(); ++uvIt) {
		NEW_MAP_CHANNEL_BEGIN(uvIt);

#if 0
		BL::MeshTextureFaceLayer uv = *uvIt;
		BL::MeshTextureFaceLayer::data_iterator uvIt;
		for(uv.data.begin(uvIt); uvIt != uv.data.end(); ++uvIt) {
			BL::MeshTextureFace uvFace = *uvIt;
		}
#endif

		const int verts = faceVerts[3] ? 4 : 3;
		for(int c = 0; c < verts; ++c) {
			mapVertex[coordIndex++] = uvIt->data[f].uv()[c * 2 + 0];
			mapVertex[coordIndex++] = uvIt->data[f].uv()[c * 2 + 1];
			mapVertex[coordIndex++] = 0.0f;
		}

		NEW_MAP_CHANNEL_END;
	}

	BL::Mesh::tessface_vertex_colors_iterator colIt;
	for(b_mesh.tessface_vertex_colors.begin(colIt); colIt != b_mesh.tessface_vertex_colors.end(); ++colIt) {
		NEW_MAP_CHANNEL_BEGIN(colIt);

		GET_FACE_VERTEX_COLOR(colIt, 1);
		GET_FACE_VERTEX_COLOR(colIt, 2);
		GET_FACE_VERTEX_COLOR(colIt, 3);
		if(faceVerts[3]) {
			GET_FACE_VERTEX_COLOR(colIt, 4);
		}

		NEW_MAP_CHANNEL_END;
	}
}


void GeomStaticMesh::initAttributes()
{
	if(m_propGroup && m_propGroup != Py_None) {
		dynamic_geometry     = GetPythonAttrInt(m_propGroup, "dynamic_geometry");
		environment_geometry = GetPythonAttrInt(m_propGroup, "environment_geometry");

		osd_subdiv_level = GetPythonAttrInt(m_propGroup, "osd_subdiv_level");
		osd_subdiv_type  = GetPythonAttrInt(m_propGroup, "osd_subdiv_type");
		osd_subdiv_uvs   = GetPythonAttrInt(m_propGroup, "osd_subdiv_uvs");

		weld_threshold = GetPythonAttrFloat(m_propGroup, "weld_threshold");
	}
}


void GeomStaticMesh::initAttributes(PointerRNA *ptr)
{
	PointerRNA geomStaticMesh = RNA_pointer_get(ptr, "GeomStaticMesh");

	dynamic_geometry     = RNA_boolean_get(&geomStaticMesh, "dynamic_geometry");
	environment_geometry = RNA_boolean_get(&geomStaticMesh, "environment_geometry");
	primary_visibility   = RNA_boolean_get(&geomStaticMesh, "primary_visibility");

	osd_subdiv_type  = RNA_enum_get(&geomStaticMesh,    "osd_subdiv_type");
	osd_subdiv_level = RNA_int_get(&geomStaticMesh,     "osd_subdiv_level");
	osd_subdiv_uvs   = RNA_boolean_get(&geomStaticMesh, "osd_subdiv_uvs");

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
		m_hashVertices    = hashArray(m_vertsArray,       m_vertsArraySize       * sizeof(float));
		m_hashNormals     = hashArray(m_normalsArray,     m_normalsArraySize     * sizeof(float));
		m_hashFaces       = hashArray(m_facesArray,       m_facesArraySize       * sizeof(int));
		m_hashFaceNormals = hashArray(m_faceNormalsArray, m_faceNormalsArraySize * sizeof(int));
		m_hashFaceMtlIDs  = hashArray(m_mtlIDsArray,      m_mtlIDArraySize       * sizeof(int));
		m_hashEdgeVis     = hashArray(m_edgeVisArray,     m_edgeVisArraySize     * sizeof(int));
	}

	freeArrays();
}


void GeomStaticMesh::writeData(PyObject *output, VRayExportable *prevState, bool keyFrame)
{
	GeomStaticMesh *prevMesh  = (GeomStaticMesh*)prevState;
	int             prevFrame = ExporterSettings::gSet.m_frameCurrent - ExporterSettings::gSet.m_frameStep;

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

				PYTHON_PRINTF(output, "\"%s\"", mapChannel->name.c_str());
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
