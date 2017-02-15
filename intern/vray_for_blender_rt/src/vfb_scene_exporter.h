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

#ifndef VRAY_FOR_BLENDER_EXPORTER_H
#define VRAY_FOR_BLENDER_EXPORTER_H

#include "vfb_export_settings.h"
#include "vfb_plugin_exporter.h"
#include "vfb_node_exporter.h"
#include "vfb_render_view.h"
#include "vfb_rna.h"

#include "vfb_thread_manager.h"

#include <cstdint>
#include <mutex>
#include <boost/thread.hpp>

#ifdef USE_BLENDER_VRAY_APPSDK
#include <vraysdk.hpp>
#endif

namespace VRayForBlender {

/// Wrapper over PyThreadState save and restore
class PythonGIL {
public:
	PythonGIL(PyThreadState * threadState = nullptr): m_threadState(threadState) {}

	// when returning to python we should lock
	~PythonGIL() {
		if (m_threadState) {
			lock();
		}
	}

	bool try_lock() {
		std::lock_guard<std::mutex> lock(m_mtx);
		if (m_threadState) {
			_lock(false);
			return true;
		} else {
			return false;
		}
	}

	void lock() {
		_lock(true);
	}

	void unlock() {
		std::lock_guard<std::mutex> lock(m_mtx);
		BLI_assert(!m_threadState && "Will overrite python thread state, recursive saves are not permitted.");
		m_threadState = PyEval_SaveThread();
		BLI_assert(m_threadState && "PyEval_SaveThread returned NULL.");
	}
private:

	void _lock(bool protect = true) {
		if (protect) {
			m_mtx.lock();
		}
		BLI_assert(m_threadState && "Restoring null python state!");
		PyEval_RestoreThread(m_threadState);
		m_threadState = nullptr;
		if (protect) {
			m_mtx.unlock();
		}
	}


	std::mutex      m_mtx; ///< lock and unlock are not atomic - lock while doing it
	PyThreadState * m_threadState; ///< pointer to the state of the thread that called the c++
};


/// Class that keeps track of what frames are exported and what need to be exported
/// Simplifies motion blur and animation export (both requre multi frame export)
class FrameExportManager {
public:
	FrameExportManager(BL::Scene scene, ExporterSettings & settings);

	/// Update internal data from the passes ExporterSettings
	/// needed because settings change
	void updateFromSettings();

	/// Reset scene state as it was before exporting
	void reset();

	/// Moves current frame 1 render frame backwards
	/// Used in RT because we only need to render one frame so we rewind after each export
	void rewind();

	/// Get the number of frames that will be rendered
	int getRenderFrameCount() const;

	/// Get the number of frames to be exported for a single render frame
	/// NOTE: if motion blur is enabled this will be 2 frames for example
	///       if not it will be 1
	int getBatchFrameCount() const {
		return m_mbFramesBefore + 1 + m_mbFramesAfter;
	}

	/// Get the number of frames that will be exported (includes motion blur frames)
	int getExportFrameCount() const {
		return getRenderFrameCount() * getBatchFrameCount();
	}

	/// Get the correct camera for current frame (used for camera loop)
	BL::Camera getActiveCamera();

	/// Call function for each frame that needs to be exported so next frame can be rendered
	void forEachFrameInBatch(std::function<bool(FrameExportManager &)> callback);

	/// Get the frame we need to set to scene for the current export
	int getSceneFrameToExport() const {
		return m_sceneFrameToExport;
	}

	int getCurrentRenderFrame() const {
		return m_frameToRender;
	}

private:
	ExporterSettings &m_settings; ///< The global settings for the exporter
	BL::Scene m_scene; ///< Current scene
	std::vector<BL::Camera> m_loopCameras; ///< All cameras with 'camera_loop' enabled if anim is Camera Loop

	int m_sceneSavedFrame; ///< m_scene.frame_current() on init, used to restore scene to correct frame
	int m_sceneFirstFrame; ///< first frame of the animation
	int m_lastFrameToRender; ///< last frame of the animation
	int m_animationFrameStep; ///< the frame step of the animation

	/// The last frame that was exported
	/// NOTE: used to skip already exported frames in case we have high motion blur radius and alot of frames overlap
	int m_lastExportedFrame; 

