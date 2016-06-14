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

#include <cstdint>
#include <mutex>
#include <boost/thread.hpp>

#ifdef USE_BLENDER_VRAY_APPSDK
#include <vraysdk.hpp>
#endif

namespace VRayForBlender {

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
	virtual bool         do_export() = 0;

	virtual void         sync_object(BL::Object ob, const int &check_updated = false, const ObjectOverridesAttrs & = ObjectOverridesAttrs());
	virtual void         sync_object_modiefiers(BL::Object ob, const int &check_updated);
	virtual void         sync_dupli(BL::Object ob, const int &check_updated=false);

	void                 sync_prepass();
	void                 sync(const int &check_updated=false);
	void                 sync_view(int check_updated=false);
	void                 sync_objects(const int &check_updated=false);
	void                 sync_effects(const int &check_updated=false);
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

	// allows python threads to resume running
	void                 python_thread_state_save();

	// blocks python threads and allows changes to python data
	void                 python_thread_state_restore();
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
	DataExporter         m_data_exporter;
	ExporterSettings     m_settings;
	ViewParams           m_viewParams;

	bool                 m_isLocalView;
	uint32_t             m_sceneComputedLayers;

	bool                 m_isRunning;
private:
	int                  is_physical_view(BL::Object &cameraObject);
	int                  is_physical_updated(ViewParams &viewParams);

private:
	boost::mutex         m_viewLock;
	boost::mutex         m_syncLock;
};

}

#endif // VRAY_FOR_BLENDER_EXPORTER_H
