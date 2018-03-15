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
#include "vfb_utils_blender.h"
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

/// Class that handles objects with subframes
class SubframesHandler {
public:
	typedef std::multimap<int, BL::Object, std::greater<int>> ObjectCollection;
	typedef ObjectCollection::iterator                        ObjectCollectionIt;

	SubframesHandler() = delete;
	SubframesHandler(BL::Scene scene, ExporterSettings & settings);

	/// Collects all the objects from the scene that have subframes
	void update();

	/// Get all the objects from the scene that have subframes
	ObjectCollection &getObjectsWithSubframes();

	/// Get the objects that will be exported on the current subframe
	std::pair<ObjectCollectionIt, ObjectCollectionIt> getObjectsWithCurrentSubframes() {
		return m_objectsWithSubframes.equal_range(m_currentSubframeDivision);
	}

	/// Count the objects that should be exported on the current subframe
	std::size_t countObjectsWithCurrentSubframes() const {
		return m_objectsWithSubframes.count(m_currentSubframeDivision);
	}

	/// Count all objects in the scene that have subframes
	std::size_t countObjectsWithSubframes() const {
		return m_objectsWithSubframes.size();
	}

	/// Get all different subframe divisions of current frame
	std::vector<int> &getSubframeValues();

	/// Get the subframe value that objects are being exported 
	int getCurrentSubframeDivision() const {
		return m_currentSubframeDivision;
	}

	/// Set the subframe value that objects are being exported 
	void setCurrentSubframeDivision(int sd) {
		m_currentSubframeDivision = sd;
	}

	/// Is the current frame a subframe
	bool isCurrentSubframe() const {
		return (m_currentSubframeDivision != 0);
	}

private:
	int               m_currentSubframeDivision; /// current subframe division that is exported
	ExporterSettings &m_settings; /// reference to the settings
	BL::Scene         m_scene; /// current scene that is exported
	ObjectCollection  m_objectsWithSubframes; /// all objects in the scene with subframes
	std::vector<int>  m_subframeValues; /// all different subframe values
	bool              m_isUpdated; /// is data for subframes updated
};

/// Class that keeps track of what frames are exported and what need to be exported
/// Simplifies motion blur and animation export (both requre multi frame export)
class FrameExportManager {
public:
	FrameExportManager(BL::Scene & scene, ExporterSettings & settings, BL::BlendData & data, BL::RenderEngine & engine);

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
	int getMotionBlurSamples() const {
		return m_mbGeomSamples;
	}

	/// Get the correct camera for current frame (used for camera loop)
	BL::Object getActiveCamera();

	/// Call function for each frame that needs to be exported so next frame can be rendered
	void forEachExportFrame(std::function<bool(FrameExportManager &)> callback);

	/// Get the frame we need to set to scene for the current export
	float getCurrentFrame() const {
		return m_currentFrame;
	}
		
	/// Get current render frame
	float getCurrentRenderFrame() const {
		return m_frameToRender;
	}

	/// Is the current frame a subframe
	bool isCurrentSubframe() const {
		return m_subframes.isCurrentSubframe();
	}

	/// Get the objects that will be exported on the current subframe
	std::pair<SubframesHandler::ObjectCollectionIt, SubframesHandler::ObjectCollectionIt> getObjectsWithCurrentSubframes() {
		return m_subframes.getObjectsWithCurrentSubframes();
	}

	/// Count the objects that should be exported on the current subframe
	std::size_t countObjectsWithCurrentSubframes() const {
		return m_subframes.countObjectsWithCurrentSubframes();
	}

	/// Count all objects in the scene that have subframes
	std::size_t countObjectsWithSubframes() const {
		return m_subframes.countObjectsWithSubframes();
	}

	/// Does the object has subframes that need to be exported separately
	bool hasObjectSubframes(BL::Object object) const {
		return Blender::getObjectSubframes(object) > 0;
	}

	/// Blender frame format
	struct BlenderFramePair {
		int frame; ///< int part of the current frame
		float subframe; ///< fraction of the current frame

		BlenderFramePair(int frame, float subframe)
		    : frame(frame)
		    , subframe(subframe)
		{}

		explicit BlenderFramePair(float value)
		    : frame(static_cast<int>(value))
		    , subframe(value - frame)
		{}

		bool operator!=(const BlenderFramePair &right) const {
			return frame != right.frame || !(fabs(subframe - right.subframe) < 1e-4);
		}
	};

	/// Convert float frame to blender frame pair
	static BlenderFramePair floatFrameToBlender(float value) {
		return BlenderFramePair(value);
	}

	/// Change current frame
	/// @param frame - the whole part of the frame
	/// @param subframe - the fractional part of the frame
	void changeSceneFrame(int frame, float subframe) {
		m_scene.frame_set(m_data.ptr.data, frame, subframe);
	}

	/// Change current frame
	/// @param pair - pair containing whole and fractional part of frame pair
	void changeSceneFrame(const BlenderFramePair & pair) {
		changeSceneFrame(pair.frame, pair.subframe);
	}

private:
	ExporterSettings &m_settings; ///< The global settings for the exporter
	BL::Scene m_scene; ///< Current scene
	BL::RenderEngine m_engine;
	BL::BlendData m_data; ///< The blender data conext
	std::vector<BL::Object> m_loopCameras; ///< All cameras with 'camera_loop' enabled if anim is Camera Loop

