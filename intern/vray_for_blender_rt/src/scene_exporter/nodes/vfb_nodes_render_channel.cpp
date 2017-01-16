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

// TODO:
AttrValue DataExporter::exportVRayNodeRenderChannelLightSelect(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
#if 0
	const std::string &pluginName = NodeExporter::GetPluginName(node, ntree, context);

	PluginDesc pluginAttrs;
	NodeExporter::getVRayNodeAttributes(pluginAttrs, ntree, node, fromSocket, context);

	// We simply need to change the pluginID
	VRayNodePluginExporter::exportPlugin("RENDERCHANNEL", "RenderChannelColor", pluginName, pluginAttrs);

	return pluginName;
#endif
	return AttrValue();
}

// TODO:
AttrValue DataExporter::exportVRayNodeRenderChannelColor(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
#if 0
	PluginDesc pluginAttrs;
	NodeExporter::getVRayNodeAttributes(pluginAttrs, ntree, node, fromSocket, context);

	return NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, pluginAttrs);
#endif
	return AttrValue();
}
