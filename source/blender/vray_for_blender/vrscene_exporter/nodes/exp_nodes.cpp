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


ExpoterSettings *VRayNodeExporter::m_set = NULL;

VRayNodeCache    VRayNodePluginExporter::m_nodeCache;
StrSet           VRayNodePluginExporter::m_namesCache;


std::string VRayNodeExporter::getPluginName(BL::Node node, BL::NodeTree ntree, VRayNodeContext *context)
{
	std::string pluginName = "NT" + ntree.name() + "N" + node.name();

	if(context) {
		BL::NodeTree  parent = context->getNodeTree();
		BL::NodeGroup group  = context->getGroupNode();
		if(parent)
			pluginName += "NP" + parent.name();
		if(group)
			pluginName += "GR" + group.name();
	}

	return StripString(pluginName);
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

		if(mappable && MAPPABLE_TYPE(attrType)) {
			attrSet.insert(attrName);
		}
		else {
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
		return BOOST_FORMAT_INT(RNA_enum_get(propGroup, attrName.c_str()));
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


BL::Node VRayNodeExporter::getConnectedNode(BL::NodeSocket socket)
{
	bNodeSocket *bSocket = (bNodeSocket*)socket.ptr.data;
	bNodeLink   *link = bSocket->link;
	if(link) {
		PointerRNA nodePtr;
		RNA_pointer_create((ID*)socket.ptr.id.data, &RNA_Node, link->fromnode, &nodePtr);
		return BL::Node(nodePtr);
	}
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


BL::Node VRayNodeExporter::getConnectedNode(BL::Node node, const std::string &socketName)
{
	BL::NodeSocket socket = VRayNodeExporter::getSocketByName(node, socketName);
	if(NOT(socket.is_linked()))
		return BL::Node(PointerRNA_NULL);
	return VRayNodeExporter::getConnectedNode(socket);
}


BL::NodeTree VRayNodeExporter::getNodeTree(BL::BlendData b_data, ID *id)
{
	RnaAccess::RnaValue VRayObject(id, "vray");

	PointerRNA  *ptr  = VRayObject.getPtr();
	PropertyRNA *prop = RNA_struct_find_property(ptr, "ntree");
	if(prop) {
		PropertyType propType = RNA_property_type(prop);
		if(propType == PROP_POINTER) {
			PointerRNA ntree = RNA_pointer_get(ptr, "ntree");
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


// NOTE: The same as VRayNodeExporter::exportLinkedSocket,
// but returns only the plugin name without any export
//
std::string VRayNodeExporter::getConnectedNodePluginName(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	std::string pluginName = "NULL";

	BL::NodeSocket toSocket = VRayNodeExporter::getConnectedSocket(fromSocket);
	// NOTE: This could happen while reconnecting nodes and material preview is active
	if(NOT(toSocket.ptr.data))
		return "NULL";

	BL::Node toNode = VRayNodeExporter::getConnectedNode(fromSocket);
	// NOTE: This could happen while reconnecting nodes and material preview is active
	if(NOT(toNode.ptr.data))
		return "NULL";

	// If we are connected to the Group node,
	// we have to found the actual socket/node from the group node tree
	//
	if(toNode.is_a(&RNA_ShaderNodeGroup) || toNode.is_a(&RNA_NodeCustomGroup)) {
		// Get group tree
		BL::NodeTree groupTree = VRayNodeExporter::getNodeGroupTree(toNode);

		// Setting nested context
		if(context) {
			context->pushGroupNode(toNode.ptr);
			context->pushParentTree(ntree);
		}

		// Get real socket / node to export
		toSocket = VRayNodeExporter::getNodeGroupSocketReal(toNode, toSocket);
		// NOTE: This could happen while reconnecting nodes and material preview is active
		if(NOT(toSocket.ptr.data))
			return "NULL";

		toNode = toSocket.node();

		pluginName = VRayNodeExporter::getPluginName(toNode, groupTree, context);

		// Restoring context
		if(context) {
			context->popParentTree();
			context->popGroupNode();
		}
	}
	else if(toNode.is_a(&RNA_NodeGroupInput)) {
		if(NOT(context)) {
			PRINT_ERROR("No context for NodeGroupInput!");
			return "NULL";
		}
		else {
			BL::NodeGroup groupNode  = context->getGroupNode();
			BL::NodeTree  parentTree = context->getNodeTree();
			if(NOT(groupNode && parentTree)) {
				PRINT_ERROR("Node tree: %s => No group node and / or tree in context!",
							ntree.name().c_str());
				return "NULL";
			}

			// Find socket connected to fromSocket on the Group Input node
			BL::NodeSocket inputNodeSocket = VRayNodeExporter::getConnectedSocket(fromSocket);

			// Now have to find a correspondent socket on the Group node
			// and then export connected node from the parent tree
			//
			BL::NodeSocket groupInputSocket(PointerRNA_NULL);
			BL::Node::inputs_iterator inIt;
			for(groupNode.inputs.begin(inIt); inIt != groupNode.inputs.end(); ++inIt ) {
				BL::NodeSocket sock = *inIt;
				if(sock.name().empty())
					continue;
				if(inputNodeSocket.name() == sock.name()) {
					groupInputSocket = sock;
					break;
				}
			}
			if(NOT(groupInputSocket)) {
				PRINT_ERROR("Node tree: %s => Group node name: %s => Input socket not found!",
							ntree.name().c_str(), groupNode.name().c_str());
				return "NULL";
			}

			// Forward the real socket
			toSocket = VRayNodeExporter::getConnectedSocket(groupInputSocket);
			if(NOT(toSocket.ptr.data))
				return "NULL";

			// Finally get the node connected to the socket on the Group node
			toNode = VRayNodeExporter::getConnectedNode(groupInputSocket);
			if(NOT(toNode)) {
				PRINT_ERROR("Node tree: %s => Node name: %s => Connected node is not found!",
							ntree.name().c_str(), groupNode.name().c_str());
				return "NULL";
			}

			// We are going out of group here
			BL::NodeTree  currentTree  = context->popParentTree();
			BL::NodeGroup currentGroup = context->popGroupNode();

			pluginName = VRayNodeExporter::getPluginName(toNode, parentTree, context);

			// We could go into the group after
			context->pushGroupNode(currentGroup);
			context->pushParentTree(currentTree);
		}
	}
	else if(toNode.is_a(&RNA_NodeReroute)) {
		if(toNode.internal_links.length()) {
			BL::NodeSocket rerouteInSock = toNode.internal_links[0].from_socket();
			if(rerouteInSock) {
				toSocket = VRayNodeExporter::getConnectedSocket(rerouteInSock);
				if(NOT(toSocket.ptr.data))
					return "NULL";
				toNode = VRayNodeExporter::getConnectedNode(rerouteInSock);
				if(toNode) {
					pluginName = VRayNodeExporter::getPluginName(toNode, ntree, context);
				}
			}
		}
	}
	else {
		pluginName = VRayNodeExporter::getPluginName(toNode, ntree, context);
	}

	std::string conSockAttrName;
	if(RNA_struct_find_property(&toSocket.ptr, "vray_attr")) {
		char rnaStringBuf[CGR_MAX_PLUGIN_NAME];
		RNA_string_get(&toSocket.ptr, "vray_attr", rnaStringBuf);
		conSockAttrName = rnaStringBuf;
	}

	if(NOT(conSockAttrName.empty())) {
		if(NOT(conSockAttrName == "uvwgen" || conSockAttrName == "bitmap")) {
			pluginName += "::" + conSockAttrName;
		}
	}

	return pluginName;
}


std::string VRayNodeExporter::exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	std::string connectedPlugin = "NULL";

	BL::NodeSocket toSocket = VRayNodeExporter::getConnectedSocket(fromSocket);
	// NOTE: This could happen while reconnecting nodes and material preview is active
	if(NOT(toSocket.ptr.data))
		return "NULL";

	BL::Node toNode = VRayNodeExporter::getConnectedNode(fromSocket);
	// NOTE: This could happen while reconnecting nodes and material preview is active
	if(NOT(toNode.ptr.data))
		return "NULL";

	// If we are connected to the Group node,
	// we have to found the actual socket/node from the group node tree
	//
	if(toNode.is_a(&RNA_ShaderNodeGroup) || toNode.is_a(&RNA_NodeCustomGroup)) {
		// Get group tree
		BL::NodeTree groupTree = VRayNodeExporter::getNodeGroupTree(toNode);

		// Setting nested context
		if(context) {
			context->pushGroupNode(toNode.ptr);
			context->pushParentTree(ntree);
		}

		// Get real socket / node to export
		toSocket = VRayNodeExporter::getNodeGroupSocketReal(toNode, toSocket);
		// NOTE: This could happen while reconnecting nodes and material preview is active
		if(NOT(toSocket.ptr.data))
			return "NULL";

		toNode = toSocket.node();

		connectedPlugin = VRayNodeExporter::exportVRayNode(groupTree, toNode, fromSocket, context);

		// Restoring context
		if(context) {
			context->popParentTree();
			context->popGroupNode();
		}
	}
	else if(toNode.is_a(&RNA_NodeGroupInput)) {
		if(NOT(context)) {
			PRINT_ERROR("No context for NodeGroupInput!");
			return "NULL";
		}
		else {
			BL::NodeGroup groupNode  = context->getGroupNode();
			BL::NodeTree  parentTree = context->getNodeTree();
			if(NOT(groupNode && parentTree)) {
				PRINT_ERROR("Node tree: %s => No group node and / or tree in context!",
							ntree.name().c_str());
				return "NULL";
			}

			// Find socket connected to fromSocket on the Group Input node
			BL::NodeSocket inputNodeSocket = VRayNodeExporter::getConnectedSocket(fromSocket);

			// Now have to find a correspondent socket on the Group node
			// and then export connected node from the parent tree
			//
			BL::NodeSocket groupInputSocket(PointerRNA_NULL);
			BL::Node::inputs_iterator inIt;
			for(groupNode.inputs.begin(inIt); inIt != groupNode.inputs.end(); ++inIt ) {
				BL::NodeSocket sock = *inIt;
				if(sock.name().empty())
					continue;
				if(inputNodeSocket.name() == sock.name()) {
					groupInputSocket = sock;
					break;
				}
			}
			if(NOT(groupInputSocket)) {
				PRINT_ERROR("Node tree: %s => Group node name: %s => Input socket not found!",
							ntree.name().c_str(), groupNode.name().c_str());
				return "NULL";
			}

			// Forward the real socket
			toSocket = VRayNodeExporter::getConnectedSocket(groupInputSocket);
			if(NOT(toSocket.ptr.data))
				return "NULL";

			// Finally get the node connected to the socket on the Group node
			toNode = VRayNodeExporter::getConnectedNode(groupInputSocket);
			if(NOT(toNode)) {
				PRINT_ERROR("Node tree: %s => Node name: %s => Connected node is not found!",
							ntree.name().c_str(), groupNode.name().c_str());
				return "NULL";
			}

			// We are going out of group here
			BL::NodeTree  currentTree  = context->popParentTree();
			BL::NodeGroup currentGroup = context->popGroupNode();

			connectedPlugin = VRayNodeExporter::exportVRayNode(parentTree, toNode, fromSocket, context);

			// We could go into the group after
			context->pushGroupNode(currentGroup);
			context->pushParentTree(currentTree);
		}
	}
	else if(toNode.is_a(&RNA_NodeReroute)) {
		if(toNode.internal_links.length()) {
			BL::NodeSocket rerouteInSock = toNode.internal_links[0].from_socket();
			if(rerouteInSock) {
				toSocket = VRayNodeExporter::getConnectedSocket(rerouteInSock);
				if(NOT(toSocket.ptr.data))
					return "NULL";
				toNode = VRayNodeExporter::getConnectedNode(rerouteInSock);
				if(toNode) {
					connectedPlugin = VRayNodeExporter::exportVRayNode(ntree, toNode, fromSocket, context);
				}
			}
		}
	}
	else if(toNode.bl_idname() == "VRayNodeDebugSwitch") {
		const int inputIndex = RNA_enum_get(&toNode.ptr, "input_index");
		const std::string inputSocketName = boost::str(boost::format("Input %i") % inputIndex);

		BL::NodeSocket inputSocket = VRayNodeExporter::getSocketByName(toNode, inputSocketName);
		if(NOT(inputSocket && inputSocket.is_linked()))
			return "NULL";

		connectedPlugin = VRayNodeExporter::exportLinkedSocket(ntree, inputSocket, context);
	}
	else {
		connectedPlugin = VRayNodeExporter::exportVRayNode(ntree, toNode, fromSocket, context);
	}

	std::string conSockAttrName;
	if(RNA_struct_find_property(&toSocket.ptr, "vray_attr")) {
		char rnaStringBuf[CGR_MAX_PLUGIN_NAME];
		RNA_string_get(&toSocket.ptr, "vray_attr", rnaStringBuf);
		conSockAttrName = rnaStringBuf;
	}

	if(NOT(conSockAttrName.empty())) {
		if(NOT(conSockAttrName == "uvwgen" || conSockAttrName == "bitmap")) {
			connectedPlugin += "::" + conSockAttrName;
		}
	}

	return connectedPlugin;
}


std::string VRayNodeExporter::exportDefaultSocket(BL::NodeTree ntree, BL::NodeSocket socket)
{
	std::string socketVRayType = socket.rna_type().identifier();

	if(socketVRayType == "VRaySocketColor") {
		float color[3];
		RNA_float_get_array(&socket.ptr, "value", color);
		return BOOST_FORMAT_ACOLOR3(color);
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
	else {
		PRINT_ERROR("Node tree: %s => Node name: %s => Unsupported socket type: %s",
					ntree.name().c_str(), socket.node().name().c_str(), socketVRayType.c_str());
	}

	return "NULL";
}


std::string VRayNodeExporter::exportSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayNodeContext *context)
{
	if(socket.is_linked())
		return VRayNodeExporter::exportLinkedSocket(ntree, socket, context);

	return VRayNodeExporter::exportDefaultSocket(ntree, socket);
}


std::string VRayNodeExporter::exportSocket(BL::NodeTree ntree, BL::Node node, const std::string &socketName, VRayNodeContext *context)
{
	BL::NodeSocket socket = VRayNodeExporter::getSocketByName(node, socketName);
	return VRayNodeExporter::exportSocket(ntree, socket, context);
}


std::string VRayNodeExporter::exportVRayNodeAttributes(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context, const AttributeValueMap &manualAttrs)
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

	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);
	if(NOT(pluginDesc)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Node is not supported!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	PRINT_INFO("  Node represents plugin \"%s\" [\"%s\"]",
			   pluginID.c_str(), pluginType.c_str());

	std::string        pluginName = VRayNodeExporter::getPluginName(node, ntree, context);
	AttributeValueMap  pluginAttrs;

	// VRayNodeContext nodeContext(ntree, node, fromSocket, context);
	// VRayNodeExporter::getAttrsFromPropGroupAndNode(pluginAttrs, pluginID, propGroup, manualAttrs, nodeContext);

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
					if(socketValue != "NULL")
						pluginAttrs[attrName] = socketValue;
				}
			}
			else {
				std::string propValue = VRayNodeExporter::getValueFromPropGroup(&propGroup, (ID*)ntree.ptr.data, attrName.c_str());
				if(propValue != "NULL")
					pluginAttrs[attrName] = propValue;
			}
		}
	}

	VRayNodePluginExporter::exportPlugin(pluginType, pluginID, pluginName, pluginAttrs);

	return pluginName;
}