	float m_sceneSavedSubframe; ///< m_scene.frame_subframe() on init, used to restore scene to correct frame
	int m_sceneSavedFrame; ///< m_scene.frame_current() on init, used to restore scene to correct frame
	int m_sceneFirstFrame; ///< first frame of the animation
	int m_lastFrameToRender; ///< last frame of the animation
	int m_animationFrameStep; ///< the frame step of the animation

	/// This is the biggest (rightmost on the timeline) frame time that we exported the whole scene
	/// NOTE: used to skip already exported frames in case we have high motion blur radius and alot of frames overlap
	float m_lastExportedFrame;

	float m_currentFrame; ///< the frame we need to set to the current scene so we can export

	/// The next frame we should actually render
	/// For animation this will jump with the frame step and will generraly mean the frames that vray will render
	/// For camera loop this will be in range [0, n) where n is the number of cameras in the camera loop
	float m_frameToRender;

	/// Number of samples that need to be exported for each render frame
	/// This is like subframes for object but affect the whole scene - for each render frame we need to export
	/// this many keyframes
	/// The default value is 2 - one in the mb interval startn and one in the end
	int m_mbGeomSamples;

	/// The distance between two motion blur keyframes (this is the analogue of the animation step in animation)
	float m_mbSampleStep;

	/// Holds objects with subframes
	/// Helps to export only objects with relevant subframe value to the current frame
	SubframesHandler m_subframes;

	/// The offset we need to add to current frame to get the beggining of the motion blur interval
	float m_mbIntervalStartOffset;
};

class SceneExporter {
public:
	SceneExporter(BL::Context         context,
	              BL::RenderEngine    engine,
	              BL::BlendData       data,
	              BL::Scene           scene,
	              BL::SpaceView3D     view3d = PointerRNA_NULL,
	              BL::RegionView3D    region3d = PointerRNA_NULL,
	              BL::Region          region = PointerRNA_NULL)
		: m_context(context)
		, m_engine(engine)
		, m_data(data)
		, m_scene(scene)
		, m_view3d(view3d)
		, m_region3d(region3d)
		, m_region(region)
		, m_active_camera(view3d ? view3d.camera() : scene.camera())
		, m_python_thread_state(nullptr)
		, m_exporter(nullptr)
		, m_frameExporter(m_scene, m_settings, m_data, m_engine)
		, m_data_exporter(m_settings)
		, m_sceneComputedLayers(0)
		, m_isLocalView(false)
		, m_isUndoSync(false)
	{}

	virtual ~SceneExporter();

public:
	virtual void         init();
	        void         init_data();
	void                 free();
	std::shared_ptr<PluginExporter>  get_plugin_exporter() { return m_exporter; };

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
	void                 pre_sync_object(const bool check_updated, BL::Object &ob, CondWaitGroup &wg);

	void                 sync_objects(const bool check_updated=false);
	void                 sync_effects(const bool check_updated=false);
	void                 sync_materials();
	void                 sync_render_settings();
	void                 sync_render_channels();

	virtual void         setup_callbacks() {}
	virtual void         draw() {}

	//void                 resize(int w, int h);

	void                 tag_update();
	void                 tag_redraw();

	virtual void         render_start();

	virtual int          is_interrupted();
	int                  is_viewport() { return !!m_view3d; }
	int                  is_preview();

	/// Check if engine has flag set when it will be freed because of undo
	bool                 is_engine_undo_taged();
	void                 pause_for_undo();
	void                 resume_from_undo(BL::Context         context,
	                                      BL::RenderEngine    engine,
	                                      BL::BlendData       data,
	                                      BL::Scene           scene);

	BL::Object           get_active_camera() const { return BL::Object(m_active_camera.ptr); }

	void                 calculate_scene_layers();

	static BL::Object    getActiveCamera(BL::SpaceView3D view3d, BL::Scene scene);

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

	/// This is the camera that should be used for exporting
	/// as it can be controlled by the exporter, by default it is m_scene.camera()
	BL::Object           m_active_camera;

	void                *m_python_thread_state; ///< Will store the python thread state when this exporter must change python data

	/// Only used if m_isAnimationRunning is true, since there are 2 threads
	/// lock before python_thread_state_restore and unlock after python_thread_state_save
	std::mutex           m_python_state_lock;
protected:
	std::shared_ptr<PluginExporter>  m_exporter; ///< Pointer to actuial plugin exporter
	ExporterSettings     m_settings; ///< Holder for all settings that affect way of export
	FrameExportManager   m_frameExporter; ///< Handles chaning the current time so all needed keyframe data is exported
	DataExporter         m_data_exporter; ///< Handle for blender data
	ViewParams           m_viewParams; ///< The view params (current camera, image settings, etc)

	uint32_t             m_sceneComputedLayers; ///< Bool values for each layer compressed in one int

	ThreadManager::Ptr   m_threadManager; ///< Pointer to the ThreadManager used for object export

	bool                 m_isLocalView; ///< True if "local view" is enabled
	bool                 m_isUndoSync; ///< True if the current sync is caused because user did undo action
private:
	int                  is_physical_view(BL::Object &cameraObject);
	int                  is_physical_updated(ViewParams &viewParams);

private:
	std::mutex           m_viewLock;
	std::mutex           m_syncLock;
};

}

#endif // VRAY_FOR_BLENDER_EXPORTER_H
