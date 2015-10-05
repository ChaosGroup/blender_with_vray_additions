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

#include "DNA_ID.h"
#include "DNA_object_types.h"

extern "C" {
#include "BKE_idprop.h"
#include "BKE_node.h" // For ntreeUpdateTree()
}

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>

#include <ctime>
#include <chrono>
#include <thread>

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


static StrSet RenderSettingsPlugins;
static StrSet RenderGIPlugins;


SceneExporter::SceneExporter(BL::Context context, BL::RenderEngine engine, BL::BlendData data, BL::Scene scene, BL::SpaceView3D view3d, BL::RegionView3D region3d, BL::Region region)
    : m_context(context)
    , m_engine(engine)
    , m_data(data)
    , m_scene(scene)
    , m_view3d(view3d)
    , m_region3d(region3d)
    , m_region(region)
    , m_exporter(nullptr)
{
	if (!RenderSettingsPlugins.size()) {
		RenderSettingsPlugins.insert("SettingsOptions");
		RenderSettingsPlugins.insert("SettingsColorMapping");
		RenderSettingsPlugins.insert("SettingsDMCSampler");
		RenderSettingsPlugins.insert("SettingsImageSampler");
		RenderSettingsPlugins.insert("SettingsGI");
		RenderSettingsPlugins.insert("SettingsIrradianceMap");
		RenderSettingsPlugins.insert("SettingsLightCache");
		RenderSettingsPlugins.insert("SettingsDMCGI");
		RenderSettingsPlugins.insert("SettingsRaycaster");
		RenderSettingsPlugins.insert("SettingsRegionsGenerator");
#if 0
		RenderSettingsPlugins.insert("SettingsOutput");
		RenderSettingsPlugins.insert("SettingsRTEngine");
#endif
	}

	if (!RenderGIPlugins.size()) {
		RenderGIPlugins.insert("SettingsGI");
		RenderGIPlugins.insert("SettingsLightCache");
		RenderGIPlugins.insert("SettingsIrradianceMap");
		RenderGIPlugins.insert("SettingsDMCGI");
	}
	m_settings.init(m_data, m_scene);
}


SceneExporter::~SceneExporter()
{
	free();
}


void SceneExporter::init()
{
	create_exporter();
	if (!m_exporter) {
		PRINT_INFO_EX("Failed to create exporter!");
	}
	assert(m_exporter && "Failed to create exporter!");

	if (m_exporter) {
		m_exporter->init();

		m_exporter->set_callback_on_image_ready(ExpoterCallback(boost::bind(&SceneExporter::tag_redraw, this)));
		m_exporter->set_callback_on_rt_image_updated(ExpoterCallback(boost::bind(&SceneExporter::tag_redraw, this)));

		// directly bind to the engine
		m_exporter->set_callback_on_message_updated(boost::bind(&BL::RenderEngine::update_stats, &m_engine, _1, _2));
	}

	m_data_exporter.init(m_exporter, m_settings);
	m_data_exporter.init_data(m_data, m_scene, m_engine, m_context);
	m_data_exporter.init_defaults();
}


void SceneExporter::create_exporter()
{
	m_exporter = ExporterCreate(m_settings.exporter_type);
	if (!m_exporter) {
		m_exporter = ExporterCreate(ExpoterType::ExporterTypeInvalid);
		if (!m_exporter) {
			return;
		}
	}
}


void SceneExporter::free()
{
	PluginDesc::cache.clear();
	ExporterDelete(m_exporter);
}


void SceneExporter::resize(int w, int h)
{
	PRINT_INFO_EX("SceneExporter::resize(%i, %i)",
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

	glTranslatef(m_viewParams.renderSize.offs_x, m_viewParams.renderSize.offs_y, 0.0f);

	if (transparent) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	glColor3f(1.0f, 1.0f, 1.0f);

	GLuint texid;
	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_2D, texid);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, image.w, image.h, 0, GL_RGBA, GL_FLOAT, image.pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEnable(GL_TEXTURE_2D);

	glPushMatrix();
	glTranslatef(0.0f, 0.0f, 0.0f);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, 0.0f);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2f((float)m_viewParams.renderSize.w, 0.0f);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2f((float)m_viewParams.renderSize.w, (float)m_viewParams.renderSize.h);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, (float)m_viewParams.renderSize.h);
	glEnd();

	glPopMatrix();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1, &texid);

	if (transparent) {
		glDisable(GL_BLEND);
	}

	glPopMatrix();

	image.free();
}


