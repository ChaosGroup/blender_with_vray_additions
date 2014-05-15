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


std::string VRayNodeExporter::exportVRayNodeTexLayered(BL::NodeTree ntree, BL::Node node)
{
	std::string pluginName = StripString("NT" + ntree.name() + "N" + node.name());

	StrVector textures;
	StrVector blend_modes;

	for(int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		std::string texSockName = boost::str(boost::format("Texture %i") % i);

		BL::NodeSocket texSock = VRayNodeExporter::getSocketByName(node, texSockName);
		if(NOT(texSock))
			continue;

		if(NOT(texSock.is_linked()))
			continue;

		std::string texture = VRayNodeExporter::exportLinkedSocket(ntree, texSock);

		// NOTE: For some reason TexLayered doesn't like ::out_smth
		size_t semiPos = texture.find("::");
		if(semiPos != std::string::npos)
			texture.erase(texture.begin()+semiPos, texture.end());

		std::string blend_mode = boost::str(boost::format("%i") % RNA_enum_get(&texSock.ptr, "value"));

		textures.push_back(texture);
		blend_modes.push_back(blend_mode);
	}

	std::reverse(textures.begin(), textures.end());
	std::reverse(blend_modes.begin(), blend_modes.end());

	AttributeValueMap pluginAttrs;
	pluginAttrs["textures"]    = boost::str(boost::format("List(%s)")    % boost::algorithm::join(textures, ","));
	pluginAttrs["blend_modes"] = boost::str(boost::format("ListInt(%s)") % boost::algorithm::join(blend_modes, ","));

	StrVector mappableValues;
	mappableValues.push_back("alpha");
	mappableValues.push_back("alpha_mult");
	mappableValues.push_back("alpha_offset");
	mappableValues.push_back("nouvw_color");
	mappableValues.push_back("color_mult");
	mappableValues.push_back("color_offset");

	for(StrVector::const_iterator mvIt = mappableValues.begin(); mvIt != mappableValues.end(); ++mvIt) {
		const std::string attrName = *mvIt;

		BL::NodeSocket attrSock = VRayNodeExporter::getSocketByAttr(node, attrName);
		if(attrSock) {
			std::string socketValue = VRayNodeExporter::exportSocket(ntree, attrSock);
			if(socketValue != "NULL")
				pluginAttrs[attrName] = socketValue;
		}
	}

	VRayNodePluginExporter::exportPlugin("TEXTURE", "TexLayered", pluginName, pluginAttrs);

	return pluginName;
}
