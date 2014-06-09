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


std::string VRayNodeExporter::exportVRayNodeTexRemap(BL::NodeTree ntree, BL::Node node)
{
	BL::Texture b_tex = VRayNodeExporter::getTextureFromIDRef(&node.ptr, "texture");
	if(b_tex) {
		std::string pluginName = StripString("NT" + ntree.name() + "N" + node.name());

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

		AttributeValueMap manualAttrs;
		manualAttrs["color_colors"]    = BOOST_FORMAT_LIST(colors);
		manualAttrs["color_positions"] = BOOST_FORMAT_LIST_FLOAT(positions);
		manualAttrs["color_types"]     = BOOST_FORMAT_LIST_INT(types);

		return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, NULL, manualAttrs);
	}

	PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with TexRemap!",
				ntree.name().c_str(), node.name().c_str());

	return "NULL";
}
