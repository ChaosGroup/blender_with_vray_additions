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
#include "vfb_utils_blender.h"


AttrValue DataExporter::exportObject(BL::Object ob, bool check_updated)
{
	AttrPlugin  node;
	AttrPlugin  geom;
	AttrPlugin  mtl;

	bool is_updated      = check_updated ? ob.is_updated()      : true;
	bool is_data_updated = check_updated ? ob.is_updated_data() : true;

	const int num_materials = Blender::GetMaterialCount(ob);

	if (!(is_data_updated) || !(m_settings.export_meshes)) {
		// XXX: Check for valid mesh?
		geom = AttrPlugin(getMeshName(ob));
	}
	else {
		AttrValue geometry = exportGeomStaticMesh(ob);
		if (geometry) {
			geom = geometry.valPlugin;
		}
	}

	if (is_updated) {
		if (num_materials) {
			// Use single material
			if (num_materials == 1) {
				BL::Material ma(ob.material_slots[0].material());
				if (ma) {
					AttrValue material = exportMaterial(ma, true);
					if (!material.type == ValueTypePlugin) {
						PRINT_ERROR("Failed to export material: \"%s\"",
						            ma.name().c_str());
					}
					else {
						mtl = material.valPlugin;
					}
				}
			}
			// Export MtlMulti
			else {
				AttrListPlugin mtls_list(num_materials);
				AttrListInt    ids_list(num_materials);

				int maIdx   = 0;
				int slotIdx = 0; // For cases with empty slots

				BL::Object::material_slots_iterator slotIt;
				for (ob.material_slots.begin(slotIt); slotIt != ob.material_slots.end(); ++slotIt, ++slotIdx) {
					BL::Material ma((*slotIt).material());
					if (ma) {
						// Get material name only
						AttrValue material = exportMaterial(ma, true);
						if (!material.type == ValueTypePlugin) {
							PRINT_ERROR("Failed to export material: \"%s\"",
							            ma.name().c_str());
						}
						else {
							(*ids_list)[maIdx]  = slotIdx;
							(*mtls_list)[maIdx] = material.valPlugin;
							maIdx++;
						}
					}
				}

				PluginDesc mtlMultiDesc(ob.name(), "MtlMulti", "Mtl@");
				mtlMultiDesc.add("mtls_list", mtls_list);
				mtlMultiDesc.add("ids_list", ids_list);
				mtlMultiDesc.add("wrap_id", true);

				mtl = m_exporter->export_plugin(mtlMultiDesc);
			}
		}
		if (!mtl) {
			mtl = m_defaults.override_material
			      ? m_defaults.override_material.valPlugin
			      : m_defaults.default_material.valPlugin;
		}
	}

	if (geom && mtl && (is_updated || is_data_updated)) {
		PluginDesc nodeDesc(ob.name(), "Node", "Node@");
		nodeDesc.add("geometry", geom);
		nodeDesc.add("material", mtl);
		nodeDesc.add("transform", AttrTransform(ob.matrix_world()));
		nodeDesc.add("objectID", ob.pass_index());

		node = m_exporter->export_plugin(nodeDesc);
	}

	BL::Object::modifiers_iterator modIt;
	for (ob.modifiers.begin(modIt); modIt != ob.modifiers.end(); ++modIt) {
		BL::Modifier mod(*modIt);
		if (mod && mod.show_render() && mod.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
			BL::ParticleSystemModifier psm(mod);
			BL::ParticleSystem psys = psm.particle_system();
			if (psys) {
				BL::ParticleSettings pset(psys.settings());
				if (pset && (pset.type() == BL::ParticleSettings::type_HAIR) && (pset.render_type() == BL::ParticleSettings::render_type_PATH)) {
					const int hair_is_updated = check_updated
					                            ? (is_updated || pset.is_updated())
					                            : true;

					const int hair_is_data_updated = check_updated
					                                 ? (is_data_updated || pset.is_updated())
					                                 : true;

					AttrValue hair_geom;
					if (!(hair_is_data_updated) || !(m_settings.export_meshes)) {
						hair_geom = AttrPlugin(getHairName(ob, psys, pset));
					}
					else {
						AttrValue geometry = exportGeomMayaHair(ob, psys, psm);
						if (geometry) {
							hair_geom = geometry.valPlugin;
						}
					}

					AttrValue hair_mtl;
					const int hair_mtl_index = pset.material() - 1;
					if (ob.material_slots.length() && (hair_mtl_index < ob.material_slots.length())) {
						BL::Material hair_material = ob.material_slots[hair_mtl_index].material();
						if (hair_material) {
							hair_mtl = exportMaterial(hair_material, true);
						}
					}
					if (!hair_mtl) {
						hair_mtl = m_defaults.override_material
						           ? m_defaults.override_material.valPlugin
						           : m_defaults.default_material.valPlugin;
					}

					if (hair_geom && hair_mtl && (hair_is_updated || hair_is_data_updated)) {
						PluginDesc hairNodeDesc(getHairName(ob, psys, pset), "Node", "Node@");
						hairNodeDesc.add("geometry", hair_geom);
						hairNodeDesc.add("material", hair_mtl);
						hairNodeDesc.add("transform", AttrTransform(ob.matrix_world()));
						hairNodeDesc.add("objectID", ob.pass_index());

						// XXX: Put hair node to the object dependent plugines
						// (will be used to remove plugin when object is removed)
						//
						m_exporter->export_plugin(hairNodeDesc);
					}
				}
			}
		}
	}

	return node;
}