void SceneExporter::render_start()
{
	if (m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRender ||
	    m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRenderAndExport) {
		m_exporter->start();
	}
}


bool SceneExporter::export_animation()
{
	using namespace std;
	using namespace std::chrono;

	const float frame = m_scene.frame_current();

	m_settings.settings_animation.frame_current = frame;
	m_exporter->set_current_frame(frame);

	PRINT_INFO_EX("Exporting animation frame %d", m_scene.frame_current());
	m_exporter->stop();
	sync(false);
	m_exporter->start();

	auto lastTime = high_resolution_clock::now();
	while (m_exporter->get_last_rendered_frame() < frame) {
		this_thread::sleep_for(milliseconds(1));

		auto now = high_resolution_clock::now();
		if (duration_cast<seconds>(now - lastTime).count() > 1) {
			lastTime = now;
			PRINT_INFO_EX("Waiting for renderer to render animation frame %f, current %f", frame, m_exporter->get_last_rendered_frame());
		}
		if (this->is_interrupted()) {
			PRINT_INFO_EX("Interrupted - stopping animation rendering!");
			return false;
		}
		if (m_exporter->is_aborted()) {
			PRINT_INFO_EX("Renderer stopped - stopping animation rendering!");
			return false;
		}
	}

	return true;
}


void SceneExporter::sync(const int &check_updated)
{
	PRINT_INFO_EX("SceneExporter::sync(%i)",
	              check_updated);

	m_settings.init(m_data, m_scene);

	clock_t begin = clock();
	m_data_exporter.clearMaterialCache();
	sync_prepass();

	PointerRNA vrayScene = RNA_pointer_get(&m_scene.ptr, "vray");

	for (const auto &pluginID : RenderSettingsPlugins) {
		PointerRNA propGroup = RNA_pointer_get(&vrayScene, pluginID.c_str());

		PluginDesc pluginDesc(pluginID, pluginID);

		m_data_exporter.setAttrsFromPropGroupAuto(pluginDesc, &propGroup, pluginID);

		m_exporter->export_plugin(pluginDesc);
	}

	if (check_updated) {
		sync_materials();
	}

	sync_view(check_updated);
	sync_objects(check_updated);
	sync_effects(check_updated);

	m_data_exporter.sync();

	clock_t end = clock();

	double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;

	PRINT_INFO_EX("Synced in %.3f sec.",
	              elapsed_secs);

	// Sync data (will remove deleted objects)
	m_exporter->sync();

	// Export stuff after sync
	if (m_settings.work_mode == ExporterSettings::WorkMode::WorkModeExportOnly ||
	    m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRenderAndExport) {
		const std::string filepath = "scene_app_sdk.vrscene";
		m_exporter->export_vrscene(filepath);
	}
}


static void TagNtreeIfIdPropTextureUpdated(BL::NodeTree ntree, BL::Node node, const std::string &texAttr)
{
	BL::Texture tex(Blender::GetDataFromProperty<BL::Texture>(&node.ptr, texAttr));
	if (tex && (tex.is_updated() || tex.is_updated_data())) {
		PRINT_INFO_EX("Texture %s is updated...",
		              tex.name().c_str());
		DataExporter::tag_ntree(ntree);
	}
}


void SceneExporter::sync_prepass()
{
	m_data_exporter.m_id_cache.clear();
	m_data_exporter.m_id_track.reset_usage();

	BL::BlendData::node_groups_iterator nIt;
	for (m_data.node_groups.begin(nIt); nIt != m_data.node_groups.end(); ++nIt) {
		BL::NodeTree ntree(*nIt);
		bNodeTree *_ntree = (bNodeTree*)ntree.ptr.data;

		if (IDP_is_ID_used((ID*)_ntree)) {
			if (boost::starts_with(ntree.bl_idname(), "VRayNodeTree")) {
				// NOTE: On scene save node links are not properly updated for some
				// reason; simply manually update everything...
				ntreeUpdateTree((Main*)m_data.ptr.data, _ntree);

				// Check nodes
				BL::NodeTree::nodes_iterator nodeIt;
				for (ntree.nodes.begin(nodeIt); nodeIt != ntree.nodes.end(); ++nodeIt) {
					BL::Node node(*nodeIt);
					if (node.bl_idname() == "VRayNodeMetaImageTexture" ||
					    node.bl_idname() == "VRayNodeBitmapBuffer"     ||
					    node.bl_idname() == "VRayNodeTexGradRamp"      ||
					    node.bl_idname() == "VRayNodeTexRemap") {
						TagNtreeIfIdPropTextureUpdated(ntree, node, "texture");
					}
					else if (node.bl_idname() == "VRayNodeTexSoftBox") {
						TagNtreeIfIdPropTextureUpdated(ntree, node, "ramp_grad_vert");
						TagNtreeIfIdPropTextureUpdated(ntree, node, "ramp_grad_horiz");
						TagNtreeIfIdPropTextureUpdated(ntree, node, "ramp_grad_rad");
						TagNtreeIfIdPropTextureUpdated(ntree, node, "ramp_frame");
					}
				}
			}
		}
	}
}


