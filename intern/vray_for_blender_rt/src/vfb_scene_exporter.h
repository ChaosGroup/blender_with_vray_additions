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
#include "vfb_rna.h"

#include <boost/thread.hpp>

#ifdef USE_BLENDER_VRAY_APPSDK
#include <vraysdk.hpp>
#endif


namespace VRayForBlender {


struct ViewParams {
	struct RenderSize {
		RenderSize():
		    w(0),
		    h(0),
		    offs_x(0),
		    offs_y(0)
		{}

		int  w;
		int  h;
		int  offs_x;
		int  offs_y;
	} render_size;

	struct RenderView {
		RenderView():
		    fov(0.785398f),
		    ortho(false),
		    ortho_width(1.0f),
		    use_clip_start(false),
		    clip_start(0.0f),
		    use_clip_end(false),
		    clip_end(1.0f)
		{}

		bool operator == (const RenderView &other) const {
			return (MemberEq(fov) &&
			        MemberEq(ortho) &&
			        MemberEq(ortho_width) &&
			        MemberEq(use_clip_start) &&
			        MemberEq(clip_start) &&
			        MemberEq(use_clip_end) &&
			        MemberEq(clip_end) &&
			        (memcmp(tm.data, other.tm.data, sizeof(BlTransform)) == 0));
		}

		bool operator != (const RenderView &other) const {
			return !(*this == other);
		}

		float        fov;
		BlTransform  tm;

		int          ortho;
		float        ortho_width;

		int          use_clip_start;
		float        clip_start;
		int          use_clip_end;
		float        clip_end;
	} render_view;

	int params_changed(const ViewParams &other) {
		const bool differs = (render_view != other.render_view);
		if (differs) {
			render_view = other.render_view;
		}
		return differs;
	}

	int size_changed(const ViewParams &other) {
		const bool differs = (render_size.w != other.render_size.w) ||
		                     (render_size.h != other.render_size.h);
		if (differs) {
			render_size.w = other.render_size.w;
			render_size.h = other.render_size.h;
		}
		return differs;
	}

	int pos_changed(const ViewParams &other) {
		const bool differs = (render_size.offs_x != other.render_size.offs_x) ||
		                     (render_size.offs_y != other.render_size.offs_y);
		if (differs) {
			render_size.offs_x = other.render_size.offs_x;
			render_size.offs_y = other.render_size.offs_y;
		}
		return differs;
	}
};




class SceneExporter {
public:
	SceneExporter(BL::Context         context,
	              BL::RenderEngine    engine,
	              BL::BlendData       data,
	              BL::Scene           scene,
	              BL::SpaceView3D     view3d,
	              BL::RegionView3D    region3d,
	              BL::Region          region);

	virtual ~SceneExporter();

public:
	virtual void         init();
	void                 free();

public:
	virtual bool         do_export() = 0;
	void                 sync(const int &check_updated=false);
	void                 sync_prepass();
	void                 sync_view(const int &check_updated=false);
	virtual void         sync_object(BL::Object ob, const int &check_updated = false, const ObjectOverridesAttrs & = ObjectOverridesAttrs());
	void                 sync_objects(const int &check_updated=false);
	virtual void         sync_dupli(BL::Object ob, const int &check_updated=false);
	void                 sync_effects(const int &check_updated=false);

	void                 draw();
	void                 resize(int w, int h);
	void                 tag_update();
	void                 tag_redraw();
	void                 tag_ntree(BL::NodeTree ntree, bool updated=true);

	void                 set_open_file(PyObject *obj) { m_open_file_wrapper = obj; }
	PyObject            *python_open_file(const std::string fname);

	void                 render_start();
	void                 render_stop();

	int                  is_interrupted();

protected:
	bool                 export_animation();
	virtual void         create_exporter();
	unsigned int         get_layer(BlLayers array);

public:
	void                *m_pythonThreadState;

protected:
	BL::Context          m_context;
	BL::RenderEngine     m_engine;
	BL::BlendData        m_data;
	BL::Scene            m_scene;
	BL::SpaceView3D      m_view3d;
	BL::RegionView3D     m_region3d;
	BL::Region           m_region;

	PluginExporter      *m_exporter;
	DataExporter         m_data_exporter;
	ExporterSettings     m_settings;
	ViewParams           m_viewParams;

	PyObject            *m_open_file_wrapper;

	bool                 m_ortho_camera;
};

}

#endif // VRAY_FOR_BLENDER_EXPORTER_H
