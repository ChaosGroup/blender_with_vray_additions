/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vfb_node_exporter.h"
#include "vfb_utils_nodes.h"

#include <boost/format.hpp>


AttrValue DataExporter::exportVRayNodeBRDFLayered(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &/*fromSocket*/, NodeContext &context)
{
	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);

	AttrListPlugin brdfs;
	AttrListPlugin weights;

	static boost::format sockBrdfFormat("BRDF %i");
	static boost::format sockWeigthFormat("Weight %i");
	static boost::format weighTexFormat(pluginName + "W%i");

	for (int i = 1; i <= CGR_MAX_LAYERED_BRDFS; ++i) {
		const std::string &brdfSockName = boost::str(sockBrdfFormat % i);
		const std::string &weigthSockName = boost::str(sockWeigthFormat % i);

		BL::NodeSocket brdfSock = Nodes::GetInputSocketByName(node, brdfSockName);
		if (brdfSock && brdfSock.is_linked()) {
			AttrValue brdf = exportLinkedSocket(ntree, brdfSock, context);
			AttrValue weight;

			BL::NodeSocket weightSock = Nodes::GetInputSocketByName(node, weigthSockName);
			if (weightSock.is_linked()) {
				weight = exportLinkedSocket(ntree, weightSock, context);
			}
			else {
				const std::string weightTexName = boost::str(weighTexFormat % i);

				PluginDesc weigthDesc(weightTexName, "TexAColor");
				weigthDesc.add("texture", AttrAColor(AttrColor(RNA_float_get(&weightSock.ptr, "value")), 1.0f));

				// NOTE: Plugin type is 'TEXTURE', but we want it to be written along with 'BRDF'
				weight = m_exporter->export_plugin(weigthDesc);
			}

			brdfs.append(brdf.valPlugin);
			weights.append(weight.valPlugin);
		}
	}

	AttrValue transparency = exportSocket(ntree, node, "Transparency", context);

	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "BRDFLayered");

	pluginDesc.add("brdfs", brdfs);
	pluginDesc.add("weights", weights);

	if (transparency) {
		pluginDesc.add("transparency", transparency);
	}

	pluginDesc.add("additive_mode", RNA_boolean_get(&node.ptr, "additive_mode"));

	return m_exporter->export_plugin(pluginDesc, true);
}


AttrValue DataExporter::exportVRayNodeBRDFVRayMtl(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "BRDFVRayMtl");

	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	PointerRNA brdfVRayMtl = RNA_pointer_get(&node.ptr, "BRDFVRayMtl");
	if (RNA_boolean_get(&brdfVRayMtl, "hilight_glossiness_lock")) {
		pluginDesc.add("hilight_glossiness", pluginDesc.get("reflect_glossiness")->attrValue);
	}

	return m_exporter->export_plugin(pluginDesc);
}
