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

#include <Python.h>


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
	Py_BEGIN_ALLOW_THREADS

	if (m_settings.settings_animation.use) {
		sync(false);
		m_animationPythonThreadState = _save;
		m_isAnimationRunning = true;
		std::thread update(&ProductionExporter::render_start, this);

		while (!m_isRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		const auto restore = m_scene.frame_current();
		float progress = 0.f, frameProgress = 1.f / ((m_scene.frame_end() - m_scene.frame_start()) / m_scene.frame_step());

		for (auto fr = restore; fr < m_scene.frame_end() && res; fr += m_scene.frame_step()) {
			Py_BLOCK_THREADS
				m_scene.frame_set(fr, 0.f);
				m_engine.update_progress(progress);
			Py_UNBLOCK_THREADS

			res = export_animation();
			while (res && !m_renderFinished) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			progress += frameProgress;
			PRINT_INFO_EX("Animation progress %d%%", static_cast<int>(progress * 100));
		}

		m_scene.frame_set(restore, 0.f);
		m_isAnimationRunning = false;
		m_renderFinished = true;
		update.join();

	}
	else {
		sync(false);
	}

	Py_END_ALLOW_THREADS
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
	BL::RenderSettings renderSettings = m_scene.render();

	BL::RenderSettings::layers_iterator rslIt;
	renderSettings.layers.begin(rslIt);
	if (rslIt != renderSettings.layers.end()) {
		BL::SceneRenderLayer sceneRenderLayer(*rslIt);
		if (sceneRenderLayer && !is_interrupted()) {
			BL::RenderResult renderResult = m_engine.begin_result(0, 0, m_viewParams.renderSize.w, m_viewParams.renderSize.h, sceneRenderLayer.name().c_str(), nullptr);
			if (renderResult) {
				m_renderResultsList.push_back(renderResult);
			}
		}
	}

	if (!is_preview()) {
		m_exporter->show_frame_buffer();
	}

	if (m_settings.exporter_type != ExpoterType::ExpoterTypeFile) {
		SceneExporter::render_start();
		PyThreadState *_save = m_animationPythonThreadState;

		do {
			m_renderFinished = false;
			m_imageDirty = false;

			m_isRunning = true;
			auto start = std::chrono::high_resolution_clock::now();
			while (!is_interrupted() && !m_renderFinished) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				auto now = std::chrono::high_resolution_clock::now();

				if (std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() > (1000 / 10)) {
					if (m_imageDirty) {
						m_imageDirty = false;
						Py_BLOCK_THREADS
						for (auto & result : m_renderResultsList) {
							BL::RenderResult::layers_iterator rrlIt;
							result.layers.begin(rrlIt);
							if (rrlIt != result.layers.end()) {
								PRINT_INFO_EX("Updating image for layer \"%s\"", rrlIt->name().c_str());
								m_engine.update_result(result);
							}
						}
						Py_UNBLOCK_THREADS
					}
					start = std::chrono::high_resolution_clock::now();
				}
			}

			Py_BLOCK_THREADS
			for (auto & result : m_renderResultsList) {
				BL::RenderResult::layers_iterator rrlIt;
				result.layers.begin(rrlIt);
				if (rrlIt != result.layers.end()) {
					m_engine.update_result(result);
				}
			}
			Py_UNBLOCK_THREADS
		} while (m_isAnimationRunning);

		{
			std::lock_guard<std::mutex> l(m_callback_mtx);
			m_exporter->set_callback_on_image_ready(ExpoterCallback());
			m_exporter->set_callback_on_rt_image_updated(ExpoterCallback());
		}
	} else {
		SceneExporter::render_start();
	}


	for (auto & result : m_renderResultsList) {
		m_engine.end_result(result, false, true);
	}
}

ProductionExporter::~ProductionExporter()
{
	std::lock_guard<std::mutex> l(m_callback_mtx);
	delete m_exporter;
	m_exporter = nullptr;
}


void ProductionExporter::cb_on_image_ready()
{
	// PRINT_INFO_EX("ProductionExporter::cb_on_image_ready()");

	std::lock_guard<std::mutex> l(m_callback_mtx);
	m_renderFinished = true;
}

void ProductionExporter::cb_on_rt_image_updated()
{
	// PRINT_INFO_EX("ProductionExporter::cb_on_rt_image_updated()");
	std::lock_guard<std::mutex> l(m_callback_mtx);
	m_imageDirty = true;

	for (auto & result : m_renderResultsList) {
		BL::RenderResult::layers_iterator rrlIt;
		result.layers.begin(rrlIt);
		if (rrlIt != result.layers.end()) {
			BL::RenderLayer renderLayer(*rrlIt);
			if (renderLayer) {
				BL::RenderLayer::passes_iterator rpIt;
				for (renderLayer.passes.begin(rpIt); rpIt != renderLayer.passes.end(); ++rpIt) {
					BL::RenderPass renderPass(*rpIt);
					if (renderPass) {
						RenderImage image = m_exporter->get_pass(renderPass.type());
						if (image && image.w == m_viewParams.renderSize.w && image.h == m_viewParams.renderSize.h) {
							renderPass.rect(image.pixels);
						}
					}
				}

				if (is_preview()) {
					m_engine.update_result(result);
				}
			}
		}
	}
}
