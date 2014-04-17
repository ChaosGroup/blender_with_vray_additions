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

#include "exp_nodes.h"

#include "CGR_rna.h"


BL::NodeSocket VRsceneExporter::getSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::inputs_iterator input;
	for(node.inputs.begin(input); input != node.inputs.end(); ++input)
		if(input->name() == socketName)
			return *input;

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::Node VRsceneExporter::getNodeByType(BL::NodeTree nodeTree, const std::string &nodeType)
{
	BL::NodeTree::nodes_iterator node;
	for(nodeTree.nodes.begin(node); node != nodeTree.nodes.end(); ++node)
		if(node->rna_type().identifier() == nodeType)
			return *node;

	return BL::Node(PointerRNA_NULL);
}


BL::Node VRsceneExporter::getConnectedNode(BL::NodeTree nodeTree, BL::NodeSocket socket)
{
	// XXX: For some reason Python's Node has 'links' attribute,
	// but here we have to iterate thought tree's 'links'
	//
	BL::NodeTree::links_iterator link;
	for(nodeTree.links.begin(link); link != nodeTree.links.end(); ++link)
		if(socket.ptr.data == link->to_socket().ptr.data)
			return link->from_node();

	return BL::Node(PointerRNA_NULL);
}


BL::Node VRsceneExporter::getConnectedNode(BL::NodeTree nodeTree, BL::Node node, const std::string &socketName)
{
	BL::NodeSocket socket = getSocketByName(node, socketName);
	if(NOT(socket.is_linked()))
		return BL::Node(PointerRNA_NULL);

	return getConnectedNode(nodeTree, socket);
}


BL::NodeTree VRsceneExporter::getNodeTree(Object *ob)
{
	PRINT_INFO("getNodeTree(%s)", ob->id.name);

	RnaAccess::RnaValue VRayObject((ID*)ob, "vray");

#if CGR_NTREE_DRIVER
	if(VRayObject.hasProperty("ntree__enum__")) {
		int ntreePtr = VRayObject.getEnum("ntree__enum__");
		if(ntreePtr != -1) {
			bNodeTree *ntree = (bNodeTree*)(intptr_t)ntreePtr;
			PointerRNA ntreeRNA;
			RNA_id_pointer_create((ID*)(&ntree->id), &ntreeRNA);
			return BL::NodeTree(ntreePtr);
		}
	}
#else
	if(VRayObject.hasProperty("ntree__name__")) {
		std::string ntreeName = VRayObject.getString("ntree__name__");
		if(NOT(ntreeName.empty())) {
			PRINT_INFO("Object '%s' node tree is '%s'", ob->id.name, ntreeName.c_str());

			BL::BlendData::node_groups_iterator nodeGroupIt;
			for(m_settings->b_data.node_groups.begin(nodeGroupIt); nodeGroupIt != m_settings->b_data.node_groups.end(); ++nodeGroupIt) {
				BL::NodeTree nodeTree = *nodeGroupIt;
				if(nodeTree.name() == ntreeName) {
					return nodeTree;
				}
			}
		}
	}
#endif

	return BL::NodeTree(PointerRNA_NULL);
}


