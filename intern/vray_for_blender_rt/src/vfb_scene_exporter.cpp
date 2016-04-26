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
#include "vfb_node_exporter.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"

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
#include <random>
#include <limits>
#include <mutex>

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
    , m_isRunning(false)
    , m_isLocalView(false)
    , m_python_thread_state(nullptr)
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

	m_settings.update(m_context, m_engine, m_data, m_scene);
}


SceneExporter::~SceneExporter()
{
	free();
}


void SceneExporter::python_thread_state_save()
{
	assert(!m_python_thread_state && "Will overrite python thread state, recursive saves are not permitted.");
	m_python_thread_state = (void*)PyEval_SaveThread();
	assert(m_python_thread_state && "PyEval_SaveThread returned NULL.");
}

void SceneExporter::python_thread_state_restore()
{
	assert(m_python_thread_state && "Restoring null python state!");
	PyEval_RestoreThread((PyThreadState*)m_python_thread_state);
	m_python_thread_state = nullptr;
}


void SceneExporter::init() {
	create_exporter();
	if (!m_exporter) {
		PRINT_INFO_EX("Failed to create exporter!");
	}
	assert(m_exporter && "Failed to create exporter!");

	m_exporter->init();

	// directly bind to the engine
	m_exporter->set_callback_on_message_updated(boost::bind(&BL::RenderEngine::update_stats, &m_engine, _1, _2));

	setup_callbacks();
}

void SceneExporter::init_data()
{
	m_data_exporter.init(m_exporter, m_settings);
	m_data_exporter.init_data(m_data, m_scene, m_engine, m_context, m_view3d);
	m_data_exporter.init_defaults();
}

