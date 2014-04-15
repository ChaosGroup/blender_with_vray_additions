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

#include "CGR_rna.h"


BL::NodeSocket VRsceneExporter::getSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::inputs_iterator input;
	for(node.inputs.begin(input); input != node.inputs.end(); ++input)
		if(input->name() == socketName)
			return *input;

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::Node VRsceneExporter::getNodeByType(BL::NodeTree nodeTree, const std::string &nodeType)
{
	BL::NodeTree::nodes_iterator node;
	for(nodeTree.nodes.begin(node); node != nodeTree.nodes.end(); ++node)
		if(node->rna_type().identifier() == nodeType)
			return *node;

	return BL::Node(PointerRNA_NULL);
}


BL::Node VRsceneExporter::getConnectedNode(BL::NodeTree nodeTree, BL::NodeSocket socket)
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


BL::Node VRsceneExporter::getConnectedNode(BL::NodeTree nodeTree, BL::Node node, const std::string &socketName)
{
	BL::NodeSocket socket = getSocketByName(node, socketName);
	if(NOT(socket.is_linked()))
		return BL::Node(PointerRNA_NULL);

	return getConnectedNode(nodeTree, socket);
}


BL::NodeTree VRsceneExporter::getNodeTree(Object *ob)
{
	PRINT_INFO("getNodeTree(%s)", ob->id.name);

	RnaAccess::RnaValue VRayObject((ID*)ob, "vray");

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

		PRINT_INFO("Object '%s' node tree is '%s'", ob->id.name, ntreeName.c_str());

		if(NOT(ntreeName.empty())) {
			BL::BlendData::node_groups_iterator nodeGroupIt;
			for(m_settings->b_data.node_groups.begin(nodeGroupIt); nodeGroupIt != m_settings->b_data.node_groups.end(); ++nodeGroupIt) {
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


void VRsceneExporter::exportNodeFromNodeTree(BL::NodeTree ntree, Object *ob, const int &checkUpdated)
{
#if 0
	BL::NodeTree::nodes_iterator b_node;

	PRINT_INFO("ntree = \"%s\"", ntree.name().c_str());

	BL::Node::inputs_iterator  b_input;
	BL::Node::outputs_iterator b_output;

	for(ntree.nodes.begin(b_node); b_node != ntree.nodes.end(); ++b_node) {
		PRINT_INFO("  node = \"%s\" ['%s']", b_node->name().c_str(), b_node->rna_type().identifier().c_str());

		for (b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
			PRINT_INFO("    input = \"%s\"", b_input->name().c_str());
		}

		for (b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
			PRINT_INFO("    output = \"%s\"", b_output->name().c_str());
		}
	}
#endif

	BL::Node nodeOutput = getNodeByType(ntree, "VRayNodeObjectOutput");
	if(NOT(nodeOutput)) {
		PRINT_ERROR("Object: %s => Output node not found!", ob->id.name);
		return;
	}

	BL::Node materialNode = getConnectedNode(ntree, nodeOutput, "Material");
	if(materialNode) {
		PRINT_INFO("Material node: '%s'", materialNode.name().c_str())
	}

	BL::Node geometryNode = getConnectedNode(ntree, nodeOutput, "Geometry");
	if(geometryNode) {
		PRINT_INFO("Geometry node: '%s'", geometryNode.name().c_str())
	}
}
