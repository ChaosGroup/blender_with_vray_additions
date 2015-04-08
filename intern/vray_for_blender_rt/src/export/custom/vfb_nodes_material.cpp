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


AttrValue DataExporter::exportVRayNodeBlenderOutputMaterial(VRayNodeExportParam)
{
	AttrPlugin output_material;
#if 0
	if (!context->object_context.object) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
	}
	else {
		PluginDesc pluginDesc;

		std::string mtlName = Node::GetNodeMtlMulti(context->object_context.ob, context->object_context.mtlOverrideName, pluginDesc);

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
#if 0
	std::string pluginName = NodeExporter::getPluginName(node, ntree, context);

	AttrListPlugin mtls_list;
	AttrListInt    ids_list;

	BL::NodeSocket mtlid_gen_sock  = NodeExporter::GetSocketByAttr(node, "mtlid_gen");
	BL::NodeSocket mtlid_gen_float_sock = NodeExporter::GetSocketByAttr(node, "mtlid_gen_float");

	for(int i = 1; i <= CGR_MAX_LAYERED_BRDFS; ++i) {
		const std::string &mtlSockName = boost::str(boost::format("Material %i") % i);

		BL::NodeSocket mtlSock = NodeExporter::getSocketByName(node, mtlSockName);
		if(NOT(mtlSock))
			continue;

		if(NOT(mtlSock.is_linked()))
			continue;

		AttrPlugin material   = NodeExporter::exportLinkedSocket(ntree, mtlSock, context);
		int        materialID = BOOST_FORMAT_INT(RNA_int_get(&mtlSock.ptr, "value"));

		mtls_list.push_back(material);
		ids_list.push_back(materialID);
	}

	PluginDesc pluginAttrs;
	pluginAttrs["mtls_list"] = BOOST_FORMAT_LIST(mtls_list);
	pluginAttrs["ids_list"]  = BOOST_FORMAT_LIST_INT(ids_list);
	if(mtlid_gen_sock.is_linked()) {
		pluginAttrs["mtlid_gen"] = NodeExporter::exportLinkedSocket(ntree, mtlid_gen_sock, context);
	}
	else if(mtlid_gen_float_sock.is_linked()) {
		pluginAttrs["mtlid_gen_float"] = NodeExporter::exportLinkedSocket(ntree, mtlid_gen_float_sock, context);
	}
	pluginAttrs["wrap_id"] = BOOST_FORMAT_INT(RNA_boolean_get(&node.ptr, "wrap_id"));

	VRayNodePluginExporter::exportPlugin("MATERIAL", "MtlMulti", pluginName, pluginAttrs);
#endif
	return AttrValue();
}


AttrValue DataExporter::exportMtlMulti(BL::BlendData bl_data, BL::Object bl_ob)
{
#if 0
	std::string pluginName = "MtlMulti@" + GetIDName(bl_ob);

	StrVector mtls_list;
	StrVector ids_list;

	BL::Object::material_slots_iterator maSlotIt;
	int i = 0;
	for (bl_ob.material_slots.begin(maSlotIt); maSlotIt != bl_ob.material_slots.end(); ++maSlotIt, ++i) {
		BL::MaterialSlot maSlot = *maSlotIt;
		if (NOT(maSlot))
			continue;

		BL::Material ma = maSlot.material();
		if (NOT(ma))
			continue;

		mtls_list.push_back(Node::GetMaterialName((Material*)ma.ptr.data, ExporterSettings::gSet.m_mtlOverrideName));
		ids_list.push_back(BOOST_FORMAT_INT(i));
	}

	if (mtls_list.size() == 0)
		return "NULL";
	else if (mtls_list.size() == 1)
		return mtls_list[0];

	PluginDesc pluginAttrs;
	pluginAttrs.add("mtls_list", PluginAttr(mtls_list));
	pluginAttrs["ids_list"]  = BOOST_FORMAT_LIST_INT(ids_list);
	pluginAttrs["wrap_id"] = "1";

	VRayNodePluginExporter::exportPlugin("Node", "MtlMulti", pluginName, pluginAttrs);

	return pluginName;
#endif
	return AttrValue();
}
