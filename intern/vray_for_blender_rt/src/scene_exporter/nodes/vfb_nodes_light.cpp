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

AttrValue DataExporter::exportVRayNodeLightMesh(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	AttrValue attrValue;

	auto ob = context.object_context.object;

	if(!ob) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
		return attrValue;
	}

	BL::NodeSocket geomSock = Nodes::GetInputSocketByName(node, "Geometry");
	if(!geomSock.is_linked()) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Geometry socket is not linked!",
					ntree.name().c_str(), node.name().c_str());

		return attrValue;
	}

	const auto pluginName = "MeshLight@" + GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(pluginName, "LightMesh");
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	pluginDesc.add("geometry", exportLinkedSocket(ntree, geomSock, context));
	pluginDesc.add("transform", AttrTransformFromBlTransform(ob.matrix_world()));
	pluginDesc.add("use_tex", pluginDesc.contains("tex"));

	return m_exporter->export_plugin(pluginDesc);
}
