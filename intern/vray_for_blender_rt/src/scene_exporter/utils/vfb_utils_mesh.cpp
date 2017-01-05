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

#include "vfb_utils_mesh.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_math.h"

#include "utils/cgr_hash.h"

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>


using namespace VRayForBlender;


boost::format VRayForBlender::Mesh::UvChanNameFmt("Uv%s");
boost::format VRayForBlender::Mesh::ColChanNameFmt("Col%s");


struct ChanVertex {
	ChanVertex():
	    v(),
	    index(0)
	{}

	ChanVertex(const BlVertUV &uv):
	    v(AttrVectorFromBlVector(uv)),
	    index(0)
	{}

	ChanVertex(const BlVertCol &col):
	    v(AttrVectorFromBlVector(col)),
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


struct MapChannelBase {
	MapChannelBase(BL::Mesh mesh, int numFaces):
	    mesh(mesh),
	    num_faces(numFaces)
	{
		num_channels = mesh.tessface_uv_textures.length() +
		               mesh.tessface_vertex_colors.length();
	}

	virtual void init()=0;
	virtual void init_attributes(AttrListString &map_channels_names, AttrMapChannels &map_channels)=0;
	virtual bool needProcessFaces() const { return false; }
	virtual int  get_map_face_vertex_index(const std::string&, const ChanVertex&) { return -1; }

