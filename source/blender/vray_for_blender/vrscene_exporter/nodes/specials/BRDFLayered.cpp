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


std::string VRayNodeExporter::exportVRayNodeBRDFLayered(BL::NodeTree ntree, BL::Node node)
{
	std::string pluginName = StripString("NT" + ntree.name() + "N" + node.name());

	StrVector brdfs;
	StrVector weights;

	for(int i = 1; i <= CGR_MAX_LAYERED_BRDFS; ++i) {
		std::string brdfSockName   = boost::str(boost::format("BRDF %i")   % i);
		std::string weigthSockName = boost::str(boost::format("Weight %i") % i);

		BL::NodeSocket brdfSock   = VRayNodeExporter::getSocketByName(node, brdfSockName);
		if(NOT(brdfSock))
			continue;
		if(NOT(brdfSock.is_linked()))
			continue;

		std::string brdf   = VRayNodeExporter::exportLinkedSocket(ntree, brdfSock);
		std::string weight = "1.0";

		BL::NodeSocket weightSock = VRayNodeExporter::getSocketByName(node, weigthSockName);
		if(weightSock.is_linked()) {
			weight = VRayNodeExporter::exportLinkedSocket(ntree, weightSock);
		}
		else {
			weight = boost::str(boost::format("%sW%i") % pluginName % i);

			float       weigthValue = RNA_float_get(&weightSock.ptr, "value");
			std::string weightColor = boost::str(boost::format("AColor(%.6f,%.6f,%.6f,1.0)")
												 % weigthValue % weigthValue % weigthValue);

			AttributeValueMap weigthAttrs;
			weigthAttrs["texture"] = weightColor;

			// NOTE: Plugin type is 'TEXTURE', but we want it to be written along with 'BRDF'
			VRayNodePluginExporter::exportPlugin("BRDF", "TexAColor", weight, weigthAttrs);
		}

		brdfs.push_back(brdf);
		weights.push_back(weight);
	}

	AttributeValueMap pluginAttrs;
	pluginAttrs["brdfs"]   = boost::str(boost::format("List(%s)") % boost::algorithm::join(brdfs, ","));
	pluginAttrs["weights"] = boost::str(boost::format("List(%s)") % boost::algorithm::join(weights, ","));

	pluginAttrs["transparency"]  = VRayNodeExporter::exportSocket(ntree, node, "Transparency");
	pluginAttrs["additive_mode"] = boost::str(boost::format("%i") % RNA_boolean_get(&node.ptr, "additive_mode"));

	VRayNodePluginExporter::exportPlugin("BRDF", "BRDFLayered", pluginName, pluginAttrs);

	return pluginName;
}
