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
#include "cgr_paths.h"

extern "C" {
#  include "BKE_node.h"
}

VRayNodeCache    VRayNodePluginExporter::m_nodeCache;
StrSet           VRayNodePluginExporter::m_namesCache;

StrSet           VRayNodeExporter::RenderChannelNames;


boost::format FmtSwitch("Input %i");


static std::string GetUniqueChannelName(const std::string &baseName)
{
	boost::format chanNameFormat("%s.%03i");

	std::string uniqueName = baseName;

	int uniqueSuffix = 0;
	while (VRayNodeExporter::RenderChannelNames.count(uniqueName)) {
		uniqueSuffix++;
		uniqueName = boost::str(chanNameFormat
								% baseName
								% uniqueSuffix);
	}

	VRayNodeExporter::RenderChannelNames.insert(uniqueName);

	return uniqueName;
}


std::string VRayNodeExporter::getPluginName(BL::Node node, BL::NodeTree ntree, VRayNodeContext &context)
{
	boost::format nodeNameFmt("%s|N%s");
	std::string pluginName = boost::str(nodeNameFmt % GetIDName(ntree) % node.name());

	for (VRayNodeContext::NodeTreeVector::iterator ntIt = context.parent.begin(); ntIt != context.parent.end(); ++ntIt) {
		BL::NodeTree &parent = *ntIt;
		if (parent) {
			pluginName += "|NT" + GetIDName(parent);
		}
	}

	for (VRayNodeContext::NodeVector::iterator gnIt = context.group.begin(); gnIt != context.group.end(); ++gnIt) {
		BL::Node &group = *gnIt;
		if (group) {
			pluginName += "|GR" + group.name();
		}
	}

	return StripString(pluginName);
}


std::string VRayNodeExporter::getPluginType(BL::Node node)
{
	if(RNA_struct_find_property(&node.ptr, "vray_type"))
		return RNA_std_string_get(&node.ptr, "vray_type");
	return "";
}


std::string VRayNodeExporter::getPluginID(BL::Node node)
{
	if(RNA_struct_find_property(&node.ptr, "vray_plugin"))
		return RNA_std_string_get(&node.ptr, "vray_plugin");
	return "";
}


void VRayNodeExporter::init(BL::BlendData data)
{
	// NOTE: On scene save node links are not properly updated for some
	// reason; simply manually update everything...
	//
	BL::BlendData::node_groups_iterator nIt;
	for (data.node_groups.begin(nIt); nIt != data.node_groups.end(); ++nIt) {
		ntreeUpdateTree((struct Main*)data.ptr.data, (struct bNodeTree*)(nIt->ptr.data));
	}
}


void VRayNodeExporter::getAttributesList(const std::string &pluginID, StrSet &attrSet, bool mappable)
{
	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);
	if(NOT(pluginDesc)) {
		PRINT_ERROR("Plugin '%s' description is not found!",
					pluginID.c_str());
		return;
	}

	BOOST_FOREACH(PluginJson::value_type &v, pluginDesc->get_child("Parameters")) {
		std::string attrName = v.second.get_child("attr").data();
		std::string attrType = v.second.get_child("type").data();

		if(SKIP_TYPE(attrType))
			continue;

		if(v.second.count("skip"))
			if(v.second.get<bool>("skip"))
				continue;

		if(mappable) {
			if(MAPPABLE_TYPE(attrType))
				attrSet.insert(attrName);
		}
		else {
			if(NOT(MAPPABLE_TYPE(attrType)))
				attrSet.insert(attrName);
		}
	}
}


std::string VRayNodeExporter::getValueFromPropGroup(PointerRNA *propGroup, ID *holder, const std::string &attrName)
{
	PropertyRNA *prop = RNA_struct_find_property(propGroup, attrName.c_str());
	if(NOT(prop))
		return "NULL";

	PropertyType propType = RNA_property_type(prop);

	if(propType == PROP_STRING) {
		char value[FILE_MAX] = "";
		RNA_string_get(propGroup, attrName.c_str(), value);

		if(strlen(value) == 0)
			return "NULL";

		std::string absFilepath = value;

		PropertySubType propSubType = RNA_property_subtype(prop);
		if(propSubType == PROP_FILEPATH || propSubType == PROP_DIRPATH) {
			BLI_path_abs(value, ID_BLEND_PATH_EX(holder));

			if(propSubType == PROP_FILEPATH) {
				absFilepath = BlenderUtils::GetFullFilepath(value, holder);
				absFilepath = BlenderUtils::CopyDRAsset(absFilepath);
			}
		}

		return BOOST_FORMAT_STRING(absFilepath.c_str());
	}
	else if(propType == PROP_BOOLEAN) {
		return BOOST_FORMAT_BOOL(RNA_boolean_get(propGroup, attrName.c_str()));
	}
	else if(propType == PROP_INT) {
		return BOOST_FORMAT_INT(RNA_int_get(propGroup, attrName.c_str()));
	}
	else if(propType == PROP_ENUM) {
		return BOOST_FORMAT_INT(RNA_enum_ext_get(propGroup, attrName.c_str()));
	}
	else if(propType == PROP_FLOAT) {
		if(NOT(RNA_property_array_check(prop))) {
			return BOOST_FORMAT_FLOAT(RNA_float_get(propGroup, attrName.c_str()));
		}
		else {
			PropertySubType propSubType = RNA_property_subtype(prop);
			if(propSubType == PROP_COLOR) {
				if(RNA_property_array_length(propGroup, prop) == 4) {
					float acolor[4];
					RNA_float_get_array(propGroup, attrName.c_str(), acolor);
					return BOOST_FORMAT_ACOLOR(acolor);
				}
				else {
					float color[3];
					RNA_float_get_array(propGroup, attrName.c_str(), color);
					return BOOST_FORMAT_COLOR(color);
				}
			}
			else {
				float vector[3];
				RNA_float_get_array(propGroup, attrName.c_str(), vector);
				return BOOST_FORMAT_VECTOR(vector);
			}
		}
	}
	else {
		PRINT_ERROR("Property '%s': Unsupported property type '%i'.", RNA_property_identifier(prop), propType);
	}

	return "NULL";
}


