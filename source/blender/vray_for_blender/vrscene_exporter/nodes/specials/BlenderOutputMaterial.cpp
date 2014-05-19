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

#include "Node.h"


std::string VRayNodeExporter::exportVRayNodeBlenderOutputMaterial(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context)
{
	if(NOT(context)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	AttributeValueMap pluginAttrs;
	std::string mtlName = Node::GetNodeMtlMulti(context->ob, context->mtlOverride, pluginAttrs);

	// NOTE: Function could return only one material in 'mtlName'
	if(pluginAttrs.find("mtls_list") == pluginAttrs.end())
		return mtlName;

	std::string pluginName = StripString("NT" + ntree.name() + "N" + node.name());

	BL::NodeSocket mtlid_gen_float = VRayNodeExporter::getSocketByName(node, "ID Generator");
	if(mtlid_gen_float.is_linked()) {
		pluginAttrs["mtlid_gen_float"] = VRayNodeExporter::exportLinkedSocket(ntree, mtlid_gen_float);

		// NOTE: if 'ids_list' presents in the plugin description 'mtlid_gen_float' won't work for some reason...
		pluginAttrs.erase(pluginAttrs.find("ids_list"));
	}

	pluginAttrs["wrap_id"] = BOOST_FORMAT_BOOL(RNA_boolean_get(&node.ptr, "wrap_id"));

	VRayNodePluginExporter::exportPlugin("MATERIAL", "MtlMulti", pluginName, pluginAttrs);

	return pluginName;
}
