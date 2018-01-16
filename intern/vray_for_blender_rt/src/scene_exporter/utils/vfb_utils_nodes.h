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


#ifndef VRAY_FOR_BLENDER_UTILS_NODES_H
#define VRAY_FOR_BLENDER_UTILS_NODES_H

#include "vfb_rna.h"


namespace VRayForBlender {

enum VRayNodeSocketType {
	vrayNodeSocketUnknown = -1,
	vrayNodeSocketBRDF = 0,
	vrayNodeSocketColor,
	vrayNodeSocketColorNoValue,
	vrayNodeSocketCoords,
	vrayNodeSocketEffect,
	vrayNodeSocketEnvironment,
	vrayNodeSocketEnvironmentOverride,
	vrayNodeSocketFloat,
	vrayNodeSocketFloatColor,
	vrayNodeSocketFloatNoValue,
	vrayNodeSocketInt,
	vrayNodeSocketMtl,
	vrayNodeSocketObject,
	vrayNodeSocketTransform,
	vrayNodeSocketVector,
	vrayNodeSocketPlugin,
};

std::string getVRayNodeSocketTypeName(BL::NodeSocket socket);
VRayNodeSocketType getVRayNodeSocketType(BL::NodeSocket socket);

namespace Nodes {

BL::NodeTree  GetNodeTree(BL::ID & id, const std::string &attr="ntree");
BL::NodeTree  GetGroupNodeTree(BL::Node group_node);

BL::NodeSocket  GetSocketByAttr(BL::Node node, const std::string &attrName);

BL::NodeSocket  GetInputSocketByName(BL::Node node, const std::string &socketName);
BL::NodeSocket  GetOutputSocketByName(BL::Node node, const std::string &socketName);

BL::Node        GetConnectedNode(BL::NodeSocket socket);
BL::NodeSocket  GetConnectedSocket(BL::NodeSocket socket);

BL::Node  GetNodeByType(BL::NodeTree nodeTree, const std::string &nodeType);

} // namespace Nodes
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_NODES_H
