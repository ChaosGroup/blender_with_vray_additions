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
	VRayExportable(scene, main, ob)
{
	mesh = NULL;

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
}


void GeomStaticMesh::init()
{
	mesh = GetRenderMesh(m_sce, m_main, m_ob);
	if(NOT(mesh))
		return;

	// NOTE: Mesh could actually have no data.
	// This could be fine for mesh animated with "Build" mod, for example.

	initVertices();
	initFaces();
	initMapChannels();
	initAttributes();

	preInit();
	initHash();

	FreeRenderMesh(m_main, mesh);
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
	if(NOT(mesh->totvert)) {
		EMPTY_HEX_DATA(m_vertices);
		return;
	}

	size_t  vertsArraySize = 3 * mesh->totvert * sizeof(float);
	float  *vertsArray = new float[vertsArraySize];

	MVert *vert = mesh->mvert;
	coordIndex = 0;
	for(int v = 0; v < mesh->totvert; ++vert, ++v) {
		vertsArray[coordIndex+0] = vert->co[0];
		vertsArray[coordIndex+1] = vert->co[1];
		vertsArray[coordIndex+2] = vert->co[2];
		coordIndex += 3;
	}

	m_vertices = m_useZip ? GetStringZip((u_int8_t*)vertsArray, coordIndex * sizeof(float)) : GetHex((u_int8_t*)vertsArray, coordIndex * sizeof(float));

	delete [] vertsArray;
}


