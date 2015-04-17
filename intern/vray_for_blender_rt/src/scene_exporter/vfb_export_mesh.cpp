/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vfb_node_exporter.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_math.h"

#include "utils/cgr_hash.h"

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>


struct ChanVertex {
	ChanVertex():
	    v(),
	    index(0)
	{}

	ChanVertex(const BlVertUV &uv):
	    v(uv),
	    index(0)
	{}

	ChanVertex(const BlVertCol &col):
	    v(col),
	    index(0)
	{}

	bool operator == (const ChanVertex &other) const {
		return (v.x == other.v.x) && (v.y == other.v.y) && (v.z == other.v.z);
	}

	AttrVector   v;
	mutable int  index;
};

struct MapVertexHash {
	std::size_t operator () (const ChanVertex &mv) const {
		MHash hash;
		MurmurHash3_x86_32(&mv.v, sizeof(AttrVector), 42, &hash);
		return (std::size_t)hash;
	}
};

typedef boost::unordered_set<ChanVertex, MapVertexHash>  ChanSet;
typedef boost::unordered_map<std::string, ChanSet>       ChanMap;


AttrValue DataExporter::exportGeomStaticMesh(BL::Object ob)
{
	AttrValue geom;

	BL::Mesh mesh = m_data.meshes.new_from_object(m_scene, ob, true, 2, false, false);
	if (!mesh) {
		PRINT_ERROR("Object: %s => Incorrect mesh!",
		            ob.name().c_str());
	}
	else {
		const int useAutoSmooth = mesh.use_auto_smooth();
		if (useAutoSmooth) {
			mesh.calc_normals_split();
		}

		mesh.calc_tessface(true);

		const int num_channels = mesh.tessface_uv_textures.length() +
		                         mesh.tessface_vertex_colors.length();

		AttrListString   map_channels_names;
		AttrMapChannels  map_channels;
		if (num_channels) {
			map_channels_names.resize(num_channels);
		}

		ChanMap chan_data;

		int numFaces  = 0;
		int faceIdx   = 0;

		BL::Mesh::tessfaces_iterator faceIt;
		for (mesh.tessfaces.begin(faceIt); faceIt != mesh.tessfaces.end(); ++faceIt, ++faceIdx) {
			BlFace faceVerts = faceIt->vertices_raw();

			// If face is quad we split it into 2 tris
			numFaces += faceVerts[3] ? 2 : 1;

			if (num_channels) {
				BL::Mesh::tessface_uv_textures_iterator uvIt;
				for(mesh.tessface_uv_textures.begin(uvIt); uvIt != mesh.tessface_uv_textures.end(); ++uvIt) {
					BL::MeshTextureFaceLayer uvLayer(*uvIt);

					ChanSet &uvSet = chan_data[uvLayer.name()];

					uvSet.insert(ChanVertex(uvLayer.data[faceIdx].uv1()));
					uvSet.insert(ChanVertex(uvLayer.data[faceIdx].uv2()));
					uvSet.insert(ChanVertex(uvLayer.data[faceIdx].uv3()));

					if (faceVerts[3]) {
						uvSet.insert(ChanVertex(uvLayer.data[faceIdx].uv4()));
					}
				}
				BL::Mesh::tessface_vertex_colors_iterator colIt;
				for(mesh.tessface_vertex_colors.begin(colIt); colIt != mesh.tessface_vertex_colors.end(); ++colIt) {
					BL::MeshColorLayer colLayer(*colIt);

					ChanSet &colSet = chan_data[colLayer.name()];

					colSet.insert(ChanVertex(colLayer.data[faceIdx].color1()));
					colSet.insert(ChanVertex(colLayer.data[faceIdx].color2()));
					colSet.insert(ChanVertex(colLayer.data[faceIdx].color3()));

					if (faceVerts[3]) {
						colSet.insert(ChanVertex(colLayer.data[faceIdx].color4()));
					}
				}
			}
		}

		if (numFaces) {
			if (num_channels) {
				int chanIdx = 0;
				for (ChanMap::iterator setsIt = chan_data.begin(); setsIt != chan_data.end(); ++setsIt, ++chanIdx) {
					const std::string &chan_name = setsIt->first;
					ChanSet           &chan_data = setsIt->second;

					// Setup channel data
					AttrMapChannels::AttrMapChannel &map_channel = map_channels.data[chan_name];
					map_channel.name = chan_name;
					map_channel.vertices.resize(chan_data.size());
					map_channel.faces.resize(numFaces * 3);

					int f = 0;
					for (ChanSet::iterator setIt = chan_data.begin(); setIt != chan_data.end(); ++setIt, ++f) {
						const ChanVertex &map_vertex = *setIt;

						// Set vertex index for lookup from faces
						map_vertex.index = f;

						// Store channel vertex
						(*map_channel.vertices)[f] = map_vertex.v;
					}
				}
			}

			AttrListVector  vertices(mesh.vertices.length());
			AttrListInt     faces(numFaces * 3);
			AttrListVector  normals(numFaces * 3);
			AttrListInt     faceNormals(numFaces * 3);
			AttrListInt     face_mtlIDs(numFaces);
			AttrListInt     edge_visibility(numFaces / 10 + ((numFaces % 10 > 0) ? 1 : 0));

			memset((*edge_visibility), 0, edge_visibility.getBytesCount());

			BL::Mesh::vertices_iterator vertIt;
			int vertexIndex = 0;
			for (mesh.vertices.begin(vertIt); vertIt != mesh.vertices.end(); ++vertIt, ++vertexIndex) {
				(*vertices)[vertexIndex].x = vertIt->co()[0];
				(*vertices)[vertexIndex].y = vertIt->co()[1];
				(*vertices)[vertexIndex].z = vertIt->co()[2];
			}

			int normalIndex   = 0;
			int faceNormIndex = 0;
			int faceVertIndex = 0;
			int faceCount     = 0;
			int edgeVisIndex  = 0;
			int chanVertIndex = 0;
			int faceIdx       = 0;
			for (mesh.tessfaces.begin(faceIt); faceIt != mesh.tessfaces.end(); ++faceIt, ++faceIdx) {
				BlFace faceVerts = faceIt->vertices_raw();

				// Normals
				float n0[3];
				float n1[3];
				float n2[3];
				float n3[3];

				if (useAutoSmooth) {
					const BL::Array<float, 12> &autoNo = faceIt->split_normals();

					copy_v3_v3(n0, &autoNo.data[0 * 3]);
					copy_v3_v3(n1, &autoNo.data[1 * 3]);
					copy_v3_v3(n2, &autoNo.data[2 * 3]);
					if (faceVerts[3]) {
						copy_v3_v3(n3, &autoNo.data[3 * 3]);
					}
				}
				else {
					if (faceIt->use_smooth()) {
						copy_v3_v3(n0, &mesh.vertices[faceVerts[0]].normal().data[0]);
						copy_v3_v3(n1, &mesh.vertices[faceVerts[1]].normal().data[0]);
						copy_v3_v3(n2, &mesh.vertices[faceVerts[2]].normal().data[0]);
						if (faceVerts[3]) {
							copy_v3_v3(n3, &mesh.vertices[faceVerts[3]].normal().data[0]);
						}
					}
					else {
						float fno[3];
						copy_v3_v3(fno, &faceIt->normal().data[0]);

						copy_v3_v3(n0, fno);
						copy_v3_v3(n1, fno);
						copy_v3_v3(n2, fno);

						if (faceVerts[3]) {
							copy_v3_v3(n3, fno);
						}
					}
				}

				// Store normals / face normals
				(*faceNormals)[faceNormIndex++] = normalIndex;
				(*normals)[normalIndex++] = n0;
				(*faceNormals)[faceNormIndex++] = normalIndex;
				(*normals)[normalIndex++] = n1;
				(*faceNormals)[faceNormIndex++] = normalIndex;
				(*normals)[normalIndex++] = n2;
				if (faceVerts[3]) {
					(*faceNormals)[faceNormIndex++] = normalIndex;
					(*normals)[normalIndex++] = n0;
					(*faceNormals)[faceNormIndex++] = normalIndex;
					(*normals)[normalIndex++] = n2;
					(*faceNormals)[faceNormIndex++] = normalIndex;
					(*normals)[normalIndex++] = n3;
				}

				// Material ID
				const int matID = faceIt->material_index();

				// Store face vertices
				(*faces)[faceVertIndex++] = faceVerts[0];
				(*faces)[faceVertIndex++] = faceVerts[1];
				(*faces)[faceVertIndex++] = faceVerts[2];

				(*face_mtlIDs)[faceCount++] = matID;

				if (faceVerts[3]) {
					(*faces)[faceVertIndex++] = faceVerts[0];
					(*faces)[faceVertIndex++] = faceVerts[2];
					(*faces)[faceVertIndex++] = faceVerts[3];

					(*face_mtlIDs)[faceCount++] = matID;
				}

				// Store edge visibility
				if (faceVerts[3]) {
					(*edge_visibility)[edgeVisIndex/10] |= (3 << ((edgeVisIndex%10)*3));
					edgeVisIndex++;
					(*edge_visibility)[edgeVisIndex/10] |= (6 << ((edgeVisIndex%10)*3));
					edgeVisIndex++;
				}
				else {
					(*edge_visibility)[edgeVisIndex/10] |= (7 << ((edgeVisIndex%10)*3));
					edgeVisIndex++;
				}

				// Store UV / vertex colors
				if (num_channels) {
					int channel_vert_index = chanVertIndex;

					BL::Mesh::tessface_uv_textures_iterator uvIt;
					for (mesh.tessface_uv_textures.begin(uvIt); uvIt != mesh.tessface_uv_textures.end(); ++uvIt) {
						BL::MeshTextureFaceLayer uvLayer(*uvIt);

						const std::string &layer_name = uvLayer.name();

						ChanSet     &uvSet  = chan_data[layer_name];
						AttrListInt &uvData = map_channels.data[layer_name].faces;

						const int v0 = uvSet.find(ChanVertex(uvLayer.data[faceIdx].uv1()))->index;
						const int v1 = uvSet.find(ChanVertex(uvLayer.data[faceIdx].uv2()))->index;
						const int v2 = uvSet.find(ChanVertex(uvLayer.data[faceIdx].uv3()))->index;

						(*uvData)[channel_vert_index+0] = v0;
						(*uvData)[channel_vert_index+1] = v1;
						(*uvData)[channel_vert_index+2] = v2;

						if (faceVerts[3]) {
							const int v3 = uvSet.find(ChanVertex(uvLayer.data[faceIdx].uv4()))->index;

							(*uvData)[channel_vert_index+3] = v0;
							(*uvData)[channel_vert_index+4] = v2;
							(*uvData)[channel_vert_index+5] = v3;
						}
					}

					BL::Mesh::tessface_vertex_colors_iterator colIt;
					for (mesh.tessface_vertex_colors.begin(colIt); colIt != mesh.tessface_vertex_colors.end(); ++colIt) {
						BL::MeshColorLayer colLayer(*colIt);

						const std::string &layer_name = colLayer.name();

						ChanSet     &colSet  = chan_data[layer_name];
						AttrListInt &colData = map_channels.data[layer_name].faces;

						const int v0 = colSet.find(ChanVertex(colLayer.data[faceIdx].color1()))->index;
						const int v1 = colSet.find(ChanVertex(colLayer.data[faceIdx].color2()))->index;
						const int v2 = colSet.find(ChanVertex(colLayer.data[faceIdx].color3()))->index;

						(*colData)[channel_vert_index+0] = v0;
						(*colData)[channel_vert_index+1] = v1;
						(*colData)[channel_vert_index+2] = v2;

						if (faceVerts[3]) {
							const int v3 = colSet.find(ChanVertex(colLayer.data[faceIdx].color4()))->index;

							(*colData)[channel_vert_index+3] = v0;
							(*colData)[channel_vert_index+4] = v2;
							(*colData)[channel_vert_index+5] = v3;
						}
					}

					chanVertIndex += faceVerts[3] ? 6 : 3;
				}
			}

			m_data.meshes.remove(mesh);

			PluginDesc geomDesc(getMeshName(ob), "GeomStaticMesh", "Geom@");
			geomDesc.add("vertices", vertices);
			geomDesc.add("faces", faces);
			geomDesc.add("normals", normals);
			geomDesc.add("faceNormals", faceNormals);
			geomDesc.add("face_mtlIDs", face_mtlIDs);
			geomDesc.add("edge_visibility", edge_visibility);

			if (num_channels) {
				int i = 0;
				for (const auto &mcIt : map_channels.data) {
					// Store channel name
					(*map_channels_names)[i++] = mcIt.second.name;
				}
				geomDesc.add("map_channels_names", map_channels_names);
				geomDesc.add("map_channels",       map_channels);
			}

			geom = m_exporter->export_plugin(geomDesc);
		}
	}

	return geom;
}