void VRsceneExporter::exportNodeFromNodeTree(BL::NodeTree ntree, Object *ob, const int &checkUpdated)
{
#if 0
	BL::NodeTree::nodes_iterator b_node;

	PRINT_INFO("ntree = \"%s\"", ntree.name().c_str());

	BL::Node::inputs_iterator  b_input;
	BL::Node::outputs_iterator b_output;

	for(ntree.nodes.begin(b_node); b_node != ntree.nodes.end(); ++b_node) {
		PRINT_INFO("  node = \"%s\" ['%s']", b_node->name().c_str(), b_node->rna_type().identifier().c_str());

		for (b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
			PRINT_INFO("    input = \"%s\"", b_input->name().c_str());
		}

		for (b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
			PRINT_INFO("    output = \"%s\"", b_output->name().c_str());
		}
	}
#endif

	BL::Node nodeOutput = getNodeByType(ntree, "VRayNodeObjectOutput");
	if(NOT(nodeOutput)) {
		PRINT_ERROR("Object: %s Node tree: %s => Output node not found!", ob->id.name, ntree.name().c_str());
		return;
	}

	std::string materialName;
	std::string geometryName;

	BL::Node materialNode = getConnectedNode(ntree, nodeOutput, "Material");
	if(materialNode) {
		PRINT_INFO("Material node: '%s'", materialNode.name().c_str());
		materialName = writeNodeFromNodeTree(ntree, materialNode);
	}
	else {
		// Export object materials as is
		// ...
	}

	BL::Node geometryNode = getConnectedNode(ntree, nodeOutput, "Geometry");
	if(geometryNode) {
		PRINT_INFO("Geometry node: '%s'", geometryNode.name().c_str());
		geometryName = writeNodeFromNodeTree(ntree, geometryNode);
	}
	else {
		// Export object geometry as is
		// ...
	}

	sstream nodePlugin;
	nodePlugin.str("");
	nodePlugin << "\n"   << "Node" << " " << "" << " {";
	nodePlugin << "\n\t" << "material" << "=" << materialName << ";";
	nodePlugin << "\n\t" << "geometry" << "=" << geometryName << ";";
	nodePlugin << "\n\t" << "objectID" << "=" << ob->index << ";";
	nodePlugin << "\n\t" << "transform" << "=";
	nodePlugin << "TransformHex(\"" << "" << "\")" << ";";
	nodePlugin << "\n}\n";
}