void SceneExporter::create_exporter()
{
	m_exporter = ExporterCreate(m_settings.exporter_type);
	if (!m_exporter) {
		m_exporter = ExporterCreate(m_settings.exporter_type = ExpoterType::ExporterTypeInvalid);
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


void SceneExporter::render_start()
{
	if (m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRender ||
	    m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRenderAndExport) {
		m_exporter->start();
	} else {
		PRINT_INFO_EX("Work mode WorkModeExportOnly, skipping renderer_start");
	}
}

bool SceneExporter::export_animation()
{
	using namespace std;
	using namespace std::chrono;

	bool frameExported = true;
	const float frame = m_scene.frame_current();

	if (m_settings.exporter_type == ExpoterType::ExpoterTypeFile) {
		PRINT_INFO_EX("Exporting animation frame %d, in file", frame);
		sync(false);
	} else {
		PRINT_INFO_EX("Exporting animation frame %d", frame);

		m_settings.settings_animation.frame_current = frame;
		m_exporter->set_current_frame(frame);

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


void SceneExporter::sync(const int &check_updated)
{
	if (!m_syncLock.try_lock()) {
		tag_update();
	}
	else {
		PRINT_INFO_EX("SceneExporter::sync(%i)",
		              check_updated);

		m_settings.update(m_context, m_engine, m_data, m_scene);

		VRayBaseTypes::RenderMode renderMode = m_view3d
		                                       ? m_settings.getViewportRenderMode()
		                                       : m_settings.getRenderMode();

		m_exporter->set_render_mode(renderMode);
		m_exporter->set_viewport_quality(m_settings.viewportQuality);

		clock_t begin = clock();

		sync_prepass();

		// Export once per viewport session
		if (!check_updated) {
			sync_render_settings();

			if (!is_viewport()) {
				sync_render_channels();
			}
		}

		// First materials sync is done from "sync_objects"
		// so run it only in update call
		if (check_updated) {
			sync_materials();
		}

		sync_view(check_updated);
		sync_objects(check_updated);
		sync_effects(check_updated);

		// Sync data (will remove deleted objects)
		m_data_exporter.sync();

		clock_t end = clock();

		double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;

		PRINT_INFO_EX("Synced in %.3f sec.",
		              elapsed_secs);

		// Sync plugins
		m_exporter->sync();

		// Export stuff after sync
		if (m_settings.work_mode == ExporterSettings::WorkMode::WorkModeExportOnly ||
		    m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRenderAndExport) {
			const std::string filepath = "scene_app_sdk.vrscene";
			m_exporter->export_vrscene(filepath);
		}

		m_syncLock.unlock();
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
	m_data_exporter.clearMaterialCache();

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

	for (unsigned int i = 0; i < 20; i++) {
		if (array[i]) {
			layer |= (1 << i);
		}
	}

	return layer;
}

void SceneExporter::sync_object_modiefiers(BL::Object ob, const int &check_updated)
{
	BL::Object::modifiers_iterator modIt;
	for (ob.modifiers.begin(modIt); modIt != ob.modifiers.end(); ++modIt) {
		BL::Modifier mod(*modIt);
		if (mod && mod.show_render() && mod.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
			BL::ParticleSystemModifier psm(mod);
			BL::ParticleSystem psys = psm.particle_system();
			if (psys) {
				BL::ParticleSettings pset(psys.settings());
				if (pset &&
				    pset.type() == BL::ParticleSettings::type_HAIR &&
				    pset.render_type() == BL::ParticleSettings::render_type_PATH) {

					m_data_exporter.exportHair(ob, psm, psys, check_updated);
				}
			}
		}
	}
}

void SceneExporter::sync_object(BL::Object ob, const int &check_updated, const ObjectOverridesAttrs & override)
{
	const std::string &obName = ob.name();
	bool add = false;
	if (override) {
		add = !m_data_exporter.m_id_cache.contains(override.id) && (!override.useInstancer || !m_data_exporter.m_id_cache.contains(ob));
	} else {
		add = !m_data_exporter.m_id_cache.contains(ob);
	}

	if (add) {
		// check if object is hidden, or we are in local view and the object is not light and not the view-ed one
		bool skip_export = (m_exporter->get_is_viewport() ? ob.hide() : ob.hide_render()) ||
		                  !(m_sceneComputedLayers & ::get_layer(ob, m_isLocalView, to_int_layer(m_scene.layers())));

		if (!skip_export || override) {
			if (override) {
				m_data_exporter.m_id_cache.insert(override.id);
				m_data_exporter.m_id_cache.insert(ob);
			} else {
				m_data_exporter.m_id_cache.insert(ob);
			}

			auto overrideAttr = override;

			if (!override && ob.modifiers.length()) {
				overrideAttr.override = true;
				overrideAttr.visible = ob_is_duplicator_renderable(ob);
				overrideAttr.tm = AttrTransformFromBlTransform(ob.matrix_world());
			}

			PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
			PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");

			PRINT_INFO_EX("Syncing: %s...", obName.c_str());
#if 0
			const int data_updated = RNA_int_get(&vrayObject, "data_updated");
			PRINT_INFO_EX("[is_updated = %i | is_updated_data = %i | data_updated = %i | check_updated = %i]: Syncing [%s]\"%s\"...",
						  ob.is_updated(), ob.is_updated_data(), data_updated, check_updated,
						  override.namePrefix.c_str(), ob.name().c_str());
#endif
			if (ob.data() && ob.type() == BL::Object::type_MESH) {
				if (RNA_boolean_get(&vrayClipper, "enabled")) {

					overrideAttr.tm = AttrTransformFromBlTransform(ob.matrix_world());
					overrideAttr.visible = false;
					overrideAttr.override = true;

					m_data_exporter.exportObject(ob, check_updated, overrideAttr);

					const std::string &excludeGroupName = RNA_std_string_get(&vrayClipper, "exclusion_nodes");

					if (NOT(excludeGroupName.empty())) {
						AttrListPlugin plList;
						BL::BlendData::groups_iterator grIt;
						for (m_data.groups.begin(grIt); grIt != m_data.groups.end(); ++grIt) {
							BL::Group gr = *grIt;
							if (gr.name() == excludeGroupName) {
								BL::Group::objects_iterator grObIt;
								for (gr.objects.begin(grObIt); grObIt != gr.objects.end(); ++grObIt) {
									BL::Object ob = *grObIt;
									sync_object(ob, check_updated);
								}
								break;
							}
						}
					}

					m_data_exporter.exportVRayClipper(ob, check_updated, overrideAttr);
				} else {
					m_data_exporter.exportObject(ob, check_updated, overrideAttr);
				}
			} else if(ob.data() && ob.type() == BL::Object::type_LAMP) {
				m_data_exporter.exportLight(ob, check_updated, overrideAttr);
			}

			// Reset update flag
			RNA_int_set(&vrayObject, "data_updated", CGR_NONE);

			sync_object_modiefiers(ob, check_updated);
		}

	}
}

void SceneExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
	const int dupli_use_instancer = RNA_boolean_get(&vrayObject, "use_instancer");


	bool skip_export = (m_exporter->get_is_viewport() ? ob.hide() : ob.hide_render()) ||
	                  !(m_sceneComputedLayers & ::get_layer(ob, m_isLocalView, to_int_layer(m_scene.layers())));

	if (skip_export) {
		PRINT_INFO_EX("Skipping duplication empty %s", ob.name().c_str());
		return;
	}

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

	int dupli_instance = 0;
	bool instancer_visible = true;
	const auto scene_layers = to_int_layer(m_scene.layers());

	BL::Object::dupli_list_iterator dupIt;
	for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {
		if (is_interrupted()) {
			return;
		}

		BL::DupliObject dupliOb(*dupIt);
		BL::Object      dupOb(dupliOb.object());

		const bool is_hidden = m_exporter->get_is_viewport() ? dupliOb.hide() : dupOb.hide_render();
		const bool visible_on_layer = m_sceneComputedLayers & ::get_layer(dupOb, m_isLocalView, scene_layers);

		//instancer_visible = instancer_visible && !is_hidden;

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
			else {
				ObjectOverridesAttrs overrideAttrs;
				overrideAttrs.override = true;
				// If dupli are shown via Instancer we need to hide
				// original object
				overrideAttrs.visible = !is_hidden && visible_on_layer && ob_is_duplicator_renderable(dupOb);

				if (dupli_use_instancer) {
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
					memset(&instancer_item.vel, 0, sizeof(instancer_item.vel));

					dupli_instance++;

					m_data_exporter.m_id_track.insert(ob, m_data_exporter.getNodeName(dupOb));
					sync_object(dupOb, check_updated, overrideAttrs);

				} else {
					overrideAttrs.useInstancer = false;

					// base objects
					overrideAttrs.tm = AttrTransformFromBlTransform(dupOb.matrix_world());
					m_data_exporter.m_id_track.insert(ob, m_data_exporter.getNodeName(dupOb));
					sync_object(dupOb, check_updated, overrideAttrs);


					char namePrefix[255] = {0, };
					snprintf(namePrefix, 250, "Dupli%u@", persistendID);
					overrideAttrs.namePrefix = namePrefix;
					overrideAttrs.tm = AttrTransformFromBlTransform(dupliOb.matrix());
					overrideAttrs.id = persistendID;
					overrideAttrs.visible = true;

					m_data_exporter.m_id_track.insert(ob, overrideAttrs.namePrefix + m_data_exporter.getNodeName(dupOb), IdTrack::DUPLI_NODE);
					sync_object(dupOb, check_updated, overrideAttrs);
				}
			}
		}
	}

	if (dupli_use_instancer) {
		m_data_exporter.exportVrayInstacer2(ob, instances);
	}
}


void SceneExporter::sync_objects(const int &check_updated) {
	PRINT_INFO_EX("SceneExporter::sync_objects(%i)", check_updated);

	// duplicate cycle's logic for layers here
	m_sceneComputedLayers = 0;
	m_isLocalView = m_view3d && m_view3d.local_view();

	auto viewLayers = m_isLocalView ? m_view3d.layers() : m_scene.layers();

	for (int c = 0; c < 20; ++c) {
		m_sceneComputedLayers |= (!!viewLayers[c] << c);
	}

	if (m_isLocalView) {
		auto viewLocalLayers = m_view3d.layers_local_view();
		for (int c = 0; c < 8; ++c) {
			m_sceneComputedLayers |= (!!viewLocalLayers[c] << (20 + c));
		}

		// truncate to local view layers
		m_sceneComputedLayers >>= 20;
	}

	m_data_exporter.setComputedLayers(m_sceneComputedLayers);

	auto scene_layers = to_int_layer(m_scene.layers());
	BL::Scene::objects_iterator obIt;
	for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
		if (is_interrupted()) {
			break;
		}

		BL::Object ob(*obIt);
		const auto & nodeName = m_data_exporter.getNodeName(ob);
		m_data_exporter.m_id_track.insert(ob, nodeName);
		const bool is_updated = check_updated ? ob.is_updated() : true;
		const bool visible_on_layer = m_sceneComputedLayers & ::get_layer(ob, m_isLocalView, scene_layers);

		if (ob.is_duplicator()) {

			if (is_updated) {
				sync_dupli(ob, check_updated);
			}

			if (is_interrupted()) {
				break;
			}

			ObjectOverridesAttrs overAttrs;

			overAttrs.override = true;
			overAttrs.id = reinterpret_cast<intptr_t>(ob.ptr.data);
			overAttrs.tm = AttrTransformFromBlTransform(ob.matrix_world());
			overAttrs.visible = visible_on_layer && ob_is_duplicator_renderable(ob);

			sync_object(ob, check_updated, overAttrs);
		}
		else if (ob.modifiers.length() && visible_on_layer) {
			BL::ArrayModifier modArray(PointerRNA_NULL);
			BL::Modifier mod = ob.modifiers[ob.modifiers.length() - 1];
			ObjectOverridesAttrs overrideAttrs;

			if (mod && mod.show_render() && mod.type() == BL::Modifier::type_ARRAY) {
				// Store modifier
				modArray = BL::ArrayModifier(mod);

				// We could have some heavy array, better use "dynamic_geometry"
				overrideAttrs.useInstancer = true;
				overrideAttrs.override = true;
				overrideAttrs.tm = AttrTransformFromBlTransform(ob.matrix_world());
				overrideAttrs.visible = visible_on_layer && ob_is_duplicator_renderable(ob);
				overrideAttrs.id = reinterpret_cast<intptr_t>(ob.ptr.data);

				// Disable for render so that object is exported without Array modifier
				modArray.show_render(false);
				modArray.show_viewport(false);
			}

			sync_object(ob, check_updated, overrideAttrs);

			if (modArray) {
				ArrayModifierData &amd = *(ArrayModifierData*)(modArray.ptr.data);
				if (!amd.dupliTms) {
					PRINT_ERROR("ArrayModifier dupliTms is null!");
				}

				if (is_updated && amd.dupliTms) {
					AttrInstancer instances;
					instances.frameNumber = m_scene.frame_current();
					instances.data.resize(amd.count - 1);

					std::default_random_engine randomEng((int)(intptr_t)ob.ptr.data);
					std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

					for (int dupliIdx = 0; dupliIdx < amd.count-1; ++dupliIdx) {
						float *dupliTm = amd.dupliTms + dupliIdx * 16;

						// move dupli to local space and let vray move it to world space from wrapper's tm
						float inverted[4][4];
						copy_m4_m4(inverted, ((Object*)ob.ptr.data)->obmat);
						invert_m4(inverted);
						mul_m4_m4m4(inverted, (float (*)[4])dupliTm, inverted);

						AttrInstancer::Item &instancer_item = (*instances.data)[dupliIdx];
						instancer_item.index = dist(randomEng);
						instancer_item.node = nodeName;
						instancer_item.tm = AttrTransformFromBlTransform(inverted);
						memset(&instancer_item.vel, 0, sizeof(instancer_item.vel));
					}

					m_data_exporter.exportVrayInstacer2(ob, instances, true);
				}

				// Restore Array modifier settings
				modArray.show_render(true);
				modArray.show_viewport(true);
			}
		}
		else {
			sync_object(ob, check_updated);
		}
	}
}


void SceneExporter::sync_effects(const int &check_updated)
{
	NodeContext ctx;
	m_data_exporter.exportEnvironment(ctx);
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


void SceneExporter::sync_render_settings()
{
	PointerRNA vrayScene = RNA_pointer_get(&m_scene.ptr, "vray");
	for (const auto &pluginID : RenderSettingsPlugins) {
		PointerRNA propGroup = RNA_pointer_get(&vrayScene, pluginID.c_str());

		PluginDesc pluginDesc(pluginID, pluginID);

		m_data_exporter.setAttrsFromPropGroupAuto(pluginDesc, &propGroup, pluginID);

		m_exporter->export_plugin(pluginDesc);
	}
}


void SceneExporter::sync_render_channels()
{
	BL::NodeTree channelsTree(Nodes::GetNodeTree(m_scene));
	if (channelsTree) {
		BL::Node channelsOutput(Nodes::GetNodeByType(channelsTree, "VRayNodeRenderChannels"));
		if (channelsOutput) {
			PluginDesc settingsRenderChannels("settingsRenderChannels", "SettingsRenderChannels");
			settingsRenderChannels.add("unfiltered_fragment_method", RNA_enum_ext_get(&channelsOutput.ptr, "unfiltered_fragment_method"));
			settingsRenderChannels.add("deep_merge_mode", RNA_enum_ext_get(&channelsOutput.ptr, "deep_merge_mode"));
			settingsRenderChannels.add("deep_merge_coeff", RNA_float_get(&channelsOutput.ptr, "deep_merge_coeff"));
			m_exporter->export_plugin(settingsRenderChannels);

			BL::Node::inputs_iterator inIt;
			for (channelsOutput.inputs.begin(inIt); inIt != channelsOutput.inputs.end(); ++inIt) {
				BL::NodeSocket inSock(*inIt);
				if (inSock && inSock.is_linked()) {
					if (RNA_boolean_get(&inSock.ptr, "use")) {
						NodeContext context;
						m_data_exporter.exportLinkedSocket(channelsTree, inSock, context);
					}
				}
			}
		}
	}
}


void SceneExporter::tag_update()
{
	m_engine.tag_update();
}


void SceneExporter::tag_redraw()
{
	m_engine.tag_redraw();
}


int SceneExporter::is_interrupted()
{
	return m_engine && m_engine.test_break() || m_exporter && m_exporter->is_aborted();
}


int SceneExporter::is_preview()
{
	return m_engine && m_engine.is_preview();
}
