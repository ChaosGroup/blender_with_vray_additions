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
#include "vfb_util_defines.h"

extern "C" {
#include "BKE_node.h" // For bNodeSocket->link access
}


BL::NodeTree VRayForBlender::Nodes::GetNodeTree(BL::ID & id, const std::string &attr)
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

namespace
{

BL::NodeSocket getConnectedSocketImpl(BL::NodeSocket socket)
{
	// if socket is output, this is the connected socket
	if (!socket || socket.is_output()) {
		return socket;
	}
	bNodeSocket *bSocket = (bNodeSocket*)socket.ptr.data;
	bNodeLink   *link = bSocket->link;
	if (link) {
		PointerRNA socketPtr;
		RNA_pointer_create(reinterpret_cast<ID*>(socket.ptr.id.data), &RNA_NodeSocket, link->fromsock, &socketPtr);
		return BL::NodeSocket(socketPtr);
	}
	return BL::NodeSocket(PointerRNA_NULL);
}

}

BL::NodeSocket VRayForBlender::Nodes::SkipLayoutNodes(BL::NodeSocket socket)
{
	// if this is input socket, do nothing
	if (!socket || !socket.is_output()) {
		return socket;
	}

	BL::Node node = socket.node();
	if (!node) {
		return socket;
	}

	if (node.bl_idname() == "VRayNodeDebugSwitch") {
		const int inputIndex = RNA_enum_get(&node.ptr, "input_index");
		char inputSocketName[64] = {0, };
		snprintf(inputSocketName, sizeof(inputSocketName), "Input %i", inputIndex);

		// we must return the connected socket (the output one) since we got output socket
		// also call SkipLayoutNodes recursivly if there are more than 1 layout nodes
		return GetConnectedSocket(SkipLayoutNodes(GetInputSocketByName(node, inputSocketName)));
	} else if (node.is_a(&RNA_NodeReroute) && node.internal_links.length()) {
		// rerout nodes have one internal link which points to the input socket
		return GetConnectedSocket(SkipLayoutNodes(node.internal_links[0].from_socket()));
	}

	// return the original input socket if the connected node is not switch
	return socket;
}

BL::NodeSocket VRayForBlender::Nodes::GetConnectedSocket(BL::NodeSocket socket)
{
	return SkipLayoutNodes(getConnectedSocketImpl(socket));
}

BL::Node VRayForBlender::Nodes::GetConnectedNode(BL::NodeSocket socket)
{
	BL::NodeSocket skipSocket = SkipLayoutNodes(getConnectedSocketImpl(socket));
	if (skipSocket) {
		return skipSocket.node();
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


std::string VRayForBlender::getVRayNodeSocketTypeName(BL::NodeSocket socket)
{
	if (RNA_struct_find_property(&socket.ptr, "vray_socket_base_type")) {
		return RNA_std_string_get(&socket.ptr, "vray_socket_base_type");
	}
	return socket.rna_type().identifier();
}


VRayForBlender::VRayNodeSocketType VRayForBlender::getVRayNodeSocketType(BL::NodeSocket socket)
{
	const std::string & socketTypeStr = getVRayNodeSocketTypeName(socket);

	if (socketTypeStr == "VRaySocketBRDF") return vrayNodeSocketBRDF;
	if (socketTypeStr == "VRaySocketColor") return vrayNodeSocketColor;
	if (socketTypeStr == "VRaySocketColorNoValue") return vrayNodeSocketColorNoValue;
	if (socketTypeStr == "VRaySocketCoords") return vrayNodeSocketCoords;
	if (socketTypeStr == "VRaySocketEffect") return vrayNodeSocketEffect;
	if (socketTypeStr == "VRaySocketEnvironment") return vrayNodeSocketEnvironment;
	if (socketTypeStr == "VRaySocketEnvironmentOverride") return vrayNodeSocketEnvironmentOverride;
	if (socketTypeStr == "VRaySocketFloat") return vrayNodeSocketFloat;
	if (socketTypeStr == "VRaySocketFloatColor") return vrayNodeSocketFloatColor;
	if (socketTypeStr == "VRaySocketFloatNoValue") return vrayNodeSocketFloatNoValue;
	if (socketTypeStr == "VRaySocketInt") return vrayNodeSocketInt;
	if (socketTypeStr == "VRaySocketMtl") return vrayNodeSocketMtl;
	if (socketTypeStr == "VRaySocketObject") return vrayNodeSocketObject;
	if (socketTypeStr == "VRaySocketTransform") return vrayNodeSocketTransform;
	if (socketTypeStr == "VRaySocketVector") return vrayNodeSocketVector;

	return vrayNodeSocketUnknown;
}
