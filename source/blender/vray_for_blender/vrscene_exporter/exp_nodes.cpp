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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "exp_nodes.h"


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