	int numChannels() const {
		return num_channels;
	}

protected:
	BL::Mesh  mesh;
	int       num_channels;
	int       num_faces;

};


struct MapChannelRaw:
        MapChannelBase
{
	MapChannelRaw(BL::Mesh mesh, int numFaces):
	    MapChannelBase(mesh, numFaces)
	{}

	virtual void init() override {}
	virtual void init_attributes(AttrListString &map_channels_names, AttrMapChannels &map_channels) override {
		if (num_channels) {
			// Init storage
			//
			BL::Mesh::tessface_uv_textures_iterator uvIt;
			for(mesh.tessface_uv_textures.begin(uvIt); uvIt != mesh.tessface_uv_textures.end(); ++uvIt) {
				BL::MeshTextureFaceLayer  uvLayer(*uvIt);
				const std::string        &uvLayerName = boost::str(VRayForBlender::Mesh::UvChanNameFmt % uvLayer.name());

				// Setup channel data storage
				AttrMapChannels::AttrMapChannel &map_channel = map_channels.data[uvLayerName];
				map_channel.name = uvLayerName;
				map_channel.vertices.resize(num_faces * 3);
				map_channel.faces.resize(num_faces * 3);
			}

			BL::Mesh::tessface_vertex_colors_iterator colIt;
			for(mesh.tessface_vertex_colors.begin(colIt); colIt != mesh.tessface_vertex_colors.end(); ++colIt) {
				BL::MeshColorLayer  colLayer(*colIt);
				const std::string  &colLayerName = boost::str(VRayForBlender::Mesh::ColChanNameFmt % colLayer.name());

				// Setup channel data storage
				AttrMapChannels::AttrMapChannel &map_channel = map_channels.data[colLayerName];
				map_channel.name = colLayerName;
				map_channel.vertices.resize(num_faces * 3);
				map_channel.faces.resize(num_faces * 3);
			}

			// Fill data
			//
			BL::Mesh::tessfaces_iterator faceIt;
			int faceIdx = 0;
			int chanVertIndex = 0;
			for (mesh.tessfaces.begin(faceIt); faceIt != mesh.tessfaces.end(); ++faceIt, ++faceIdx) {
				BlFace faceVerts = faceIt->vertices_raw();

				BL::Mesh::tessface_uv_textures_iterator uvIt;
				for(mesh.tessface_uv_textures.begin(uvIt); uvIt != mesh.tessface_uv_textures.end(); ++uvIt) {
					BL::MeshTextureFaceLayer  uvLayer(*uvIt);
					const std::string        &uvLayerName = boost::str(VRayForBlender::Mesh::UvChanNameFmt % uvLayer.name());

					AttrMapChannels::AttrMapChannel &uv_channel = map_channels.data[uvLayerName];

					const AttrVector v0(uvLayer.data[faceIdx].uv1());
					const AttrVector v1(uvLayer.data[faceIdx].uv2());
					const AttrVector v2(uvLayer.data[faceIdx].uv3());

					(*uv_channel.vertices)[chanVertIndex+0] = v0;
					(*uv_channel.vertices)[chanVertIndex+1] = v1;
					(*uv_channel.vertices)[chanVertIndex+2] = v2;

					if (faceVerts[3]) {
						const AttrVector v3(uvLayer.data[faceIdx].uv4());

						(*uv_channel.vertices)[chanVertIndex+3] = v0;
						(*uv_channel.vertices)[chanVertIndex+4] = v2;
						(*uv_channel.vertices)[chanVertIndex+5] = v3;
					}
				}

				BL::Mesh::tessface_vertex_colors_iterator colIt;
				for(mesh.tessface_vertex_colors.begin(colIt); colIt != mesh.tessface_vertex_colors.end(); ++colIt) {
					BL::MeshColorLayer  colLayer(*colIt);
					const std::string  &colLayerName = boost::str(VRayForBlender::Mesh::ColChanNameFmt % colLayer.name());

					AttrMapChannels::AttrMapChannel &col_channel = map_channels.data[colLayerName];

					const AttrVector v0(colLayer.data[faceIdx].color1());
					const AttrVector v1(colLayer.data[faceIdx].color2());
					const AttrVector v2(colLayer.data[faceIdx].color3());

					(*col_channel.vertices)[chanVertIndex+0] = v0;
					(*col_channel.vertices)[chanVertIndex+1] = v1;
					(*col_channel.vertices)[chanVertIndex+2] = v2;

					if (faceVerts[3]) {
						const AttrVector v3(colLayer.data[faceIdx].color4());

						(*col_channel.vertices)[chanVertIndex+3] = v0;
						(*col_channel.vertices)[chanVertIndex+4] = v2;
						(*col_channel.vertices)[chanVertIndex+5] = v3;
					}
				}

				chanVertIndex += faceVerts[3] ? 6 : 3;
			}

			// Setup face data
			for (auto &mcIt : map_channels.data) {
				for (int i = 0; i < mcIt.second.faces.ptr()->size(); ++i) {
					(*mcIt.second.faces)[i] = i;
				}
			}

			// Store channel names
			map_channels_names.resize(num_channels);
			int i = 0;
			for (const auto &mcIt : map_channels.data) {
				(*map_channels_names)[i++] = mcIt.second.name;
			}
		}
	}
};


struct MapChannelMerge:
        MapChannelBase
{
	MapChannelMerge(BL::Mesh mesh, int numFaces):
	    MapChannelBase(mesh, numFaces)
	{}

	virtual void init() override {
		if (num_channels) {
			BL::Mesh::tessfaces_iterator faceIt;
			int faceIdx = 0;
			for (mesh.tessfaces.begin(faceIt); faceIt != mesh.tessfaces.end(); ++faceIt, ++faceIdx) {
				BlFace faceVerts = faceIt->vertices_raw();

				BL::Mesh::tessface_uv_textures_iterator uvIt;
				for(mesh.tessface_uv_textures.begin(uvIt); uvIt != mesh.tessface_uv_textures.end(); ++uvIt) {
					BL::MeshTextureFaceLayer uvLayer(*uvIt);

					const std::string &layerName = boost::str(VRayForBlender::Mesh::UvChanNameFmt % uvLayer.name());

					ChanSet &uvSet = chan_data[layerName];

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

					const std::string &layerName = boost::str(VRayForBlender::Mesh::ColChanNameFmt % colLayer.name());

					ChanSet &colSet = chan_data[layerName];

					colSet.insert(ChanVertex(colLayer.data[faceIdx].color1()));
					colSet.insert(ChanVertex(colLayer.data[faceIdx].color2()));
					colSet.insert(ChanVertex(colLayer.data[faceIdx].color3()));

					if (faceVerts[3]) {
						colSet.insert(ChanVertex(colLayer.data[faceIdx].color4()));
					}
				}
			}
		}
	}

	virtual void init_attributes(AttrListString &map_channels_names, AttrMapChannels &map_channels) override {
		if (num_channels) {
			for (ChanMap::iterator setsIt = chan_data.begin(); setsIt != chan_data.end(); ++setsIt) {
				const std::string &chan_name = setsIt->first;
				ChanSet           &chan_data = setsIt->second;

				// Setup channel data storage
				AttrMapChannels::AttrMapChannel &map_channel = map_channels.data[chan_name];
				map_channel.name = chan_name;
				map_channel.vertices.resize(chan_data.size());
				map_channel.faces.resize(num_faces * 3);

				int f = 0;
				for (ChanSet::iterator setIt = chan_data.begin(); setIt != chan_data.end(); ++setIt, ++f) {
					const ChanVertex &map_vertex = *setIt;

					// Set vertex index for lookup from faces
					map_vertex.index = f;

					// Store channel vertex
					(*map_channel.vertices)[f] = map_vertex.v;
				}
			}

			// Store channel names
			map_channels_names.resize(num_channels);
			int i = 0;
			for (const auto &mcIt : map_channels.data) {
				(*map_channels_names)[i++] = mcIt.second.name;
			}
		}
	}

	virtual bool needProcessFaces() const override { return true; }

	virtual int get_map_face_vertex_index(const std::string &layerName, const ChanVertex &cv) override {
		return chan_data[layerName].find(cv)->index;
	}

private:
	ChanMap  chan_data;

};


int VRayForBlender::Mesh::FillMeshData(BL::BlendData data, BL::Scene scene, BL::Object ob, VRayForBlender::Mesh::ExportOptions options, PluginDesc &pluginDesc)
{
	int err = 0;
	WRITE_LOCK_BLENDER_RAII;

	BL::Mesh mesh = data.meshes.new_from_object(scene, ob, true, options.mode, false, false);
	if (!mesh) {
		PRINT_ERROR("Object: %s => Incorrect mesh!",
		            ob.name().c_str());
		err = 1;
	}
	else {
		const int useAutoSmooth = mesh.use_auto_smooth();
		if (useAutoSmooth) {
			mesh.calc_normals_split();
		}
		mesh.calc_tessface(true);

		BL::Mesh::tessfaces_iterator faceIt;
		int numFaces  = 0;
		for (mesh.tessfaces.begin(faceIt); faceIt != mesh.tessfaces.end(); ++faceIt) {
			BlFace faceVerts = faceIt->vertices_raw();

			// If face is quad we split it into 2 tris
			numFaces += faceVerts[3] ? 2 : 1;
		}

		if (numFaces) {
			AttrListVector  vertices(mesh.vertices.length());
			AttrListInt     faces(numFaces * 3);
			AttrListVector  normals(numFaces * 3);
			AttrListInt     faceNormals(numFaces * 3);
			AttrListInt     face_mtlIDs(numFaces);
			AttrListInt     edge_visibility(numFaces / 10 + ((numFaces % 10 > 0) ? 1 : 0));

			AttrListString  map_channels_names;
			AttrMapChannels map_channels;

			MapChannelBase *channels_data = nullptr;
			if (options.merge_channel_vertices) {
				channels_data = new MapChannelMerge(mesh, numFaces);
			}
			else {
				channels_data = new MapChannelRaw(mesh, numFaces);
			}
			channels_data->init();
			channels_data->init_attributes(map_channels_names, map_channels);

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
				float n0[3] = {0.0f, 0.0f, 0.0f};
				float n1[3] = {0.0f, 0.0f, 0.0f};
				float n2[3] = {0.0f, 0.0f, 0.0f};
				float n3[3] = {0.0f, 0.0f, 0.0f};

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
				if (channels_data->numChannels() && channels_data->needProcessFaces()) {
					int channel_vert_index = chanVertIndex;

					BL::Mesh::tessface_uv_textures_iterator uvIt;
					for (mesh.tessface_uv_textures.begin(uvIt); uvIt != mesh.tessface_uv_textures.end(); ++uvIt) {
						BL::MeshTextureFaceLayer uvLayer(*uvIt);

						const std::string &layerName = boost::str(UvChanNameFmt % uvLayer.name());

						AttrListInt &uvData = map_channels.data[layerName].faces;

						const int v0 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(uvLayer.data[faceIdx].uv1()));
						const int v1 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(uvLayer.data[faceIdx].uv2()));
						const int v2 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(uvLayer.data[faceIdx].uv3()));

						(*uvData)[channel_vert_index+0] = v0;
						(*uvData)[channel_vert_index+1] = v1;
						(*uvData)[channel_vert_index+2] = v2;

						if (faceVerts[3]) {
							const int v3 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(uvLayer.data[faceIdx].uv4()));

							(*uvData)[channel_vert_index+3] = v0;
							(*uvData)[channel_vert_index+4] = v2;
							(*uvData)[channel_vert_index+5] = v3;
						}
					}

