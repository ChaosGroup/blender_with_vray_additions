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
#include "vfb_utils_nodes.h"
#include "vfb_utils_blender.h"

#include <boost/format.hpp>


AttrValue DataExporter::exportVRayNodeBlenderOutputMaterial(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket&, NodeContext &context)
{
	AttrValue output_material = getDefaultMaterial();

	auto ob = context.object_context.object;
	if (!ob) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		const int mtlCount = Blender::GetMaterialCount(ob);
		if (mtlCount) {
			if (mtlCount == 1) {
				output_material = exportSingleMaterial(ob);
			}
			else {
				PluginDesc mtlMultiDesc(ob.name(), "MtlMulti", "Mtl@");
				fillMtlMulti(ob, mtlMultiDesc);

				mtlMultiDesc.add("wrap_id", RNA_boolean_get(&node.ptr, "wrap_id"));

				BL::NodeSocket mtlid_gen_float = Nodes::GetInputSocketByName(node, "ID Generator");
				if (mtlid_gen_float.is_linked()) {
					mtlMultiDesc.add("mtlid_gen_float", exportLinkedSocket(ntree, mtlid_gen_float, context));

					// NOTE: if 'ids_list' presents in the plugin description 'mtlid_gen_*' won't work
					mtlMultiDesc.del("ids_list");
				}

				output_material = m_exporter->export_plugin(mtlMultiDesc);
			}

		}
	}

	return output_material;
}


AttrValue DataExporter::exportVRayNodeMtlMulti(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket&, NodeContext &context)
{
	AttrListPlugin mtls_list(0);
	AttrListInt    ids_list(0);

	for(int i = 0; i <= CGR_MAX_LAYERED_BRDFS; ++i) {
		char materialSocket[32] = {0, };
		snprintf(materialSocket, sizeof(materialSocket), "Material %i", i);

		BL::NodeSocket mtlSock = Nodes::GetInputSocketByName(node, materialSocket);
		if (mtlSock && mtlSock.is_linked()) {
			AttrValue material = exportLinkedSocket(ntree, mtlSock, context);
			int materialID = RNA_int_get(&mtlSock.ptr, "value");

			mtls_list.append(material.as<AttrPlugin>());
			ids_list.append(materialID);
		}
	}

	const std::string &pluginName = GenPluginName(node, ntree, context);

	PluginDesc mtlMultiDesc(pluginName, "MtlMulti", "Mtl@");
	mtlMultiDesc.add("mtls_list", mtls_list);
	mtlMultiDesc.add("ids_list", ids_list);
	mtlMultiDesc.add("wrap_id", RNA_boolean_get(&node.ptr, "wrap_id"));

	BL::NodeSocket mtlid_gen_sock  = Nodes::GetSocketByAttr(node, "mtlid_gen");
	BL::NodeSocket mtlid_gen_float_sock = Nodes::GetSocketByAttr(node, "mtlid_gen_float");

	if (mtlid_gen_sock.is_linked()) {
		mtlMultiDesc.add("mtlid_gen", exportLinkedSocket(ntree, mtlid_gen_sock, context));
		mtlMultiDesc.del("ids_list");
	}
	else if (mtlid_gen_float_sock.is_linked()) {
		mtlMultiDesc.add("mtlid_gen_float", exportLinkedSocket(ntree, mtlid_gen_float_sock, context));
		mtlMultiDesc.del("ids_list");
	}

	return m_exporter->export_plugin(mtlMultiDesc);
}

AttrPlugin DataExporter::getTextureUVWGen(BL::NodeTree &ntree, BL::Node &node, NodeContext &context, BL::NodeSocket textureSocket) {
	if (textureSocket) {
		auto texNode = Nodes::GetConnectedNode(textureSocket);
		if (textureSocket) {
			auto uvwgenSock = Nodes::GetSocketByAttr(texNode, "uvwgen");
			if (uvwgenSock) {
				auto uvwgenNode = Nodes::GetConnectedNode(uvwgenSock);
				if (uvwgenNode) {
					return AttrPlugin(GenPluginName(uvwgenNode, ntree, context));
				} else {
					// if no uvwgen is attached, setAttrsFromNode will generate one
					return AttrPlugin("UVW@" + DataExporter::GenPluginName(node, ntree, context));
				}
			}
		}
	}

	return "";
}

