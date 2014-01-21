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

#include "CGR_config.h"

#include "GeomStaticMesh.h"

#include "CGR_blender_data.h"
#include "CGR_string.h"
#include "CGR_vrscene.h"

extern "C" {
#  include "BKE_mesh.h"
#  include "BKE_customdata.h"
#  include "BLI_math.h"
#  include "DNA_meshdata_types.h"
#  include "MEM_guardedalloc.h"
#  include "BLI_sys_types.h"
#  include "BLI_string.h"
#  include "BLI_path_util.h"
}


BLI_INLINE MHash HashCode(const char* s)
{
	int h = 0;
	while (*s) {
		h = 31*h + (*s++);
	}
	return h;
}


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


GeomStaticMesh::GeomStaticMesh()
{
	hash = 0;

	mesh = NULL;
	object = NULL;

	vertices = NULL;
	coordIndex = 0;

	faces = NULL;
	vertIndex = 0;

	normals = NULL;
	faceNormals = NULL;
	face_mtlIDs = NULL;
	edge_visibility = NULL;

	map_channels.clear();

	osd_subdiv_level = 0;
	osd_subdiv_type = 0;
	osd_subdiv_uvs = 0;
}


void GeomStaticMesh::init(Scene *sce, Main *main, Object *ob)
{
	mesh = GetRenderMesh(sce, main, ob);
	if(NOT(mesh))
		return;

	if(NOT(mesh->totface)) {
		FreeRenderMesh(main, mesh);
		return;
	}

	object = ob;

	initVertices();
	initFaces();
	initMapChannels();

	initName();
	initHash();

	FreeRenderMesh(main, mesh);
}


void GeomStaticMesh::freeData()
{
	if(vertices) {
		delete [] vertices;
		vertices = NULL;
	}
	if(faces) {
		delete [] faces;
		faces = NULL;
	}
	if(normals) {
		delete [] normals;
		normals = NULL;
	}
	if(faceNormals) {
		delete [] faceNormals;
		faceNormals = NULL;
	}
	if(face_mtlIDs) {
		delete [] face_mtlIDs;
		face_mtlIDs = NULL;
	}
	if(edge_visibility) {
		delete [] edge_visibility;
		edge_visibility = NULL;
	}

	for(int i = 0; i < map_channels.size(); ++i) {
		map_channels[i]->freeData();
	}
}


void GeomStaticMesh::initName()
{
	char obName[MAX_ID_NAME] = "";

	BLI_strncpy(obName, object->id.name+2, MAX_ID_NAME);
	StripString(obName);

	name.clear();
	name.append("ME");
	name.append(obName);

	const ID *dataID = (ID*)object->data;
	if(dataID->lib) {
		char libFilename[FILE_MAX] = "";

		BLI_split_file_part(dataID->lib->name+2, libFilename, FILE_MAX);
		BLI_replace_extension(libFilename, FILE_MAX, "");

		StripString(libFilename);

		name.append("LI");
		name.append(libFilename);
	}
}


const MChan* GeomStaticMesh::getMapChannel(const size_t i) const
{
	if(i >= map_channels.size())
		return NULL;
	return map_channels[i];
}


void GeomStaticMesh::initVertices()
{
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

	vertices = GetStringZip((u_int8_t*)vertsArray, coordIndex * sizeof(float));

	delete [] vertsArray;
}


void GeomStaticMesh::initFaces()
{
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

	faces           = GetStringZip((u_int8_t*)facesArray,       vertIndex      * sizeof(int));
	normals         = GetStringZip((u_int8_t*)normalsArray,     normIndex      * sizeof(float));
	faceNormals     = GetStringZip((u_int8_t*)faceNormalsArray, faceNormIndex  * sizeof(int));
	face_mtlIDs     = GetStringZip((u_int8_t*)face_mtlIDsArray, faceMtlIDIndex * sizeof(int));
	edge_visibility = GetStringZip((u_int8_t*)evArray,          evIndex        * sizeof(int));

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
		mapChannel->index = uv_layer_id++;

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
			mapChannel->uv_faces = map_channels[0]->uv_faces;
			mapChannel->cloned   = 1;
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

			delete [] mapFaces;
		}

		mapChannel->uv_vertices = GetStringZip((u_int8_t*)mapVertex, coordIndex * sizeof(float));

		delete [] mapVertex;

		map_channels.push_back(mapChannel);
	}
}


void GeomStaticMesh::initHash()
{
#if 0
	u_int32_t vertexHash[4];
	u_int32_t facesHash[4];

	MurmurHash3_x64_128(vertices, coordIndex * sizeof(float), 42,            vertexHash);
	MurmurHash3_x64_128(faces,    vertIndex  * sizeof(int),   vertexHash[0], facesHash);

	PRINT_INFO("Object: %s => hash = 0x%X", object->id.name+2, hash);
#else
	hash = HashCode(vertices);
#endif
}
