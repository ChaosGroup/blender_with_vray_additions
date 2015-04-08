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

#include "vfb_utils_nodes.h"
#include "vfb_utils_blender.h"

extern "C" {
#include "BKE_node.h" // For bNodeSocket->link access
}


BL::NodeTree VRayForBlender::Nodes::GetNodeTree(BL::ID id, const std::string &attr)
{
	PointerRNA vrayPtr = RNA_pointer_get(&id.ptr, "vray");

	return VRayForBlender::Blender::GetDataFromProperty<BL::NodeTree>(&vrayPtr, attr);
}


BL::NodeTree VRayForBlender::Nodes::GetGroupNodeTree(BL::Node group_node)
{
	BL::NodeTree groupTree = BL::NodeTree(PointerRNA_NULL);
	if (group_node.is_a(&RNA_ShaderNodeGroup)) {
		BL::NodeGroup groupNode(group_node);
		groupTree = groupNode.node_tree();
	}
	else {
		BL::NodeCustomGroup groupNode(group_node);
		groupTree = groupNode.node_tree();
	}
	return groupTree;
}


BL::NodeSocket VRayForBlender::Nodes::GetInputSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::inputs_iterator input;
	for (node.inputs.begin(input); input != node.inputs.end(); ++input) {
		if (input->name() == socketName) {
			return *input;
		}
	}
	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayForBlender::Nodes::GetOutputSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::outputs_iterator input;
	for (node.outputs.begin(input); input != node.outputs.end(); ++input) {
		if (input->name() == socketName) {
			return *input;
		}
	}
	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayForBlender::Nodes::GetSocketByAttr(BL::Node node, const std::string &attrName)
{
	BL::NodeSocket socket(PointerRNA_NULL);

	BL::Node::inputs_iterator sockIt;
	for (node.inputs.begin(sockIt); sockIt != node.inputs.end(); ++sockIt) {
		if (RNA_struct_find_property(&sockIt->ptr, "vray_attr")) {
			std::string sockAttrName = RNA_std_string_get(&sockIt->ptr, "vray_attr");
			if ((!sockAttrName.empty()) && (attrName == sockAttrName)) {
				socket = *sockIt;
				break;
			}
		}
	}

	return socket;
}


BL::NodeSocket VRayForBlender::Nodes::GetConnectedSocket(BL::NodeSocket socket)
{
	bNodeSocket *bSocket = (bNodeSocket*)socket.ptr.data;
	bNodeLink   *link = bSocket->link;
	if (link) {
		PointerRNA socketPtr;
		RNA_pointer_create((ID*)socket.ptr.id.data, &RNA_NodeSocket, link->fromsock, &socketPtr);
		return BL::NodeSocket(socketPtr);
	}
	return BL::NodeSocket(PointerRNA_NULL);
}


BL::Node VRayForBlender::Nodes::GetConnectedNode(BL::NodeSocket socket)
{
	bNodeSocket *bSocket = (bNodeSocket*)socket.ptr.data;
	bNodeLink   *link = bSocket->link;
	if (link) {
		PointerRNA nodePtr;
		RNA_pointer_create((ID*)socket.ptr.id.data, &RNA_Node, link->fromnode, &nodePtr);
		return BL::Node(nodePtr);
	}
	return BL::Node(PointerRNA_NULL);
}


BL::Node VRayForBlender::Nodes::GetNodeByType(BL::NodeTree nodeTree, const std::string &nodeType)
{
	BL::NodeTree::nodes_iterator node;
	for(nodeTree.nodes.begin(node); node != nodeTree.nodes.end(); ++node) {
		if(node->rna_type().identifier() == nodeType) {
			return *node;
		}
	}
	return BL::Node(PointerRNA_NULL);
}
