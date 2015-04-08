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

#include <boost/format.hpp>

#include "vfb_params_json.h"
#include "vfb_node_exporter.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_string.h"
#include "vfb_utils_nodes.h"


// NOTE: The same as VRayNodeExporter::exportLinkedSocket,
// but returns connected node
//
BL::Node DataExporter::getConnectedNode(BL::NodeSocket fromSocket, NodeContext *context)
{
	BL::NodeSocket conSock = Nodes::GetConnectedSocket(fromSocket);
	// NOTE: This could happen while reconnecting nodes and material preview is active
	if (NOT(conSock.ptr.data))
		return BL::Node(PointerRNA_NULL);

	BL::Node conNode = conSock.node();
	// NOTE: This could happen while reconnecting nodes and material preview is active
	if (NOT(conNode.ptr.data))
		return BL::Node(PointerRNA_NULL);

	// If we are connected to the Group node,
	// we have to found the actual socket/node from the group node tree
	//
	if (conNode.is_a(&RNA_ShaderNodeGroup) || conNode.is_a(&RNA_NodeCustomGroup)) {
		// Setting nested context
		if (context) {
			context->pushGroupNode(conNode.ptr);
		}

		// Get real socket / node to export
		conSock = DataExporter::getNodeGroupSocketReal(conNode, conSock);
		// NOTE: This could happen while reconnecting nodes and material preview is active
		if (NOT(conSock.ptr.data))
			return BL::Node(PointerRNA_NULL);

		conNode = conSock.node();

		// Restoring context
		if (context) {
			context->popParentTree();
			context->popGroupNode();
		}
	}
	else if (conNode.is_a(&RNA_NodeGroupInput)) {
		if (NOT(context)) {
			PRINT_ERROR("No context for NodeGroupInput!");
			return BL::Node(PointerRNA_NULL);
		}
		else {
			BL::NodeGroup groupNode  = context->getGroupNode();
			if (NOT(groupNode)) {
				PRINT_ERROR("Socket: %s => No group node and / or tree in context!",
				            fromSocket.name().c_str());
				return BL::Node(PointerRNA_NULL);
			}

			// Find socket connected to fromSocket on the Group Input node
			BL::NodeSocket inputNodeSocket = Nodes::GetConnectedSocket(fromSocket);

			// Now have to find a correspondent socket on the Group node
			// and then export connected node from the parent tree
			//
			BL::NodeSocket groupInputSocket(PointerRNA_NULL);
			BL::Node::inputs_iterator inIt;
			for(groupNode.inputs.begin(inIt); inIt != groupNode.inputs.end(); ++inIt ) {
				BL::NodeSocket sock = *inIt;
				if (sock.name().empty())
					continue;
				if (inputNodeSocket.name() == sock.name()) {
					groupInputSocket = sock;
					break;
				}
			}
			if (NOT(groupInputSocket)) {
				PRINT_ERROR("Group node name: %s => Node tree: %s => Input socket not found!",
				            groupNode.name().c_str(), groupNode.node_tree().name().c_str());
				return BL::Node(PointerRNA_NULL);
			}

			// Forward the real socket
			conSock = Nodes::GetConnectedSocket(groupInputSocket);
			if (NOT(conSock.ptr.data))
				return BL::Node(PointerRNA_NULL);

			// Finally get the node connected to the socket on the Group node
			conNode = DataExporter::getConnectedNode(groupInputSocket, context);
			if (NOT(conNode)) {
				PRINT_ERROR("Group node name: %s => Connected node is not found!",
				            groupNode.name().c_str());
				return BL::Node(PointerRNA_NULL);
			}
		}
	}
	else if (conNode.is_a(&RNA_NodeReroute)) {
		if (conNode.internal_links.length()) {
			BL::NodeSocket rerouteInSock = conNode.internal_links[0].from_socket();
			if (rerouteInSock) {
				conSock = Nodes::GetConnectedSocket(rerouteInSock);
				if (NOT(conSock.ptr.data))
					return BL::Node(PointerRNA_NULL);
				conNode = DataExporter::getConnectedNode(rerouteInSock, context);
			}
		}
	}
	else if (conNode.bl_idname() == "VRayNodeDebugSwitch") {
		const int inputIndex = RNA_enum_get(&conNode.ptr, "input_index");
		const std::string inputSocketName = boost::str(boost::format("Input %i") % inputIndex);

		BL::NodeSocket inputSocket = Nodes::GetInputSocketByName(conNode, inputSocketName);
		if (NOT(inputSocket && inputSocket.is_linked()))
			return BL::Node(PointerRNA_NULL);

		conNode = DataExporter::getConnectedNode(inputSocket, context);
	}

	return conNode;
}


