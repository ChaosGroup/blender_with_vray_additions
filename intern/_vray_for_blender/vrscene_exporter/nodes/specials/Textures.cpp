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


void VRayNodeExporter::exportRampAttribute(VRayNodeExportParam, AttributeValueMap &attrs,
											const std::string &texAttrName, const std::string &colAttrName, const std::string &posAttrName, const std::string &typesAttrName)
{
	BL::Texture b_tex = VRayNodeExporter::getTextureFromIDRef(&node.ptr, texAttrName);
	if(b_tex) {
		std::string pluginName = VRayNodeExporter::getPluginName(node, ntree, context) + "@" + texAttrName;

		BL::ColorRamp ramp = b_tex.color_ramp();

		StrVector   colors;
		StrVector   positions;
		StrVector   types;
		std::string interpType;

		int interp;
		switch(ramp.interpolation()) {
			case BL::ColorRamp::interpolation_CONSTANT: interp = 0; break;
			case BL::ColorRamp::interpolation_LINEAR:   interp = 1; break;
			case BL::ColorRamp::interpolation_EASE:     interp = 2; break;
			case BL::ColorRamp::interpolation_CARDINAL: interp = 3; break;
			case BL::ColorRamp::interpolation_B_SPLINE: interp = 4; break;
			default:                                    interp = 1;
		}
		interpType = BOOST_FORMAT_INT(interp);

		BL::ColorRamp::elements_iterator elIt;
		int                              elNum = 0;

		// XXX: Check why in Python I use reversed order
		//
		for(ramp.elements.begin(elIt); elIt != ramp.elements.end(); ++elIt) {
			BL::ColorRampElement el = *elIt;

			std::string colPluginName = boost::str(boost::format("%sPos%i") % pluginName % elNum++);

			std::string color    = BOOST_FORMAT_ACOLOR(el.color());
			std::string position = BOOST_FORMAT_FLOAT(el.position());

			AttributeValueMap colAttrs;
			colAttrs["texture"] = color;

			VRayNodePluginExporter::exportPlugin("TEXTURE", "TexAColor", colPluginName, colAttrs);

			colors.push_back(colPluginName);
			positions.push_back(position);
			types.push_back(interpType);
		}

		attrs[colAttrName] = BOOST_FORMAT_LIST(colors);
		attrs[posAttrName] = BOOST_FORMAT_LIST_FLOAT(positions);
		if (NOT(typesAttrName.empty())) {
			attrs[typesAttrName] = BOOST_FORMAT_LIST_INT(types);
		}
	}
}


std::string VRayNodeExporter::exportVRayNodeTexEdges(VRayNodeExportParam)
{
	AttributeValueMap pluginAttrs;
	VRayNodeExporter::getVRayNodeAttributes(pluginAttrs, ntree, node, fromSocket, context);

	pluginAttrs["world_width"] = pluginAttrs["pixel_width"];

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, pluginAttrs);
}