AttrValue DataExporter::exportVRayNodeBRDFBumpMtl(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	const std::string &brdfBumpName = "BRDFBump@" + DataExporter::GenPluginName(node, ntree, context);

	PluginDesc brdfBump(brdfBumpName, "BRDFBump");

	setAttrsFromNode(ntree, node, fromSocket, context, brdfBump, "BRDFBump", ParamDesc::PluginBRDF);

	BL::NodeSocket sockBump   = Nodes::GetSocketByAttr(node, "bump_tex_float");
	BL::NodeSocket sockNormal = Nodes::GetSocketByAttr(node, "bump_tex_color");
	BL::NodeSocket sockTex(PointerRNA_NULL);
	if (sockBump && sockBump.is_linked()) {
		brdfBump.del("bump_tex_color");
		sockTex = sockBump;
	} else {
		brdfBump.del("bump_tex_float");
		sockTex = sockNormal;
	}

	auto uvwgenPlg = getTextureUVWGen(ntree, node, context, sockTex);
	if (!uvwgenPlg.plugin.empty()) {
		brdfBump.add("normal_uvwgen", uvwgenPlg);
	}

	return m_exporter->export_plugin(brdfBump);
}

AttrValue DataExporter::exportVRayNodeMetaStandardMaterial(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	const std::string &baseName = DataExporter::GenPluginName(node, ntree, context);

	// BRDFVRayMtl
	//
	const std::string &brdfVRayMtlName = "BRDFVRayMtl@" + baseName;

	PluginDesc brdfVRayMtl(brdfVRayMtlName, "BRDFVRayMtl");
	setAttrsFromNode(ntree, node, fromSocket, context, brdfVRayMtl, "BRDFVRayMtl", ParamDesc::PluginBRDF);

	PointerRNA brdfVRayMtlPtr = RNA_pointer_get(&node.ptr, "BRDFVRayMtl");
	if (RNA_boolean_get(&brdfVRayMtlPtr, "hilight_glossiness_lock")) {
		brdfVRayMtl.add("hilight_glossiness", brdfVRayMtl.get("reflect_glossiness")->attrValue);
	}

	m_exporter->export_plugin(brdfVRayMtl);

	// Material BRDF
	std::string materialBrdf = brdfVRayMtlName;

	// BRDFBump
	//
	BL::NodeSocket sockBump   = Nodes::GetSocketByAttr(node, "bump_tex_float");
	BL::NodeSocket sockNormal = Nodes::GetSocketByAttr(node, "bump_tex_color");
	const bool useBump = (sockBump && sockBump.is_linked()) || (sockNormal && sockNormal.is_linked());
	if (useBump) {
		const std::string &brdfBumpName = "BRDFBump@" + baseName;

		PluginDesc brdfBump(brdfBumpName, "BRDFBump");
		brdfBump.add("base_brdf", AttrPlugin(brdfVRayMtlName));

		setAttrsFromNode(ntree, node, fromSocket, context, brdfBump, "BRDFBump", ParamDesc::PluginBRDF);

		BL::NodeSocket sockTex(PointerRNA_NULL);
		if (sockBump && sockBump.is_linked()) {
			brdfBump.del("bump_tex_color");
			sockTex = sockBump;
		} else {
			brdfBump.del("bump_tex_float");
			sockTex = sockNormal;
		}

		auto uvwgenPlg = getTextureUVWGen(ntree, node, context, sockTex);
		if (!uvwgenPlg.plugin.empty()) {
			brdfBump.add("normal_uvwgen", uvwgenPlg);
		}

		m_exporter->export_plugin(brdfBump);

		materialBrdf = brdfBumpName;
	}

	// MtlSingleBRDF
	//
	const std::string &mtlSingleBrdfName = "MtlSingleBRDF@" + baseName;

	PluginDesc mtlSingleBrdf(mtlSingleBrdfName, "MtlSingleBRDF");
	mtlSingleBrdf.add("brdf", AttrPlugin(materialBrdf));

	setAttrsFromNode(ntree, node, fromSocket, context, mtlSingleBrdf, "MtlSingleBRDF", ParamDesc::PluginMaterial);
	m_exporter->export_plugin(mtlSingleBrdf);

	// MtlMaterialID
	//
	PluginDesc mtlMaterialId(baseName, "MtlMaterialID");
	mtlMaterialId.add("base_mtl", AttrPlugin(mtlSingleBrdfName));

	setAttrsFromNode(ntree, node, fromSocket, context, mtlMaterialId, "MtlMaterialID", ParamDesc::PluginMaterial);

	return m_exporter->export_plugin(mtlMaterialId);
}
