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

#include "vfb_scene_exporter_pro.h"

#include <thread>
#include <chrono>


void ProductionExporter::create_exporter()
{
	SceneExporter::create_exporter();

	if (m_exporter) {
		m_exporter->set_is_viewport(false);
		m_exporter->set_settings(m_settings);
	}
}


void ProductionExporter::setup_callbacks()
{
	m_exporter->set_callback_on_image_ready(ExpoterCallback(boost::bind(&ProductionExporter::cb_on_image_ready, this)));
	m_exporter->set_callback_on_rt_image_updated(ExpoterCallback(boost::bind(&ProductionExporter::cb_on_image_ready, this)));
}


bool ProductionExporter::do_export()
{
	if (m_settings.settings_animation.use) {
		return export_animation();
	}
	else {
		sync(false);
		m_exporter->start();
		return true;
	}
}


void ProductionExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(m_scene, EvalMode::EvalModeRender);

	SceneExporter::sync_dupli(ob, check_updated);

	ob.dupli_list_clear();
}


void ProductionExporter::sync_object(BL::Object ob, const int &check_updated, const ObjectOverridesAttrs &override)
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
						psys.set_resolution(m_scene, ob, EvalModeRender);
						m_data_exporter.exportHair(ob, psm, psys, check_updated);
						psys.set_resolution(m_scene, ob, EvalModePreview);
					}
				}
			}
		}
	}
}


void ProductionExporter::render_start()
{
	SceneExporter::render_start();

	std::thread wait_render = std::thread([this] {
		while (!is_interrupted()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	});

	wait_render.join();
}


void ProductionExporter::cb_on_image_ready()
{
	PRINT_INFO_EX("ProductionExporter::on_image_ready()");

	RenderImage image = m_exporter->get_image();
	if (image) {
		image.flip();

		BL::RenderSettings renderSettings = m_scene.render();
		BL::RenderSettings::layers_iterator rslIt;
		for (renderSettings.layers.begin(rslIt); rslIt != renderSettings.layers.end(); ++rslIt) {
			BL::SceneRenderLayer sceneRenderLayer(*rslIt);
			if (sceneRenderLayer) {
				BL::RenderResult renderResult = m_engine.begin_result(0, 0, m_viewParams.renderSize.w, m_viewParams.renderSize.h, sceneRenderLayer.name().c_str(), nullptr);
				if (renderResult) {
					BL::RenderResult::layers_iterator rrlIt;
					renderResult.layers.begin(rrlIt);

					// Layer will be missing if it was disabled in the UI
					if (rrlIt != renderResult.layers.end()) {
						BL::RenderLayer renderLayer(*rrlIt);
						if (renderLayer) {
							BL::RenderLayer::passes_iterator rpIt;
							for (renderLayer.passes.begin(rpIt); rpIt != renderLayer.passes.end(); ++rpIt) {
								BL::RenderPass renderPass(*rpIt);
								if (renderPass) {
									if (renderPass.type() == BL::RenderPass::type_COMBINED) {
										renderPass.rect(image.pixels);
										break;
									}
								}
							}

							m_engine.update_result(renderResult);
						}
					}

					m_engine.end_result(renderResult, false, true);
				}
			}
		}

		image.free();
	}
}