BL::NodeSocket VRayNodeExporter::getSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::inputs_iterator input;
	for(node.inputs.begin(input); input != node.inputs.end(); ++input)
		if(input->name() == socketName)
			return *input;

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayNodeExporter::getOutputSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::outputs_iterator input;
	for(node.outputs.begin(input); input != node.outputs.end(); ++input)
		if(input->name() == socketName)
			return *input;

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayNodeExporter::getSocketByAttr(BL::Node node, const std::string &attrName)
{
	BL::Node::inputs_iterator sockIt;
	for(node.inputs.begin(sockIt); sockIt != node.inputs.end(); ++sockIt) {
		std::string sockAttrName;
		if(RNA_struct_find_property(&sockIt->ptr, "vray_attr")) {
			sockAttrName = RNA_std_string_get(&sockIt->ptr, "vray_attr");
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


BL::NodeSocket VRayNodeExporter::getConnectedSocket(BL::NodeSocket socket)
{
	bNodeSocket *bSocket = (bNodeSocket*)socket.ptr.data;
	bNodeLink   *link = bSocket->link;
	if(link) {
		PointerRNA socketPtr;
		RNA_pointer_create((ID*)socket.ptr.id.data, &RNA_NodeSocket, link->fromsock, &socketPtr);
		return BL::NodeSocket(socketPtr);
	}
	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeTree VRayNodeExporter::getNodeTree(BL::BlendData b_data, ID *id)
{
	PointerRNA idPtr;
	RNA_id_pointer_create(id, &idPtr);

	PointerRNA   ptr  = RNA_pointer_get(&idPtr, "vray");
	PropertyRNA *prop = RNA_struct_find_property(&ptr, "ntree");
	if(prop) {
		PropertyType propType = RNA_property_type(prop);
		if(propType == PROP_POINTER) {
			PointerRNA ntree = RNA_pointer_get(&ptr, "ntree");
			BL::NodeTree nodeTree(ntree);
			if(nodeTree)
				return nodeTree;
		}
	}

	return BL::NodeTree(PointerRNA_NULL);
}


BL::Texture VRayNodeExporter::getTextureFromIDRef(PointerRNA *ptr, const std::string &propName)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propName.c_str());
	if(prop) {
		PropertyType propType = RNA_property_type(prop);
		if(propType == PROP_POINTER) {
			PointerRNA texPtr = RNA_pointer_get(ptr, propName.c_str());
			BL::Texture texture(texPtr);
			if(texture)
				return texture;
		}
	}

	return BL::Texture(PointerRNA_NULL);
}


BL::NodeTree VRayNodeExporter::getNodeGroupTree(BL::Node node)
{
	BL::NodeTree groupTree = BL::NodeTree(PointerRNA_NULL);
	if(node.is_a(&RNA_ShaderNodeGroup)) {
		BL::NodeGroup groupNode(node);
		groupTree = groupNode.node_tree();
	}
	else {
		BL::NodeCustomGroup groupNode(node);
		groupTree = groupNode.node_tree();
	}
	return groupTree;
}


BL::NodeSocket VRayNodeExporter::getNodeGroupSocketReal(BL::Node node, BL::NodeSocket fromSocket)
{
	BL::NodeTree groupTree = VRayNodeExporter::getNodeGroupTree(node);
	if(NOT(groupTree)) {
		PRINT_ERROR("Group node name: %s => Tree not found!",
					node.name().c_str());
		return BL::NodeSocket(PointerRNA_NULL);
	}
	BL::NodeGroupOutput groupOutput(PointerRNA_NULL);
	BL::NodeTree::nodes_iterator nodeIt;
	for(groupTree.nodes.begin(nodeIt); nodeIt != groupTree.nodes.end(); ++nodeIt) {
		BL::Node gNode = *nodeIt;

		if(gNode.is_a(&RNA_NodeGroupOutput)) {
			groupOutput = BL::NodeGroupOutput(gNode);
			break;
		}
	}
	if(NOT(groupOutput)) {
		PRINT_ERROR("Node tree: %s => Active output not found!",
					groupTree.name().c_str());
		return BL::NodeSocket(PointerRNA_NULL);
	}

	// fromSocket is a socket on the Group node.
	// We have to find a corresponding socket on a GroupOutput node
	// Searching by name atm...
	//
	BL::NodeSocket toSocket(PointerRNA_NULL);
	BL::Node::inputs_iterator inIt;
	for(groupOutput.inputs.begin(inIt); inIt != groupOutput.inputs.end(); ++inIt ) {
		BL::NodeSocket sock = *inIt;
		if(sock.name().empty())
			continue;
		if(fromSocket.name() == sock.name()) {
			toSocket = sock;
			break;
		}
	}
	if(NOT(toSocket)) {
		PRINT_ERROR("Node tree: %s => Group node name: %s => Input socket not found!",
					groupTree.name().c_str(), node.name().c_str());
		return BL::NodeSocket(PointerRNA_NULL);
	}

	// Finally get the socket connected to the Group Output socket
	//
	toSocket = VRayNodeExporter::getConnectedSocket(toSocket);

	return toSocket;
}


void VRayNodeExporter::exportLinkedSocketEx(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext &context,
                                            ExpMode expMode, BL::Node &outNode, std::string &outPlugin)
{
	outNode   = BL::Node(PointerRNA_NULL);
	outPlugin = "NULL";

	BL::NodeSocket toSocket(VRayNodeExporter::getConnectedSocket(fromSocket));
	if (toSocket) {
		BL::Node toNode(toSocket.node());
		if (toNode)  {
			if (toNode.is_a(&RNA_ShaderNodeGroup) ||
			    toNode.is_a(&RNA_NodeCustomGroup))
			{
				BL::Node groupNode(toNode);

				BL::NodeTree groupTree = VRayNodeExporter::getNodeGroupTree(groupNode);
				if (!groupTree) {
					PRINT_ERROR("Node tree \"%s\", group node \"%s\": Tree not found!",
					            ntree.name().c_str(), groupNode.name().c_str());
				}
				else {
					// Setting nested context
					context.pushGroupNode(groupNode);
					context.pushParentTree(ntree);

					// Find group tree output node that we are connected to
					BL::NodeGroupOutput groupOutput(PointerRNA_NULL);
					BL::NodeTree::nodes_iterator nodeIt;
					for (groupTree.nodes.begin(nodeIt); nodeIt != groupTree.nodes.end(); ++nodeIt) {
						BL::Node n(*nodeIt);
						if (n.is_a(&RNA_NodeGroupOutput)) {
							groupOutput = BL::NodeGroupOutput(n);
							break;
						}
					}
					if (!groupOutput) {
						PRINT_ERROR("Group node \"%s\", group tree \"%s\": Output node not found!",
						            groupNode.name().c_str(), groupTree.name().c_str());
					}
					else {
						// Find the correspondend socket on a GroupOutput node:
						//   fromSocket is a socket of the connected node,
						//   but we need to find a socket corresponding to the one on a group node,
						//   which is toSocket and comprate with its name
						//
						BL::NodeSocket groupOutputInSocket(PointerRNA_NULL);
						BL::Node::inputs_iterator inSockIt;
						for(groupOutput.inputs.begin(inSockIt); inSockIt != groupOutput.inputs.end(); ++inSockIt ) {
							BL::NodeSocket sock(*inSockIt);
							if (!sock.name().empty() && sock.name() == toSocket.name()) {
								groupOutputInSocket = sock;
								break;
							}
						}
						if (!groupOutputInSocket) {
							PRINT_ERROR("Group tree \"%s\", group output \"%s\": Input socket is not found!",
							            groupTree.name().c_str(), groupOutput.name().c_str());
						}
						else {
							// Finally get node connected to the socket on a group output node
							exportLinkedSocketEx(groupTree, groupOutputInSocket, context, expMode, outNode, outPlugin);
						}
					}

					// Restoring context
					context.popParentTree();
					context.popGroupNode();
				}
			}
			else if (toNode.is_a(&RNA_NodeGroupInput)) {
				BL::Node groupInputNode(toNode);

				BL::NodeGroup groupNode  = context.getGroupNode();
				BL::NodeTree  parentTree = context.getNodeTree();
				if (!parentTree) {
					PRINT_ERROR("Group tree \"%s\", group input \"%s\": Parent tree is not found in context!",
					            ntree.name().c_str(), groupInputNode.name().c_str());
				}
				else if (!groupNode) {
					PRINT_ERROR("Group tree \"%s\", group input \"%s\": Parent group node is not found in context!",
					            ntree.name().c_str(), groupInputNode.name().c_str());
				}
				else {
					// Find socket connected to fromSocket on the Group Input node
					//
					BL::NodeSocket groupInputOutputSocket(VRayNodeExporter::getConnectedSocket(fromSocket));

					// Now have to find a correspondent socket on the Group node
					// and then export connected node from the parent tree
					//
					BL::NodeSocket groupNodeInputSocket(PointerRNA_NULL);
					BL::Node::inputs_iterator inSockIt;

					for (groupNode.inputs.begin(inSockIt); inSockIt != groupNode.inputs.end(); ++inSockIt) {
						BL::NodeSocket sock(*inSockIt);
						if (!sock.name().empty() && sock.name() == groupInputOutputSocket.name()) {
							groupNodeInputSocket = sock;
							break;
						}
					}
					if (!groupNodeInputSocket) {
						PRINT_ERROR("Group tree \"%s\", group node \"%s\": Input socket is not found!",
						            parentTree.name().c_str(), groupNode.name().c_str());
					}
					else {
						if (!groupNodeInputSocket.is_linked()) {
							outNode   = groupNode;
							outPlugin = VRayNodeExporter::exportDefaultSocket(ntree, groupNodeInputSocket);
						}
						else {
							// We are going out of group here
							BL::NodeTree  currentTree  = context.popParentTree();
							BL::NodeGroup currentGroup = context.popGroupNode();

							// Finally get the node connected to the socket on the Group node
							VRayNodeExporter::exportLinkedSocketEx(parentTree, groupNodeInputSocket, context, expMode, outNode, outPlugin);

							// We could go into the group after
							context.pushGroupNode(currentGroup);
							context.pushParentTree(currentTree);
						}
					}
				}
			}
			else if (toNode.is_a(&RNA_NodeReroute)) {
				if (toNode.internal_links.length()) {
					BL::NodeSocket rerouteInSock = toNode.internal_links[0].from_socket();
					if (rerouteInSock) {
						// NOTE: We using "rerouteInSock", because "exportLinkedSocketEx"
						// accepts fromSocket and get the connected socket itself
						VRayNodeExporter::exportLinkedSocketEx(ntree, rerouteInSock, context, expMode, outNode, outPlugin);
					}
				}
			}
			else if(toNode.bl_idname() == "VRayNodeDebugSwitch") {
				const int inputIndex = RNA_enum_get(&toNode.ptr, "input_index");
				const std::string inputSocketName = boost::str(FmtSwitch % inputIndex);

				BL::NodeSocket inputSocket = VRayNodeExporter::getSocketByName(toNode, inputSocketName);
				if (inputSocket && inputSocket.is_linked()) {
					VRayNodeExporter::exportLinkedSocketEx(ntree, inputSocket, context, expMode, outNode, outPlugin);
				}
			}
			else {
				if (expMode == ExpModePluginName) {
					outPlugin = VRayNodeExporter::getPluginName(toNode, ntree, context);
				}
				else if (expMode == ExpModePlugin) {
					outPlugin = VRayNodeExporter::exportVRayNode(ntree, toNode, fromSocket, context);
				}
				outNode = toNode;
			}

			// Adjust plugin name
			if (expMode == ExpModePluginName ||
			    expMode == ExpModePlugin)
			{
				if (RNA_struct_find_property(&toSocket.ptr, "vray_attr")) {
					const std::string &conSockAttrName = RNA_std_string_get(&toSocket.ptr, "vray_attr");
					if (!conSockAttrName.empty()) {
						if (!(conSockAttrName == "uvwgen" ||
							  conSockAttrName == "bitmap"))
						{
							outPlugin += "::" + conSockAttrName;

							if (toNode.bl_idname() == "VRayNodeTexMayaFluid") {
								boost::replace_all(outPlugin, "::out_flame",   "@Flame");
								boost::replace_all(outPlugin, "::out_density", "@Density");
								boost::replace_all(outPlugin, "::out_fuel",    "@Fuel");
							}
						}
					}
				}
			}
		}
	}
}


BL::Node VRayNodeExporter::getConnectedNode(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	BL::Node     conNode(PointerRNA_NULL);
	std::string  conPlugin;

	VRayNodeExporter::exportLinkedSocketEx(ntree, fromSocket, context, VRayNodeExporter::ExpModeNode, conNode, conPlugin);

	return conNode;
}


std::string VRayNodeExporter::exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	BL::Node     conNode(PointerRNA_NULL);
	std::string  conPlugin;

	VRayNodeExporter::exportLinkedSocketEx(ntree, fromSocket, context, VRayNodeExporter::ExpModePlugin, conNode, conPlugin);

	return conPlugin;
}


std::string VRayNodeExporter::getConnectedNodePluginName(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	BL::Node     conNode(PointerRNA_NULL);
	std::string  conPlugin;

	VRayNodeExporter::exportLinkedSocketEx(ntree, fromSocket, context, VRayNodeExporter::ExpModePluginName, conNode, conPlugin);

	return conPlugin;
}


std::string VRayNodeExporter::exportDefaultSocket(BL::NodeTree ntree, BL::NodeSocket socket)
{
	std::string socketVRayType = socket.rna_type().identifier();

	if(socketVRayType == "VRaySocketColor") {
		float color[3];
		RNA_float_get_array(&socket.ptr, "value", color);
		return BOOST_FORMAT_ACOLOR3(color);
	}
	else if(socketVRayType == "VRaySocketEnvironment") {
		float color[3];
		RNA_float_get_array(&socket.ptr, "value", color);
		return BOOST_FORMAT_COLOR(color);
	}
	else if(socketVRayType == "VRaySocketEnvironmentOverride") {
		if(RNA_boolean_get(&socket.ptr, "use")) {
			float color[3];
			RNA_float_get_array(&socket.ptr, "value", color);
			return BOOST_FORMAT_COLOR(color);
		}
	}
	else if(socketVRayType == "VRaySocketFloatColor") {
		return BOOST_FORMAT_FLOAT(RNA_float_get(&socket.ptr, "value"));
	}
	else if(socketVRayType == "VRaySocketInt") {
		return BOOST_FORMAT_INT(RNA_int_get(&socket.ptr, "value"));
	}
	else if(socketVRayType == "VRaySocketFloat") {
		return BOOST_FORMAT_FLOAT(RNA_float_get(&socket.ptr, "value"));
	}
	else if(socketVRayType == "VRaySocketVector") {
		float vector[3];
		RNA_float_get_array(&socket.ptr, "value", vector);
		return BOOST_FORMAT_VECTOR(vector);
	}
	else if(socketVRayType == "VRaySocketBRDF") {
		PRINT_ERROR("Node tree: %s => Node name: %s => Mandatory socket of type '%s' is not linked!",
					ntree.name().c_str(), socket.node().name().c_str(), socketVRayType.c_str());
	}
	// These sockets do not have default value, they must be linked or skipped otherwise.
	//
	else if(socketVRayType == "VRaySocketTransform") {}
	else if(socketVRayType == "VRaySocketFloatNoValue") {}
	else if(socketVRayType == "VRaySocketColorNoValue") {}
	else if(socketVRayType == "VRaySocketCoords") {}
	else if(socketVRayType == "VRaySocketObject") {}
	else if(socketVRayType == "VRaySocketEffect") {}
	else if(socketVRayType == "VRaySocketMtl") {}
	else {
		PRINT_ERROR("Node tree: %s => Node name: %s => Unsupported socket type: %s",
					ntree.name().c_str(), socket.node().name().c_str(), socketVRayType.c_str());
	}

	return "NULL";
}


std::string VRayNodeExporter::exportSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayNodeContext &context)
{
	int exportLinked = false;

	if (socket.is_linked()) {
		BL::Node toNode(VRayNodeExporter::getConnectedNode(ntree, socket, context));

		exportLinked = toNode && !toNode.mute();
	}

	return exportLinked
	        ? VRayNodeExporter::exportLinkedSocket(ntree, socket, context)
	        : VRayNodeExporter::exportDefaultSocket(ntree, socket);
}


