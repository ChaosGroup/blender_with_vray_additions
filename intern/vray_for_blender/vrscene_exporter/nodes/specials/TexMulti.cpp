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


std::string VRayNodeExporter::exportVRayNodeTexMulti(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	std::string pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

	StrVector textures;
	StrVector textures_ids;

	BL::NodeSocket textureDefaultSock = VRayNodeExporter::getSocketByName(node, "Default");

	for(int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		std::string texSockName = boost::str(boost::format("Texture %i") % i);

		BL::NodeSocket texSock = VRayNodeExporter::getSocketByName(node, texSockName);
		if(NOT(texSock))
			continue;

		if(NOT(texSock.is_linked()))
			continue;

		std::string texture   = VRayNodeExporter::exportLinkedSocket(ntree, texSock, context);
		std::string textureID = BOOST_FORMAT_INT(RNA_int_get(&texSock.ptr, "value"));

		textures.push_back(texture);
		textures_ids.push_back(textureID);
	}

	AttributeValueMap pluginAttrs;
	pluginAttrs["textures_list"]   = BOOST_FORMAT_LIST(textures);
	pluginAttrs["ids_list"]        = BOOST_FORMAT_LIST_INT(textures_ids);
	pluginAttrs["mode"]            = BOOST_FORMAT_INT(RNA_enum_get(&node.ptr, "mode"));
	pluginAttrs["default_texture"] = VRayNodeExporter::exportLinkedSocket(ntree, textureDefaultSock, context);

	VRayNodePluginExporter::exportPlugin("TEXTURE", "TexMulti", pluginName, pluginAttrs);

	return pluginName;
}
