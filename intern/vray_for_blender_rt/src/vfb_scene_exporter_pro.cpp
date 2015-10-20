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
	m_exporter->set_callback_on_rt_image_updated(ExpoterCallback(boost::bind(&ProductionExporter::cb_on_rt_image_updated, this)));
}


bool ProductionExporter::do_export()
{
	bool res = true;

	if (m_settings.settings_animation.use) {
		res = export_animation();
	}
	else {
		sync(false);
	}

	return res;
}


void ProductionExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(m_scene, EvalMode::EvalModeRender);

	SceneExporter::sync_dupli(ob, check_updated);

	ob.dupli_list_clear();
}

void ProductionExporter::sync_object_modiefiers(BL::Object ob, const int &check_updated)
{
	BL::Object::modifiers_iterator modIt;
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


void ProductionExporter::render_start()
{
	SceneExporter::render_start();

	if (!is_preview()) {
		m_exporter->show_frame_buffer();
	}

	if (m_settings.exporter_type != ExpoterType::ExpoterTypeFile) {
		m_renderFinished = false;

		std::thread wait_render = std::thread([this] {
			while (!(is_interrupted() || m_renderFinished)) {
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
		});

		wait_render.join();
	}
}


void ProductionExporter::cb_on_image_ready()
{
	// PRINT_INFO_EX("ProductionExporter::cb_on_image_ready()");

	m_renderFinished = true;
}


void ProductionExporter::cb_on_rt_image_updated()
{
	// PRINT_INFO_EX("ProductionExporter::cb_on_rt_image_updated()");

	BL::RenderSettings renderSettings = m_scene.render();

	BL::RenderSettings::layers_iterator rslIt;
	renderSettings.layers.begin(rslIt);
	if (rslIt != renderSettings.layers.end()) {
		BL::SceneRenderLayer sceneRenderLayer(*rslIt);
		if (sceneRenderLayer) {
			BL::RenderResult renderResult = m_engine.begin_result(0, 0, m_viewParams.renderSize.w, m_viewParams.renderSize.h, sceneRenderLayer.name().c_str(), nullptr);
			if (renderResult) {
				BL::RenderResult::layers_iterator rrlIt;
				renderResult.layers.begin(rrlIt);
				if (rrlIt != renderResult.layers.end()) {
					BL::RenderLayer renderLayer(*rrlIt);
					if (renderLayer) {
						BL::RenderLayer::passes_iterator rpIt;
						for (renderLayer.passes.begin(rpIt); rpIt != renderLayer.passes.end(); ++rpIt) {
							BL::RenderPass renderPass(*rpIt);
							if (renderPass) {
								RenderImage image = m_exporter->get_pass(renderPass.type());
								if (image) {
									image.flip();
									image.resetAlpha();
									image.clamp(1.0f, 1.0f);

									renderPass.rect(image.pixels);

									image.free();
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
}
