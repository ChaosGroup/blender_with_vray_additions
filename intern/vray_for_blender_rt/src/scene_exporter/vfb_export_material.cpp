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


AttrValue DataExporter::exportMaterial(BL::Material ma, bool dont_export)
{
	AttrValue material = m_defaults.override_material
	                     ? m_defaults.override_material
	                     : m_defaults.default_material;

	if (ma) {
		BL::NodeTree ntree = Nodes::GetNodeTree(ma);
		if (ntree) {
			BL::Node output = Nodes::GetNodeByType(ntree, "VRayNodeOutputMaterial");
			if (output) {
				bool use_override = m_defaults.override_material && !(RNA_boolean_get(&output.ptr, "dontOverride"));
				if (!use_override) {
					BL::NodeSocket materialSock = Nodes::GetInputSocketByName(output, "Material");
					if (materialSock) {
						NodeContext ctx;
						material = dont_export
						           ? exportLinkedSocket(ntree, materialSock, &ctx, true)
						           : exportVRayNode(ntree, output, materialSock, &ctx);
					}
				}
			}
		}
	}

	return material;
}
