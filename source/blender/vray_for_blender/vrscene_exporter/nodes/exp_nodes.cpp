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


ExpoterSettings *VRayNodeExporter::m_exportSettings = NULL;
AttributeCache   VRayNodeExporter::m_attrCache;


BL::NodeSocket VRayNodeExporter::getSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::inputs_iterator input;
	for(node.inputs.begin(input); input != node.inputs.end(); ++input)
		if(input->name() == socketName)
			return *input;

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayNodeExporter::getSocketByAttr(BL::Node node, const std::string &attrName)
{
	char rnaStringBuf[CGR_MAX_PLUGIN_NAME];

	BL::Node::inputs_iterator sockIt;
	for(node.inputs.begin(sockIt); sockIt != node.inputs.end(); ++sockIt) {
		std::string sockAttrName;
		if(RNA_struct_find_property(&sockIt->ptr, "vray_attr")) {
			RNA_string_get(&sockIt->ptr, "vray_attr", rnaStringBuf);
			sockAttrName = rnaStringBuf;
		}

		if(sockAttrName.empty())
			continue;

		if(attrName == sockAttrName)
			return *sockIt;
	}

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::Node VRayNodeExporter::getNodeByType(BL::NodeTree nodeTree, const std::string &nodeType)
{
	BL::NodeTree::nodes_iterator node;
	for(nodeTree.nodes.begin(node); node != nodeTree.nodes.end(); ++node)
		if(node->rna_type().identifier() == nodeType)
			return *node;

	return BL::Node(PointerRNA_NULL);
}


BL::Node VRayNodeExporter::getConnectedNode(BL::NodeTree nodeTree, BL::NodeSocket socket)
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


BL::NodeSocket VRayNodeExporter::getConnectedSocket(BL::NodeTree nodeTree, BL::NodeSocket socket)
{
	BL::NodeTree::links_iterator link;
	for(nodeTree.links.begin(link); link != nodeTree.links.end(); ++link)
		if(socket.ptr.data == link->to_socket().ptr.data)
			return link->from_socket();

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::Node VRayNodeExporter::getConnectedNode(BL::NodeTree nodeTree, BL::Node node, const std::string &socketName)
{
	BL::NodeSocket socket = getSocketByName(node, socketName);
	if(NOT(socket.is_linked()))
		return BL::Node(PointerRNA_NULL);

	return getConnectedNode(nodeTree, socket);
}


BL::NodeTree VRayNodeExporter::getNodeTree(BL::BlendData b_data, ID *id)
{
	PRINT_INFO("ID: %s -> getNodeTree()", id->name);

	RnaAccess::RnaValue VRayObject(id, "vray");

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
			PRINT_INFO("ID: '%s' -> node tree is '%s'", id->name, ntreeName.c_str());

			BL::BlendData::node_groups_iterator nodeGroupIt;
			for(b_data.node_groups.begin(nodeGroupIt); nodeGroupIt != b_data.node_groups.end(); ++nodeGroupIt) {
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


BL::Texture VRayNodeExporter::getTextureFromIDRef(PointerRNA *ptr, const std::string &propName)
{
#if CGR_NTREE_DRIVER
#else
	if(RNA_struct_find_property(ptr, propName.c_str())) {
		char textureName[MAX_ID_NAME];
		RNA_string_get(ptr, propName.c_str(), textureName);

		BL::BlendData b_data = VRayNodeExporter::m_exportSettings->b_data;

		BL::BlendData::textures_iterator texIt;
		for(b_data.textures.begin(texIt); texIt != b_data.textures.end(); ++texIt) {
			BL::Texture b_tex = *texIt;
			if(b_tex.name() == textureName)
				return b_tex;
		}
	}
#endif

	return BL::Texture(PointerRNA_NULL);
}


std::string VRayNodeExporter::exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayObjectContext *context)
{
	BL::Node       conNode = VRayNodeExporter::getConnectedNode(ntree, socket);
	BL::NodeSocket conSock = VRayNodeExporter::getConnectedSocket(ntree, socket);

	std::string connectedPlugin = VRayNodeExporter::exportVRayNodeGeneric(ntree, conNode, context);

	std::string conSockAttrName;
	if(RNA_struct_find_property(&conSock.ptr, "vray_attr")) {
		char rnaStringBuf[CGR_MAX_PLUGIN_NAME];
		RNA_string_get(&conSock.ptr, "vray_attr", rnaStringBuf);
		conSockAttrName = rnaStringBuf;
	}

	if(NOT(conSockAttrName.empty())) {
		if(NOT(conSockAttrName == "uvwgen" || conSockAttrName == "bitmap")) {
			connectedPlugin += "::" + conSockAttrName;
		}
	}

	return connectedPlugin;
}


std::string VRayNodeExporter::exportVRayNodeAttributes(BL::NodeTree ntree, BL::Node node, const AttributeValueMap &manualAttrs)
{
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
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node plugin ID!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	if(NOT(RNA_struct_find_property(&node.ptr, pluginID.c_str()))) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Property group not found!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	PointerRNA propGroup = RNA_pointer_get(&node.ptr, pluginID.c_str());

	boost::property_tree::ptree *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);
	if(NOT(pluginDesc)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Node is not supported!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	PRINT_INFO("  Node represents: type = \"%s\" plugin = \"%s\"", pluginType.c_str(), pluginID.c_str());

	std::string        pluginName = StripString("NT" + ntree.name() + "N" + node.name());
	AttributeValueMap  pluginSettings;

	BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pluginDesc->get_child("Parameters")) {
		std::string attrName = v.second.get_child("attr").data();
		std::string attrType = v.second.get_child("type").data();

		if(SKIP_TYPE(attrType))
			continue;

		// PRINT_INFO("  Processing attribute: \"%s\"", attrName.c_str());

		if(pluginID == "TexGradRamp") {
			PRINT_INFO("  Processing attribute: \"%s\"", attrName.c_str());
		}

		AttributeValueMap::const_iterator manualAttrIt = manualAttrs.find(attrName);
		if(manualAttrIt != manualAttrs.end()) {
			pluginSettings[attrName] = manualAttrIt->second;
		}
		else {
			if(v.second.count("skip"))
				if(v.second.get<bool>("skip"))
					continue;

			if(MAPPABLE_TYPE(attrType)) {
				BL::NodeSocket            sock(PointerRNA_NULL);
				BL::Node::inputs_iterator sockIt;

				// Go through all sockets and find the one for our attribute
				//
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
						pluginSettings[attrName] = exportLinkedSocket(ntree, sock);
					}
					else {
						std::string socketVRayType = sock.rna_type().identifier();

						if(socketVRayType == "VRaySocketColor") {
							float color[3];
							RNA_float_get_array(&sock.ptr, "value", color);
							pluginSettings[attrName] = boost::str(boost::format("AColor(%.6f,%.6f,%.6f,1.0)")
																  % color[0] % color[1] % color[2]);
						}
						else if(socketVRayType == "VRaySocketFloatColor") {
							pluginSettings[attrName] = boost::str(boost::format("%.6f") % RNA_float_get(&sock.ptr, "value"));
						}
						else if(socketVRayType == "VRaySocketInt") {
							pluginSettings[attrName] = boost::str(boost::format("%i") % RNA_int_get(&sock.ptr, "value"));
						}
						else if(socketVRayType == "VRaySocketFloat") {
							pluginSettings[attrName] = boost::str(boost::format("%.6f") % RNA_float_get(&sock.ptr, "value"));
						}
						else if(socketVRayType == "VRaySocketFloatNoValue") {
							// If it's not mapped simply skip it.
							continue;
						}
						else if(socketVRayType == "VRaySocketVector") {
							float vector[3];
							RNA_float_get_array(&sock.ptr, "value", vector);
							pluginSettings[attrName] = boost::str(boost::format("Vector(%.6f,%.6f,%.6f)")
																  % vector[0] % vector[1] % vector[2]);
						}
						else if(socketVRayType == "VRaySocketCoords") {
							// This is the UVWGEN socket; if it's not mapped simply skip it
							continue;
						}
						else {
							PRINT_ERROR("Node tree: %s => Node name: %s => Unsupported socket type: %s",
										ntree.name().c_str(), node.name().c_str(), socketVRayType.c_str());
							continue;
						}
					}
				}
			}
			else {
				PropertyRNA *prop = RNA_struct_find_property(&propGroup, attrName.c_str());
				if(NOT(prop))
					continue;

				// PropertyType propType = RNA_property_type(prop);

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

					pluginSettings[attrName] = boost::str(boost::format("\"%s\"") % value);
				}
				else {
					if(attrType == "BOOL") {
						pluginSettings[attrName] = boost::str(boost::format("%i") % RNA_boolean_get(&propGroup, attrName.c_str()));
					}
					else if(attrType == "INT") {
						pluginSettings[attrName] = boost::str(boost::format("%i") % RNA_int_get(&propGroup, attrName.c_str()));
					}
					else if(attrType == "ENUM") {
						pluginSettings[attrName] = boost::str(boost::format("%i") % RNA_enum_get(&propGroup, attrName.c_str()));
					}
					else if(attrType == "FLOAT") {
						pluginSettings[attrName] = boost::str(boost::format("%.6f") % RNA_float_get(&propGroup, attrName.c_str()));
					}
					else if(attrType == "VECTOR") {
						PropertySubType propSubType = RNA_property_subtype(prop);
						if(propSubType == PROP_COLOR) {
							if(RNA_property_array_length(&propGroup, prop) == 4) {
								float acolor[4];
								RNA_float_get_array(&propGroup, attrName.c_str(), acolor);
								pluginSettings[attrName] = boost::str(boost::format("AColor(%.6f,%.6f,%.6f,%.6f)")
																	  % acolor[0] % acolor[1] % acolor[2] % acolor[3]);
							}
							else {
								float color[3];
								RNA_float_get_array(&propGroup, attrName.c_str(), color);
								pluginSettings[attrName] = boost::str(boost::format("Color(%.6f,%.6f,%.6f)")
																	  % color[0] % color[1] % color[2]);
							}
						}
						else {
							float vector[3];
							RNA_float_get_array(&propGroup, attrName.c_str(), vector);
							pluginSettings[attrName] = boost::str(boost::format("Vector(%.6f,%.6f,%.6f)")
																  % vector[0] % vector[1] % vector[2]);
						}
					}
				}
			}
		}
	}

	sstream plugin;

	plugin << "\n" << pluginID << " " << pluginName << " {";

	AttributeValueMap::const_iterator attrIt;
	for(attrIt = pluginSettings.begin(); attrIt != pluginSettings.end(); ++attrIt) {
		const std::string attrName  = attrIt->first;
		const std::string attrValue = attrIt->second;

		plugin << "\n\t" << attrName << "=" << VRayExportable::m_interpStart << attrValue << VRayExportable::m_interpEnd << ";";
	}

	plugin << "\n}\n";

	if(pluginType == "TEXTURE" || pluginType == "UVWGEN") {
		PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileTex, plugin.str().c_str());
	}
	else if(pluginType == "MATERIAL" || pluginType == "BRDF") {
		PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileMat, plugin.str().c_str());
	}
	else if(pluginType == "GEOMETRY") {
		if(pluginID == "GeomDisplacedMesh" || pluginID == "GeomStaticSmoothedMesh") {
			PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileObject, plugin.str().c_str());
		}
		else {
			PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileGeom, plugin.str().c_str());
		}
	}
	else {
		PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileObject, plugin.str().c_str());
	}

	return pluginName;
}


