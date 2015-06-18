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


AttrValue DataExporter::exportVRayNodeLightMesh(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, NodeContext &context)
{
	AttrValue attrValue;

#if 0
	if(NOT(context.object_context.ob)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	BL::NodeSocket geomSock = NodeExporter::getSocketByName(node, "Geometry");
	if(NOT(geomSock.is_linked())) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Geometry socket is not linked!",
					ntree.name().c_str(), node.name().c_str());

		return "NULL";
	}

	char transform[CGR_TRANSFORM_HEX_SIZE];
	GetTransformHex(context.object_context.ob->obmat, transform);

	PluginDesc manualAttrs;
	manualAttrs["geometry"]  = NodeExporter::exportLinkedSocket(ntree, geomSock, context);
	manualAttrs["transform"] = BOOST_FORMAT_TM(transform);

	return NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, manualAttrs);
#endif

	return attrValue;
}
