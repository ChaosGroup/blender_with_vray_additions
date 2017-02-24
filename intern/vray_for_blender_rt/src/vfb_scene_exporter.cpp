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
#include "vfb_utils_string.h"
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
#include <atomic>

using namespace VRayForBlender;
FrameExportManager::FrameExportManager(BL::Scene scene, ExporterSettings & settings)
	: m_settings(settings)
	, m_scene(scene)
{
	updateFromSettings();
}

void FrameExportManager::updateFromSettings()
{
	m_lastExportedFrame = INT_MIN; // remove exported cache
	m_animationFrameStep = m_scene.frame_step();
	m_lastFrameToRender = m_scene.frame_end();

	m_sceneSavedFrame = m_scene.frame_current();
	m_sceneFirstFrame = m_scene.frame_start();

	if (m_settings.use_motion_blur) {
		const int framesPerRender = m_settings.mb_duration + 1; // total frames to export for single render frame
		const int midFrame = framesPerRender * m_settings.mb_intervalCenter; // the 'render' frame
		m_mbFramesBefore = std::max(0, midFrame - 1); // motion blur frames before 'render' frame

		// if mb frames is 1, and interval center is 0.5, we want 'render' frame to be 0 and have one more mb frame after it
		m_mbFramesAfter = std::max(0, framesPerRender - midFrame); // motion blur frames after 'render' frame

	} else {
		m_mbFramesBefore = 0;
		m_mbFramesAfter = 0;
	}

	if (m_settings.settings_animation.use) {
		if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop) {
			if (m_loopCameras.empty()) {
				BL::Scene::objects_iterator obIt;
				for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
					BL::Object ob(*obIt);
					if (ob.type() == BL::Object::type_CAMERA) {
						auto dataPtr = ob.data().ptr;
						PointerRNA vrayCamera = RNA_pointer_get(&dataPtr, "vray");
						if (RNA_boolean_get(&vrayCamera, "use_camera_loop")) {
							m_loopCameras.push_back(BL::Camera(ob));
						}
					}
				}

				std::sort(m_loopCameras.begin(), m_loopCameras.end(), [](const BL::Camera & l, const BL::Camera & r) {
					return const_cast<BL::Camera&>(l).name() < const_cast<BL::Camera&>(r).name();
				});

				if (m_loopCameras.empty()) {
					PRINT_WARN("Using camera loop without any camera's marked!");
				} else {
					PRINT_INFO_EX("Camera loop mode, in order camera list:");
					for (auto & c : m_loopCameras) {
						PRINT_INFO_EX("Loop camera \"%s\"", c.name().c_str());
					}
				}
			}
			m_frameToRender = 0;
			m_animationFrameStep = 0;
		} else if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeFrameByFrame) {
			// frae by frame is actually not animation and we need to export current frame only
			m_frameToRender = m_scene.frame_current(); // only current frame
			m_animationFrameStep = 0; // no animation
		} else {
			m_frameToRender = m_scene.frame_start() - m_animationFrameStep;
		}
	} else {
		m_animationFrameStep = 0; // we have no animation so dont move
		m_lastFrameToRender = m_frameToRender = m_sceneSavedFrame;
	}

}

int FrameExportManager::getRenderFrameCount() const {
	if (m_animationFrameStep) {
		return ((m_lastFrameToRender + 1) - m_sceneFirstFrame) / m_animationFrameStep;
	} else if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop) {
		return m_loopCameras.size();
	} else {
		return 1;
	}
}

void FrameExportManager::rewind()
{
	m_frameToRender = m_sceneSavedFrame; // we need to render the current frame
	m_sceneFrameToExport -= (m_mbFramesBefore + 1 + m_mbFramesAfter); // rewind number of exported frames
	m_lastExportedFrame = INT_MIN; // remove exported cache
}

void FrameExportManager::reset()
{
	if (m_scene.frame_current() != m_sceneSavedFrame) {
		m_scene.frame_set(m_sceneSavedFrame, 0.f);
	}
	updateFromSettings();
}

