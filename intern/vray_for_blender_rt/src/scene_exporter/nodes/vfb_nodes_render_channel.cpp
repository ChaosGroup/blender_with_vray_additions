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

using namespace VRayForBlender;

AttrValue DataExporter::exportVRayNodeRenderChannelLightSelect(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	// override plugin id to be "RenderChannelColor"
	const auto plgName = DataExporter::GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(plgName, "RenderChannelColor");

	PointerRNA chanGroup = RNA_pointer_get(&node.ptr, "RenderChannelLightSelect");

	auto type = static_cast<LightSelect::ChannelType>(RNA_enum_get(&chanGroup, "type"));

	BL::NodeSocket lightsSock = Nodes::GetInputSocketByName(node, "Lights");
	if (lightsSock && lightsSock.is_linked()) {
		BL::Node selectNode = Nodes::GetConnectedNode(lightsSock);
		if (selectNode) {
			ObList lights;
			getSelectorObjectList(selectNode, lights);
			for (const auto & lamp : lights) {
				m_light_select_channel.addLightSelect(lamp, type, plgName);
			}
		}
	}

	return exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}

AttrValue DataExporter::exportVRayNodeRenderChannelColor(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context), DataExporter::GetNodePluginID(node));
	return exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}
