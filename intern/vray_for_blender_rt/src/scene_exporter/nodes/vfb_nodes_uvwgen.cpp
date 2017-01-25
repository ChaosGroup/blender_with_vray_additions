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
#include "vfb_utils_math.h"
#include "vfb_utils_mesh.h"


AttrValue DataExporter::exportVRayNodeUVWGenChannel(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "UVWGenChannel");

	// NOTE: Why Python code was setting this?
	// manualAttrs["uvw_transform"] = mathutils.Matrix.Identity(4)

	return DataExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}


AttrValue DataExporter::exportVRayNodeUVWGenEnvironment(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PointerRNA UVWGenEnvironment = RNA_pointer_get(&node.ptr, "UVWGenEnvironment");

	const int mapping_type = RNA_enum_get(&UVWGenEnvironment, "mapping_type");

	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "UVWGenEnvironment");

	pluginDesc.add("mapping_type", EnvironmentMappingType[mapping_type]);

	return DataExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}


AttrValue DataExporter::exportVRayNodeUVWGenMayaPlace2dTexture(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "UVWGenMayaPlace2dTexture");

	BL::NodeSocket rotateFrameTexSock = Nodes::GetSocketByAttr(node, "rotate_frame_tex");
	if (rotateFrameTexSock && NOT(rotateFrameTexSock.is_linked())) {
		const float rotate_frame_tex = DEG_TO_RAD(RNA_float_get(&rotateFrameTexSock.ptr, "value"));
		pluginDesc.add("rotate_frame_tex", rotate_frame_tex);
	}

	// Export attributes automatically from node
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	// TODO: Check for type, may be color set is selected
	PluginAttr *uv_set_name = pluginDesc.get("uv_set_name");
	if (uv_set_name) {
		uv_set_name->attrValue.valString = boost::str(boost::format(VRayForBlender::Mesh::UvChanNameFmt) % uv_set_name->attrValue.valString);
	}
	else {
		pluginDesc.add("uv_set_name", "UvUVMap");
	}

	return m_exporter->export_plugin(pluginDesc);
}