unsigned int SceneExporter::get_layer(BlLayers array)
{
	unsigned int layer = 0;

	for(unsigned int i = 0; i < 20; i++)
		if (array[i])
			layer |= (1 << i);

	return layer;
}


void SceneExporter::sync_object(BL::Object ob, const int &check_updated, const ObjectOverridesAttrs & override)
{
	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
#if 0
	const int data_updated = RNA_int_get(&vrayObject, "data_updated");
	PRINT_INFO_EX("[is_updated = %i | is_updated_data = %i | data_updated = %i | check_updated = %i]: Syncing \"%s\"...",
				  ob.is_updated(), ob.is_updated_data(), data_updated, check_updated,
	              ob.name().c_str());
#endif
	if (ob.data() && ob.type() == BL::Object::type_MESH) {
		m_data_exporter.exportObject(ob, check_updated, override);
	}
	else if (ob.data() && ob.type() == BL::Object::type_LAMP) {
		m_data_exporter.exportLight(ob, check_updated, override);
	}

	// Reset update flag
	RNA_int_set(&vrayObject, "data_updated", CGR_NONE);
}

static int ob_has_dupli(BL::Object ob) {
	return ((ob.dupli_type() != BL::Object::dupli_type_NONE) && (ob.dupli_type() != BL::Object::dupli_type_FRAMES));
}

static int ob_is_duplicator_renderable(BL::Object ob) {
	bool is_renderable = true;

	// Dulpi
	if (ob_has_dupli(ob)) {
		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		is_renderable = RNA_boolean_get(&vrayObject, "dupliShowEmitter");
	}

	// Particles
	// Particle system "Show / Hide Emitter" has priority over dupli
	if (ob.particle_systems.length()) {
		is_renderable = true;

		BL::Object::modifiers_iterator mdIt;
		for (ob.modifiers.begin(mdIt); mdIt != ob.modifiers.end(); ++mdIt) {
			BL::Modifier md(*mdIt);
			if (md.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
				BL::ParticleSystemModifier pmod(md);
				BL::ParticleSystem psys(pmod.particle_system());
				if (psys) {
					BL::ParticleSettings pset(psys.settings());
					if (pset) {
						if (!pset.use_render_emitter()) {
							is_renderable = false;
							break;
						}
					}
				}
			}
		}
	}

	return is_renderable;
}


void SceneExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
	const int dupli_override_id   = RNA_int_get(&vrayObject, "dupliGroupIDOverride");
	const int dupli_use_instancer = RNA_boolean_get(&vrayObject, "use_instancer");

	AttrInstancer instances;
	instances.frameNumber = m_scene.frame_current();
	if (dupli_use_instancer) {
		int num_instances = 0;

		BL::Object::dupli_list_iterator dupIt;
		for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {
			BL::DupliObject dupliOb(*dupIt);
			BL::Object      dupOb(dupliOb.object());

			const bool is_hidden = dupliOb.hide() || dupOb.hide_render();
			const bool is_light = Blender::IsLight(dupOb);
			const bool supported_type = Blender::IsGeometry(dupOb) || is_light;

			if (!is_hidden && !is_light && supported_type) {
				num_instances++;
			}
		}

		instances.data.resize(num_instances);
	}

	if (is_interrupted()) {
		return;
	}

	BL::Object::dupli_list_iterator dupIt;
	int dupli_instance = 0;
	for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {
		if (is_interrupted()) {
			return;
		}

		BL::DupliObject dupliOb(*dupIt);
		BL::Object      dupOb(dupliOb.object());

		const bool is_hidden = dupliOb.hide() || dupOb.hide_render();

		const bool is_light = Blender::IsLight(dupOb);
		const bool supported_type = Blender::IsGeometry(dupOb) || is_light;

		MHash persistendID;
		MurmurHash3_x86_32((const void*)dupIt->persistent_id().data, 8 * sizeof(int), 42, &persistendID);

		if (!is_hidden && supported_type) {
			if (is_light) {
				ObjectOverridesAttrs overrideAttrs;

				overrideAttrs.override = true;
				overrideAttrs.visible = true;
				overrideAttrs.tm = AttrTransformFromBlTransform(dupliOb.matrix());
				overrideAttrs.id = persistendID;

				char namePrefix[255] = {0, };
				namePrefix[0] = 'D';
				snprintf(namePrefix + 1, 250, "%u", persistendID);
				strcat(namePrefix, "@");
				strcat(namePrefix, ob.name().c_str());

				overrideAttrs.namePrefix = namePrefix;

				sync_object(dupOb, check_updated, overrideAttrs);
			}
			else if (dupli_use_instancer) {
				ObjectOverridesAttrs overrideAttrs;
				overrideAttrs.override = true;
				// If dupli are shown via Instancer we need to hide
				// original object
				overrideAttrs.visible = ob_is_duplicator_renderable(dupOb);
				overrideAttrs.tm = AttrTransformFromBlTransform(dupOb.matrix_world());
				overrideAttrs.id = reinterpret_cast<intptr_t>(dupOb.ptr.data);

				float inverted[4][4];
				copy_m4_m4(inverted, ((Object*)dupOb.ptr.data)->obmat);
				invert_m4(inverted);

				float tm[4][4];
				mul_m4_m4m4(tm, ((DupliObject*)dupliOb.ptr.data)->mat, inverted);

				AttrInstancer::Item &instancer_item = (*instances.data)[dupli_instance];
				instancer_item.index = persistendID;

				instancer_item.node = m_data_exporter.getNodeName(dupOb);
				instancer_item.tm = AttrTransformFromBlTransform(tm);

				dupli_instance++;

				sync_object(dupOb, check_updated, overrideAttrs);
			}
		}
	}

	if (dupli_use_instancer) {
		static boost::format InstancerFmt("Dupli@%s");

		PluginDesc instancerDesc(boost::str(InstancerFmt % m_data_exporter.getNodeName(ob)), "Instancer");
		instancerDesc.add("instances", instances);

		m_exporter->export_plugin(instancerDesc);
	}
}


void SceneExporter::sync_objects(const int &check_updated)
{
	PRINT_INFO_EX("SceneExporter::sync_objects(%i)",
	              check_updated);

	// TODO:
	// [ ] Track new objects (creation / layer settings change)
	// [ ] Track deleted objects

	// Sync objects
	//
	BL::Scene::objects_iterator obIt;
	for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
		if (is_interrupted()) {
			break;
		}

		BL::Object ob(*obIt);

		if (ob.is_duplicator()) {
			sync_dupli(ob, check_updated);
			if (is_interrupted()) {
				break;
			}

			ObjectOverridesAttrs overAttrs;

			overAttrs.override = true;
			overAttrs.id = reinterpret_cast<intptr_t>(ob.ptr.data);
			overAttrs.tm = AttrTransformFromBlTransform(ob.matrix_world());
			overAttrs.visible = ob_is_duplicator_renderable(ob);

			sync_object(ob, check_updated, overAttrs);
		} else {
			sync_object(ob, check_updated);
		}
	}
}


void SceneExporter::sync_effects(const int &check_updated)
{
	NodeContext ctx;
	m_data_exporter.exportVRayEnvironment(ctx);
}


void SceneExporter::sync_materials()
{
	BL::BlendData::materials_iterator maIt;

	for (m_data.materials.begin(maIt); maIt != m_data.materials.end(); ++maIt) {
		BL::Material ma(*maIt);
		BL::NodeTree ntree(Nodes::GetNodeTree(ma));
		if (ntree) {
			const bool updated = ma.is_updated() || ma.is_updated_data() || ntree.is_updated();
			if (updated) {
				m_data_exporter.exportMaterial(ma);
			}
		}
	}
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


int SceneExporter::is_interrupted()
{
	bool export_interrupt = false;
	if (m_engine && m_engine.test_break()) {
		export_interrupt = true;
	}
	return export_interrupt;
}