void FrameExportManager::forEachFrameInBatch(std::function<bool(FrameExportManager &)> callback)
{
	if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop) {
		// for camera loop we ignore motion blur
		m_sceneFrameToExport = m_sceneSavedFrame; // for camera loop we export only the current frame, but from different cameras
		callback(*this);
		m_lastExportedFrame++;
		m_frameToRender++;
	} else {
		m_frameToRender += m_animationFrameStep;

		const int firstFrame = std::max(m_frameToRender - m_mbFramesBefore, m_lastExportedFrame + 1);
		const int lastFrame = m_frameToRender + m_mbFramesAfter;

		// this is motion blur frames so the step is always 1
		for (int c = firstFrame; c <= lastFrame; c++) {
			m_sceneFrameToExport = c;
			if (!callback(*this)) {
				break;
			}
			m_lastExportedFrame = c;
		}

	}
}

BL::Camera FrameExportManager::getActiveCamera()
{
	return m_loopCameras[m_frameToRender];
}


// TODO: possible data race when multiple exporters start at the same time
static StrSet RenderSettingsPlugins;
static StrSet RenderGIPlugins;

namespace {
MHash getParticleID(BL::Object dupliGenerator, BL::DupliObject dupliObject, int dupliIndex)
{
	MHash particleID = dupliIndex ^
	                   dupliObject.index() ^
	                   reinterpret_cast<intptr_t>(dupliObject.object().ptr.data) ^
	                   reinterpret_cast<intptr_t>(dupliGenerator.ptr.data);

	for (int i = 0; i < 16; ++i) {
		particleID ^= dupliObject.persistent_id()[i];
	}

	return particleID;
}

MHash getParticleID(BL::Object arrayGenerator, int arrayIndex)
{
	const MHash particleID = arrayIndex ^
	                         reinterpret_cast<intptr_t>(arrayGenerator.ptr.data);

	return particleID;
}
}


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
    , m_frameExporter(m_scene, m_settings)
    , m_data_exporter(m_settings)
	, m_renderWidth(-1)
	, m_renderHeight(-1)
    , m_isLocalView(false)
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
		RenderSettingsPlugins.insert("SettingsOutput");
		RenderSettingsPlugins.insert("SettingsRTEngine");
	}

	if (!RenderGIPlugins.size()) {
		RenderGIPlugins.insert("SettingsGI");
		RenderGIPlugins.insert("SettingsLightCache");
		RenderGIPlugins.insert("SettingsIrradianceMap");
		RenderGIPlugins.insert("SettingsDMCGI");
	}

	m_settings.update(m_context, m_engine, m_data, m_scene, m_view3d);
	m_frameExporter.updateFromSettings();
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

	m_settings.update(m_context, m_engine, m_data, m_scene, m_view3d);

	m_exporter->set_callback_on_message_updated(boost::bind(&BL::RenderEngine::update_stats, &m_engine, _1, _2));

	setup_callbacks();

	m_data_exporter.init(m_exporter);
	m_data_exporter.init_data(m_data, m_scene, m_engine, m_context, m_view3d);

	m_isUndoSync = true;
}

SceneExporter::~SceneExporter()
{
	free();
}

void SceneExporter::init() {
	create_exporter();
	BLI_assert(m_exporter && "Failed to create exporter!");

	// make sure we update settings before exporter - it will read from settings
	m_settings.update(m_context, m_engine, m_data, m_scene, m_view3d);
	m_exporter->init();

	// directly bind to the engine
	m_exporter->set_callback_on_message_updated(boost::bind(&BL::RenderEngine::update_stats, &m_engine, _1, _2));

	setup_callbacks();

	m_exporter->set_commit_state(VRayBaseTypes::CommitAutoOff);

	if (!m_threadManager) {
		// lets init ThreadManager based on object count
		if (m_scene.objects.length() > 10) { // TODO: change to appropriate number
			m_threadManager = ThreadManager::make(2);
		} else {
			// thread manager with 0 means all objects will be exported from current thread
			m_threadManager = ThreadManager::make(0);
		}
	}
}

void SceneExporter::init_data()
{
	m_data_exporter.init(m_exporter);
	m_data_exporter.init_data(m_data, m_scene, m_engine, m_context, m_view3d);
}

void SceneExporter::create_exporter()
{
	m_exporter = ExporterCreate(m_settings.exporter_type, m_settings);
	if (!m_exporter) {
		m_exporter = ExporterCreate(m_settings.exporter_type = ExporterType::ExporterTypeInvalid, m_settings);
	}
}