BL::NodeSocket DataExporter::getNodeGroupSocketReal(BL::Node node, BL::NodeSocket fromSocket)
{
	BL::NodeTree groupTree = Nodes::GetGroupNodeTree(node);
	if (NOT(groupTree)) {
		PRINT_ERROR("Group node name: %s => Tree not found!",
		            node.name().c_str());
		return BL::NodeSocket(PointerRNA_NULL);
	}
	BL::NodeGroupOutput groupOutput(PointerRNA_NULL);
	BL::NodeTree::nodes_iterator nodeIt;
	for(groupTree.nodes.begin(nodeIt); nodeIt != groupTree.nodes.end(); ++nodeIt) {
		BL::Node gNode = *nodeIt;

		if (gNode.is_a(&RNA_NodeGroupOutput)) {
			groupOutput = BL::NodeGroupOutput(gNode);
			break;
		}
	}
	if (NOT(groupOutput)) {
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
		if (sock.name().empty())
			continue;
		if (fromSocket.name() == sock.name()) {
			toSocket = sock;
			break;
		}
	}
	if (NOT(toSocket)) {
		PRINT_ERROR("Node tree: %s => Group node name: %s => Input socket not found!",
		            groupTree.name().c_str(), node.name().c_str());
		return BL::NodeSocket(PointerRNA_NULL);
	}

	// Finally get the socket connected to the Group Output socket
	//
	toSocket = Nodes::GetConnectedSocket(toSocket);

	return toSocket;
}


