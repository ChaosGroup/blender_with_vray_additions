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
#include "vfb_params_json.h"
#include "utils/cgr_string.h"


void DataExporter::clearMaterialCache()
{
	m_exported_materials.clear();
}


AttrValue DataExporter::getDefaultMaterial()
{
	return m_defaults.override_material
	        ? m_defaults.override_material
	        : m_defaults.default_material;
}


AttrValue DataExporter::exportMaterial(BL::Material ma, BL::Object ob)
{
	AttrValue material = getDefaultMaterial();

	if (ma) {
		auto iter = m_exported_materials.find(ma);
		if (iter != m_exported_materials.end()) {
			material = iter->second;
		}
		else {
			BL::NodeTree ntree(Nodes::GetNodeTree(ma));
			if (ntree) {
				BL::Node output(Nodes::GetNodeByType(ntree, "VRayNodeOutputMaterial"));
				if (output) {
					const bool use_override = m_defaults.override_material && !(RNA_boolean_get(&output.ptr, "dontOverride"));
					if (!use_override) {
						BL::NodeSocket materialSock(Nodes::GetInputSocketByName(output, "Material"));
						if (materialSock && materialSock.is_linked()) {
							NodeContext ctx(PointerRNA_NULL, PointerRNA_NULL, ob);

							bool needExport = true;
							if (m_is_export_selected && m_is_preview) {
								BL::Node selected = getNtreeSelectedNode(ntree);
								if (selected && ob.name().find("preview_") != std::string::npos) {
									BL::Node  conNode(PointerRNA_NULL);
									AttrValue val;
									const auto nodeClass = selected.bl_idname();
									exportLinkedSocketEx2(ntree, materialSock, ctx, DataExporter::ExpModePlugin, conNode, val, selected);

									auto pluginType = GetNodePluginType(selected);
									using PT = ParamDesc::PluginType;

									if (pluginType == PT::PluginBRDF || pluginType == PT::PluginMaterial) {
										PRINT_INFO_EX("Exporting selected material node only %s", nodeClass.c_str());
										material = val;
										needExport = false;
									} else if (pluginType == PT::PluginTexture) {
										PRINT_INFO_EX("Exporting selected texture node only %s", nodeClass.c_str());
										PluginDesc brdfLightWrapper("BRDFLightWrapper@" + val.valPlugin.plugin, "BRDFLight");
										brdfLightWrapper.add("color", val);
										material = m_exporter->export_plugin(brdfLightWrapper);
										needExport = false;
									} else {
										PRINT_INFO_EX("Selected node is of unsupported type!");
									}
								}
							}
							if (needExport) {
								material = exportLinkedSocket(ntree, materialSock, ctx);
							}

							// If connected node is not of 'MATERIAL' type we need to wrap it with it for GPU
							if (material.type == ValueTypePlugin && material.valPlugin && material.valPlugin.plugin.find("Mtl") == std::string::npos) {

								const std::string wrapper_name = "MtlSingleBRDF@" + StripString(material.valPlugin.plugin);
								PluginDesc mtlSingleWrapper(wrapper_name, "MtlSingleBRDF");
								mtlSingleWrapper.add("brdf", material.valPlugin);

								// PRINT_INFO_EX("Wrapping BRDF in single material %s", wrapper_name.c_str());

								material = m_exporter->export_plugin(mtlSingleWrapper);
							}

							if (m_exporter->get_is_viewport()) {
								PluginDesc genericWrapper("MtlRenderStats@" + ntree.name(), "MtlRenderStats");
								genericWrapper.add("base_mtl", material.valPlugin);
								material = m_exporter->export_plugin(genericWrapper);
							}

							m_exported_materials.insert(std::make_pair(ma, material));
						}
					}
				}
			}
		}
	}

	return material;
}


void DataExporter::fillMtlMulti(BL::Object ob, PluginDesc &mtlMultiDesc)
{
	const int numMaterials = Blender::GetMaterialCount(ob);

	AttrListPlugin mtls_list(numMaterials);
	AttrListInt    ids_list(numMaterials);

	int maIdx   = 0;
	int slotIdx = 0; // For cases with empty slots

	BL::Object::material_slots_iterator slotIt;
	for (ob.material_slots.begin(slotIt); slotIt != ob.material_slots.end(); ++slotIt, ++slotIdx) {
		BL::Material ma((*slotIt).material());
		if (ma) {
			(*ids_list)[maIdx]  = slotIdx;
			(*mtls_list)[maIdx] = exportMaterial(ma, ob);
			maIdx++;
		}
	}

	mtlMultiDesc.add("mtls_list", mtls_list);
	mtlMultiDesc.add("ids_list", ids_list);
}


AttrValue DataExporter::exportSingleMaterial(BL::Object &ob)
{
	AttrValue mtl = getDefaultMaterial();

	BL::Object::material_slots_iterator slotIt;
	for (ob.material_slots.begin(slotIt); slotIt != ob.material_slots.end(); ++slotIt) {
		BL::Material ma((*slotIt).material());
		if (ma) {
			mtl = exportMaterial(ma, ob);
			break;
		}
	}

	return mtl;
}


AttrValue DataExporter::exportMtlMulti(BL::Object ob)
{
	AttrValue mtl = getDefaultMaterial();

	const int numMaterials = Blender::GetMaterialCount(ob);
	if (numMaterials) {
		// Use single material
		if (numMaterials == 1) {
			mtl = exportSingleMaterial(ob);
		}
		// Export MtlMulti
		else {
			PluginDesc mtlMultiDesc(ob.name(), "MtlMulti", "Mtl@");
			fillMtlMulti(ob, mtlMultiDesc);
			mtlMultiDesc.add("wrap_id", true);

			mtl = m_exporter->export_plugin(mtlMultiDesc);
		}
	}

	return mtl;
}
