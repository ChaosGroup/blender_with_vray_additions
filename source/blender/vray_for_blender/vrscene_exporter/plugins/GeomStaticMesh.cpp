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

#include "CGR_config.h"

#include "GeomStaticMesh.h"

#include <boost/lexical_cast.hpp>
#include <locale>
#include <algorithm>

#include "CGR_blender_data.h"
#include "CGR_string.h"
#include "CGR_vrscene.h"
#include "CGR_rna.h"

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


BLI_INLINE int AddEdgeVisibility(int k, int *evArray, int &evIndex, int &ev)
{
	if(k == 9) {
		evArray[evIndex++] = ev;
		ev = 0;
		return 0;
	}
	return k+1;
}


MChan::MChan()
{
	name = "";
	index = 0;
	cloned = 0;
	uv_vertices = NULL;
	uv_faces = NULL;
}


void MChan::freeData()
{
	if(uv_vertices) {
		delete [] uv_vertices;
		uv_vertices = NULL;
	}
	if(uv_faces) {
		if(NOT(cloned))
			delete [] uv_faces;
		uv_faces = NULL;
	}
}


GeomStaticMesh::GeomStaticMesh(Scene *scene, Main *main, Object *ob, int checkComponents):
	VRayExportable(scene, main, ob),
	b_data(PointerRNA_NULL),
	b_scene(PointerRNA_NULL),
	b_object(PointerRNA_NULL),
	b_mesh(PointerRNA_NULL)
{
	m_vertices = NULL;
	coordIndex = 0;

	m_faces = NULL;
	vertIndex = 0;

	m_normals = NULL;
	m_faceNormals = NULL;
	m_faceMtlIDs = NULL;
	m_edge_visibility = NULL;

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

	useDisplace = false;
	useSmooth   = false;

	displaceTextureName = "";

	m_useZip = true;

	m_checkComponents = checkComponents;

	m_hashVertices = 1;
	m_hashFaces = 1;
	m_hashNormals = 1;
	m_hashFaceNormals = 1;
	m_hashFaceMtlIDs = 1;
	m_hashEdgeVisibility = 1;

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
	if(m_checkComponents) {
		initDisplace();
		initSmooth();
	}

	initName();
}


void GeomStaticMesh::freeData()
{
	DEBUG_PRINT(CGR_USE_DESTR_DEBUG, COLOR_RED"GeomStaticMesh::freeData("COLOR_YELLOW"%s"COLOR_RED")"COLOR_DEFAULT, m_name.c_str());

	if(m_vertices) {
		delete [] m_vertices;
		m_vertices = NULL;
	}
	if(m_faces) {
		delete [] m_faces;
		m_faces = NULL;
	}
	if(m_normals) {
		delete [] m_normals;
		m_normals = NULL;
	}
	if(m_faceNormals) {
		delete [] m_faceNormals;
		m_faceNormals = NULL;
	}
	if(m_faceMtlIDs) {
		delete [] m_faceMtlIDs;
		m_faceMtlIDs = NULL;
	}
	if(m_edge_visibility) {
		delete [] m_edge_visibility;
		m_edge_visibility = NULL;
	}

	for(int i = 0; i < map_channels.size(); ++i) {
		map_channels[i]->freeData();
	}
}


void GeomStaticMesh::initName(const std::string &name)
{
	if(NOT(name.empty()))
		m_name = name;
	else
		m_name = "Me" + GetIDName((ID*)m_ob);

	meshComponentNames.push_back(m_name);
	if(useSmooth && useDisplace)
		meshComponentNames.push_back("smoothDisp" + m_name);
	else if(useSmooth)
		meshComponentNames.push_back("smooth" + m_name);
	else if(useDisplace)
		meshComponentNames.push_back("disp" + m_name);

	m_name = meshComponentNames.back();
}


