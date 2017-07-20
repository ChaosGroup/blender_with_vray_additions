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
	BL::NodeTree ntree(PointerRNA_NULL);

	if (RNA_struct_find_property(&id.ptr, "vray")) {
		PointerRNA vrayPtr = RNA_pointer_get(&id.ptr, "vray");
		ntree = VRayForBlender::Blender::GetDataFromProperty<BL::NodeTree>(&vrayPtr, attr);
	}

	return ntree;
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
	if (!node) {
		return BL::NodeSocket(PointerRNA_NULL);
	}

	for (auto input : Blender::collection(node.inputs)) {
		if (input.name() == socketName) {
			return input;
		}
	}

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayForBlender::Nodes::GetOutputSocketByName(BL::Node node, const std::string &socketName)
{
	if (!node) {
		return BL::NodeSocket(PointerRNA_NULL);
	}

	for (auto output : Blender::collection(node.outputs)) {
		if (output.name() == socketName) {
			return output;
		}
	}

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayForBlender::Nodes::GetSocketByAttr(BL::Node node, const std::string &attrName)
{
	if (!node) {
		return BL::NodeSocket(PointerRNA_NULL);
	}

	for (auto socket : Blender::collection(node.inputs)) {
		if (RNA_struct_find_property(&socket.ptr, "vray_attr")) {
			std::string sockAttrName = RNA_std_string_get(&socket.ptr, "vray_attr");
			if ((!sockAttrName.empty()) && (attrName == sockAttrName)) {
				return socket;
				break;
			}
		}
	}

	return BL::NodeSocket(PointerRNA_NULL);
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