void SceneExporter::free()
{
	if (m_threadManager) {
		m_threadManager->stop();
	}
	PluginDesc::cache.clear();
}


void SceneExporter::resize(int w, int h) {
	PRINT_INFO_EX("SceneExporter::resize(%i, %i)", w, h);

	if (m_renderHeight == -1 || m_renderWidth == -1) {
		m_exporter->set_render_size(w, h);
	} else if (m_renderHeight != h || m_renderWidth != w) {
		m_exporter->set_render_size(w, h);
	}

	m_renderWidth = w;
	m_renderHeight = h;
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

bool SceneExporter::export_scene(const bool check_updated)
{
	std::lock_guard<std::mutex> lock(m_syncLock);

	PRINT_INFO_EX("SceneExporter::sync(%i)", check_updated);

	m_settings.update(m_context, m_engine, m_data, m_scene, m_view3d);
	if (m_settings.showViewport) {
		m_exporter->show_frame_buffer();
	} else {
		m_exporter->hide_frame_buffer();
	}

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

	return true;
}

void SceneExporter::sync(const bool check_updated)
{
	m_data_exporter.syncStart(m_isUndoSync);

	// TODO: this is hack so we can export object dependent on effect before any other objects so we
	// can hide/show them correctly
	m_exporter->set_prepass(true);
	sync_effects(false);
	m_exporter->set_prepass(false);

	sync_prepass();

	sync_render_settings();
	// Export once per viewport session
	if (!check_updated && !is_viewport()) {
		sync_render_channels();
	}

	m_data_exporter.exportMaterialSettings();

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

	// Sync plugins
	m_exporter->sync();

	m_data_exporter.syncEnd();
	m_isUndoSync = false;
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
		auto lock = m_data_exporter.raiiLock();
		add = !m_data_exporter.m_id_cache.contains(override.id) && (!override.useInstancer || !m_data_exporter.m_id_cache.contains(ob));
	} else {
		auto lock = m_data_exporter.raiiLock();
		add = !m_data_exporter.m_id_cache.contains(ob);
	}
	// this object's ID is already synced - skip
	if (!add) {
		return;
	}

	bool skip_export = !m_data_exporter.isObjectVisible(ob) || m_data_exporter.isObjectInHideList(ob, "export");

	auto overrideAttr = override;

	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
	PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");

	// we should skip this object, but maybe it's already exported then we need to hide it
	if (skip_export && !overrideAttr) {
		std::string exportName;
		bool remove = false, isClipper = false;;
		if (DataExporter::isObMesh(ob)) {
			if (RNA_boolean_get(&vrayClipper, "enabled")) {
				remove = true;
				isClipper = true;
				exportName = "Clipper@" + overrideAttr.namePrefix + m_data_exporter.getNodeName(ob);
			} else {
				exportName = overrideAttr.namePrefix + m_data_exporter.getNodeName(ob);
			}
		} else if (DataExporter::isObLamp(ob)) {
			// we cant hide lamps, so we must remove
			exportName = overrideAttr.namePrefix + m_data_exporter.getLightName(ob);
		}

		ObjectOverridesAttrs oattrs;

		if (m_exporter->getPluginManager().inCache(exportName)) {
			// lamps and clippers should be removed, others should be hidden
			auto lock = m_data_exporter.raiiLock();
			m_data_exporter.m_id_cache.insert(ob);
			if (remove) {
				m_exporter->remove_plugin(exportName);
				if (isClipper) {
					// for clipper we have also node which we need to hide since it will appear when clipper is removed
					oattrs.override = true;
					oattrs.visible = false;
					oattrs.tm = AttrTransformFromBlTransform(ob.matrix_world());
				}
			} else {
				oattrs.override = true;
				oattrs.visible = false;
				oattrs.tm = AttrTransformFromBlTransform(ob.matrix_world());
			}
		}

		if (oattrs.override) {
			if (ob.data() && DataExporter::isObMesh(ob)) {
				m_data_exporter.exportObject(ob, check_updated, oattrs);
			} else if (ob.data() && DataExporter::isObLamp(ob)) {
				m_data_exporter.exportLight(ob, check_updated, oattrs);
			}
		}
	}

	// export the object
	if (!skip_export || overrideAttr) {
		m_data_exporter.saveSyncedObject(ob);

		if (overrideAttr) {
			auto lock = m_data_exporter.raiiLock();
			m_data_exporter.m_id_cache.insert(overrideAttr.id);
			m_data_exporter.m_id_cache.insert(ob);
		} else {
			auto lock = m_data_exporter.raiiLock();
			m_data_exporter.m_id_cache.insert(ob);
		}

		if (!overrideAttr && ob.modifiers.length()) {
			overrideAttr.override = true;
			overrideAttr.visible = m_data_exporter.isObjectVisible(ob);
			overrideAttr.tm = AttrTransformFromBlTransform(ob.matrix_world());
		}

		//PRINT_INFO_EX("Syncing: %s...", obName.c_str());
#if 0
		const int data_updated = RNA_int_get(&vrayObject, "data_updated");
		PRINT_INFO_EX("[is_updated = %i | is_updated_data = %i | data_updated = %i | check_updated = %i]: Syncing [%s]\"%s\"...",
						ob.is_updated(), ob.is_updated_data(), data_updated, check_updated,
						override.namePrefix.c_str(), ob.name().c_str());
#endif
		if (ob.data() && DataExporter::isObMesh(ob)) {
			if (RNA_boolean_get(&vrayClipper, "enabled")) {

				if (!overrideAttr) {
					overrideAttr.tm = AttrTransformFromBlTransform(ob.matrix_world());
					overrideAttr.override = true;
				}
				overrideAttr.visible = RNA_boolean_get(&vrayClipper, "use_obj_mesh");

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
		} else if(ob.data() && DataExporter::isObLamp(ob)) {
			m_data_exporter.exportLight(ob, check_updated, overrideAttr);
		}
	}

	// Reset update flag
	RNA_int_set(&vrayObject, "data_updated", CGR_NONE);

	sync_object_modiefiers(ob, check_updated);
}

void SceneExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
	PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
	bool dupli_use_instancer = RNA_boolean_get(&vrayObject, "use_instancer") && !RNA_boolean_get(&vrayClipper, "enabled");

	using OVisibility = DataExporter::ObjectVisibility;

	const int base_visibility = OVisibility::HIDE_VIEWPORT | OVisibility::HIDE_RENDER | OVisibility::HIDE_LAYER;
	const bool skip_export = !m_data_exporter.isObjectVisible(ob, DataExporter::ObjectVisibility(base_visibility));

	if (skip_export && dupli_use_instancer) {
		const auto exportInstName = "NodeWrapper@Instancer2@" + m_data_exporter.getNodeName(ob);
		m_exporter->remove_plugin(exportInstName);

		PRINT_INFO_EX("Skipping duplication empty %s", ob.name().c_str());
		return;
	}
	MHash maxParticleId = 0;

	AttrInstancer instances;
	instances.frameNumber = m_scene.frame_current();
	int num_instances = 0;
	if (dupli_use_instancer) {
		BL::Object::dupli_list_iterator dupIt;
		for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {
			BL::DupliObject dupliOb(*dupIt);
			BL::Object      dupOb(dupliOb.object());

			const bool is_hidden = dupliOb.hide() || (!m_exporter->get_is_viewport() && dupOb.hide_render());
			const bool is_light = Blender::IsLight(dupOb);
			const bool supported_type = Blender::IsGeometry(dupOb) || is_light;

			if (!is_hidden && supported_type) {
				PointerRNA vrayObject = RNA_pointer_get(&dupOb.ptr, "vray");
				PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
				maxParticleId = std::max(maxParticleId, getParticleID(ob, dupliOb, num_instances++));

				if (is_light || RNA_boolean_get(&vrayClipper, "enabled")) {
					// if any of the duplicated objects is clipper or light we cant use instancer
					dupli_use_instancer = false;
				}
			}
		}

		if (dupli_use_instancer) { // this could be removed
			instances.data.resize(num_instances);
		}
	}

	if (is_interrupted()) {
		return;
	}

	int dupli_instance = 0;
	// if parent is empty or it is hidden in some way, do not show base objects
	const bool hide_from_parent = !m_data_exporter.isObjectVisible(ob) || ob.type() == BL::Object::type_EMPTY;

	BL::Object::dupli_list_iterator dupIt;
	for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {
		if (is_interrupted()) {
			return;
		}

		BL::DupliObject dupliOb(*dupIt);
		BL::Object      dupOb(dupliOb.object());

		const bool is_hidden = dupliOb.hide() || (!m_exporter->get_is_viewport() && dupOb.hide_render());
		const bool is_visible = !hide_from_parent && m_data_exporter.isObjectVisible(dupOb);

		const bool is_light = Blender::IsLight(dupOb);
		const bool supported_type = Blender::IsGeometry(dupOb) || is_light;

		if (!supported_type || is_hidden) {
			continue;
		}

		MHash persistendID;
		persistendID = maxParticleId - getParticleID(ob, dupliOb, dupli_instance);

		ObjectOverridesAttrs overrideAttrs;
		overrideAttrs.override = true;

		if (is_light || !dupli_use_instancer) {
			overrideAttrs.useInstancer = false;

			// sync dupli base object
			if (!is_light && !hide_from_parent) {
				overrideAttrs.visible = is_visible;
				overrideAttrs.tm = AttrTransformFromBlTransform(dupOb.matrix_world());
				{
					auto lock = m_data_exporter.raiiLock();
					m_data_exporter.m_id_track.insert(ob, m_data_exporter.getNodeName(dupOb));
				}
				sync_object(dupOb, check_updated, overrideAttrs);
			}
			overrideAttrs.visible = true;
			overrideAttrs.override = true;
			overrideAttrs.tm = AttrTransformFromBlTransform(dupliOb.matrix());
			overrideAttrs.id = persistendID;

			char namePrefix[255] = {0, };
			snprintf(namePrefix, 250, "Dupli%u@", persistendID);
			overrideAttrs.namePrefix = namePrefix;

			// overrideAttrs.visible = true; do this?

			if (!is_light) {
				// mark the duplication so we can remove in rt
				auto lock = m_data_exporter.raiiLock();
				m_data_exporter.m_id_track.insert(ob, overrideAttrs.namePrefix + m_data_exporter.getNodeName(dupOb), IdTrack::DUPLI_NODE);
			}
			sync_object(dupOb, check_updated, overrideAttrs);
		} else {

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

			{
				auto lock = m_data_exporter.raiiLock();
				m_data_exporter.m_id_track.insert(ob, m_data_exporter.getNodeName(dupOb));
			}
			sync_object(dupOb, check_updated, overrideAttrs);
		}

		dupli_instance++;
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

		const bool doShow = is_viewport() ? mod.show_viewport() : mod.show_render();

		if (doShow) {
			arrModIndecies.push_back(c);
			auto arrMod = BL::ArrayModifier(ob.modifiers[c]);
			auto arrModData = reinterpret_cast<ArrayModifierData*>(arrMod.ptr.data);

			if (!arrModData->dupliTms) {
				// force calculation of meshes
				const int mode = is_viewport() ? 1 : 2;
				WRITE_LOCK_BLENDER_RAII;
				m_data.meshes.new_from_object(m_scene, ob, /*apply_modifiers=*/true, mode, false, false);
			}

			if (is_viewport()) {
				arrMod.show_viewport(false);
			} else {
				arrMod.show_render(false);
			}
			const int modSize = arrModData->count;
			totalCount *= modSize;
			arrModSizes.push_back(modSize);
		}
	}
	// export the node for the base object
	sync_object(ob, check_updated, overrideAttrs);

	for (int c = 0; c < arrModIndecies.size(); ++c) {
		auto arrMod = ob.modifiers[arrModIndecies[c]];
		if (is_viewport()) {
			arrMod.show_viewport(true);
		} else {
			arrMod.show_render(true);
		}
	}

	std::reverse(arrModIndecies.begin(), arrModIndecies.end());
	std::reverse(arrModSizes.begin(), arrModSizes.end());

	float objectInvertedTm[4][4];
	copy_m4_m4(objectInvertedTm, ((Object*)ob.ptr.data)->obmat);
	invert_m4(objectInvertedTm);

	AttrInstancer instances;
	instances.frameNumber = m_scene.frame_current();
	instances.data.resize(totalCount);

	float m4Identity[4][4];
	unit_m4(m4Identity);

	MHash maxInstanceId = 0;
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
				PRINT_ERROR("ArrayModifier dupliTms is null for object \"%s\"", nodeName.c_str());
				return;
			}

			// index 0, means object itself, so we offset all indecies by -1 and skip first
			if (tmIndecies[r] != 0) {
				const float * amdTm = amd->dupliTms + (tmIndecies[r]-1) * 16;
				mul_m4_m4m4(dupliLocalTm, dupliLocalTm, (float (*)[4])amdTm);
			}
		}

		mul_m4_m4m4(dupliLocalTm, dupliLocalTm, objectInvertedTm);

		MHash instanceId = getParticleID(ob, c);
		maxInstanceId = std::max(maxInstanceId, instanceId);

		AttrInstancer::Item &instancer_item = (*instances.data)[c];
		instancer_item.index = instanceId;
		instancer_item.node = nodeName;
		instancer_item.tm = AttrTransformFromBlTransform(dupliLocalTm);
		memset(&instancer_item.vel, 0, sizeof(instancer_item.vel));
	}

	for (int c = 0; c < totalCount; ++c) {
		(*instances.data)[c].index = maxInstanceId - (*instances.data)[c].index;
	}

	m_data_exporter.exportVrayInstacer2(ob, instances, IdTrack::DUPLI_MODIFIER, true);
}


