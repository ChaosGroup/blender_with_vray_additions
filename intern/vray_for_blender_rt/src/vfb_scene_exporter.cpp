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

#include "cgr_config.h"

#include "vfb_util_defines.h"

#include "vfb_scene_exporter.h"
#include "vfb_utils_nodes.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_math.h"

#include "BLI_rect.h"
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "BKE_camera.h"

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <ctime>


/* OpenGL header includes, used everywhere we use OpenGL, to deal with
 * platform differences in one central place. */

#ifdef WITH_GLEW_MX
#  include "glew-mx.h"
#else
#  include <GL/glew.h>
#  define mxCreateContext() glewInit()
#  define mxMakeCurrentContext(x) (x)
#endif


using namespace VRayForBlender;


SceneExporter::SceneExporter(BL::RenderEngine engine, BL::UserPreferences userpref, BL::BlendData data, BL::Scene scene, BL::SpaceView3D view3d, BL::RegionView3D region3d, BL::Region region):
    m_engine(engine),
    m_userPref(userpref),
    m_data(data),
    m_scene(scene),
    m_view3d(view3d),
    m_region3d(region3d),
    m_region(region),
    m_context(PointerRNA_NULL), // TODO:
    m_exporter(nullptr)
{}


SceneExporter::~SceneExporter()
{
	free();
}


void SceneExporter::init()
{
	m_exporter = ExporterCreate(ExpoterTypeAppSDK);
	m_exporter->set_callback_on_image_ready(ExpoterCallback(boost::bind(&SceneExporter::tag_redraw, this)));
	m_exporter->set_callback_on_rt_image_updated(ExpoterCallback(boost::bind(&SceneExporter::tag_redraw, this)));

	ExporterSettings settings(m_data, m_scene);

	m_data_exporter.init(m_exporter, settings);
	m_data_exporter.init_data(m_data, m_scene, m_engine, m_context);
	m_data_exporter.init_defaults();
}


void SceneExporter::free()
{
	ExporterDelete(m_exporter);
}


void SceneExporter::resize(int w, int h)
{
	PRINT_INFO_EX("SceneExporter->resize(%i, %i)",
	              w, h);

	m_exporter->set_render_size(w, h);
}


void SceneExporter::draw()
{
	sync_view(true);

	RenderImage image = m_exporter->get_image();
	if (!image) {
		tag_redraw();
		return;
	}

	const bool transparent = false;

	glPushMatrix();

	glTranslatef(m_viewParams.render_size.offs_x, m_viewParams.render_size.offs_y, 0.0f);

	if (transparent) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	glColor3f(1.0f, 1.0f, 1.0f);

#if 1
	GLuint texid;
	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_2D, texid);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, m_viewParams.render_size.w, m_viewParams.render_size.h, 0, GL_RGBA, GL_FLOAT, image.pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEnable(GL_TEXTURE_2D);

	glPushMatrix();
	glTranslatef(0.0f, 0.0f, 0.0f);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2f((float)m_viewParams.render_size.w, 0.0f);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2f((float)m_viewParams.render_size.w, (float)m_viewParams.render_size.h);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, (float)m_viewParams.render_size.h);
	glEnd();

	glPopMatrix();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1, &texid);
#else
	/* fallback for old graphics cards that don't support GLSL, half float,
	 * and non-power-of-two textures */
	glRasterPos2f(0.0f, 0.0f);
	glDrawPixels(m_viewParams.render_size.w, m_viewParams.render_size.h, GL_RGBA, GL_FLOAT, image.pixels);
	glRasterPos2f(0.0f, 0.0f);
#endif

	if (transparent) {
		glDisable(GL_BLEND);
	}

	glPopMatrix();

	image.free();
}


void SceneExporter::render_start()
{
	m_exporter->start();
}


void SceneExporter::sync(const int &check_updated)
{
	PRINT_INFO_EX("SceneExporter->sync(%i)",
	              check_updated);

	clock_t begin = clock();

	sync_view(check_updated);
	sync_materials(check_updated);
	sync_objects(check_updated);

	clock_t end = clock();

	double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;

	PRINT_INFO_EX("Synced in %.3f sec.",
	              elapsed_secs);
}