	int m_sceneFrameToExport; ///< the frame we need to set to the current scene so we can export

	/// The next frame we should actually render
	/// For animation this will jump with the frame step and will generraly mean the frames that vray will render
	/// For camera loop this will be in range [0, n) where n is the number of cameras in the camera loop
	int m_frameToRender;

	int m_mbFramesBefore; ///< number of motion blur frames to export before m_frameToRender
	int m_mbFramesAfter; ///< number of motion blur frames to export after m_frameToRender
};

class SceneExporter {
public:
	SceneExporter(BL::Context         context,
	              BL::RenderEngine    engine,
	              BL::BlendData       data,
	              BL::Scene           scene,
	              BL::SpaceView3D     view3d = PointerRNA_NULL,
	              BL::RegionView3D    region3d = PointerRNA_NULL,
	              BL::Region          region = PointerRNA_NULL);

	virtual ~SceneExporter();

public:
	virtual void         init();
	        void         init_data();
	void                 free();
	PluginExporter      *get_plugin_exporter() { return m_exporter; };

public:

	virtual void         sync_object(BL::Object ob, const int &check_updated = false, const ObjectOverridesAttrs & = ObjectOverridesAttrs());
	virtual void         sync_object_modiefiers(BL::Object ob, const int &check_updated);
	virtual void         sync_dupli(BL::Object ob, const int &check_updated=false);

	void                 sync_array_mod(BL::Object ob, const int &check_updated);

	/// Export all scene data to render current frame (may include exporting multiple frames for motion blur)
	/// @check_updated - true if we need to check object's flag or just export everything
	virtual bool         export_scene(const bool check_updated = false);
	void                 sync_prepass();

	ViewParams           get_current_view_params();

	/// Export all scene data for the current frame
	void                 sync(const bool check_updated=false);
	void                 sync_view(const bool check_updated=false);
	void                 sync_objects(const bool check_updated=false);
	void                 sync_effects(const bool check_updated=false);
	void                 sync_materials();
	void                 sync_render_settings();
	void                 sync_render_channels();

	virtual void         setup_callbacks() {}
	virtual void         draw() {}

	void                 resize(int w, int h);

	void                 tag_update();
	void                 tag_redraw();

	virtual void         render_start();
	void                 render_stop();

	virtual int          is_interrupted();
	int                  is_viewport() { return !!m_view3d; }
	int                  is_preview();

	bool                 is_engine_undo_taged();
	void                 pause_for_undo();
	void                 resume_from_undo(BL::Context         context,
	                                      BL::RenderEngine    engine,
	                                      BL::BlendData       data,
	                                      BL::Scene           scene);

	PythonGIL            m_pyGIL;
protected:
	virtual void         create_exporter();

	void                 get_view_from_camera(ViewParams &viewParams, BL::Object &cameraObject);
	void                 get_view_from_viewport(ViewParams &viewParams);

protected:
	BL::Context          m_context;
	BL::RenderEngine     m_engine;
	BL::BlendData        m_data;
	BL::Scene            m_scene;
	BL::SpaceView3D      m_view3d;
	BL::RegionView3D     m_region3d;
	BL::Region           m_region;

	// this is the camera that should be used for exporting
	// as it can be controlled by the exporter, by default it is m_scene.camera()
	BL::Camera           m_active_camera;

	// will store the python thread state when this exporter must change python data
	void                *m_python_thread_state;
	// only used if m_isAnimationRunning is true, since there are 2 threads
	// lock before python_thread_state_restore and unlock after python_thread_state_save
	std::mutex           m_python_state_lock;
protected:
	PluginExporter      *m_exporter;
	ExporterSettings     m_settings;
	FrameExportManager   m_frameExporter;
	DataExporter         m_data_exporter;
	ViewParams           m_viewParams;

	bool                 m_isLocalView;
	uint32_t             m_sceneComputedLayers;

	ThreadManager::Ptr   m_threadManager;

	bool                 m_isRunning;
	bool                 m_isUndoSync;
	bool                 m_isFirstSync;
private:
	int                  is_physical_view(BL::Object &cameraObject);
	int                  is_physical_updated(ViewParams &viewParams);

private:
	boost::mutex         m_viewLock;
	boost::mutex         m_syncLock;
};

}

#endif // VRAY_FOR_BLENDER_EXPORTER_H