std::string VRayNodeExporter::exportVRayNodeGeneric(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context, const AttributeValueMap &manualAttrs)
{
	std::string nodeClass = node.bl_idname();

	PRINT_INFO("Exporting: ntree = \"%s\" node = \"%s\"", ntree.name().c_str(), node.name().c_str());
	PRINT_INFO("  Class: node = \"%s\"", nodeClass.c_str());

	if(nodeClass == "VRayNodeBlenderOutputMaterial") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputMaterial(ntree, node, context);
	}
	else if(nodeClass == "VRayNodeBlenderOutputGeometry") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputGeometry(ntree, node, context);
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
	else if(nodeClass == "VRayNodeSelectNodeTree") {
		return VRayNodeExporter::exportVRayNodeSelectNodeTree(ntree, node);
	}
	else if(nodeClass == "VRayNodeLightMesh") {
		return VRayNodeExporter::exportVRayNodeLightMesh(ntree, node, context);
	}
	else if(nodeClass == "VRayNodeGeomDisplacedMesh") {
		return VRayNodeExporter::exportVRayNodeGeomDisplacedMesh(ntree, node, context);
	}
	else if(nodeClass == "VRayNodeBitmapBuffer") {
		return VRayNodeExporter::exportVRayNodeBitmapBuffer(ntree, node);
	}
	else if(nodeClass == "VRayNodeTexGradRamp") {
		return VRayNodeExporter::exportVRayNodeTexGradRamp(ntree, node);
	}
	else if(nodeClass == "VRayNodeTexRemap") {
		return VRayNodeExporter::exportVRayNodeTexRemap(ntree, node);
	}
	else if(nodeClass == "VRayNodeOutputMaterial") {
		BL::NodeSocket materialInSock = VRayNodeExporter::getSocketByName(node, "Material");
		if(materialInSock.is_linked()) {
			return VRayNodeExporter::exportLinkedSocket(ntree, materialInSock);
		}
	}
	else if(nodeClass == "VRayNodeOutputTexture") {
		BL::NodeSocket textureInSock = VRayNodeExporter::getSocketByName(node, "Texture");
		if(textureInSock.is_linked()) {
			return VRayNodeExporter::exportLinkedSocket(ntree, textureInSock);
		}
	}

	return exportVRayNodeAttributes(ntree, node, manualAttrs);
}

