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

#include "RE_pipeline.h"
#include "RE_engine.h"
#include "render_types.h"

#include <inttypes.h>

#include <thread>
#include <chrono>

#include <Python.h>

using namespace std;
using namespace std::chrono;

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
	m_exporter->set_callback_on_bucket_ready(boost::bind(&ProductionExporter::cb_on_bucket_ready, this, boost::placeholders::_1));
}

int	ProductionExporter::is_interrupted()
{
	bool is_interrupted = SceneExporter::is_interrupted();

	if (m_settings.settings_animation.use) {
		is_interrupted = is_interrupted || !m_isAnimationRunning;
	} else {
		is_interrupted = is_interrupted || m_renderFinished;
	}

	return is_interrupted;
}

bool ProductionExporter::export_animation_frame(const int &check_updated)
{
	bool frameExported = true;

	m_settings.settings_animation.frame_current = m_frameCurrent;
	m_exporter->set_current_frame(m_frameCurrent);

	if (m_settings.exporter_type == ExpoterType::ExpoterTypeFile) {
		PRINT_INFO_EX("Exporting animation frame %d, in file", m_frameCurrent);
		sync(check_updated);
	} else {
		PRINT_INFO_EX("Exporting animation frame %d", m_frameCurrent);

		m_exporter->stop();
		sync(check_updated);
		if (m_isFirstFrame) {
			render_start();
		}
		m_exporter->start();
		PRINT_INFO_EX("Waiting for renderer to render animation frame %d, current %f", m_frameCurrent, m_exporter->get_last_rendered_frame());

		auto lastTime = high_resolution_clock::now();
		while (m_exporter->get_last_rendered_frame() < m_frameCurrent) {
			this_thread::sleep_for(milliseconds(1));

			auto now = high_resolution_clock::now();
			if (duration_cast<seconds>(now - lastTime).count() > 1) {
				lastTime = now;
				PRINT_INFO_EX("Waiting for renderer to render animation frame %d, current %f", m_frameCurrent, m_exporter->get_last_rendered_frame());
			}
			if (this->is_interrupted()) {
				PRINT_INFO_EX("Interrupted - stopping animation rendering!");
				frameExported = false;
				break;
			}
			if (m_exporter->is_aborted()) {
				PRINT_INFO_EX("Renderer stopped - stopping animation rendering!");
				frameExported = false;
				break;
			}
		}
	}

	return frameExported;
}