void GeomStaticMesh::initFaces()
{
	if(NOT(mesh->totpoly)) {
		EMPTY_HEX_DATA(m_faces);
		EMPTY_HEX_DATA(m_normals);
		EMPTY_HEX_DATA(m_faceNormals);
		EMPTY_HEX_DATA(m_faceMtlIDs);
		EMPTY_HEX_DATA(m_edge_visibility);
		return;
	}

	// Assume all faces are 4-vertex => 2 tri-faces:
	//   6 vertex indices
	//   6 normals of 3 coords
	//
	size_t  facesArraySize   = 6 *     mesh->totface * sizeof(int);
	size_t  faceNormalsSize  = 6 *     mesh->totface * sizeof(int);
	size_t  normalsArraySize = 6 * 3 * mesh->totface * sizeof(int);
	size_t  face_mtlIDsSize  = 2 *     mesh->totface * sizeof(int);
	size_t  evArraySize      =         mesh->totedge * sizeof(int);

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

	MFace *face = mesh->mface;

	int    normIndex      = 0;
	int    faceNormIndex  = 0;
	int    faceMtlIDIndex = 0;
	int    evIndex        = 0;

	vertIndex = 0;

	if(mesh->totface <= 5) {
		for(int f = 0; f < mesh->totface; ++face, ++f) {
			if(face->v4)
				ev = (ev << 6) | 27;
			else
				ev = (ev << 3) | 8;
		}
		evArray[evIndex++] = ev;
	}

	face = mesh->mface;
	for(int f = 0; f < mesh->totface; ++face, ++f) {
		// Compute normals
		if(face->flag & ME_SMOOTH) {
			normal_short_to_float_v3(n0, mesh->mvert[face->v1].no);
			normal_short_to_float_v3(n1, mesh->mvert[face->v2].no);
			normal_short_to_float_v3(n2, mesh->mvert[face->v3].no);

			if(face->v4)
				normal_short_to_float_v3(n3, mesh->mvert[face->v4].no);
		}
		else {
			if(face->v4)
				normal_quad_v3(fno, mesh->mvert[face->v1].co, mesh->mvert[face->v2].co, mesh->mvert[face->v3].co, mesh->mvert[face->v4].co);
			else
				normal_tri_v3(fno,  mesh->mvert[face->v1].co, mesh->mvert[face->v2].co, mesh->mvert[face->v3].co);

			copy_v3_v3(n0, fno);
			copy_v3_v3(n1, fno);
			copy_v3_v3(n2, fno);

			if(face->v4)
				copy_v3_v3(n3, fno);
		}

		// Material ID
		int matID = face->mat_nr + 1;

		// Store face vertices
		facesArray[vertIndex++] = face->v1;
		facesArray[vertIndex++] = face->v2;
		facesArray[vertIndex++] = face->v3;
		if(face->v4) {
			facesArray[vertIndex++] = face->v3;
			facesArray[vertIndex++] = face->v4;
			facesArray[vertIndex++] = face->v1;
		}

		// Store normals
		COPY_VECTOR(normalsArray, normIndex, n0);
		COPY_VECTOR(normalsArray, normIndex, n1);
		COPY_VECTOR(normalsArray, normIndex, n2);
		if(face->v4) {
			COPY_VECTOR(normalsArray, normIndex, n2);
			COPY_VECTOR(normalsArray, normIndex, n3);
			COPY_VECTOR(normalsArray, normIndex, n0);
		}

		// Store face normals
		int verts = face->v4 ? 6 : 3;
		for(int v = 0; v < verts; ++v) {
			faceNormalsArray[faceNormIndex] = faceNormIndex;
			faceNormIndex++;
		}

		// Store material ID
		face_mtlIDsArray[faceMtlIDIndex++] = matID;
		if(face->v4)
			face_mtlIDsArray[faceMtlIDIndex++] = matID;

		// Store edge visibility
		if(face->v4) {
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

	m_faces           = m_useZip ? GetStringZip((u_int8_t*)facesArray,       vertIndex      * sizeof(int))   : GetHex((u_int8_t*)facesArray,       vertIndex      * sizeof(int));
	m_normals         = m_useZip ? GetStringZip((u_int8_t*)normalsArray,     normIndex      * sizeof(float)) : GetHex((u_int8_t*)normalsArray,     normIndex      * sizeof(float));
	m_faceNormals     = m_useZip ? GetStringZip((u_int8_t*)faceNormalsArray, faceNormIndex  * sizeof(int))   : GetHex((u_int8_t*)faceNormalsArray, faceNormIndex  * sizeof(int));
	m_faceMtlIDs      = m_useZip ? GetStringZip((u_int8_t*)face_mtlIDsArray, faceMtlIDIndex * sizeof(int))   : GetHex((u_int8_t*)face_mtlIDsArray, faceMtlIDIndex * sizeof(int));
	m_edge_visibility = m_useZip ? GetStringZip((u_int8_t*)evArray,          evIndex        * sizeof(int))   : GetHex((u_int8_t*)evArray,          evIndex        * sizeof(int));

	delete [] facesArray;
	delete [] normalsArray;
	delete [] faceNormalsArray;
	delete [] face_mtlIDsArray;
	delete [] evArray;
}


void GeomStaticMesh::initMapChannels()
{
	CustomData *fdata = &mesh->fdata;
	int         channelCount = 0;

	channelCount += CustomData_number_of_layers(fdata, CD_MTFACE);
	channelCount += CustomData_number_of_layers(fdata, CD_MCOL);

	if(NOT(channelCount))
		return;

	int uv_layer_id = 0;
	for(int l = 0; l < fdata->totlayer; ++l) {
		if(NOT(fdata->layers[l].type == CD_MTFACE || fdata->layers[l].type == CD_MCOL))
			continue;

		MChan *mapChannel = new MChan();
		mapChannel->name  = fdata->layers[l].name;

		// This allows us to sync digit layer name with layer index,
		// a little creepy, but should work =)
		if(std::count_if(mapChannel->name.begin(), mapChannel->name.end(), ::isdigit) == mapChannel->name.size()) {
			mapChannel->index = boost::lexical_cast<int>(mapChannel->name);
		}
		else {
			mapChannel->index = uv_layer_id++;
		}

		if(mesh->totface) {
			// Collect vertices
			//
			size_t  mapVertexArraySize = 4 * 3 * mesh->totface * sizeof(float);
			float  *mapVertex = new float[mapVertexArraySize];

			MTFace *mtface = (MTFace*)fdata->layers[l].data;
			MCol   *mcol   = (MCol*)fdata->layers[l].data;

			MFace  *face = mesh->mface;
			int     coordIndex = 0;
			for(int f = 0; f < mesh->totface; ++face, ++f) {
				int verts = face->v4 ? 4 : 3;

				for(int i = 0; i < verts; i++) {
					if(fdata->layers[l].type == CD_MTFACE) {
						mapVertex[coordIndex++] = mtface[f].uv[i][0];
						mapVertex[coordIndex++] = mtface[f].uv[i][1];
						mapVertex[coordIndex++] = 0.0f;
					}
					else {
						mapVertex[coordIndex++] = (float)mcol[f * 4 + i].b / 255.0;
						mapVertex[coordIndex++] = (float)mcol[f * 4 + i].g / 255.0;
						mapVertex[coordIndex++] = (float)mcol[f * 4 + i].r / 255.0;
					}
				}
			}

			// Collect faces
			// Face topology is always the same so we could reuse first created channel
			//
			if(map_channels.size()) {
				mapChannel->uv_faces    = map_channels[0]->uv_faces;
				mapChannel->hashUvFaces = map_channels[0]->hashUvFaces;
				mapChannel->cloned = 1;
			}
			else {
				size_t  mapFacesArraySize  = 4 * mesh->totface * sizeof(int);
				int    *mapFaces = new int[mapFacesArraySize];

				int vertIndex = 0;
				int k = 0;
				int u = 0;

				face = mesh->mface;
				for(int f = 0; f < mesh->totface; ++face, ++f) {
					if(face->v4) {
						mapFaces[vertIndex++] = u; k = u+1;
						mapFaces[vertIndex++] = k; k = u+2;
						mapFaces[vertIndex++] = k;
						mapFaces[vertIndex++] = k; k = u+3;
						mapFaces[vertIndex++] = k;
						mapFaces[vertIndex++] = u;
						u += 4;
					} else {
						mapFaces[vertIndex++] = u; k = u+1;
						mapFaces[vertIndex++] = k; k = u+2;
						mapFaces[vertIndex++] = k;
						u += 3;
					}
				}

				mapChannel->uv_faces = GetStringZip((u_int8_t*)mapFaces, vertIndex * sizeof(int));
				mapChannel->hashUvFaces = HashCode(mapChannel->uv_faces);

				delete [] mapFaces;
			}

			mapChannel->uv_vertices = GetStringZip((u_int8_t*)mapVertex, coordIndex * sizeof(float));
			mapChannel->hashUvVertices = HashCode(mapChannel->uv_vertices);

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
