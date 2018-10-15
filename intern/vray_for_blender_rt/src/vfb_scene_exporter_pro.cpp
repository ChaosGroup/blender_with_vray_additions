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
#include "utils/vfb_utils_string.h"

#include "vfb_plugin_exporter_zmq.h"

#include <inttypes.h>

#include <thread>
#include <chrono>
#include <boost/filesystem.hpp>

#include <Python.h>
#include "BKE_global.h"

using namespace std;
using namespace std::chrono;

using namespace VRayForBlender;

void ProductionExporter::create_exporter()
{
	SceneExporter::create_exporter();

	if (m_exporter) {
		m_exporter->set_is_viewport(false);
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

	if (m_settings.exporter_type == ExporterType::ExpoterTypeZMQ) {
		is_interrupted = is_interrupted || !ZmqServer::isRunning();
	}

	if (m_settings.settings_animation.use) {
		is_interrupted = is_interrupted || !m_isAnimationRunning;
	} else {
		is_interrupted = is_interrupted || m_renderFinished;
	}

	return is_interrupted;
}

bool ProductionExporter::wait_for_frame_render()
{
	bool stop = false;
	m_exporter->start();
	if (m_settings.settings_animation.use) {
		PRINT_INFO_EX("Waiting for renderer to render animation frame %f, current %f", m_frameExporter.getCurrentRenderFrame(), m_exporter->get_last_rendered_frame());
	}

	auto lastTime = high_resolution_clock::now();
	while (m_exporter->get_last_rendered_frame() != m_frameExporter.getCurrentRenderFrame()) {
		this_thread::sleep_for(milliseconds(1));

		auto now = high_resolution_clock::now();
		if (duration_cast<seconds>(now - lastTime).count() > 1) {
			lastTime = now;
			if (m_settings.settings_animation.use) {
				PRINT_INFO_EX("Waiting for renderer to render animation frame %f, current %f", m_frameExporter.getCurrentRenderFrame(), m_exporter->get_last_rendered_frame());
			}
		}
		if (is_interrupted()) {
			PRINT_INFO_EX("Interrupted - stopping animation rendering!");
			stop = true;
			break;
		}
		if (m_exporter->is_aborted()) {
			PRINT_INFO_EX("Renderer stopped - stopping animation rendering!");
			stop = true;
			break;
		}
	}

	return !stop;
}


void ProductionExporter::for_each_exported_frame(FrameExportManager & frameExp, bool &isFirstExport, bool isFileExport)
{
	using AnimMode = SettingsAnimation::AnimationMode;

	const auto aMode = m_settings.settings_animation.mode;
	{
		std::unique_lock<std::mutex> uLock(m_python_state_lock, std::defer_lock);
		std::unique_lock<PythonGIL> lock(m_pyGIL, std::defer_lock);
		if (!isFileExport) {
			std::lock(uLock, lock);
		}

		const FrameExportManager::BlenderFramePair sceneFramePair(m_scene.frame_current(), m_scene.frame_subframe());
		const auto setFramePair = FrameExportManager::floatFrameToBlender(frameExp.getCurrentFrame());

		if (sceneFramePair != setFramePair) {
			FrameExportManager::changeSceneFrame(m_scene, m_data, setFramePair);
		}

		if (aMode == AnimMode::AnimationModeCameraLoop) {
			m_active_camera = frameExp.getActiveCamera();
		}
	}
	// set this on the settings obj so it is accessible from data exporter
	m_settings.settings_animation.frame_current = frameExp.getCurrentFrame();

	// set the frame to export (so values are inserted for that time)
	if (aMode == AnimMode::AnimationModeCameraLoop) {
		// for camera loop render frames == export frames
		// and also export frame is constant
		m_exporter->set_current_frame(frameExp.getCurrentRenderFrame() + 1); // frames are 1 based
	} else {
		m_exporter->set_current_frame(frameExp.getCurrentFrame());
	}

	if (!isFirstExport && aMode == AnimMode::AnimationModeFullNoGeometry) {
		m_settings.export_meshes = false;
	}

	// for camera-loop we could just sync view, but then we will miss camera's hide lists which
	// are exported as MtlRenderStats
	const bool onlyView = !isFirstExport &&
		(aMode == AnimMode::AnimationModeFullCamera ||
		 (aMode == AnimMode::AnimationModeCameraLoop && !m_settings.use_hide_from_view)
	);

	if (onlyView) {
		sync_view(false);
	} else {
		// sync(!isFirstExport);
		sync(false); // TODO: can we make blender keep the updated/data_updated tag?
	}

	isFirstExport = false;
}

bool ProductionExporter::export_scene(const bool)
{
	m_frameExporter.updateFromSettings(m_scene);
	SceneExporter::export_scene(false);

	const bool isFileExport = m_settings.exporter_type == ExporterType::ExpoterTypeFile;

	std::unique_lock<PythonGIL> fileExportLock(m_pyGIL, std::defer_lock);
	const bool actualRendering = !isFileExport && m_settings.work_mode != ExporterSettings::WorkMode::WorkModeExportOnly;

	m_isAnimationRunning = m_settings.settings_animation.use;
	if (isFileExport) {
		fileExportLock.lock();
	}

	std::thread renderThread;
	if (actualRendering) {
		render_start();
		renderThread = std::thread(&ProductionExporter::render_loop, this);
	}

	double totalSyncTime = 0.;
	const int renderFrames = m_frameExporter.getRenderFrameCount();
	bool isFirstExport = true;
	bool firstFrame = true;
	for (int c = 0; c < renderFrames; ++c) {
		const clock_t frameBeginTime = clock();

		if (!firstFrame && m_settings.settings_animation.mode == SettingsAnimation::AnimationModeFrameByFrame) {
			m_viewParams = {};
			// call reset on vray
			m_exporter->reset();
			// clear all cached plugins
			m_exporter->getPluginManager().clear();
			// reset all caches and id track
			m_data_exporter.reset();
		}

		// export current render frame data
		m_frameExporter.forEachExportFrame([this, &isFirstExport, isFileExport](FrameExportManager & frameExp) {
			bool isFirstCopy = isFirstExport;
			for_each_exported_frame(frameExp, isFirstExport, isFileExport);
			// export background if we have
			if (m_settings.background_scene) {
				BL::Scene main = m_scene;
				m_scene = m_settings.background_scene;
				// pass isFirstCopy here because for_each_exported_frame will change it
				// but 'first' export should be first for both scenes
				for_each_exported_frame(frameExp, isFirstCopy, isFileExport);
				m_scene = main;
			}
			return true;
		});

		m_exporter->set_current_frame(m_frameExporter.getCurrentRenderFrame());

		if (m_exporter->get_commit_state() != VRayBaseTypes::CommitAction::CommitAutoOn) {
			m_exporter->commit_changes();
		}

		const clock_t frameEndTime = clock();
		const double frameSyncSeconds = double(frameEndTime - frameBeginTime) / CLOCKS_PER_SEC;
		totalSyncTime += frameSyncSeconds;

		// wait render for current frame only
		if (actualRendering) {
			PRINT_INFO_EX("Frame sync time %.3f sec.", frameSyncSeconds);
			if (!wait_for_frame_render()) {
				break;
			}
		}
		firstFrame = false;
	}

	m_data_exporter.flushInstancerData();

	PRINT_INFO_EX("Total sync time %.3f sec.", totalSyncTime);

	if (!isFileExport) {
		std::unique_lock<std::mutex> uLock(m_python_state_lock, std::defer_lock);
		std::unique_lock<PythonGIL> lock(m_pyGIL, std::defer_lock);
		std::lock(uLock, lock);
		m_frameExporter.reset(m_scene, m_data);
	} else {
		m_frameExporter.reset(m_scene, m_data);
	}

	m_isAnimationRunning = false;
	m_renderFinished = true;

	// Export stuff after sync
	if (m_settings.work_mode == ExporterSettings::WorkMode::WorkModeExportOnly ||
		m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRenderAndExport) {
		// TODO: handle this per frame for FrameByFrame
		std::string vrsceneDest;
		const std::string & blendPath = m_data.filepath();

		switch (m_settings.settings_files.output_type) {
		case SettingsFiles::OutputDirType::OutputDirTypeTmp:
			vrsceneDest = (boost::filesystem::temp_directory_path() / "appsdk.vrscene").string();
			break;
		case SettingsFiles::OutputDirType::OutputDirTypeUser:
			vrsceneDest = VRayForBlender::String::AbsFilePath(m_settings.settings_files.output_dir, blendPath) + "/appsdk.vrscene";
			break;
		case SettingsFiles::OutputDirType::OutputDirTypeScene:
		default:
			if (!m_data.filepath().empty()) {
				vrsceneDest = boost::filesystem::path(m_data.filepath()).replace_extension(".vrscene").string();
			} else {
				vrsceneDest = (boost::filesystem::temp_directory_path() / "appsdk.vrscene").string();
			}
			break;
		}

		m_exporter->export_vrscene(vrsceneDest);
	}

	if (actualRendering) {
		VFB_Assert(renderThread.joinable() && "Render thread not joinable");
		renderThread.join();
		render_end();
	}

	return true;
}


void ProductionExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(G_MAIN, m_scene, EvalMode::EvalModeRender);

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
				psys.set_resolution(G_MAIN, m_scene, ob, EvalModeRender);
				m_data_exporter.exportHair(ob, psm, psys, check_updated);
				psys.set_resolution(G_MAIN, m_scene, ob, EvalModePreview);
			}
		}
	}
}


