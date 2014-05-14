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


std::string VRayNodeExporter::exportVRayNodeTexGradRamp(BL::NodeTree ntree, BL::Node node)
{
	BL::Texture b_tex = VRayNodeExporter::getTextureFromIDRef(&node.ptr, "texture__name__");
	if(b_tex) {
		std::string pluginName = StripString("NT" + ntree.name() + "N" + node.name());

		BL::ColorRamp ramp = b_tex.color_ramp();

		StrVector colors;
		StrVector positions;

		BL::ColorRamp::elements_iterator elIt;
		int                              elNum = 0;
		for(ramp.elements.begin(elIt); elIt != ramp.elements.end(); ++elIt) {
			BL::ColorRampElement el = *elIt;

			std::string       colPluginName = boost::str(boost::format("%sPos%i") % pluginName % elNum++);
			std::stringstream colplugin;

			std::string color = boost::str(boost::format("AColor(%.6f,%.6f,%.6f,%.6f)")
										   % el.color()[0] % el.color()[1] % el.color()[2] % el.color()[3]);

			std::string position = boost::str(boost::format("%.3f") % el.position());

			colplugin << "\n"   << "TexAColor" << " " << colPluginName << " {";
			colplugin << "\n\t" << "texture" << "=" << color << ";";
			colplugin << "\n}\n";

			PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileTex, colplugin.str().c_str());

			colors.push_back(colPluginName);
			positions.push_back(position);
		}

		AttributeValueMap manualAttrs;
		manualAttrs["colors"]    = boost::str(boost::format("List(%s)")      % boost::algorithm::join(colors, ","));
		manualAttrs["positions"] = boost::str(boost::format("ListFloat(%s)") % boost::algorithm::join(positions, ","));

		return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, manualAttrs);
	}

	PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with TexGradRamp!",
				ntree.name().c_str(), node.name().c_str());

	return "NULL";
}