std::string VRayNodeExporter::exportSocket(BL::NodeTree ntree, BL::Node node, const std::string &socketName, VRayNodeContext &context)
{
	BL::NodeSocket socket = VRayNodeExporter::getSocketByName(node, socketName);
	return VRayNodeExporter::exportSocket(ntree, socket, context);
}


void VRayNodeExporter::getVRayNodeAttributes(AttributeValueMap &pluginAttrs,
											 VRayNodeExportParam,
											 const AttributeValueMap &manualAttrs,
											 const std::string &customID,
											 const std::string &customType)
{
	const std::string &pluginType = customType.empty() ? VRayNodeExporter::getPluginType(node) : customType;
	const std::string &pluginID   = customID.empty()   ? VRayNodeExporter::getPluginID(node)   : customID;

	if(pluginID.empty()) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node plugin ID!",
					ntree.name().c_str(), node.name().c_str());
		return;
	}

	if(NOT(RNA_struct_find_property(&node.ptr, pluginID.c_str()))) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Property group not found!",
					ntree.name().c_str(), node.name().c_str());
		return;
	}

	PointerRNA propGroup = RNA_pointer_get(&node.ptr, pluginID.c_str());

	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);
	if(NOT(pluginDesc)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Node is not supported!",
					ntree.name().c_str(), node.name().c_str());
		return;
	}

	PRINT_INFO("  Node represents plugin \"%s\" [\"%s\"]",
			   pluginID.c_str(), pluginType.c_str());

	BOOST_FOREACH(PluginJson::value_type &v, pluginDesc->get_child("Parameters")) {
		std::string attrName = v.second.get_child("attr").data();
		std::string attrType = v.second.get_child("type").data();

		if(OUTPUT_TYPE(attrType))
			continue;

		// Skip attributes only if they are not manully specified
		if(SKIP_TYPE(attrType) && NOT(manualAttrs.count(attrName)))
			continue;

		// PRINT_INFO("  Processing attribute: \"%s\"", attrName.c_str());

		AttributeValueMap::const_iterator manualAttrIt = manualAttrs.find(attrName);
		if(manualAttrIt != manualAttrs.end()) {
			pluginAttrs[attrName] = manualAttrIt->second;
		}
		else {
			if(v.second.count("skip"))
				if(v.second.get<bool>("skip"))
					continue;

			if(MAPPABLE_TYPE(attrType)) {
				BL::NodeSocket sock = VRayNodeExporter::getSocketByAttr(node, attrName);
				if(sock) {
					std::string socketValue = VRayNodeExporter::exportSocket(ntree, sock, context);
					if(socketValue != "NULL") {
						if (RNA_struct_find_property(&sock.ptr, "multiplier")) {
							const float mult = RNA_float_get(&sock.ptr, "multiplier") / 100.0f;
							if (mult != 1.0f) {
								boost::format multFmt("NT%sN%sS%sMult");

								std::string multPluginName = boost::str(multFmt
								                                        % ntree.name()
								                                        % sock.node().name()
								                                        % sock.name());
								multPluginName = StripString(multPluginName);

								const bool is_float_socket = (sock.rna_type().identifier().find("Float") != std::string::npos);
								if (is_float_socket) {
									AttributeValueMap multTex;
									multTex["float_a"] = socketValue;
									multTex["float_b"] = BOOST_FORMAT_FLOAT(mult);
									multTex["mode"]    = "2"; // product

									VRayNodePluginExporter::exportPlugin("TEXTURE", "TexFloatOp", multPluginName, multTex);
								}
								else {
									AttributeValueMap multTex;
									multTex["color_a"] = socketValue;
									multTex["mult_a"]  = BOOST_FORMAT_FLOAT(mult);
									multTex["mode"]    = "0"; // result_a

									VRayNodePluginExporter::exportPlugin("TEXTURE", "TexAColorOp", multPluginName, multTex);
								}

								socketValue = multPluginName;
							}
						}

						pluginAttrs[attrName] = socketValue;
					}
					else {
						if (pluginType == "TEXTURE" && attrName == "uvwgen") {
							const std::string uvwgenName = "UVW@" + VRayNodeExporter::getPluginName(node, ntree, context);
							std::string       uvwgenType = "UVWGenObject";
							AttributeValueMap uvwgenAttrs;

							if (pluginID == "TexBitmap") {
								uvwgenType = "UVWGenChannel";
								uvwgenAttrs["uvw_channel"] = "0";
							}
							else if (pluginID == "TexSoftbox" && VRayNodeExporter::fromNodePluginType(fromSocket) == "LIGHT") {
								uvwgenType = "UVWGenChannel";
								uvwgenAttrs["uvw_channel"] = "0";
							}
							else {
								if (ExporterSettings::gSet.m_defaultMapping == ExporterSettings::eCube) {
									uvwgenType = "UVWGenProjection";
									uvwgenAttrs["type"] = "5";
									uvwgenAttrs["object_space"] = "1";
								}
								else if (ExporterSettings::gSet.m_defaultMapping == ExporterSettings::eObject) {
									uvwgenType = "UVWGenObject";
									uvwgenAttrs["uvw_transform"] = "TransformHex(\"" CGR_IDENTITY_TM  "\")";
								}
								else if (ExporterSettings::gSet.m_defaultMapping == ExporterSettings::eChannel) {
									uvwgenType = "UVWGenChannel";
									uvwgenAttrs["uvw_channel"] = "0";
								}
							}

							VRayNodePluginExporter::exportPlugin("TEXTURE", uvwgenType, uvwgenName, uvwgenAttrs);

							pluginAttrs[attrName] = uvwgenName;
						}
						else if (attrType == "TRANSFORM" ||
								 attrType == "TRANSFORM_TEXTURE") {
							pluginAttrs[attrName] = "TransformHex(\"" CGR_IDENTITY_TM  "\")";
						}
						else if (attrType == "MATRIX" ||
								 attrType == "MATRIX_TEXTURE") {
							pluginAttrs[attrName] = "Matrix(Vector(1,0,0),Vector(0,1,0),Vector(0,0,1))";
						}
					}
				}
			}
			else {
				std::string propValue = VRayNodeExporter::getValueFromPropGroup(&propGroup, (ID*)ntree.ptr.data, attrName.c_str());
				if(propValue != "NULL")
					pluginAttrs[attrName] = propValue;
			}
		}
	}

	if (pluginType == "RENDERCHANNEL" && NOT(manualAttrs.count("name"))) {
		// Value will already contain quotes
		boost::replace_all(pluginAttrs["name"], "\"", "");

		std::string chanName = pluginAttrs["name"];
		if (NOT(chanName.length())) {
			PRINT_WARN("Node tree: \"%s\" => Node: \"%s\" => Render channel name is not set! Generating default..",
					   ntree.name().c_str(), node.name().c_str());

			if (pluginID == "RenderChannelColor") {
				PointerRNA renderChannelColor = RNA_pointer_get(&node.ptr, "RenderChannelColor");
				chanName = RNA_enum_name_get(&renderChannelColor, "alias");
			}
			else if (pluginID == "RenderChannelLightSelect") {
				chanName = "Light Select";
			}
			else {
				chanName = VRayNodeExporter::getPluginName(node, ntree, context);
			}
		}

		// Export in quotes
		pluginAttrs["name"] = BOOST_FORMAT_STRING(GetUniqueChannelName(chanName));
	}
}