void SceneExporter::sync_view(const int &check_updated)
{
	const float sensor_size = 35.5981627f;

	if (!m_scene.camera() && !m_view3d) {
		PRINT_ERROR("Unable to setup view!")
	}
	else {
		ViewParams viewParams;
		if (!m_view3d) {
			PRINT_ERROR("Final frame render is not supported.")
		}
		else {
			if(m_region3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA) {
				BL::Object camera = m_view3d.lock_camera_and_layers()
				                    ? m_scene.camera()
				                    : m_view3d.camera();

				if (!camera) {
					PRINT_ERROR("Camera is not found!")
				}
				else {
					rctf view_border;

					// NOTE: Taken from source/blender/editors/space_view3d/view3d_draw.c:
					// static void view3d_camera_border(...) {...}
					//
					bool no_zoom = false;
					bool no_shift = false;

					Scene *scene = (Scene *)m_scene.ptr.data;
					const ARegion *ar = (const ARegion*)m_region.ptr.data;
					const View3D *v3d = (const View3D *)m_view3d.ptr.data;
					const RegionView3D *rv3d = (const RegionView3D *)m_region3d.ptr.data;

					CameraParams params;
					rctf rect_view, rect_camera;

					/* get viewport viewplane */
					BKE_camera_params_init(&params);
					BKE_camera_params_from_view3d(&params, v3d, rv3d);
					if (no_zoom)
						params.zoom = 1.0f;
					BKE_camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
					rect_view = params.viewplane;

					/* get camera viewplane */
					BKE_camera_params_init(&params);
					/* fallback for non camera objects */
					params.clipsta = v3d->near;
					params.clipend = v3d->far;
					BKE_camera_params_from_object(&params, v3d->camera);
					if (no_shift) {
						params.shiftx = 0.0f;
						params.shifty = 0.0f;
					}
					BKE_camera_params_compute_viewplane(&params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
					rect_camera = params.viewplane;

					/* get camera border within viewport */
					view_border.xmin = ((rect_camera.xmin - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
					view_border.xmax = ((rect_camera.xmax - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
					view_border.ymin = ((rect_camera.ymin - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;
					view_border.ymax = ((rect_camera.ymax - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;

					viewParams.render_size.offs_x = view_border.xmin;
					viewParams.render_size.offs_y = view_border.ymin;
					// NOTE: +2 to match camera border
					viewParams.render_size.w = view_border.xmax - view_border.xmin + 2;
					viewParams.render_size.h = view_border.ymax - view_border.ymin + 2;

					const float aspect = float(viewParams.render_size.w) / float(viewParams.render_size.h);

					BL::Object camera(m_scene.camera());
					BL::Camera camera_data(camera.data());

					PointerRNA vrayCamera = RNA_pointer_get(&camera_data.ptr, "vray");

					PointerRNA renderView = RNA_pointer_get(&vrayCamera, "RenderView");

					viewParams.render_view.fov = RNA_boolean_get(&vrayCamera, "override_fov")
					                             ? RNA_float_get(&vrayCamera, "fov")
					                             : camera_data.angle();

					viewParams.render_view.ortho = (camera_data.type() == BL::Camera::type_ORTHO);
					viewParams.render_view.ortho_width = camera_data.ortho_scale();

					if (aspect < 1.0f) {
						viewParams.render_view.fov = 2.0f * atanf(tanf(viewParams.render_view.fov / 2.0f) * aspect);
						viewParams.render_view.ortho_width *= aspect;
					}

					// TODO: Setup crop render if viewport is zoomed

					viewParams.render_view.use_clip_start = RNA_boolean_get(&renderView, "clip_near");
					viewParams.render_view.use_clip_end   = RNA_boolean_get(&renderView, "clip_far");

					viewParams.render_view.clip_start = camera_data.clip_start();
					viewParams.render_view.clip_end   = camera_data.clip_end();

					viewParams.render_view.tm  = camera.matrix_world();
				}
			}
			else {
				viewParams.render_size.offs_x = 0;
				viewParams.render_size.offs_y = 0;
				viewParams.render_size.w = m_region.width();
				viewParams.render_size.h = m_region.height();
#if 0
				const float aspect = float(viewParams.render_size.w) / float(viewParams.render_size.h);
				viewParams.render_view.fov = 2.0f * atanf((0.5f * sensor_size) / m_view3d.lens() / (1.0f / aspect));
#else
				viewParams.render_view.fov   = sensor_size / m_view3d.lens();
#endif
				viewParams.render_view.ortho = (m_region3d.view_perspective() == BL::RegionView3D::view_perspective_ORTHO);
				viewParams.render_view.ortho_width = m_region3d.view_distance() * sensor_size / m_view3d.lens();

				viewParams.render_view.use_clip_start = true;
				viewParams.render_view.use_clip_end   = true;

				viewParams.render_view.clip_start = m_view3d.clip_start();
				viewParams.render_view.clip_end   = m_view3d.clip_end();

				viewParams.render_view.tm = Math::InvertTm(m_region3d.view_matrix());
			}
		}

		if (m_viewParams.size_changed(viewParams)) {
			// PRINT_WARN("View resize: %i x %i", viewParams.render_size.w, viewParams.render_size.h);
			resize(m_viewParams.render_size.w, m_viewParams.render_size.h);
		}
		if (m_viewParams.pos_changed(viewParams)) {
			// PRINT_WARN("Pos change: %i x %i", viewParams.render_size.offs_x, viewParams.render_size.offs_y);
			tag_redraw();
		}
		if (m_viewParams.params_changed(viewParams)) {
			// PRINT_WARN("View update: fov = %.3f", viewParams.render_view.fov);

			PluginDesc viewDesc("renderView", "RenderView");
			viewDesc.add("fov", viewParams.render_view.fov);
			viewDesc.add("transform", AttrTransform(viewParams.render_view.tm));

			viewDesc.add("clipping",     (viewParams.render_view.use_clip_start || viewParams.render_view.use_clip_end));
			viewDesc.add("clipping_near", viewParams.render_view.clip_start);
			viewDesc.add("clipping_far",  viewParams.render_view.clip_end);

			viewDesc.add("orthographic", viewParams.render_view.ortho);
			viewDesc.add("orthographicWidth", viewParams.render_view.ortho_width);

			m_exporter->export_plugin(viewDesc);
		}
	}
}


void SceneExporter::sync_materials(const int &check_updated)
{
	PRINT_INFO_EX("SceneExporter->sync_materials(%i)",
	              check_updated);

	BL::BlendData::materials_iterator maIt;
	for (m_data.materials.begin(maIt); maIt != m_data.materials.end(); ++maIt) {
		BL::Material ma(*maIt);
		BL::NodeTree ntree = Nodes::GetNodeTree(ma);
		if (!ntree) {
			// PRINT_ERROR("");
		}
		else {
			const bool is_updated = check_updated
			                        ? (ma.is_updated() || ntree.is_updated())
			                        : true;
			if (is_updated) {
				m_data_exporter.exportMaterial(ma);
			}
		}
	}
}


static inline uint get_layer(BlLayers array)
{
	uint layer = 0;

	for(uint i = 0; i < 20; i++)
		if (array[i])
			layer |= (1 << i);

	return layer;
}


void SceneExporter::sync_objects(const int &check_updated)
{
	PRINT_INFO_EX("SceneExporter->sync_objects(%i)",
	              check_updated);

	// Sync objects
	//
	BL::Scene::objects_iterator obIt;
	for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
		BL::Object ob(*obIt);

		bool is_on_visible_layer = get_layer(ob.layers()) & get_layer(m_scene.layers());
		bool is_hidden = ob.hide() || ob.hide_render();

		bool is_updated      = check_updated ? ob.is_updated()      : true;
		bool is_data_updated = check_updated ? ob.is_updated_data() : true;

		if ((!is_hidden) && is_on_visible_layer && (is_updated || is_data_updated)) {
			if (ob.data() && ob.type() == BL::Object::type_MESH) {
				m_data_exporter.exportObject(ob, check_updated);
			}
			else if (ob.data() && ob.type() == BL::Object::type_LAMP) {
				m_data_exporter.exportLight(ob, check_updated);
			}
		}
	}

	m_exporter->sync();

	// m_mutex.unlock();
}


void SceneExporter::tag_update()
{
	/* tell blender that we want to get another update callback */
	m_engine.tag_update();
}


void SceneExporter::tag_redraw()
{
#if 0
	if (background) {
		/* update stats and progress, only for background here because
		 * in 3d view we do it in draw for thread safety reasons */
		update_status_progress();

		/* offline render, redraw if timeout passed */
		if (time_dt() - last_redraw_time > 1.0) {
			b_engine.tag_redraw();
			last_redraw_time = time_dt();
		}
	}
	else {
#endif
		/* tell blender that we want to redraw */
		m_engine.tag_redraw();
#if 0
	}
#endif
}