AttrValue DataExporter::exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket fromSocket, NodeContext *context, bool dont_export)
{
	AttrValue attrValue;

	BL::NodeSocket toSocket = Nodes::GetConnectedSocket(fromSocket);
	BL::Node       toNode   = Nodes::GetConnectedNode(fromSocket);

	if (toSocket && toNode) {
		// If we are connected to the Group node,
		// we have to found the actual socket/node from the group node tree
		if (toNode.is_a(&RNA_ShaderNodeGroup) || toNode.is_a(&RNA_NodeCustomGroup)) {
			// Get group tree
			BL::NodeTree groupTree = Nodes::GetGroupNodeTree(toNode);

			// Setting nested context
			if (context) {
				context->pushGroupNode(toNode.ptr);
				context->pushParentTree(ntree);
			}

			// Get real socket / node to export
			toSocket = DataExporter::getNodeGroupSocketReal(toNode, toSocket);

			// NOTE: This could happen while reconnecting nodes and material preview is active
			if (toSocket) {
				toNode = toSocket.node();

				attrValue = dont_export
				            ? AttrPlugin(DataExporter::GenPluginName(toNode, groupTree, context))
				            : exportVRayNode(groupTree, toNode, fromSocket, context);

			}

			// Restoring context
			if (context) {
				context->popParentTree();
				context->popGroupNode();
			}
		}
		else if (toNode.is_a(&RNA_NodeGroupInput)) {
			if (NOT(context)) {
				PRINT_ERROR("No context for NodeGroupInput!");
			}
			else {
				BL::NodeGroup groupNode  = context->getGroupNode();
				BL::NodeTree  parentTree = context->getNodeTree();
				if (NOT(groupNode && parentTree)) {
					PRINT_ERROR("Node tree: %s => No group node and / or tree in context!",
					            ntree.name().c_str());
				}
				else {
					// Find socket connected to fromSocket on the Group Input node
					BL::NodeSocket inputNodeSocket = Nodes::GetConnectedSocket(fromSocket);

					// Now have to find a correspondent socket on the Group node
					// and then export connected node from the parent tree
					//
					BL::NodeSocket groupInputSocket(PointerRNA_NULL);
					BL::Node::inputs_iterator inIt;
					for(groupNode.inputs.begin(inIt); inIt != groupNode.inputs.end(); ++inIt ) {
						BL::NodeSocket sock = *inIt;
						if (sock.name().empty())
							continue;
						if (inputNodeSocket.name() == sock.name()) {
							groupInputSocket = sock;
							break;
						}
					}
					if (NOT(groupInputSocket)) {
						PRINT_ERROR("Node tree: %s => Group node name: %s => Input socket not found!",
						            ntree.name().c_str(), groupNode.name().c_str());
					}
					else if (NOT(groupInputSocket.is_linked())) {
						attrValue = DataExporter::exportDefaultSocket(ntree, groupInputSocket);
					}
					// Forward the real socket
					else {
						toSocket = Nodes::GetConnectedSocket(groupInputSocket);
						if (toSocket) {
							// Finally get the node connected to the socket on the Group node
							toNode = DataExporter::getConnectedNode(groupInputSocket, context);
							if (NOT(toNode)) {
								PRINT_ERROR("Node tree: %s => Node name: %s => Connected node is not found!",
								            ntree.name().c_str(), groupNode.name().c_str());
							}
							else {
								// We are going out of group here
								BL::NodeTree  currentTree  = context->popParentTree();
								BL::NodeGroup currentGroup = context->popGroupNode();

								attrValue = dont_export
								            ? AttrPlugin(DataExporter::GenPluginName(toNode, parentTree, context))
								            : exportVRayNode(parentTree, toNode, fromSocket, context);

								// We could go into the group after
								context->pushGroupNode(currentGroup);
								context->pushParentTree(currentTree);
							}
						}
					}
				}
			}
		}
		else if (toNode.is_a(&RNA_NodeReroute)) {
			if (toNode.internal_links.length()) {
				BL::NodeSocket rerouteInSock = toNode.internal_links[0].from_socket();
				if (rerouteInSock) {
					toSocket = Nodes::GetConnectedSocket(rerouteInSock);
					toNode   = DataExporter::getConnectedNode(rerouteInSock, context);
					if (toNode && toNode) {
						attrValue = dont_export
						            ? AttrPlugin(DataExporter::GenPluginName(toNode, ntree, context))
						            : exportVRayNode(ntree, toNode, fromSocket, context);
					}
				}
			}
		}
		else if (toNode.bl_idname() == "VRayNodeDebugSwitch") {
			const int inputIndex = RNA_enum_get(&toNode.ptr, "input_index");
			const std::string inputSocketName = boost::str(boost::format("Input %i") % inputIndex);

			BL::NodeSocket inputSocket = Nodes::GetInputSocketByName(toNode, inputSocketName);
			if (inputSocket && inputSocket.is_linked()) {
				attrValue = DataExporter::exportLinkedSocket(ntree, inputSocket, context, dont_export);
			}
		}
		else {
			attrValue = dont_export
			            ? AttrPlugin(DataExporter::GenPluginName(toNode, ntree, context))
			            : exportVRayNode(ntree, toNode, fromSocket, context);
		}

		if (RNA_struct_find_property(&toSocket.ptr, "vray_attr")) {
			std::string conSockAttrName = RNA_std_string_get(&toSocket.ptr, "vray_attr");
			if (!conSockAttrName.empty()) {
				if (!(conSockAttrName == "uvwgen" || conSockAttrName == "bitmap")) {
					// Store output attribute name
					attrValue.valPluginOutput = conSockAttrName;
				}
			}
		}
	}

	return attrValue;
}