bool ProductionExporter::do_export()
{
	PRINT_INFO_EX("ProductionExporter::do_export()");
	bool res = true;
	const bool is_file_export = m_settings.exporter_type == ExpoterType::ExpoterTypeFile;

	if (is_file_export) {
		python_thread_state_restore();
	}

	if (m_settings.settings_animation.use) {
		m_isAnimationRunning = true;

		const bool is_camera_loop = m_settings.settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop;

		std::vector<BL::Camera> loop_cameras;

		if (is_camera_loop) {
			BL::Scene::objects_iterator obIt;
			for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
				BL::Object ob(*obIt);
				if (ob.type() == BL::Object::type_CAMERA) {
					auto dataPtr = ob.data().ptr;
					PointerRNA vrayCamera = RNA_pointer_get(&dataPtr, "vray");
					if (RNA_boolean_get(&vrayCamera, "use_camera_loop")) {
						loop_cameras.push_back(BL::Camera(ob));
					}
				}
			}

			std::sort(loop_cameras.begin(), loop_cameras.end(), [](const BL::Camera & l, const BL::Camera & r) {
				return const_cast<BL::Camera&>(l).name() < const_cast<BL::Camera&>(r).name();
			});

			m_frameCount = loop_cameras.size();
			m_frameStep = 1;
			m_frameCurrent = 0;
		} else {
			m_frameCurrent = m_scene.frame_start();
			m_frameStep = m_scene.frame_step();
			m_frameCount = (m_scene.frame_end() - m_scene.frame_start()) / m_frameStep;
		}

		const auto restore = m_scene.frame_current();
		m_animationProgress = 0.f;


		std::thread runner;
		if (!is_file_export && m_settings.work_mode != ExporterSettings::WorkMode::WorkModeExportOnly) {
			runner = std::thread(&ProductionExporter::render_loop, this);
		}

		for (int c = 0; c < m_frameCount && res && !is_interrupted(); ++c) {
			if (is_camera_loop) {
				m_active_camera = loop_cameras[c];
			}
			m_isFirstFrame = c == 0;
			// make first frame for camera loop be 1
			m_frameCurrent = m_frameStep * (c + is_camera_loop);
			m_animationProgress = (float)c / m_frameCount;

			if (!is_file_export) {
				std::lock_guard<std::mutex> l(m_python_state_lock);
				if (is_interrupted()) {
					break;
				}
				python_thread_state_restore();
				if (!is_camera_loop) {
					m_scene.frame_set(m_frameCurrent, 0.f);
				}
				python_thread_state_save();
			} else {
				if (!is_camera_loop) {
					m_scene.frame_set(m_frameCurrent, 0.f);
				}
				m_engine.update_progress(m_animationProgress);
				PRINT_INFO_EX("Animation progress %d%%, frame %d", static_cast<int>(m_animationProgress * 100), m_frameCurrent);
			}

			res = export_animation_frame(false);
			while (!is_file_export && res && !m_renderFinished && !is_interrupted()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		m_isAnimationRunning = false;
		m_renderFinished = true;

		if (is_file_export) {
			m_scene.frame_set(restore, 0.f);
		} else {
			runner.join();
			python_thread_state_restore();
			m_scene.frame_set(restore, 0.f);
			python_thread_state_save();
			render_end();
		}
	}
	else {
		sync(false);
	}

	if (is_file_export) {
		python_thread_state_save();
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


void ProductionExporter::render_frame()
{
	if (!m_isRunning) {
		return;
	}

	const float frame_contrib = 1.f / m_frameCount;

	auto now = high_resolution_clock::now();
	if (duration_cast<milliseconds>(now - m_lastReportTime).count() > 1000) {
		m_lastReportTime = now;
		PRINT_INFO_EX("Rendering progress: this frame %d [%d%%], total[%d%%]", m_frameCurrent, (int)(m_progress * 100), (int)((m_animationProgress + m_progress * frame_contrib) * 100));
	}

	std::unique_lock<std::mutex> uLock(m_python_state_lock, std::defer_lock);

	if (m_imageDirty) {
		m_imageDirty = false;
		float progress = 0.f;
		if (m_settings.settings_animation.use) {
			uLock.lock();
			if (is_interrupted()) {
				return;
			}
			python_thread_state_restore();

			// for animation add frames progress + current image progress * frame contribution
			progress = m_animationProgress + m_progress * frame_contrib;
		} else {
			// for singe frame - get progress from image
			progress = m_progress;
		}

		m_engine.update_progress(progress);
		for (auto & result : m_renderResultsList) {
			if (result.layers.length() > 0) {
				m_engine.update_result(result);
			}
		}

		if (m_settings.settings_animation.use) {
			python_thread_state_save();
			uLock.unlock();
		}
	}
}

void ProductionExporter::render_loop()
{
	m_lastReportTime = std::chrono::high_resolution_clock::now();
	while (!is_interrupted()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		render_frame();
	}
}

void ProductionExporter::render_start()
{
	if (m_settings.exporter_type == ExpoterType::ExpoterTypeFile || m_settings.work_mode == ExporterSettings::WorkMode::WorkModeExportOnly) {
		return SceneExporter::render_start();
	}

	if (m_settings.settings_animation.use && !m_isAnimationRunning) {
		return;
	}

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

	if (m_settings.showViewport) {
		m_exporter->show_frame_buffer();
	}


	m_isRunning = true;

	if (!m_settings.settings_animation.use &&
		m_settings.work_mode != ExporterSettings::WorkMode::WorkModeExportOnly &&
		m_settings.exporter_type != ExpoterType::ExpoterTypeFile) {

		SceneExporter::render_start();
		m_frameCount = m_frameCurrent = m_frameStep = 1;
		m_progress = 0;
		render_loop();
		render_end();
	}
}

void ProductionExporter::render_end()
{
	if (m_settings.exporter_type != ExpoterType::ExpoterTypeFile) {
		std::lock_guard<std::mutex> l(m_callback_mtx);
		m_exporter->stop();
		m_exporter->set_callback_on_image_ready(ExpoterCallback());
		m_exporter->set_callback_on_rt_image_updated(ExpoterCallback());
		m_exporter->set_callback_on_bucket_ready([](const VRayBaseTypes::AttrImage &) {});
	}
	python_thread_state_restore();
	for (auto & result : m_renderResultsList) {
		m_engine.end_result(result, false, true);
	}
	python_thread_state_save();
}

ProductionExporter::~ProductionExporter()
{
	{
		std::lock_guard<std::mutex> l(m_python_state_lock);
		if (m_settings.settings_animation.use) {
			m_isAnimationRunning = false;
		}
		if (m_python_thread_state) {
			python_thread_state_restore();
		}
	}
	{
		std::lock_guard<std::mutex> l(m_callback_mtx);
		delete m_exporter;
		m_exporter = nullptr;
	}
}


void ProductionExporter::cb_on_image_ready()
{
	std::lock_guard<std::mutex> l(m_callback_mtx);
	m_renderFinished = true;
}


void ProductionExporter::cb_on_bucket_ready(const VRayBaseTypes::AttrImage & img)
{
	BLI_assert(img.isBucket() && "Image for cb_on_bucket_ready is not bucket image");
	if (!img.isBucket()) {
		return;
	}
	std::lock_guard<std::mutex> l(m_callback_mtx);
	m_imageDirty = true;

	for (auto & result : m_renderResultsList) {
		for (int c = 0; c < result.layers.length(); ++c) {
			for (int r = 0; r < result.layers[c].passes.length(); ++r) {
				auto & pass = result.layers[c].passes[r];
				if (pass.type() == BL::RenderPass::type_COMBINED) {
					m_progress += static_cast<float>(img.width * img.height) / std::max(1, result.resolution_x() * result.resolution_y());
					auto * bPass = reinterpret_cast<RenderPass*>(pass.ptr.data);
					RenderImage::updateImageRegion(bPass->rect, bPass->rectx, bPass->recty, img.x, img.y, reinterpret_cast<const float *>(img.data.get()), img.width, img.height, bPass->channels);
					break;
				}
			}
		}
	}
}

void ProductionExporter::cb_on_rt_image_updated()
{
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
							auto resx = result.resolution_x();
							auto resy = result.resolution_y();

							if (resx != image.w || resy != image.h) {
								image.cropTo(resx, resy);
							}

							renderPass.rect(image.pixels);
						}
					}
				}

				if (is_preview()) {
					python_thread_state_restore();
					m_engine.update_result(result);
					python_thread_state_save();
				}
			}
		}
	}
}
