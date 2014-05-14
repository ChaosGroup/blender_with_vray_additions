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
//	pluginName = clean_string("nt%sn%s" % (nodetree.name, node.name))

//	textures    = []
//	blend_modes = []

//	for inputSocket in node.inputs:
//		if not inputSocket.is_linked:
//			continue
//		tex = WriteConnectedNode(bus, nodetree, inputSocket)

//		# XXX: For some reason TexLayered doesn't like ::out_smth
//		semiPos = tex.find("::")
//		if semiPos != -1:
//			tex = tex[:semiPos]

//		textures.append(tex)
//		blend_modes.append(inputSocket.value)

//	alpha        = WriteConnectedNode(bus, nodetree, node.inputs['Alpha'])
//	alpha_mult   = WriteConnectedNode(bus, nodetree, node.inputs['Alpha Mult'])
//	alpha_offset = WriteConnectedNode(bus, nodetree, node.inputs['Alpha Offset'])
//	nouvw_color  = WriteConnectedNode(bus, nodetree, node.inputs['No UV Color'])
//	color_mult   = WriteConnectedNode(bus, nodetree, node.inputs['Color Mult'])
//	color_offset = WriteConnectedNode(bus, nodetree, node.inputs['Color Offset'])

//	o.set('TEXTURE', 'TexLayered', pluginName)
//	o.writeHeader()
//	o.writeAttibute('textures', "List(%s)" % ','.join(reversed(textures)))
//	o.writeAttibute('blend_modes', "ListInt(%s)" % ','.join(reversed(blend_modes)))
//	o.writeAttibute('alpha_from_intensity', node.alpha_from_intensity)
//	o.writeAttibute('invert', node.invert)
//	o.writeAttibute('invert_alpha', node.invert_alpha)
//	o.writeAttibute('alpha', alpha)
//	o.writeAttibute('alpha_mult', alpha_mult)
//	o.writeAttibute('alpha_offset', alpha_offset)
//	o.writeAttibute('nouvw_color', nouvw_color)
//	o.writeAttibute('color_mult', color_mult)
//	o.writeAttibute('color_offset', color_offset)
//	o.writeFooter()

	std::string pluginName = StripString("NT" + ntree.name() + "N" + node.name());

	StrVector textures;
	StrVector blend_modes;

	for(int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		std::string texSockName = boost::str(boost::format("Texture %i") % i);

		BL::NodeSocket texSock = VRayNodeExporter::getSocketByName(node, texSockName);
		if(NOT(texSock))
			break;

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

	AttributeValueMap manualAttrs;
	manualAttrs["textures"]    = boost::str(boost::format("List(%s)")    % boost::algorithm::join(textures, ","));
	manualAttrs["blend_modes"] = boost::str(boost::format("ListInt(%s)") % boost::algorithm::join(blend_modes, ","));

	StrVector mappableValues;
	mappableValues.push_back("alpha");
	mappableValues.push_back("alpha_mult");
	mappableValues.push_back("alpha_offset");
	mappableValues.push_back("nouvw_color");
	mappableValues.push_back("color_mult");
	mappableValues.push_back("color_offset");

	StrVector::const_iterator strVecIt;
	for(strVecIt = mappableValues.begin(); strVecIt != mappableValues.end(); ++strVecIt) {
		const std::string attrName = *strVecIt;

		BL::NodeSocket attrSock = VRayNodeExporter::getSocketByAttr(node, attrName);
		if(attrSock) {
			std::string socketValue = VRayNodeExporter::exportSocket(ntree, attrSock);
			if(socketValue != "NULL")
				manualAttrs[attrName] = socketValue;
		}
	}

	std::stringstream plugin;

	plugin << "\n" << "TexLayered" << " " << pluginName << " {";

	AttributeValueMap::const_iterator attrIt;
	for(attrIt = manualAttrs.begin(); attrIt != manualAttrs.end(); ++attrIt) {
		const std::string attrName  = attrIt->first;
		const std::string attrValue = attrIt->second;

		plugin << "\n\t" << attrName << "=" << VRayExportable::m_interpStart << attrValue << VRayExportable::m_interpEnd << ";";
	}

	plugin << "\n}\n";

	PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileTex, plugin.str().c_str());

	return pluginName;
}
