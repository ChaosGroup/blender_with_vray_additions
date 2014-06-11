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


std::string VRayNodeExporter::exportBlenderNodeNormal(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	BL::NodeSocket socket = VRayNodeExporter::getOutputSocketByName(node, "Normal");

	float vector[3];
	RNA_float_get_array(&socket.ptr, "default_value", vector);

	return BOOST_FORMAT_VECTOR(vector);
}


std::string VRayNodeExporter::exportBlenderNodeGroupInput(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	if(NOT(context)) {
		PRINT_ERROR("No context for NodeGroupInput!");
		return "NULL";
	}

	BL::NodeGroup groupNode  = context->getGroupNode();
	BL::NodeTree  parentTree = context->getNodeTree();
	if(NOT(groupNode && parentTree)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => No group node and / or tree in context!",
					ntree.name().c_str(), node.name().c_str());
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
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	// Finally get the node connected to the socket on the Group node
	BL::Node conNode = VRayNodeExporter::getConnectedNode(groupInputSocket);
	if(NOT(conNode)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Connected node is not found!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	// We are going out of group here
	BL::NodeTree  currentTree  = context->popParentTree();
	BL::NodeGroup currentGroup = context->popGroupNode();
	
	std::string pluginName = VRayNodeExporter::exportVRayNode(parentTree, conNode, fromSocket, context);
	
	// We could go into the group after
	context->pushGroupNode(currentGroup);
	context->pushParentTree(currentTree);
	
	return pluginName;
}


std::string VRayNodeExporter::exportBlenderNodeReroute(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
//	BL::NodeGroup::internal_links_iterator inLinkIt;
//	for(node.internal_links.begin(inLinkIt); inLinkIt != node.internal_links.end(); ++inLinkIt) {
//		if(toSocket.ptr.data == inLinkIt->to_socket().ptr.data) {
//			PRINT_INFO("Socket: %s", inLinkIt->from_socket().node().name().c_str());
//			BL::Node conNode = VRayNodeExporter::getConnectedNode(groupTree, toSocket);
//			if(conNode) {
//				PRINT_INFO("Node: %s", conNode.name().c_str());
//			}
//		}
//	}
	return "NULL";
}