std::string VRsceneExporter::writeNodeFromNodeTree(BL::NodeTree ntree, BL::Node node)
{
	std::string nodeClass = node.bl_idname();

	PRINT_INFO("Exporting: ntree = \"%s\" node = \"%s\"", ntree.name().c_str(), node.name().c_str());
	PRINT_INFO("  Class: node = \"%s\"", nodeClass.c_str());

	if(nodeClass == "VRayNodeBlenderOutputMaterial") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputMaterial(ntree, node);
	}
	else if(nodeClass == "VRayNodeBlenderOutputGeometry") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputGeometry(ntree, node);
	}
	else if(nodeClass == "VRayNodeBRDFLayered") {
		return VRayNodeExporter::exportVRayNodeBRDFLayered(ntree, node);
	}
	else if(nodeClass == "VRayNodeTexLayered") {
		return VRayNodeExporter::exportVRayNodeTexLayered(ntree, node);
	}
	else if(nodeClass == "VRayNodeSelectObject") {
		return VRayNodeExporter::exportVRayNodeSelectObject(ntree, node);
	}
	else if(nodeClass == "VRayNodeSelectGroup") {
		return VRayNodeExporter::exportVRayNodeSelectGroup(ntree, node);
	}
	else {
		std::string pluginType;
		std::string pluginID;

		char rnaStringBuf[CGR_MAX_PLUGIN_NAME];

		if(RNA_struct_find_property(&node.ptr, "vray_type")) {
			RNA_string_get(&node.ptr, "vray_type", rnaStringBuf);
			pluginType = rnaStringBuf;
		}

		if(RNA_struct_find_property(&node.ptr, "vray_plugin")) {
			RNA_string_get(&node.ptr, "vray_plugin", rnaStringBuf);
			pluginID = rnaStringBuf;
		}

		if(pluginID.empty()) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node plugin ID!", ntree.name().c_str(), node.name().c_str());
			return "NULL";
		}

		PointerRNA propGroup;
		if(NOT(RNA_struct_find_property(&node.ptr, pluginID.c_str()))) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Property group not found!", ntree.name().c_str(), node.name().c_str());
			return "NULL";
		}
		else {
			propGroup = RNA_pointer_get(&node.ptr, pluginID.c_str());
		}

		boost::property_tree::ptree *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);
		if(NOT(pluginDesc)) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Node is not supported!", ntree.name().c_str(), node.name().c_str());
			return "NULL";
		}

		PRINT_INFO("  Node represents: type = \"%s\" plugin = \"%s\"", pluginType.c_str(), pluginID.c_str());

		std::string                        pluginName = "NT" + ntree.name() + "N" + node.name();
		std::map<std::string, std::string> pluginSettings;

		BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pluginDesc->get_child("Parameters")) {
			std::string attrName = v.second.get_child("attr").data();
			std::string attrType = v.second.get_child("type").data();

			if(v.second.count("skip"))
				if(v.second.get<bool>("skip"))
					continue;

			if(SKIP_TYPE(attrType))
				continue;

			PRINT_INFO("  Processing attribute: \"%s\"", attrName.c_str());

			if(MAPPABLE_TYPE(attrType)) {
				BL::NodeSocket            sock(PointerRNA_NULL);
				BL::Node::inputs_iterator sockIt;

				for(node.inputs.begin(sockIt); sockIt != node.inputs.end(); ++sockIt) {
					std::string sockAttrName;
					if(RNA_struct_find_property(&sockIt->ptr, "vray_attr")) {
						RNA_string_get(&sockIt->ptr, "vray_attr", rnaStringBuf);
						sockAttrName = rnaStringBuf;
					}

					if(sockAttrName.empty())
						continue;
					if(attrName == sockAttrName) {
						sock = *sockIt;
						break;
					}
				}

				if(sock) {
					if(sock.is_linked()) {
						BL::Node conNode = getConnectedNode(ntree, sock);
						pluginSettings[attrName] = writeNodeFromNodeTree(ntree, conNode);
					}
					else {
						// Get socket value based on socket type
						//
						//	pluginSettings[attrName] = ;
						//
						//	switch(sock.type()) {
						//		case BL::NodeSocket::type_BOOLEAN: {
						//		}
						//			break;
						//		case BL::NodeSocket::type_VALUE: {
						//		}
						//			break;
						//		case BL::NodeSocket::type_CUSTOM: {
						//			PRINT_INFO("      Socket custom type: \"%s\"", sock.bl_idname().c_str());
						//		}
						//			break;
						//		default: {
						//		}
						//			break;
						//	}
					}
				}
			}
			else {
				if(attrType == "STRING") {
					char value[FILE_MAX] = "";
					RNA_string_get(&propGroup, attrName.c_str(), value);

					if(strlen(value) == 0)
						continue;

					if(v.second.count("subtype")) {
						std::string subType = v.second.get_child("subtype").data();
						if(subType == "FILE_PATH" || subType == "DIR_PATH") {
							BLI_path_abs(value, ID_BLEND_PATH(G.main, ((ID*)node.ptr.data)));
						}
					}

					// pluginSettings[attrName] = "\"" + value + "\"";
				}
				else {
					if(attrType == "BOOL") {
						// pluginSettings[attrName] = RNA_boolean_get(&propGroup, attrName.c_str());
					}
					else if(attrType == "INT") {
						// pluginSettings[attrName] = RNA_int_get(&propGroup, attrName.c_str());
					}
					else if(attrType == "FLOAT") {
						// pluginSettings[attrName] = RNA_float_get(&propGroup, attrName.c_str());
					}
				}
			}
		}

		return pluginName;
	}

	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeBlenderOutputMaterial(BL::NodeTree ntree, BL::Node node)
{
	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeBlenderOutputGeometry(BL::NodeTree ntree, BL::Node node)
{
	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeBRDFLayered(BL::NodeTree ntree, BL::Node node)
{
	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeTexLayered(BL::NodeTree ntree, BL::Node node)
{
	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node)
{
	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeSelectGroup(BL::NodeTree ntree, BL::Node node)
{
	return "NULL";
}
