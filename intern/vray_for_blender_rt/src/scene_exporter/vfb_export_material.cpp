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
#include "vfb_utils_blender.h"


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


AttrValue DataExporter::exportMtlMulti(BL::Object ob)
{
	AttrValue mtl;

	const int numMaterials = Blender::GetMaterialCount(ob);

	// Use single material
	if (numMaterials) {
		if (numMaterials == 1) {
			BL::Material ma(ob.material_slots[0].material());
			if (ma) {
				mtl = exportMaterial(ma, true);
			}
		}
		// Export MtlMulti
		else {
			AttrListPlugin mtls_list(numMaterials);
			AttrListInt    ids_list(numMaterials);

			int maIdx   = 0;
			int slotIdx = 0; // For cases with empty slots

			BL::Object::material_slots_iterator slotIt;
			for (ob.material_slots.begin(slotIt); slotIt != ob.material_slots.end(); ++slotIt, ++slotIdx) {
				BL::Material ma((*slotIt).material());
				if (ma) {
					(*ids_list)[maIdx]  = slotIdx;
					(*mtls_list)[maIdx] = exportMaterial(ma, true);
					maIdx++;
				}
			}

			PluginDesc mtlMultiDesc(ob.name(), "MtlMulti", "Mtl@");
			mtlMultiDesc.add("mtls_list", mtls_list);
			mtlMultiDesc.add("ids_list", ids_list);
			mtlMultiDesc.add("wrap_id", true);

			mtl = m_exporter->export_plugin(mtlMultiDesc);
		}
	}

	return mtl;
}