std::string VRayNodeExporter::exportVRayNodeAttributes(VRayNodeExportParam, const AttributeValueMap &customAttrs, const std::string &customName, const std::string &customID, const std::string &customType)
{
	const std::string &pluginName = customName.empty() ? VRayNodeExporter::getPluginName(node, ntree, context) : customName;
	const std::string &pluginType = customType.empty() ? VRayNodeExporter::getPluginType(node)                 : customType;
	const std::string &pluginID   = customID.empty()   ? VRayNodeExporter::getPluginID(node)                   : customID;

	AttributeValueMap pluginAttrs;
	VRayNodeExporter::getVRayNodeAttributes(pluginAttrs, ntree, node, fromSocket, context, customAttrs, pluginID, pluginType);

	VRayNodePluginExporter::exportPlugin(pluginType, pluginID, pluginName, pluginAttrs);

	return pluginName;
}


std::string VRayNodeExporter::exportVRayNode(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context, const AttributeValueMap &manualAttrs)
{
	const std::string &nodeClass = node.bl_idname();

	if (node.mute()) {
		return "NULL";
	}

	PRINT_INFO("Exporting \"%s\" from tree \"%s\"...",
			   node.name().c_str(), ntree.name().c_str());

	if(nodeClass == "VRayNodeBlenderOutputMaterial") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputMaterial(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeBlenderOutputGeometry") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputGeometry(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeBRDFLayered") {
		return VRayNodeExporter::exportVRayNodeBRDFLayered(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeBRDFVRayMtl") {
		return VRayNodeExporter::exportVRayNodeBRDFVRayMtl(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexLayered") {
		return VRayNodeExporter::exportVRayNodeTexLayered(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexMulti") {
		return VRayNodeExporter::exportVRayNodeTexMulti(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeSelectObject") {
		BL::Object b_ob = VRayNodeExporter::exportVRayNodeSelectObject(ntree, node, fromSocket, context);
		if(NOT(b_ob))
			return "NULL";
		return GetIDName(b_ob);
	}
	else if(nodeClass == "VRayNodeSelectGroup") {
		BL::Group b_gr = VRayNodeExporter::exportVRayNodeSelectGroup(ntree, node, fromSocket, context);
		return VRayNodeExporter::getObjectNameList(b_gr);
	}
	else if(nodeClass == "VRayNodeLightMesh") {
		return VRayNodeExporter::exportVRayNodeLightMesh(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeGeomDisplacedMesh") {
		return VRayNodeExporter::exportVRayNodeGeomDisplacedMesh(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeGeomStaticSmoothedMesh") {
		return VRayNodeExporter::exportVRayNodeGeomStaticSmoothedMesh(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeBitmapBuffer") {
		return VRayNodeExporter::exportVRayNodeBitmapBuffer(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexGradRamp") {
		return VRayNodeExporter::exportVRayNodeTexGradRamp(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexRemap") {
		return VRayNodeExporter::exportVRayNodeTexRemap(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexSoftbox") {
		return VRayNodeExporter::exportVRayNodeTexSoftbox(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexSky") {
		return VRayNodeExporter::exportVRayNodeTexSky(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexFalloff") {
		return VRayNodeExporter::exportVRayNodeTexFalloff(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexEdges") {
		return VRayNodeExporter::exportVRayNodeTexEdges(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexVoxelData") {
		return VRayNodeExporter::exportVRayNodeTexVoxelData(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeTexMayaFluid") {
		return VRayNodeExporter::exportVRayNodeTexMayaFluid(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeMtlMulti") {
		return VRayNodeExporter::exportVRayNodeMtlMulti(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeOutputMaterial") {
		std::string maName = "NULL";
		BL::NodeSocket materialInSock = VRayNodeExporter::getSocketByName(node, "Material");
		if (materialInSock && materialInSock.is_linked()) {
			BL::Node conNode = VRayNodeExporter::getConnectedNode(ntree, materialInSock, context);
			if (conNode) {
				maName = VRayNodeExporter::exportLinkedSocket(ntree, materialInSock, context);

				// If connected node is not of 'MATERIAL' type we need to wrap it with it for GPU
				const std::string &nodePluginType = VRayNodeExporter::getPluginType(conNode);
				if (!nodePluginType.empty() && nodePluginType != "MATERIAL") {
					AttributeValueMap mtlSingleBRDF;
					mtlSingleBRDF["brdf"] = maName;

					maName = "MtlSingleBRDF@" + maName;

					VRayNodePluginExporter::exportPlugin("MATERIAL", "MtlSingleBRDF", maName, mtlSingleBRDF);
				}
			}
		}
		return maName;
	}
	else if(nodeClass == "VRayNodeOutputTexture") {
		BL::NodeSocket textureInSock = VRayNodeExporter::getSocketByName(node, "Texture");
		if(textureInSock.is_linked())
			return VRayNodeExporter::exportLinkedSocket(ntree, textureInSock, context);
		else
			return "NULL";
	}
	else if(nodeClass == "VRayNodeTransform") {
		return VRayNodeExporter::exportVRayNodeTransform(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeMatrix") {
		return VRayNodeExporter::exportVRayNodeMatrix(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeVector") {
		return VRayNodeExporter::exportVRayNodeVector(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeEnvFogMeshGizmo") {
		return VRayNodeExporter::exportVRayNodeEnvFogMeshGizmo(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeEnvironmentFog") {
		return VRayNodeExporter::exportVRayNodeEnvironmentFog(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeUVWGenEnvironment") {
		return VRayNodeExporter::exportVRayNodeUVWGenEnvironment(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeUVWGenMayaPlace2dTexture") {
		return VRayNodeExporter::exportVRayNodeUVWGenMayaPlace2dTexture(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeUVWGenChannel") {
		return VRayNodeExporter::exportVRayNodeUVWGenChannel(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeRenderChannelLightSelect") {
		return VRayNodeExporter::exportVRayNodeRenderChannelLightSelect(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeRenderChannelColor") {
		return VRayNodeExporter::exportVRayNodeRenderChannelColor(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodePhxShaderSimVol") {
		return VRayNodeExporter::exportVRayNodePhxShaderSimVol(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodePhxShaderSim") {
		return VRayNodeExporter::exportVRayNodePhxShaderSim(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeSphereFadeGizmo") {
		return VRayNodeExporter::exportVRayNodeSphereFadeGizmo(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeSphereFade") {
		return VRayNodeExporter::exportVRayNodeSphereFade(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeVolumeVRayToon") {
		return VRayNodeExporter::exportVRayNodeVolumeVRayToon(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeMetaImageTexture") {
		return VRayNodeExporter::exportVRayNodeMetaImageTexture(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeMetaStandardMaterial") {
		return VRayNodeExporter::exportVRayNodeMetaStandardMaterial(ntree, node, fromSocket, context);
	}
	else if(node.is_a(&RNA_ShaderNodeNormal)) {
		return VRayNodeExporter::exportBlenderNodeNormal(ntree, node, fromSocket, context);
	}

	return exportVRayNodeAttributes(ntree, node, fromSocket, context, manualAttrs);
}


static const std::string GetAttrType(PluginJson *pluginDesc, const std::string &attributeName)
{
	if(pluginDesc) {
		BOOST_FOREACH(PluginJson::value_type &v, pluginDesc->get_child("Parameters")) {
			const std::string attrName = v.second.get_child("attr").data();
			const std::string attrType = v.second.get_child("type").data();
			if(attrName == attributeName)
				return attrType;
		}
	}

	return "NULL";
}


int VRayNodePluginExporter::exportPlugin(const std::string &pluginType, const std::string &pluginID, const std::string &pluginName, const AttributeValueMap &pluginAttrs)
{
	// Check names cache not to export duplicated data for this frame
	//
	if (ExporterSettings::gSet.m_anim_check_cache) {
		if(m_namesCache.find(pluginName) != m_namesCache.end())
			return 1;
		m_namesCache.insert(pluginName);
	}

	bool pluginIsInCache = ExporterSettings::gSet.m_isAnimation ? m_nodeCache.pluginInCache(pluginName) : false;

	std::stringstream outAttributes;
	std::stringstream outPlugin;

	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);

	AttributeValueMap::const_iterator attrIt;
	for(attrIt = pluginAttrs.begin(); attrIt != pluginAttrs.end(); ++attrIt) {
		const std::string attrName  = attrIt->first;
		const std::string attrValue = attrIt->second;
		const std::string attrType  = GetAttrType(pluginDesc, attrName);

		if(NOT(ExporterSettings::gSet.m_isAnimation)) {
			outAttributes << "\n\t" << attrName << "=" << attrValue << ";";
		}
		else {
			MHash attrHash = HashCode(attrValue.c_str());

			const float currentFrame = ExporterSettings::gSet.m_frameCurrent;
			const float prevFrame    = currentFrame - ExporterSettings::gSet.m_frameStep;

			const int attrNonAnim = NOT_ANIMATABLE_TYPE(attrType);

			// Check if plugin is in cache
			// If plugin is in cache check the stored attribute value
			// and decide whether to export the keyframe
			//
			if(NOT(pluginIsInCache)) {
				if(attrNonAnim) {
					outAttributes << "\n\t" << attrName << "=" << attrValue << ";";
				}
				else {
					outAttributes << "\n\t" << attrName << "=interpolate((" << currentFrame << "," << attrValue << "));";
				}
			}
			else {
				if(NOT(attrNonAnim)) {
					MHash cachedHash = m_nodeCache.getCachedHash(pluginName, attrName);

					if(cachedHash == attrHash)
						continue;

					const float       cachedFrame = m_nodeCache.getCachedFrame(pluginName, attrName);
					const std::string cachedValue = m_nodeCache.getCachedValue(pluginName, attrName);

					outAttributes << "\n\t" << attrName << "=interpolate(";
					if(cachedFrame < prevFrame) {
						// Cached value is more then 'frameStep' before the current frame
						// need to insert keyframe
						outAttributes << "(" << prevFrame    << "," << cachedValue << "),";
						outAttributes << "(" << currentFrame << "," << attrValue   << ")";
					}
					else {
						// It's simply the next frame - export as usual
						outAttributes << "(" << currentFrame << "," << attrValue << ")";
					}
					outAttributes << ");";
				}
			}

			// Store/update value in cache
			m_nodeCache.addToCache(pluginName, attrName, currentFrame, attrValue, attrHash);
		}
	}

	if(outAttributes.str().empty()) {
		// We have some plugins without input attributes
		// Export them only for the first frame
		if(ExporterSettings::gSet.DoUpdateCheck())
			return 0;
	}

	outPlugin << "\n" << pluginID << " " << pluginName << " {";
	outPlugin << outAttributes.str();
	outPlugin << "\n}\n";

	PyObject *output = ExporterSettings::gSet.m_fileObject;

	if(pluginType == "TEXTURE" || pluginType == "UVWGEN") {
		output = ExporterSettings::gSet.m_fileTex;
	}
	else if(pluginType == "MATERIAL" || pluginType == "BRDF") {
		output = ExporterSettings::gSet.m_fileMat;
	}
	else if(pluginType == "EFFECT" || pluginType == "ENVIRONMENT") {
		output = ExporterSettings::gSet.m_fileEnv;
	}
	else if(pluginType == "RENDERCHANNEL") {
		output = ExporterSettings::gSet.m_fileMain;
	}
	else if(pluginType == "LIGHT") {
		output = ExporterSettings::gSet.m_fileLights;
	}
	else if(pluginType == "GEOMETRY") {
		if (  pluginID == "GeomStaticMesh"
		   || pluginID == "GeomMayaHair"
		   )
		{
			output = ExporterSettings::gSet.m_fileGeom;
		}
		else {
			// Store dynamic geometry plugins in 'Node' file
			output = ExporterSettings::gSet.m_fileObject;
		}
	}

	PYTHON_PRINT(output, outPlugin.str().c_str());

	return 0;
}


void VRayNodePluginExporter::clearNamesCache()
{
	VRayNodePluginExporter::m_namesCache.clear();
	VRayNodeExporter::RenderChannelNames.clear();
}


void VRayNodePluginExporter::clearNodesCache()
{
	VRayNodePluginExporter::m_nodeCache.clearCache();
}


std::string VRayNodeExporter::exportMaterial(BL::BlendData b_data, BL::Material b_ma)
{
	std::string maName = "NULL";

	BL::NodeTree b_ma_ntree = VRayNodeExporter::getNodeTree(b_data, (ID*)b_ma.ptr.data);
	if(b_ma_ntree) {
		BL::Node b_ma_output = VRayNodeExporter::getNodeByType(b_ma_ntree, "VRayNodeOutputMaterial");
		if(b_ma_output) {
			VRayNodeContext ctx;
			maName = VRayNodeExporter::exportVRayNode(b_ma_ntree, b_ma_output, PointerRNA_NULL, ctx);
		}
	}
	else {
		maName = GetIDName((ID*)b_ma.ptr.data);

		AttributeValueMap maAttrs;
		maAttrs["brdf"] = CGR_DEFAULT_BRDF;

		VRayNodePluginExporter::exportPlugin("MATERIAL", "MtlSingleBRDF", maName, maAttrs);
	}

	if(maName == "NULL") {
		PRINT_ERROR("Failed to export material: '%s'", b_ma.name().c_str());
	}

	return maName;
}


void VRayNodeExporter::getUserAttributes(PointerRNA *ptr, StrVector &user_attributes)
{
	RNA_BEGIN(ptr, itemptr, "user_attributes")
	{
		bool useAttr = RNA_boolean_get(&itemptr, "use");
		if (NOT(useAttr))
			continue;

		char buf[MAX_ID_NAME];
		RNA_string_get(&itemptr, "name", buf);

		std::string attrName  = buf;
		std::string attrValue = "0";

		int attrType = RNA_enum_get(&itemptr, "value_type");
		switch (attrType) {
			case 0:
				attrValue = BOOST_FORMAT_INT(RNA_int_get(&itemptr, "value_int"));
				break;
			case 1:
				attrValue = BOOST_FORMAT_FLOAT(RNA_float_get(&itemptr, "value_float"));
				break;
			case 2:
				float color[3];
				RNA_float_get_array(&itemptr, "value_color", color);
				attrValue = BOOST_FORMAT_COLOR(color);
				break;
			case 3:
				RNA_string_get(&itemptr, "value_string", buf);
				attrValue = buf;
				break;
			default:
				break;
		}

		std::string userAttr = attrName + "=" + attrValue;
		user_attributes.push_back(userAttr);
	}
	RNA_END;
}


int VRayNodeExporter::isObjectVisible(BL::Object b_ob)
{
	Object *ob = (Object*)b_ob.ptr.data;
	if (!ob)
		return false;
	if(ob->restrictflag & OB_RESTRICT_RENDER)
		return false;
	if(NOT(ob->lay & ExporterSettings::gSet.m_activeLayers))
		return false;
	return true;
}


std::string VRayNodeExporter::fromNodePluginID(BL::NodeSocket fromSocket)
{
	BL::Node fromNode = fromSocket.node();
	if (fromNode) {
		return VRayNodeExporter::getPluginID(fromNode);
	}
	return "";
}


std::string VRayNodeExporter::fromNodePluginType(BL::NodeSocket fromSocket)
{
	BL::Node fromNode = fromSocket.node();
	if (fromNode) {
		return VRayNodeExporter::getPluginType(fromNode);
	}
	return "";
}
