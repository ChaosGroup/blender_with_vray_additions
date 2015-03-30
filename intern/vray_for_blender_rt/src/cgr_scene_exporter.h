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

#ifndef VRAY_FOR_HOUDINI_EXPORTER_H
#define VRAY_FOR_HOUDINI_EXPORTER_H

#include "cgr_plugin_exporter.h"

#include "MEM_guardedalloc.h"
#include "RNA_types.h"
#include "RNA_access.h"
#include "RNA_blender_cpp.h"

#include <boost/thread.hpp>
#include <vraysdk.hpp>


namespace VRayForBlender {

class SceneExporter {
public:
	SceneExporter(BL::RenderEngine    engine,
	              BL::UserPreferences userpref,
	              BL::BlendData       data,
	              BL::Scene           scene,
	              BL::SpaceView3D     view3d,
	              BL::RegionView3D    region3d,
	              BL::Region          region);

	~SceneExporter();

public:
	void                 init();
	void                 free();

public:
	void                 synchronize();

	void                 draw(const int &w, const int &h);
	void                 resize(const int &w, const int &h);
	void                 tag_update();
	void                 tag_redraw();

	void                 export_scene();

	void                 render_start();
	void                 render_stop();

private:
	void                 export_objects();
	void                 update_objects();

public:
	void                *m_pythonThreadState;

protected:
	BL::RenderEngine     m_engine;
	BL::UserPreferences  m_userPref;
	BL::BlendData        m_data;
	BL::Scene            m_scene;
	BL::SpaceView3D      m_view3d;
	BL::RegionView3D     m_region3d;
	BL::Region           m_region;

private:
	PluginExporter      *m_exporter;
	boost::mutex         m_mutex;
	std::set<void*>      m_id_cache;

	int                  m_w;
	int                  m_h;

};

}

#endif // VRAY_FOR_HOUDINI_EXPORTER_H