void SceneExporter::sync_objects(const bool check_updated) {
	PRINT_INFO_EX("SceneExporter::sync_objects(%i)", check_updated);

	CondWaitGroup wg(m_scene.objects.length());

	BL::Scene::objects_iterator obIt;
	for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
		if (is_interrupted()) {
			break;
		}

		BL::Object ob(*obIt);
		const auto & nodeName = m_data_exporter.getNodeName(ob);
		{
			auto lock = m_data_exporter.raiiLock();
			m_data_exporter.m_id_track.insert(ob, nodeName);
		}

		m_threadManager->addTask([this, check_updated, ob, &wg, nodeName](int, const volatile bool &) mutable {
			// make wrapper to call wg.done() on function exit
			RAIIWaitGroupTask<CondWaitGroup> doneTask(wg);
			if (is_interrupted()) {
				return;
			}

			const bool is_updated = (check_updated ? ob.is_updated() : true) || m_data_exporter.hasLayerChanged();
			const bool visible = m_data_exporter.isObjectVisible(ob);

			PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
			PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
			const bool dupli_use_instancer = RNA_boolean_get(&vrayObject, "use_instancer") && !RNA_boolean_get(&vrayClipper, "enabled");

			bool has_array_mod = false;
			if (dupli_use_instancer) {
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
			}
			if (ob.is_duplicator()) {
				if (is_updated) {
					sync_dupli(ob, check_updated);
				}
				if (is_interrupted()) {
					return;
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

		}, ThreadManager::Priority::LOW);
	}

	if (m_threadManager->workerCount()) {
		PRINT_INFO_EX("Started export for all objects - waiting for all.");
		wg.wait();
	}
}


void SceneExporter::sync_effects(const bool)
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
	if (m_settings.exporter_type == ExporterType::ExpoterTypeFile) {
		return;
	}

	PointerRNA vrayScene = RNA_pointer_get(&m_scene.ptr, "vray");
	for (const auto &pluginID : RenderSettingsPlugins) {
		if (!RNA_struct_find_property(&vrayScene, pluginID.c_str())) {
			continue;
		}
		PointerRNA propGroup = RNA_pointer_get(&vrayScene, pluginID.c_str());

		PluginDesc pluginDesc(pluginID, pluginID);

		m_data_exporter.setAttrsFromPropGroupAuto(pluginDesc, &propGroup, pluginID);

		if (pluginID == "SettingsOutput") {
			if (!RNA_boolean_get(&vrayScene, "auto_save_render")) {
				continue;
			}

			auto * imgFile = pluginDesc.get("img_file");
			int format = RNA_int_get(&propGroup, "img_format");
			const char * formatNames[] = {"png", "jpg", "tiff", "tga", "sgi", "exr", "vrimg"};
			const char * imgFormat = format >= 0 && format < ArraySize(formatNames) ? formatNames[format] : "";

			if (imgFile) {
				std::lock_guard<PythonGIL> lck(m_pyGIL);

				// this will call python to try to parse any time expressions so we need to restore the state
				imgFile->attrValue.valString = String::ExpandFilenameVariables(
					imgFile->attrValue.valString,
					m_active_camera ? m_active_camera.name() : "Untitled",
					m_scene.name(),
					m_data.filepath(),
					imgFormat);

			}
		}

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