std::string VRayNodeExporter::exportVRayNode(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context, const AttributeValueMap &manualAttrs)
{
	std::string nodeClass = node.bl_idname();

	PRINT_INFO("Exporting \"%s\" from \"%s\"...",
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
		return GetIDName((ID*)b_ob.ptr.data);
	}
	else if(nodeClass == "VRayNodeSelectGroup") {
		BL::Group b_gr = VRayNodeExporter::exportVRayNodeSelectGroup(ntree, node, fromSocket, context);
		if(NOT(b_gr))
			return "List()";

		StrVector obNames;

		BL::Group::objects_iterator obIt;
		for(b_gr.objects.begin(obIt); obIt != b_gr.objects.end(); ++obIt) {
			BL::Object b_ob = *obIt;

			obNames.push_back(GetIDName((ID*)b_ob.ptr.data));
		}

		return BOOST_FORMAT_LIST(obNames);
	}
	else if(nodeClass == "VRayNodeLightMesh") {
		return VRayNodeExporter::exportVRayNodeLightMesh(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeGeomDisplacedMesh") {
		return VRayNodeExporter::exportVRayNodeGeomDisplacedMesh(ntree, node, fromSocket, context);
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
	else if(nodeClass == "VRayNodeMtlMulti") {
		return VRayNodeExporter::exportVRayNodeMtlMulti(ntree, node, fromSocket, context);
	}
	else if(nodeClass == "VRayNodeOutputMaterial") {
		BL::NodeSocket materialInSock = VRayNodeExporter::getSocketByName(node, "Material");
		if(materialInSock.is_linked())
			return VRayNodeExporter::exportLinkedSocket(ntree, materialInSock, context);
		else
			return "NULL";
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
	if(m_namesCache.find(pluginName) != m_namesCache.end())
		return 1;

	m_namesCache.insert(pluginName);

	bool pluginIsInCache = VRayExportable::m_set->m_isAnimation ? m_nodeCache.pluginInCache(pluginName) : false;

	std::stringstream outAttributes;
	std::stringstream outPlugin;

	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);

	AttributeValueMap::const_iterator attrIt;
	for(attrIt = pluginAttrs.begin(); attrIt != pluginAttrs.end(); ++attrIt) {
		const std::string attrName  = attrIt->first;
		const std::string attrValue = attrIt->second;
		const std::string attrType  = GetAttrType(pluginDesc, attrName);

		if(NOT(VRayExportable::m_set->m_isAnimation)) {
			outAttributes << "\n\t" << attrName << "=" << attrValue << ";";
		}
		else {
			MHash attrHash = HashCode(attrValue.c_str());

			const int currentFrame = VRayExportable::m_set->m_frameCurrent;
			const int prevFrame    = currentFrame - VRayExportable::m_set->m_frameStep;

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

					const int         cachedFrame = m_nodeCache.getCachedFrame(pluginName, attrName);
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
		if(VRayNodeExporter::m_set->DoUpdateCheck())
			return 0;
	}

	outPlugin << "\n" << pluginID << " " << pluginName << " {";
	outPlugin << outAttributes.str();
	outPlugin << "\n}\n";

	PyObject *output = VRayNodeExporter::m_set->m_fileObject;

	if(pluginType == "TEXTURE" || pluginType == "UVWGEN") {
		output = VRayNodeExporter::m_set->m_fileTex;
	}
	else if(pluginType == "MATERIAL" || pluginType == "BRDF") {
		output = VRayNodeExporter::m_set->m_fileMat;
	}
	else if(pluginType == "LIGHT") {
		output = VRayNodeExporter::m_set->m_fileLights;
	}
	else if(pluginType == "GEOMETRY") {
		if(pluginID == "GeomDisplacedMesh"      ||
		   pluginID == "GeomStaticSmoothedMesh" ||
		   pluginID == "GeomPlane")
		{
			// Store dynamic geometry plugins in 'Node' file
			output = VRayNodeExporter::m_set->m_fileObject;
		}
		else {
			output = VRayNodeExporter::m_set->m_fileGeom;
		}
	}

	PYTHON_PRINT(output, outPlugin.str().c_str());

	return 0;
}


void VRayNodePluginExporter::clearNamesCache()
{
	VRayNodePluginExporter::m_namesCache.clear();
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
			maName = VRayNodeExporter::exportVRayNode(b_ma_ntree, b_ma_output, PointerRNA_NULL, &ctx);
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