					BL::Mesh::tessface_vertex_colors_iterator colIt;
					for (mesh.tessface_vertex_colors.begin(colIt); colIt != mesh.tessface_vertex_colors.end(); ++colIt) {
						BL::MeshColorLayer colLayer(*colIt);

						const std::string &layerName = boost::str(ColChanNameFmt % colLayer.name());

						AttrListInt &colData = map_channels.data[layerName].faces;

						const int v0 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(colLayer.data[faceIdx].color1()));
						const int v1 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(colLayer.data[faceIdx].color2()));
						const int v2 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(colLayer.data[faceIdx].color3()));

						(*colData)[channel_vert_index+0] = v0;
						(*colData)[channel_vert_index+1] = v1;
						(*colData)[channel_vert_index+2] = v2;

						if (faceVerts[3]) {
							const int v3 = channels_data->get_map_face_vertex_index(layerName, ChanVertex(colLayer.data[faceIdx].color4()));

							(*colData)[channel_vert_index+3] = v0;
							(*colData)[channel_vert_index+4] = v2;
							(*colData)[channel_vert_index+5] = v3;
						}
					}

					chanVertIndex += faceVerts[3] ? 6 : 3;
				}
			}

			data.meshes.remove(mesh, false);

			pluginDesc.add("vertices", vertices);
			pluginDesc.add("faces", faces);
			pluginDesc.add("normals", normals);
			pluginDesc.add("faceNormals", faceNormals);
			pluginDesc.add("face_mtlIDs", face_mtlIDs);
			pluginDesc.add("edge_visibility", edge_visibility);

			if (channels_data->numChannels()) {
				pluginDesc.add("map_channels_names", map_channels_names);
				pluginDesc.add("map_channels",       map_channels);
			}

			if (options.force_dynamic_geometry) {
				pluginDesc.add("dynamic_geometry", true);
			}
		}
	}

	return err;
}
