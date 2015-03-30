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
#include "cgr_util_defines.h"
#include "cgr_scene_exporter.h"

#include <boost/function.hpp>
#include <boost/bind.hpp>


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
    m_exporter(nullptr)
{
	/* is interactive viewport session */
	if (m_view3d) {
		m_w = m_region.width();
		m_h = m_region.height();
	}
}


SceneExporter::~SceneExporter()
{
	free();
}


void SceneExporter::init()
{
	m_exporter = ExporterCreate(ExpoterTypeAppSDK);
	m_exporter->set_callback_on_image_ready(ExpoterCallback(boost::bind(&SceneExporter::tag_redraw, this)));
	m_exporter->set_callback_on_rt_image_updated(ExpoterCallback(boost::bind(&SceneExporter::tag_redraw, this)));
}


void SceneExporter::free()
{
	ExporterDelete(m_exporter);
}


void SceneExporter::resize(const int &w, const int &h)
{
	PRINT_INFO_EX("SceneExporter->resize(%i, %i)",
	              w, h);

	if (w != m_w || h != m_h) {
		m_w = w;
		m_h = h;
		m_exporter->set_render_size(m_w, m_h);
	}
}


void SceneExporter::draw(const int &w, const int &h)
{
	PRINT_INFO_EX("SceneExporter->draw(%i, %i)",
	              w, h);

	// Timer check here
	if (w != m_w || h != m_h) {
		resize(w, h);
		tag_update();
		return;
	}

	RenderImage image = m_exporter->get_image();
	if (!image) {
		tag_update();
		return;
	}

	if (image.w != w || image.h != h) {
		resize(w, h);
		tag_update();
		return;
	}

	// We're recieving JPEG only
	const bool transparent = false;

	glPushMatrix();

	float full_x = 0.0f;
	float full_y = 0.0f;

	glTranslatef(full_x, full_y, 0.0f);

	if(transparent) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	glColor3f(1.0f, 1.0f, 1.0f);
#if 0
	if(rgba.data_type == TYPE_HALF) {
		/* for multi devices, this assumes the inefficient method that we allocate
		 * all pixels on the device even though we only render to a subset */
		GLhalf *data_pointer = (GLhalf*)rgba.data_pointer;
		data_pointer += 4*y*w;

		/* draw half float texture, GLSL shader for display transform assumed to be bound */
		GLuint texid;
		glGenTextures(1, &texid);
		glBindTexture(GL_TEXTURE_2D, texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, w, h, 0, GL_RGBA, GL_HALF_FLOAT, data_pointer);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glEnable(GL_TEXTURE_2D);

		if(draw_params.bind_display_space_shader_cb) {
			draw_params.bind_display_space_shader_cb();
		}

		glPushMatrix();
		glTranslatef(0.0f, (float)dy, 0.0f);

		glBegin(GL_QUADS);

		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(0.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2f((float)width, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2f((float)width, (float)height);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2f(0.0f, (float)height);

		glEnd();

		glPopMatrix();

		if(draw_params.unbind_display_space_shader_cb) {
			draw_params.unbind_display_space_shader_cb();
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glDeleteTextures(1, &texid);
	}
	else {
#endif
		const float dy = 0.0f;

		/* fallback for old graphics cards that don't support GLSL, half float,
		 * and non-power-of-two textures */
		glPixelZoom((float)image.w/(float)w, (float)image.h/(float)h);
		glRasterPos2f(0.0f, dy);

		glDrawPixels(w, h, GL_RGBA, GL_FLOAT, image.pixels);
		image.free();

		glRasterPos2f(0.0f, 0.0f);
		glPixelZoom(1.0f, 1.0f);
#if 0
	}
#endif

	if(transparent) {
		glDisable(GL_BLEND);
	}

	glPopMatrix();
}


void SceneExporter::export_scene()
{
	PRINT_INFO_EX("SceneExporter->export_scene()");

	if (!m_exporter) {
		return;
	}

	// setup view size
	// TODO: Region render
	m_exporter->set_render_size(m_w, m_h);

	export_objects();
}


void SceneExporter::export_objects()
{
	PRINT_INFO_EX("SceneExporter->export_objects()");

	BL::Scene::objects_iterator obIt;
	for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
		BL::Object ob = *obIt;

		PRINT_INFO_EX("Exporting object: %s",
		              ob.name().c_str())
	}
}


void SceneExporter::synchronize()
{
	PRINT_INFO_EX("SceneExporter->update_scene()");

	// only used for viewport rendering
	if (NOT(m_view3d)) {
		return;
	}

	update_objects();
}


void SceneExporter::render_start()
{
	m_exporter->start();
}


static inline uint get_layer(BL::Array<int, 20> array)
{
	uint layer = 0;

	for(uint i = 0; i < 20; i++)
		if(array[i])
			layer |= (1 << i);

	return layer;
}


void SceneExporter::update_objects()
{
	PRINT_INFO_EX("SceneExporter->update_objects()");

//	if (!m_mutex.try_lock()) {
//		tag_update();
//		return;
//	}

	// Sync camera
	//
	BL::Object camera_object = m_scene.camera();
	if (m_view3d) {
		if (m_view3d.lock_camera_and_layers()) {
			camera_object = m_scene.camera();
		}
		else {
			camera_object = m_view3d.camera();
		}
	}
	if (!camera_object) {
		PRINT_ERROR("No camera!")
	}
	else {
		PluginDesc viewDesc(camera_object.name(), "RenderView", "Camera@");
		viewDesc.add(PluginAttr("transform", AttrTransform(camera_object.matrix_world())));

		BL::Camera camera_data(camera_object.data());
		if (camera_data) {
			viewDesc.add(PluginAttr("fov", camera_data.angle()));
		}

		m_exporter->export_plugin(viewDesc);
	}

	// Sync objects
	//
	BL::Scene::objects_iterator obIt;
	for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
		BL::Object ob = *obIt;

		bool is_on_visible_layer = get_layer(ob.layers()) & get_layer(m_scene.layers());
		bool is_hidden = ob.hide() || ob.hide_render();

		bool is_updated      = ob.is_updated();
		bool is_data_updated = ob.is_updated_data();

		// NOTE: Test hack to export objects in sync
		if (!is_updated) {
			if (m_id_cache.find(ob.ptr.data) == m_id_cache.end()) {
				m_id_cache.insert(ob.ptr.data);

				is_updated      = true;
				is_data_updated = true;
			}
		}

		if ((!is_hidden) && is_on_visible_layer && (is_updated || is_data_updated)) {
			if (ob.data() && ob.type() == BL::Object::type_MESH) {
				AttrPlugin  geom;
				AttrPlugin  mtl;

				if (!is_data_updated) {
					std::string geomPluginName = ob.name();
					geomPluginName.insert(0, "Geom@");

					geom = AttrPlugin(geomPluginName);
				}
				else {
					BL::Mesh b_mesh = m_data.meshes.new_from_object(m_scene, ob, true, 2, false, false);
					if (b_mesh) {
						if (b_mesh.use_auto_smooth()) {
							b_mesh.calc_normals_split();
						}

						b_mesh.calc_tessface(true);

						int numFaces = 0;

						BL::Mesh::tessfaces_iterator faceIt;
						for (b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt) {
							BlFace faceVerts = faceIt->vertices_raw();

							// If face is quad we split it into 2 tris
							numFaces += faceVerts[3] ? 2 : 1;
						}

						if (numFaces) {
							AttrListVector  vertices(b_mesh.vertices.length());
							AttrListInt     faces(numFaces * 3);

							// Vertices
							BL::Mesh::vertices_iterator vertIt;
							int vertexIndex = 0;
							for (b_mesh.vertices.begin(vertIt); vertIt != b_mesh.vertices.end(); ++vertIt, ++vertexIndex) {
								(*vertices)[vertexIndex].x = vertIt->co()[0];
								(*vertices)[vertexIndex].y = vertIt->co()[1];
								(*vertices)[vertexIndex].z = vertIt->co()[2];
							}

							// Faces
							int faceVertIndex = 0;
							for (b_mesh.tessfaces.begin(faceIt); faceIt != b_mesh.tessfaces.end(); ++faceIt) {
								BlFace faceVerts = faceIt->vertices_raw();

								// Store face vertices
								(*faces)[faceVertIndex++] = faceVerts[0];
								(*faces)[faceVertIndex++] = faceVerts[1];
								(*faces)[faceVertIndex++] = faceVerts[2];
								if(faceVerts[3]) {
									(*faces)[faceVertIndex++] = faceVerts[0];
									(*faces)[faceVertIndex++] = faceVerts[2];
									(*faces)[faceVertIndex++] = faceVerts[3];
								}
							}

							m_data.meshes.remove(b_mesh);

							PluginDesc geomDesc(ob.name(), "GeomStaticMesh", "Geom@");
							geomDesc.add(PluginAttr("vertices", vertices));
							geomDesc.add(PluginAttr("faces", faces));

							geom = m_exporter->export_plugin(geomDesc);
						}
					}
				}

				if (NOT(mtl)) {
					PluginDesc mtlDesc("BRDFDiffuse@Clay", "BRDFDiffuse");
					mtlDesc.add(PluginAttr("color", AttrColor(0.5f, 0.5f, 0.5f)));

					mtl = m_exporter->export_plugin(mtlDesc);
				}

				if (geom && mtl) {
					PluginDesc nodeDesc(ob.name(), "Node", "Node@");
					nodeDesc.add(PluginAttr("geometry", AttrPlugin(geom)));
					nodeDesc.add(PluginAttr("material", AttrPlugin(mtl)));
					nodeDesc.add(PluginAttr("transform", AttrTransform(ob.matrix_world())));

					m_exporter->export_plugin(nodeDesc);
				}
			}
			else if (ob.data() && ob.type() == BL::Object::type_LAMP) {
				BL::Lamp lamp(ob.data());
				if (lamp) {
					PointerRNA  vrayLamp = RNA_pointer_get(&lamp.ptr, "vray");

					// Find plugin ID
					//
					std::string pluginID;

					if (lamp.type() == BL::Lamp::type_AREA) {
						pluginID = "LightRectangle";
					}
					else if (lamp.type() == BL::Lamp::type_HEMI) {
						pluginID = "LightDome";
					}
					else if (lamp.type() == BL::Lamp::type_SPOT) {
						const int spotType = RNA_enum_get(&vrayLamp, "spot_type");
						switch(spotType) {
							case 0: pluginID = "LightSpotMax"; break;
							case 1: pluginID = "LightIESMax";  break;
						}
					}
					else if (lamp.type() == BL::Lamp::type_POINT) {
						const int omniType = RNA_enum_get(&vrayLamp, "omni_type");
						switch(omniType) {
							case 0: pluginID = "LightOmniMax";    break;
							case 1: pluginID = "LightAmbientMax"; break;
							case 2: pluginID = "LightSphere";     break;
						}
					}
					else if (lamp.type() == BL::Lamp::type_SUN) {
						const int directType = RNA_enum_get(&vrayLamp, "direct_type");
						switch(directType) {
							case 0: pluginID = "LightDirectMax"; break;
							case 1: pluginID = "SunLight";       break;
						}
					}
					else {
						PRINT_ERROR("Lamp: %s Type: %i => Lamp type is not supported!",
						            ob.name().c_str(), lamp.type());
					}

					if (!pluginID.empty()) {
						PointerRNA lampPropGroup = RNA_pointer_get(&vrayLamp, pluginID.c_str());

						PluginDesc lampDesc(ob.name(), pluginID, "Lamp@");
						lampDesc.add(PluginAttr("transform", AttrTransform(ob.matrix_world())));

						if(pluginID == "LightRectangle") {
							BL::AreaLamp  areaLamp(lamp);

							const float sizeX = areaLamp.size() / 2.0f;
							const float sizeY = areaLamp.shape() == BL::AreaLamp::shape_SQUARE
							                    ? sizeX
							                    : areaLamp.size_y() / 2.0f;

							lampDesc.add(PluginAttr("u_size", sizeX));
							lampDesc.add(PluginAttr("v_size", sizeY));
						}
						else if(pluginID == "LightDome") {
							// ...
						}
						else if(pluginID == "LightSpotMax") {
							// ...
						}
						else if(ELEM(pluginID, "LightRectangle", "LightSphere", "LightDome")) {
							lampDesc.add(PluginAttr("objectID", ob.pass_index()));
						}

						float color[3];
						RNA_float_get_array(&lampPropGroup, "color", color);

						lampDesc.add(PluginAttr("intensity", RNA_float_get(&lampPropGroup, "intensity")));
						lampDesc.add(PluginAttr("color", AttrColor(color[0], color[1], color[2])));

						m_exporter->export_plugin(lampDesc);
					}
				}
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
	if(background) {
		/* update stats and progress, only for background here because
		 * in 3d view we do it in draw for thread safety reasons */
		update_status_progress();

		/* offline render, redraw if timeout passed */
		if(time_dt() - last_redraw_time > 1.0) {
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
