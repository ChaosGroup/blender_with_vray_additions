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

#include "RE_engine.h"

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
    , m_active_camera(view3d ? view3d.camera() : scene.camera())
    , m_python_thread_state(nullptr)
    , m_exporter(nullptr)
    , m_isLocalView(false)
    , m_isRunning(false)
	, m_isUndoSync(false)
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

void SceneExporter::pause_for_undo()
{
	m_context = PointerRNA_NULL;
	m_engine = PointerRNA_NULL;
	m_data = PointerRNA_NULL;
	m_scene = PointerRNA_NULL;
	m_view3d = PointerRNA_NULL;
	m_region3d = PointerRNA_NULL;
	m_region = PointerRNA_NULL;

	m_exporter->set_callback_on_message_updated([](const char *, const char *) {});
}

void SceneExporter::resume_from_undo(BL::Context         context,
	                                 BL::RenderEngine    engine,
	                                 BL::BlendData       data,
	                                 BL::Scene           scene)
{
	m_context = context;
	m_engine = engine;
	m_data = data;
	m_scene = scene;
	m_view3d = BL::SpaceView3D(context.space_data());
	m_region3d = context.region_data();
	m_region = context.region();
	m_active_camera = BL::Camera(m_view3d ? m_view3d.camera() : scene.camera());

	m_settings.update(m_context, m_engine, m_data, m_scene);

	m_exporter->set_callback_on_message_updated(boost::bind(&BL::RenderEngine::update_stats, &m_engine, _1, _2));

	setup_callbacks();

	m_data_exporter.init(m_exporter, m_settings);
	m_data_exporter.init_data(m_data, m_scene, m_engine, m_context, m_view3d);

	m_isUndoSync = true;
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

void SceneExporter::render_start()
{
	if (m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRender ||
	    m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRenderAndExport) {
		m_exporter->start();

		// TODO: check if sync is faster with manual commit
		// m_exporter->set_commit_state(VRayBaseTypes::CommitAutoOff);
	} else {
		PRINT_INFO_EX("Work mode WorkModeExportOnly, skipping renderer_start");
	}
}

void SceneExporter::sync(const int &check_updated)
{
	if (!m_syncLock.try_lock()) {
		tag_update();
		return;
	}

	PRINT_INFO_EX("SceneExporter::sync(%i)",
		            check_updated);

	clock_t begin = clock();

	m_data_exporter.syncStart(m_isUndoSync);

	m_settings.update(m_context, m_engine, m_data, m_scene);
	if (m_settings.showViewport) {
		m_exporter->show_frame_buffer();
	} else {
		m_exporter->hide_frame_buffer();
	}
	sync_prepass();

	// duplicate cycle's logic for layers here
	m_sceneComputedLayers = 0;
	m_isLocalView = m_view3d && m_view3d.local_view();

	BlLayers viewLayers;
	if (m_isLocalView) {
		viewLayers = m_view3d.layers();
	} else {
		if (m_settings.use_active_layers == ExporterSettings::ActiveLayersCustom) {
			viewLayers = m_settings.active_layers;
		} else if (m_settings.use_active_layers == ExporterSettings::ActiveLayersAll) {
			for (int c = 0; c < 20; ++c) {
				viewLayers[c] = 1;
			}
		} else {
			viewLayers = m_scene.layers();
		}
	}

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
	m_data_exporter.setComputedLayers(m_sceneComputedLayers, m_isLocalView);


	VRayBaseTypes::RenderMode renderMode = m_view3d
		                                    ? m_settings.getViewportRenderMode()
		                                    : m_settings.getRenderMode();

	m_exporter->set_render_mode(renderMode);
	m_exporter->set_viewport_quality(m_settings.viewportQuality);


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

	if (m_exporter->get_commit_state() != VRayBaseTypes::CommitAction::CommitAutoOn) {
		m_exporter->commit_changes();
	}

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

	m_data_exporter.syncEnd();
	m_isUndoSync = false;

	m_syncLock.unlock();
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
	m_data_exporter.setActiveCamera(m_active_camera);
	m_data_exporter.resetSyncState();

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

	m_exporter->set_prepass(true);
	sync_effects(false);
	m_exporter->set_prepass(false);
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
		bool skip_export = !m_data_exporter.isObjectVisible(ob) || m_data_exporter.isObjectInHideList(ob, "export");

		auto overrideAttr = override;

		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");

		if (skip_export && !overrideAttr) {
			// we should skip this object, but maybe it's already exported then we need to hide it
			std::string exportName;
			bool remove = false;
			if (ob.type() == BL::Object::type_MESH) {
				if (RNA_boolean_get(&vrayClipper, "enabled")) {
					remove = true;
					exportName = "Clipper@" + obName;
				} else {
					exportName = m_data_exporter.getNodeName(ob);
				}
			} else if (ob.type() == BL::Object::type_LAMP) {
				// we cant hide lamps, so we must remove
				exportName = m_data_exporter.getLightName(ob);
				remove = true;
			}

			if (m_exporter->getPluginManager().inCache(exportName)) {
				// lamps and clippers should be removed, others should be hidden
				if (remove) {
					m_exporter->remove_plugin(exportName);
					// also check modifiers to remove
					sync_object_modiefiers(ob, check_updated);
				} else {
					skip_export = false;
					overrideAttr.override = true;
					overrideAttr.visible = false;
					overrideAttr.tm = AttrTransformFromBlTransform(ob.matrix_world());
				}
			}
		}

		if (!skip_export || overrideAttr) {
			m_data_exporter.saveSyncedObject(ob);

			if (overrideAttr) {
				m_data_exporter.m_id_cache.insert(overrideAttr.id);
				m_data_exporter.m_id_cache.insert(ob);
			} else {
				m_data_exporter.m_id_cache.insert(ob);
			}

			if (!overrideAttr && ob.modifiers.length()) {
				overrideAttr.override = true;
				overrideAttr.visible = m_data_exporter.isObjectVisible(ob);
				overrideAttr.tm = AttrTransformFromBlTransform(ob.matrix_world());
			}

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
					overrideAttr.visible = true;
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

	using OVisibility = DataExporter::ObjectVisibility;

	const int base_visibility = OVisibility::HIDE_VIEWPORT | OVisibility::HIDE_RENDER | OVisibility::HIDE_LAYER;
	const bool skip_export = !m_data_exporter.isObjectVisible(ob, DataExporter::ObjectVisibility(base_visibility));

	if (skip_export && dupli_use_instancer) {
		const auto exportInstName = "NodeWrapper@Instancer2@" + m_data_exporter.getNodeName(ob);
		m_exporter->remove_plugin(exportInstName);

		PRINT_INFO_EX("Skipping duplication empty %s", ob.name().c_str());
		return;
	}

	AttrInstancer instances;
	instances.frameNumber = m_scene.frame_current();
	int num_instances = 0;
	if (dupli_use_instancer) {
		BL::Object::dupli_list_iterator dupIt;
		for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {
			BL::DupliObject dupliOb(*dupIt);
			BL::Object      dupOb(dupliOb.object());

			const bool is_hidden = m_exporter->get_is_viewport() ? dupliOb.hide() : dupOb.hide_render();
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

	BL::Object::dupli_list_iterator dupIt;
	for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {
		if (is_interrupted()) {
			return;
		}

		BL::DupliObject dupliOb(*dupIt);
		BL::Object      dupOb(dupliOb.object());

		const bool is_hidden = m_exporter->get_is_viewport() ? dupliOb.hide() : dupOb.hide_render();
		const bool is_visible =  ob.type() == BL::Object::type_EMPTY ? false : m_data_exporter.isObjectVisible(dupOb);

		const bool is_light = Blender::IsLight(dupOb);
		const bool supported_type = Blender::IsGeometry(dupOb) || is_light;

		MHash persistendID;
		MurmurHash3_x86_32((const void*)dupIt->persistent_id().data, 8 * sizeof(int), 42, &persistendID);

		if (!is_hidden && supported_type) {
			if (is_light) {
				ObjectOverridesAttrs overrideAttrs;

				overrideAttrs.override = true;
				overrideAttrs.visible = is_visible;
				overrideAttrs.tm = AttrTransformFromBlTransform(dupliOb.matrix());
				overrideAttrs.useInstancer = false;
				overrideAttrs.id = persistendID;

				char namePrefix[255] = {0, };
				snprintf(namePrefix + 1, 250, "D%u@", persistendID);
				overrideAttrs.namePrefix = namePrefix;

				sync_object(dupOb, check_updated, overrideAttrs);
			}
			else {
				ObjectOverridesAttrs overrideAttrs;
				overrideAttrs.override = true;

				// If dupli are shown via Instancer we need to hide
				// original object

				if (dupli_use_instancer) {
					overrideAttrs.visible = is_visible;
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
					overrideAttrs.visible = is_visible;

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

	if (dupli_use_instancer && num_instances) {
		m_data_exporter.exportVrayInstacer2(ob, instances, IdTrack::DUPLI_INSTACER);
	}
}

static std::vector<int> unravel_index(int index, const std::vector<int> & dimSizes) {
	auto result = dimSizes;
	for (int c = dimSizes.size() - 1; c >= 0; --c) {
		result[c] = index % dimSizes[c];
		index /= dimSizes[c];
	}
	return result;
}

void SceneExporter::sync_array_mod(BL::Object ob, const int &check_updated) {
	const auto & nodeName = m_data_exporter.getNodeName(ob);
	const bool is_updated = (check_updated ? ob.is_updated() : true) || m_data_exporter.hasLayerChanged();
	const bool visible = m_data_exporter.isObjectVisible(ob);

	ObjectOverridesAttrs overrideAttrs;

	overrideAttrs.useInstancer = true;
	overrideAttrs.override = true;
	overrideAttrs.tm = AttrTransformFromBlTransform(ob.matrix_world());
	overrideAttrs.visible = false;
	overrideAttrs.id = reinterpret_cast<intptr_t>(ob.ptr.data);

	if (!visible) {
		// we have array mod but OB is not rendereable, remove mod
		const auto exportInstName = "NodeWrapper@Instancer2@" + m_data_exporter.getNodeName(ob);
		m_exporter->remove_plugin(exportInstName);
		// export base just in case we need to hide it
		sync_object(ob, check_updated, overrideAttrs);
		return;
	}

	// if we have N array modifiers we have N dimentional grid
	// so total objects is product of all dimension's sizes

	std::vector<int> arrModIndecies, arrModSizes;
	int totalCount = 1;
	for (int c = ob.modifiers.length() - 1; c >= 0; --c) {
		auto mod = ob.modifiers[c];
		if (mod.type() != BL::Modifier::type_ARRAY) {
			break;
		}

		if (mod.show_render()) {
			arrModIndecies.push_back(c);
			auto arrMod = BL::ArrayModifier(ob.modifiers[c]);
			arrMod.show_viewport(false);
			arrMod.show_render(false);
			const int modSize = reinterpret_cast<ArrayModifierData*>(arrMod.ptr.data)->count;
			totalCount *= modSize;
			arrModSizes.push_back(modSize);
		}
	}
	// export the node for the base object
	sync_object(ob, check_updated, overrideAttrs);

	for (int c = 0; c < arrModIndecies.size(); ++c) {
		auto arrMod = ob.modifiers[arrModIndecies[c]];
		arrMod.show_render(true);
		arrMod.show_viewport(true);
	}

	std::reverse(arrModIndecies.begin(), arrModIndecies.end());
	std::reverse(arrModSizes.begin(), arrModSizes.end());

	float objectInvertedTm[4][4];
	copy_m4_m4(objectInvertedTm, ((Object*)ob.ptr.data)->obmat);
	invert_m4(objectInvertedTm);

	AttrInstancer instances;
	instances.frameNumber = m_scene.frame_current();
	instances.data.resize(totalCount);

	std::default_random_engine randomEng((int)(intptr_t)ob.ptr.data);
	std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

	float m4Identity[4][4];
	unit_m4(m4Identity);

	for (int c = 0; c < totalCount; ++c) {
		auto tmIndecies = unravel_index(c, arrModSizes);
		BLI_assert(tmIndecies.size() == arrModIndecies.size());
		// tmIndecies maps index to TM in n'th modifier

		float dupliLocalTm[4][4];
		unit_m4(dupliLocalTm);

		for (int r = 0; r < tmIndecies.size(); ++r) {
			auto arrMod = ob.modifiers[arrModIndecies[r]];
			const auto * amd = reinterpret_cast<ArrayModifierData*>(arrMod.ptr.data);

			if (!amd->dupliTms) {
				PRINT_ERROR("ArrayModifier dupliTms is null!");
				return;
			}

			// index 0, means object itself, so we offset all indecies by -1 and skip first
			if (tmIndecies[r] != 0) {
				const float * amdTm = amd->dupliTms + (tmIndecies[r]-1) * 16;
				mul_m4_m4m4(dupliLocalTm, dupliLocalTm, (float (*)[4])amdTm);
			}
		}

		mul_m4_m4m4(dupliLocalTm, dupliLocalTm, objectInvertedTm);

		AttrInstancer::Item &instancer_item = (*instances.data)[c];
		instancer_item.index = dist(randomEng);
		instancer_item.node = nodeName;
		instancer_item.tm = AttrTransformFromBlTransform(dupliLocalTm);
		memset(&instancer_item.vel, 0, sizeof(instancer_item.vel));
	}

	m_data_exporter.exportVrayInstacer2(ob, instances, IdTrack::DUPLI_MODIFIER, true);
}


void SceneExporter::sync_objects(const int &check_updated) {
	PRINT_INFO_EX("SceneExporter::sync_objects(%i)", check_updated);

	BL::Scene::objects_iterator obIt;
	for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
		if (is_interrupted()) {
			break;
		}

		BL::Object ob(*obIt);
		const auto & nodeName = m_data_exporter.getNodeName(ob);
		m_data_exporter.m_id_track.insert(ob, nodeName);
		const bool is_updated = (check_updated ? ob.is_updated() : true) || m_data_exporter.hasLayerChanged();
		const bool visible = m_data_exporter.isObjectVisible(ob);

		bool has_array_mod = false;
		for (int c = ob.modifiers.length() - 1 ; c >= 0; --c) {
			if (ob.modifiers[c].type() != BL::Modifier::type_ARRAY) {
				// stop on last non array mod - we export only array mods on top of mod stack
				break;
			}
			if (ob.modifiers[c].show_render()) {
				// we found atleast one stuitable array mod
				has_array_mod = true;
				break;
			}
		}

		if (ob.is_duplicator()) {
			if (is_updated) {
				sync_dupli(ob, check_updated);
			}

			if (is_interrupted()) {
				break;
			}

			// As in old exporter - dont sync base if its light dupli
			if (!Blender::IsLight(ob)) {
				ObjectOverridesAttrs overAttrs;

				overAttrs.override = true;
				overAttrs.id = reinterpret_cast<intptr_t>(ob.ptr.data);
				overAttrs.tm = AttrTransformFromBlTransform(ob.matrix_world());
				overAttrs.visible = visible;

				sync_object(ob, check_updated, overAttrs);
			}
		}
		else if (has_array_mod) {
			sync_array_mod(ob, check_updated);
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
				m_data_exporter.exportMaterial(ma, PointerRNA_NULL);
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
	if (m_engine) {
		m_engine.tag_update();
	}
}


void SceneExporter::tag_redraw()
{
	if (m_engine) {
		m_engine.tag_redraw();
	}
}


int SceneExporter::is_interrupted()
{
	return (m_engine && m_engine.test_break()) || (m_exporter && m_exporter->is_aborted());
}


int SceneExporter::is_preview()
{
	return m_engine && m_engine.is_preview();
}

bool SceneExporter::is_engine_undo_taged()
{
	if (m_engine) {
		RenderEngine *re = reinterpret_cast<RenderEngine*>(m_engine.ptr.data);
		return re->flag & RE_ENGINE_UNDO_REDO_FREE;
	}
	return false;
}