void GeomStaticMesh::initDisplace()
{
	for(int a = 1; a <= m_ob->totcol; ++a) {
		Material *ma = give_current_material(m_ob, a);
		if(NOT(ma))
			continue;

		for(int t = 0; t < MAX_MTEX; ++t) {
			if(ma->mtex[t] && ma->mtex[t]->tex) {
				Tex *tex = ma->mtex[t]->tex;

				PointerRNA texRNA;
				RNA_id_pointer_create(&tex->id, &texRNA);
				if(RNA_struct_find_property(&texRNA, "vray_slot")) {
					PointerRNA VRayTextureSlot = RNA_pointer_get(&texRNA, "vray_slot");
					if(RNA_struct_find_property(&VRayTextureSlot, "map_displacement")) {
						int mapDisplacement = RNA_boolean_get(&VRayTextureSlot, "map_displacement");

						if(mapDisplacement) {
							useDisplace = true;

							// NOTE: Texture blend is not supported,
							// and will be supported only in vb30
							//
							displaceTexture     = tex;
							displaceTextureName = GetIDName((ID*)tex);

							break;
						}
					}
				}
			}
		}
	}
}


void GeomStaticMesh::initSmooth()
{
	RnaAccess::RnaValue rna(&m_ob->id, "vray.GeomStaticSmoothedMesh");

	useSmooth = rna.getBool("use");
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


const MChan* GeomStaticMesh::getMapChannel(const size_t i) const
{
	if(i >= map_channels.size())
		return NULL;
	return map_channels[i];
}


void GeomStaticMesh::initVertices()
{
	int totvert = b_mesh.vertices.length();
	if(NOT(totvert)) {
		EMPTY_HEX_DATA(m_vertices);
		return;
	}

	size_t  vertsArraySize = 3 * totvert * sizeof(float);
	float  *vertsArray = new float[vertsArraySize];

	BL::Mesh::vertices_iterator vertIt;
	for(b_mesh.vertices.begin(vertIt); vertIt != b_mesh.vertices.end(); ++vertIt) {
		vertsArray[coordIndex+0] = vertIt->co()[0];
		vertsArray[coordIndex+1] = vertIt->co()[1];
		vertsArray[coordIndex+2] = vertIt->co()[2];
		coordIndex += 3;
	}

	m_vertices = GetStringZip((u_int8_t*)vertsArray, coordIndex * sizeof(float));

	delete [] vertsArray;
}


void GeomStaticMesh::initFaces()
{
	int totface = b_mesh.tessfaces.length();
	int totedge = b_mesh.edges.length();
	if(NOT(totface)) {
		EMPTY_HEX_DATA(m_faces);
		EMPTY_HEX_DATA(m_normals);
		EMPTY_HEX_DATA(m_faceNormals);
		EMPTY_HEX_DATA(m_faceMtlIDs);
		EMPTY_HEX_DATA(m_edge_visibility);
		return;
	}

	BL::Mesh::tessfaces_iterator faceIt;

	// Assume all faces are 4-vertex => 2 tri-faces:
	//   6 vertex indices
	//   6 normals of 3 coords
	//
	size_t  facesArraySize   = 6 *     totface * sizeof(int);
	size_t  faceNormalsSize  = 6 *     totface * sizeof(int);
	size_t  normalsArraySize = 6 * 3 * totface * sizeof(int);
	size_t  face_mtlIDsSize  = 2 *     totface * sizeof(int);
	size_t  evArraySize      =         totedge * sizeof(int);

	int    *facesArray       = new int[facesArraySize];
	int    *faceNormalsArray = new int[faceNormalsSize];
	float  *normalsArray     = new float[normalsArraySize];
	int    *face_mtlIDsArray = new int[face_mtlIDsSize];
	int    *evArray          = new int[evArraySize];

	float  fno[3];
	float  n0[3];
	float  n1[3];
	float  n2[3];
	float  n3[3];

	int    ev = 0;
	int    k  = 0;

	int    normIndex      = 0;
	int    faceNormIndex  = 0;
	int    faceMtlIDIndex = 0;
	int    evIndex        = 0;

	bool   useAutoSmooth  = b_mesh.use_auto_smooth();

	vertIndex = 0;

	if(totface <= 5) {
		for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt) {
			FaceVerts faceVerts = faceIt->vertices_raw();
			if(faceVerts[3])
				ev = (ev << 6) | 27;
			else
				ev = (ev << 3) | 8;
		}
		evArray[evIndex++] = ev;
	}

	for(b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt) {
		FaceVerts faceVerts = faceIt->vertices_raw();

		// Normals
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
		facesArray[vertIndex++] = faceVerts[0];
		facesArray[vertIndex++] = faceVerts[1];
		facesArray[vertIndex++] = faceVerts[2];
		if(faceVerts[3]) {
			facesArray[vertIndex++] = faceVerts[2];
			facesArray[vertIndex++] = faceVerts[3];
			facesArray[vertIndex++] = faceVerts[0];
		}

		// Store normals
		COPY_VECTOR(normalsArray, normIndex, n0);
		COPY_VECTOR(normalsArray, normIndex, n1);
		COPY_VECTOR(normalsArray, normIndex, n2);
		if(faceVerts[3]) {
			COPY_VECTOR(normalsArray, normIndex, n2);
			COPY_VECTOR(normalsArray, normIndex, n3);
			COPY_VECTOR(normalsArray, normIndex, n0);
		}

		// Store face normals
		int verts = faceVerts[3] ? 6 : 3;
		for(int v = 0; v < verts; ++v) {
			faceNormalsArray[faceNormIndex] = faceNormIndex;
			faceNormIndex++;
		}

		// Store material ID
		face_mtlIDsArray[faceMtlIDIndex++] = matID;
		if(faceVerts[3])
			face_mtlIDsArray[faceMtlIDIndex++] = matID;

		// Store edge visibility
		if(faceVerts[3]) {
			ev = (ev << 3) | 3;
			k = AddEdgeVisibility(k, evArray, evIndex, ev);
			ev = (ev << 3) | 3;
			k = AddEdgeVisibility(k, evArray, evIndex, ev);
		} else {
			ev = (ev << 3) | 8;
			k = AddEdgeVisibility(k, evArray, evIndex, ev);
		}
	}

	// Store edge visibility if smth is left there
	if(k)
		evArray[evIndex++] = ev;

	m_faces           = GetStringZip((u_int8_t*)facesArray,       vertIndex      * sizeof(int));
	m_normals         = GetStringZip((u_int8_t*)normalsArray,     normIndex      * sizeof(float));
	m_faceNormals     = GetStringZip((u_int8_t*)faceNormalsArray, faceNormIndex  * sizeof(int));
	m_faceMtlIDs      = GetStringZip((u_int8_t*)face_mtlIDsArray, faceMtlIDIndex * sizeof(int));
	m_edge_visibility = GetStringZip((u_int8_t*)evArray,          evIndex        * sizeof(int));

	delete [] facesArray;
	delete [] normalsArray;
	delete [] faceNormalsArray;
	delete [] face_mtlIDsArray;
	delete [] evArray;
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
	if(totface) { \
	   size_t  mapVertexArraySize = 4 * 3 * totface * sizeof(float); \
	   float  *mapVertex = new float[mapVertexArraySize]; \
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
		   size_t  mapFacesArraySize  = 4 * totface * sizeof(int); \
		   int    *mapFaces = new int[mapFacesArraySize]; \
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

	BL::Mesh::tessfaces_iterator              faceIt;

	int uv_layer_id = 0;
	int totface     = b_mesh.tessfaces.length();

	BL::Mesh::tessface_uv_textures_iterator   uvIt;
	for(b_mesh.tessface_uv_textures.begin(uvIt); uvIt != b_mesh.tessface_uv_textures.end(); ++uvIt) {
		NEW_MAP_CHANNEL_BEGIN(uvIt);

		int verts = faceVerts[3] ? 4 : 3;
		for(int i = 0; i < verts; i++) {
			mapVertex[coordIndex++] = uvIt->data[f].uv()[i * 2 + 0];
			mapVertex[coordIndex++] = uvIt->data[f].uv()[i * 2 + 1];
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
	if(m_propGroup && NOT(m_propGroup == Py_None)) {
		dynamic_geometry     = GetPythonAttrInt(m_propGroup, "dynamic_geometry");
		environment_geometry = GetPythonAttrInt(m_propGroup, "environment_geometry");

		osd_subdiv_level = GetPythonAttrInt(m_propGroup, "osd_subdiv_level");
		osd_subdiv_type  = GetPythonAttrInt(m_propGroup, "osd_subdiv_type");
		osd_subdiv_uvs   = GetPythonAttrInt(m_propGroup, "osd_subdiv_uvs");

		weld_threshold = GetPythonAttrFloat(m_propGroup, "weld_threshold");
	}
	else {
		RnaAccess::RnaValue rna(&m_ob->id, "vray.GeomStaticMesh");
		if(rna.hasProperty("dynamic_geometry")) {
			dynamic_geometry = rna.getBool("dynamic_geometry");
		}
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


void GeomStaticMesh::initHash()
{
#if CGR_USE_MURMUR_HASH
	u_int32_t vertexHash[4];
	u_int32_t facesHash[4];

	MurmurHash3_x64_128(vertices, coordIndex * sizeof(float), 42,            vertexHash);
	MurmurHash3_x64_128(faces,    vertIndex  * sizeof(int),   vertexHash[0], facesHash);

	PRINT_INFO("Object: %s => hash = 0x%X", object->id.name+2, hash);
#else
	m_hash = 1;

	// If not animation don't waste time calculating hashes
	if(VRayExportable::m_set->m_isAnimation) {
		if(m_vertices) {
			m_hashVertices = HashCode(m_vertices);
			m_hash ^= m_hashVertices;
		}
		if(m_normals) {
			m_hashNormals = HashCode(m_normals);
			m_hash ^= m_hashNormals;
		}
		if(m_faces) {
			m_hashFaces = HashCode(m_faces);
			m_hash ^= m_hashFaces;
		}
		if(m_faceNormals) {
			m_hashFaceNormals = HashCode(m_faceNormals);
			m_hash ^= m_hashFaceNormals;
		}
		if(m_faceMtlIDs) {
			m_hashFaceMtlIDs = HashCode(m_faceMtlIDs);
			m_hash ^= m_hashFaceMtlIDs;
		}
		if(m_edge_visibility) {
			m_hashEdgeVisibility = HashCode(m_edge_visibility);
			m_hash ^= m_hashEdgeVisibility;
		}

		if(useSmooth)
			m_hash ^= HashCode(m_pluginSmooth.str().c_str());
		if(useDisplace)
			m_hash ^= HashCode(m_pluginDisplace.str().c_str());
	}
#endif
}


void GeomStaticMesh::writeGeomStaticSmoothedMesh(PyObject *output)
{
	RnaAccess::RnaValue smoothRna(&m_ob->id, "vray.GeomStaticSmoothedMesh");

	size_t mCompSize = meshComponentNames.size();

	m_pluginSmooth << "\n" << "GeomStaticSmoothedMesh" << " " << meshComponentNames[mCompSize-1] << " {";
	m_pluginSmooth << "\n\t" << "mesh=" << meshComponentNames[mCompSize-2] << ";";

	writeAttributes(smoothRna.getPtr(), m_pluginDesc.getTree("GeomStaticSmoothedMesh"), m_pluginSmooth);

	if(useDisplace) {
		RnaAccess::RnaValue dispRna(&displaceTexture->id, "vray_slot.GeomDisplacedMesh");

		StrSet skipDispAttrs;
		skipDispAttrs.insert("map_channel");

		writeAttributes(dispRna.getPtr(), m_pluginDesc.getTree("GeomDisplacedMesh"), m_pluginSmooth, skipDispAttrs);

		// Overrider type settings
		//
		int displace_type = dispRna.getEnum("type");

		if(displace_type == 1) {
			m_pluginSmooth << "\n\t" << "displace_2d"         << "=" << 0 << ";";
			m_pluginSmooth << "\n\t" << "vector_displacement" << "=" << 0 << ";";
		}
		else if(displace_type == 0) {
			m_pluginSmooth << "\n\t" << "displace_2d"         << "=" << 1 << ";";
			m_pluginSmooth << "\n\t" << "vector_displacement" << "=" << 0 << ";";
		}
		else if(displace_type == 2) {
			m_pluginSmooth << "\n\t" << "displace_2d"         << "=" << 0 << ";";
			m_pluginSmooth << "\n\t" << "vector_displacement" << "=" << 1 << ";";
		}

		if(displace_type == 2) {
			m_pluginSmooth << "\n\t" << "displacement_tex_color=" << displaceTextureName << ";";
		}
		else {
			m_pluginSmooth << "\n\t" << "displacement_tex_float=" << displaceTextureName << "::out_intensity;";
		}

		// Use first channel for displace
		m_pluginSmooth << "\n\t" << "map_channel" << "=" << 0 << ";";
	}
	m_pluginSmooth << "\n}\n";

	meshComponentNames.pop_back();

	PYTHON_PRINT(output, m_pluginSmooth.str().c_str());
}


void GeomStaticMesh::writeGeomDisplacedMesh(PyObject *output)
{
	RnaAccess::RnaValue rna(&displaceTexture->id, "vray_slot.GeomDisplacedMesh");

	size_t mCompSize = meshComponentNames.size();


	StrSet skipDispAttrs;
	skipDispAttrs.insert("map_channel");

	m_pluginDisplace << "\n" << "GeomDisplacedMesh" << " " << meshComponentNames[mCompSize-1] << " {";
	m_pluginDisplace << "\n\t" << "mesh=" << meshComponentNames[mCompSize-2] << ";";

	writeAttributes(rna.getPtr(), m_pluginDesc.getTree("GeomDisplacedMesh"), m_pluginDisplace, skipDispAttrs);

	meshComponentNames.pop_back();

	// Overrider type settings
	//
	int displace_type = rna.getEnum("type");

	if(displace_type == 1) {
		m_pluginDisplace << "\n\t" << "displace_2d"         << "=" << 0 << ";";
		m_pluginDisplace << "\n\t" << "vector_displacement" << "=" << 0 << ";";
	}
	else if(displace_type == 0) {
		m_pluginDisplace << "\n\t" << "displace_2d"         << "=" << 1 << ";";
		m_pluginDisplace << "\n\t" << "vector_displacement" << "=" << 0 << ";";
	}
	else if(displace_type == 2) {
		m_pluginDisplace << "\n\t" << "displace_2d"         << "=" << 0 << ";";
		m_pluginDisplace << "\n\t" << "vector_displacement" << "=" << 1 << ";";
	}

	if(displace_type == 2) {
		m_pluginDisplace << "\n\t" << "displacement_tex_color=" << displaceTextureName << ";";
	}
	else {
		m_pluginDisplace << "\n\t" << "displacement_tex_float=" << displaceTextureName << "::out_intensity;";
	}

	// Use first channel for displace
	m_pluginDisplace << "\n\t" << "map_channel" << "=" << 0 << ";";
	m_pluginDisplace << "\n}\n";

	PYTHON_PRINT(output, m_pluginDisplace.str().c_str());
}


void GeomStaticMesh::writeData(PyObject *output, VRayExportable *prevState, bool keyFrame)
{
	if(useSmooth && useDisplace)
		writeGeomStaticSmoothedMesh(output);
	else if(useSmooth)
		writeGeomStaticSmoothedMesh(output);
	else if(useDisplace)
		writeGeomDisplacedMesh(output);

	if(useSmooth || useDisplace) {
		smooth_uv         = true;
		smooth_uv_borders = true;
	}

	GeomStaticMesh *prevMesh  = (GeomStaticMesh*)prevState;
	int             prevFrame = VRayExportable::m_set->m_frameCurrent - VRayExportable::m_set->m_frameStep;

	size_t      compSize = meshComponentNames.size();
	std::string geomName = meshComponentNames[compSize-1];

	PYTHON_PRINTF(output, "\nGeomStaticMesh %s {", geomName.c_str());

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
						  m_edge_visibility, m_hashEdgeVisibility,
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

	if(NOT(prevMesh)) {
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