void ProductionExporter::draw()
{
	if (!m_isRunning) {
		return;
	}

	auto now = high_resolution_clock::now();
	if (duration_cast<milliseconds>(now - m_lastReportTime).count() > 1000) {
		m_lastReportTime = now;
		if (m_settings.settings_animation.use) {
			int progress = 100.f * m_exporter->get_progress();
			PRINT_INFO_EX("Rendering progress frame: %f [%d%%]", m_frameExporter.getCurrentRenderFrame(), progress);
		}
	}

	std::unique_lock<std::mutex> uLock(m_python_state_lock, std::defer_lock);
	std::unique_lock<PythonGIL> pyLock(m_pyGIL, std::defer_lock);

	if (m_imageDirty) {
		m_imageDirty = false;
		if (m_settings.settings_animation.use) {
			std::lock(uLock, pyLock);
			if (is_interrupted()) {
				return;
			}
		}

		if (m_engine) {
			m_engine.update_progress(m_exporter->get_progress());
			for (auto & result : m_renderResultsList) {
				if (result.layers.length() > 0) {
					m_engine.update_result(result);
				}
			}
		}
	}
}

void ProductionExporter::render_loop()
{
	m_lastReportTime = std::chrono::high_resolution_clock::now();
	while (!is_interrupted()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		draw();
	}
}

