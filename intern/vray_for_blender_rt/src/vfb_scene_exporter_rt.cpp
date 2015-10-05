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

#include "vfb_scene_exporter_rt.h"


void InteractiveExporter::create_exporter()
{
	if (m_settings.exporter_type == ExpoterType::ExpoterTypeFile) {
#if defined(USE_BLENDER_VRAY_APPSDK)
		m_settings.exporter_type = ExpoterType::ExpoterTypeAppSDK;
#elif defined(USE_BLENDER_VRAY_ZMQ)
		m_settings.exporter_type = ExpoterType::ExpoterTypeZMQ;
#else
		m_settings.exporter_type = ExpoterType::ExporterTypeInvalid;
#endif
	}

	SceneExporter::create_exporter();

	if (m_exporter) {
		m_exporter->set_is_viewport(true);
		m_exporter->set_settings(m_settings);
	}
}


void InteractiveExporter::setup_callbacks()
{
	m_exporter->set_callback_on_image_ready(ExpoterCallback(boost::bind(&InteractiveExporter::cb_on_image_ready, this)));
	m_exporter->set_callback_on_rt_image_updated(ExpoterCallback(boost::bind(&InteractiveExporter::cb_on_image_ready, this)));
}


bool InteractiveExporter::do_export()
{
	sync(false);
	m_exporter->start();
	return true;
}


void InteractiveExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(m_scene, EvalMode::EvalModePreview);

	SceneExporter::sync_dupli(ob, check_updated);

	ob.dupli_list_clear();
}


void InteractiveExporter::sync_object(BL::Object ob, const int &check_updated, const ObjectOverridesAttrs &override)
{
	bool add = false;
	if (override) {
		add = !m_data_exporter.m_id_cache.contains(override.id);
	}
	else {
		add = !m_data_exporter.m_id_cache.contains(ob);
	}

	if (add) {
		if (override) {
			m_data_exporter.m_id_cache.insert(override.id);
		}
		else {
			m_data_exporter.m_id_cache.insert(ob);
		}

		bool is_on_visible_layer = get_layer(ob.layers()) & get_layer(m_scene.layers());
		bool is_hidden = ob.hide() || ob.hide_render() || !is_on_visible_layer;

		if (!is_hidden || override) {
			BL::Object::modifiers_iterator modIt;
			SceneExporter::sync_object(ob, check_updated, override);

			for (ob.modifiers.begin(modIt); modIt != ob.modifiers.end(); ++modIt) {
				BL::Modifier mod(*modIt);
				if (mod && mod.show_render() && mod.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
					BL::ParticleSystemModifier psm(mod);
					BL::ParticleSystem psys = psm.particle_system();
					if (psys) {
						m_data_exporter.exportHair(ob, psm, psys, check_updated);
					}
				}
			}
		}
	}
}
