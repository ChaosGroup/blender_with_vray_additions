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


std::string VRayNodeExporter::exportBlenderNodeNormal(BL::NodeTree ntree, BL::Node node)
{
	BL::NodeSocket socket = VRayNodeExporter::getOutputSocketByName(node, "Normal");

	float vector[3];
	RNA_float_get_array(&socket.ptr, "default_value", vector);

	return BOOST_FORMAT_VECTOR(vector);
}


std::string VRayNodeExporter::exportBlenderNodeGroup(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket)
{
	if(NOT(fromSocket)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect socket!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	BL::NodeTree groupTree(PointerRNA_NULL);

	if(node.is_a(&RNA_ShaderNodeGroup)) {
		BL::NodeGroup groupNode(node);
		groupTree = groupNode.node_tree();
	}
	else {
		BL::NodeCustomGroup groupNode(node);
		groupTree = groupNode.node_tree();
	}

	if(NOT(groupTree)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node group tree!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	// Socket on a Group node
	BL::NodeSocket toSocket = VRayNodeExporter::getConnectedSocket(ntree, fromSocket);
	if(NOT(toSocket)) {
		PRINT_ERROR("Node tree: %s => Node name: Socket is not connected!",
					ntree.name().c_str());
		return "NULL";
	}

	BL::NodeGroupOutput groupOutput(PointerRNA_NULL);
	
	BL::NodeTree::nodes_iterator nodeIt;
	for(groupTree.nodes.begin(nodeIt); nodeIt != groupTree.nodes.end(); ++nodeIt) {
		BL::Node gNode = *nodeIt;

		if(gNode.is_a(&RNA_NodeGroupOutput)) {
			BL::NodeGroupOutput gOutput(gNode);
			if(gOutput.is_active_output()) {
				groupOutput = gOutput;
				break;
			}
		}
	}

	if(NOT(groupOutput)) {
		PRINT_ERROR("Node tree: %s => Active output not found!",
					groupTree.name().c_str());
		return "NULL";
	}
	
	VRayNodeContext ctx;
	ctx.node   = groupOutput;
	ctx.ntree  = groupTree;
	ctx.group  = BL::NodeGroup(node);
	ctx.parent = ntree;
	
	return VRayNodeExporter::exportVRayNode(groupTree, groupOutput, toSocket);
}


std::string VRayNodeExporter::exportBlenderNodeGroupInput(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket)
{
	BL::NodeSocket     inputSocket(PointerRNA_NULL);
	BL::NodeGroupInput groupInput(node);

	PRINT_INFO("fromSocket: %s", fromSocket.name().c_str());
	
	BL::NodeTree::inputs_iterator inIt;
	for(ntree.inputs.begin(inIt); inIt != ntree.inputs.end(); ++inIt ) {
		BL::NodeSocketInterface sock = *inIt;
		if(sock.name().empty())
			continue;
		
		PRINT_INFO("Group input socket: %s", sock.name().c_str());
		if(fromSocket.name() == sock.name()) {
			inputSocket = BL::NodeSocket(sock.ptr);
			break;
		}
	}
	
	if(NOT(inputSocket)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Input socket not found!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	BL::Node conNode = VRayNodeExporter::getConnectedNode(ntree, inputSocket);
	if(NOT(conNode)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Connected node is not found!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}
	
	return VRayNodeExporter::exportVRayNode(ntree, conNode);
}


std::string VRayNodeExporter::exportBlenderNodeGroupOutput(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket)
{
	BL::NodeSocket      inputSocket(PointerRNA_NULL);
	BL::NodeGroupOutput groupOutput(node);

	BL::Node::inputs_iterator inIt;
	for(groupOutput.inputs.begin(inIt); inIt != groupOutput.inputs.end(); ++inIt ) {
		BL::NodeSocket sock = *inIt;
		if(sock.name().empty())
			continue;
		if(fromSocket.name() == sock.name()) {
			inputSocket = sock;
			break;
		}
	}
	
	if(NOT(inputSocket)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Input socket not found!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	BL::Node conNode = VRayNodeExporter::getConnectedNode(ntree, inputSocket);
	if(NOT(conNode)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Connected node is not found!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}
	
	return VRayNodeExporter::exportVRayNode(ntree, conNode);
}


std::string VRayNodeExporter::exportBlenderNodeReroute(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket)
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