void ProductionExporter::render_start()
{
	if (m_settings.exporter_type == ExporterType::ExpoterTypeFile || m_settings.work_mode == ExporterSettings::WorkMode::WorkModeExportOnly) {
		return SceneExporter::render_start();
	}

	if (m_settings.settings_animation.use && !m_isAnimationRunning) {
		return;
	}

	const auto viewParams = get_current_view_params();

	BL::RenderSettings renderSettings = m_scene.render();

	if (m_engine) {
		BL::RenderSettings::layers_iterator rslIt;
		renderSettings.layers.begin(rslIt);
		if (rslIt != renderSettings.layers.end()) {
			BL::SceneRenderLayer sceneRenderLayer(*rslIt);
			if (sceneRenderLayer && !is_interrupted()) {
				BL::RenderResult renderResult = m_engine.begin_result(0, 0, viewParams.renderSize.w, viewParams.renderSize.h, sceneRenderLayer.name().c_str(), nullptr);
				if (renderResult) {
					m_renderResultsList.push_back(renderResult);
				}
			}
		}
	}

	m_isRunning = true;
}

void ProductionExporter::render_end()
{
	if (m_settings.exporter_type != ExporterType::ExpoterTypeFile) {
		std::lock_guard<std::mutex> l(m_callback_mtx);

		if (m_settings.close_on_stop) {
			m_exporter->free();
		} else {
			m_exporter->stop();
		}
		std::static_pointer_cast<ZmqExporter>(m_exporter)->wait_for_server();
		m_exporter->set_callback_on_image_ready(ExpoterCallback());
		m_exporter->set_callback_on_rt_image_updated(ExpoterCallback());
		m_exporter->set_callback_on_bucket_ready([](const VRayBaseTypes::AttrImage &) {});
	}

	if (m_engine) {
		std::lock_guard<PythonGIL> pLock(m_pyGIL);
		for (auto & result : m_renderResultsList) {
			m_engine.end_result(result, false, true, true);
		}
	}
}

ProductionExporter::~ProductionExporter()
{
	{
		std::lock_guard<std::mutex> uLock(m_python_state_lock);
		if (m_settings.settings_animation.use) {
			m_isAnimationRunning = false;
		}
	}
	{
		std::lock_guard<std::mutex> l(m_callback_mtx);
		m_exporter.reset();
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
	VFB_Assert(img.isBucket() && "Image for cb_on_bucket_ready is not bucket image");
	if (!img.isBucket()) {
		return;
	}
	std::lock_guard<std::mutex> l(m_callback_mtx);
	m_imageDirty = true;

	for (auto & result : m_renderResultsList) {
		for (int c = 0; c < result.layers.length(); ++c) {
			for (int r = 0; r < result.layers[c].passes.length(); ++r) {
				auto pass = result.layers[c].passes[r];
				if (pass && pass.fullname() == "Combined") {
					auto * bPass = reinterpret_cast<RenderPass*>(pass.ptr.data);
					ImageSize passSize = {result.resolution_x(), result.resolution_y(), bPass->channels};
					ImageRegion passRegion = {img.x - m_viewParams.regionStart.w, img.y - m_viewParams.regionStart.h, img.width, img.height};

					ImageSize sourceSize = {img.width, img.height, bPass->channels};
					ImageRegion sourceRegion = sourceSize;

					updateImageRegion(bPass->rect, passSize, passRegion, img.data.get(), sourceSize, sourceRegion);
					break;
				}
			}
		}
	}
}

void ProductionExporter::cb_on_rt_image_updated()
{
	std::lock_guard<std::mutex> l(m_callback_mtx);

	if (!m_exporter) {
		return;
	}

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
						RenderImage image = m_exporter->get_pass(renderPass.fullname());
						RenderSizeParams imageSize = {image.w, image.h};

						if (image && (imageSize == m_viewParams.renderSize || imageSize == m_viewParams.regionSize)) {
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
					std::lock_guard<PythonGIL> pLock(m_pyGIL);
					m_engine.update_result(result);
				}
			}
		}
	}
}
