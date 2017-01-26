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

#include "DNA_node_types.h"

void DataExporter::exportLinkedSocketEx2(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context,
                                         ExpMode expMode, BL::Node &outNode, AttrValue &outPlugin, BL::Node toNode)
{
	BL::NodeSocket toSocket(Nodes::GetConnectedSocket(fromSocket));
	if (toNode.is_a(&RNA_ShaderNodeGroup) ||
		toNode.is_a(&RNA_NodeCustomGroup))
	{
		BL::Node     groupNode(toNode);
		BL::NodeTree groupTree(Nodes::GetGroupNodeTree(groupNode));
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
			BL::NodeSocket groupInputOutputSocket(Nodes::GetConnectedSocket(fromSocket));

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
					outPlugin = exportDefaultSocket(ntree, groupNodeInputSocket);
				}
				else {
					// We are going out of group here
					BL::NodeTree  currentTree  = context.popParentTree();
					BL::NodeGroup currentGroup = context.popGroupNode();

					// Finally get the node connected to the socket on the Group node
					exportLinkedSocketEx(parentTree, groupNodeInputSocket, context, expMode, outNode, outPlugin);

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
				exportLinkedSocketEx(ntree, rerouteInSock, context, expMode, outNode, outPlugin);
			}
		}
	}
	else if(toNode.bl_idname() == "VRayNodeDebugSwitch") {
		const int inputIndex = RNA_enum_get(&toNode.ptr, "input_index");
		char inputSocketName[64] = {0, };
		snprintf(inputSocketName, sizeof(inputSocketName), "Input %i", inputIndex);

		BL::NodeSocket inputSocket = Nodes::GetInputSocketByName(toNode, inputSocketName);
		if (inputSocket && inputSocket.is_linked()) {
			exportLinkedSocketEx(ntree, inputSocket, context, expMode, outNode, outPlugin);
		}
	}
	else {
		if (expMode == ExpModePluginName) {
			outPlugin = AttrPlugin(DataExporter::GenPluginName(toNode, ntree, context));
		}
		else if (expMode == ExpModePlugin) {
			outPlugin = exportVRayNode(ntree, toNode, fromSocket, context);
		}
		outNode = toNode;

		// Check if we need to use specific output
		if (expMode == ExpModePluginName || expMode == ExpModePlugin) {
			if (RNA_struct_find_property(&toSocket.ptr, "vray_attr")) {
				const std::string &conSockAttrName = RNA_std_string_get(&toSocket.ptr, "vray_attr");
				if (!conSockAttrName.empty()) {
					if (!(conSockAttrName == "uvwgen" ||
							conSockAttrName == "bitmap"))
					{
						outPlugin.valPlugin.output = conSockAttrName;
					}
				}
			}
		}
	}
}


void DataExporter::exportLinkedSocketEx(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context,
                                        ExpMode expMode, BL::Node &outNode, AttrValue &outPlugin)
{
	outNode   = BL::Node(PointerRNA_NULL);
	outPlugin = AttrPlugin();

	BL::NodeSocket toSocket(Nodes::GetConnectedSocket(fromSocket));
	if (toSocket) {
		BL::Node toNode(toSocket.node());
		if (toNode) {
			exportLinkedSocketEx2(ntree, fromSocket, context, expMode, outNode, outPlugin, toNode);
		}
	}
}


BL::Node DataExporter::getConnectedNode(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context)
{
	BL::Node  conNode(PointerRNA_NULL);
	AttrValue conPlugin;

	exportLinkedSocketEx(ntree, fromSocket, context, DataExporter::ExpModeNode, conNode, conPlugin);

	return conNode;
}


AttrValue DataExporter::exportLinkedSocket(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context)
{
	BL::Node  conNode(PointerRNA_NULL);
	AttrValue conPlugin;

	exportLinkedSocketEx(ntree, fromSocket, context, DataExporter::ExpModePlugin, conNode, conPlugin);

	return conPlugin;
}


AttrValue DataExporter::getConnectedNodePluginName(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context)
{
	BL::Node  conNode(PointerRNA_NULL);
	AttrValue conPlugin;

	exportLinkedSocketEx(ntree, fromSocket, context, DataExporter::ExpModePluginName, conNode, conPlugin);

	return conPlugin;
}


BL::Node DataExporter::getNtreeSelectedNode(BL::NodeTree &ntree)
{
	BL::NodeTree::nodes_iterator iter;
	for (ntree.nodes.begin(iter); iter != ntree.nodes.end(); ++iter) {
		BL::Node node(*iter);
		bNode * blender_node = (bNode*)node.ptr.data;
		if (blender_node->flag & NODE_SELECT) {
			return node;
		}
	}
	return PointerRNA_NULL;
}
