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
#include <boost/format.hpp>


AttrValue DataExporter::exportVRayNodeBlenderOutputMaterial(VRayNodeExportParam)
{
	AttrPlugin output_material;
#if 0
	if (!context.object_context.object) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
	}
	else {
		PluginDesc pluginDesc;

		std::string mtlName = Node::GetNodeMtlMulti(context.object_context.ob, context.object_context.mtlOverrideName, pluginDesc);

		// NOTE: Function could return only one material in 'mtlName'
		if(pluginDesc.find("mtls_list") == pluginDesc.end())
			return mtlName;

		std::string pluginName = NodeExporter::GenPluginName(node, ntree, context);

		BL::NodeSocket mtlid_gen_float = Nodes::GetInputSocketByName(node, "ID Generator");
		if(mtlid_gen_float.is_linked()) {
			pluginDesc.add("mtlid_gen_float", NodeExporter::exportLinkedSocket(ntree, mtlid_gen_float, context));

			// NOTE: if 'ids_list' presents in the plugin description 'mtlid_gen_*' won't work
			pluginDesc.del("ids_list");
		}

		pluginDesc["wrap_id"] = BOOST_FORMAT_BOOL(RNA_boolean_get(&node.ptr, "wrap_id"));

		output_material = m_exporter->export_plugin(pluginDesc);
	}
#endif
	return output_material;
}


AttrValue DataExporter::exportVRayNodeMtlMulti(VRayNodeExportParam)
{
	AttrListPlugin mtls_list(0);
	AttrListInt    ids_list(0);

	for(int i = 0; i < CGR_MAX_LAYERED_BRDFS; ++i) {
		const std::string &mtlSockName = boost::str(boost::format("Material %i") % i);

		BL::NodeSocket mtlSock = Nodes::GetInputSocketByName(node, mtlSockName);
		if (!mtlSock || !mtlSock.is_linked()) {
			continue;
		}

		AttrValue material = exportLinkedSocket(ntree, mtlSock, context);
		int materialID = RNA_int_get(&mtlSock.ptr, "value");

		mtls_list.append(material.valPlugin);
		ids_list.append(materialID);
	}

	const std::string & pluginName = GenPluginName(node, ntree, context);

	PluginDesc mtlMultiDesc(pluginName, "MtlMulti", "Mtl@");
	mtlMultiDesc.add("mtls_list", mtls_list);
	mtlMultiDesc.add("ids_list", ids_list);
	mtlMultiDesc.add("wrap_id", RNA_boolean_get(&node.ptr, "wrap_id"));

	BL::NodeSocket mtlid_gen_sock  = Nodes::GetSocketByAttr(node, "mtlid_gen");
	BL::NodeSocket mtlid_gen_float_sock = Nodes::GetSocketByAttr(node, "mtlid_gen_float");

	if(mtlid_gen_sock.is_linked()) {
		mtlMultiDesc.add("mtlid_gen", exportLinkedSocket(ntree, mtlid_gen_sock, context));
	} else if(mtlid_gen_float_sock.is_linked()) {
		mtlMultiDesc.add("mtlid_gen_float", exportLinkedSocket(ntree, mtlid_gen_float_sock, context));
	}

	return m_exporter->export_plugin(mtlMultiDesc);
}


AttrValue DataExporter::exportVRayNodeMetaStandardMaterial(VRayNodeExportParam)
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
