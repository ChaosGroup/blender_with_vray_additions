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

SubframesHandler::SubframesHandler(BL::Scene scene, ExporterSettings & settings)
	: m_currentSubframeDivision(0)
	, m_settings(settings)
	, m_scene(scene)
	, m_isUpdated(false)
{
}

void SubframesHandler::update() {
	if (m_isUpdated) {
		return;
	}
	m_isUpdated = true;

	// if there is no mblur - we dont have to export subframes
	if (!m_settings.use_motion_blur) {
		m_objectsWithSubframes.clear();
		m_subframeValues.clear();
		return;
	}

	for (auto & ob : Blender::collection(m_scene.objects)) {
		int subframesCount = Blender::getObjectKeyframes(ob);
		if (subframesCount > 2) {
			m_objectsWithSubframes.insert(std::pair<int, BL::Object>(subframesCount, ob));
		}
	}

	for (ObjectCollectionIt it = m_objectsWithSubframes.begin();
		 it != m_objectsWithSubframes.end();
		 it = m_objectsWithSubframes.upper_bound(it->first))
	{
		m_subframeValues.push_back(it->first);
	}
}

SubframesHandler::ObjectCollection &SubframesHandler::getObjectsWithSubframes() {
	update();
	return m_objectsWithSubframes;
}

std::vector<int> &SubframesHandler::getSubframeValues() {
	update();
	return m_subframeValues;
}

FrameExportManager::FrameExportManager(BL::Scene scene, ExporterSettings & settings, BL::BlendData & data)
	: m_settings(settings)
	, m_scene(scene)
	, m_data(data)
	, m_subframes(scene, m_settings)
{

}

void FrameExportManager::updateFromSettings()
{
	m_subframes.update();
	m_lastExportedFrame = std::numeric_limits<float>::lowest(); // remove exported cache
	m_animationFrameStep = m_scene.frame_step();
	m_lastFrameToRender = m_scene.frame_end();

	m_sceneSavedSubframe = m_scene.frame_subframe();
	m_sceneSavedFrame = m_scene.frame_current();
	m_sceneFirstFrame = m_scene.frame_start();
	m_mbGeomSamples = m_settings.mb_samples;

	if (m_settings.use_motion_blur) {
		// motion blur breaks the pattern of exporting only integer values for current frame
		// even so that we may not at all export integer value frames
		// E.G: sceneFrame = 3, mbInterval = 1, mbOffset = -0.5, mbSamples = 3 so export frames are [2.5, 3, 3.5]
		// time line       : 1  .  2  .  3  .  4  .  5
		// export frames   :          ^  ^  ^

		m_mbSampleStep = m_settings.mb_duration / (m_mbGeomSamples - 1); // we must have export sample at the end if the mb interval
		m_mbIntervalStartOffset = m_settings.mb_offset - m_settings.mb_duration * 0.5;
	} else {
		m_mbGeomSamples = 1; // force this to 1 so we can export 1 sample per frame with MB disabled
		m_mbSampleStep = 0;
		m_mbIntervalStartOffset = 0;
	}

	if (m_settings.settings_animation.use) {
		if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop) {
			if (m_loopCameras.empty()) {
				for (auto & ob : Blender::collection(m_scene.objects)) {
					if (ob.type() == BL::Object::type_CAMERA) {
						auto dataPtr = ob.data().ptr;
						PointerRNA vrayCamera = RNA_pointer_get(&dataPtr, "vray");
						if (RNA_boolean_get(&vrayCamera, "use_camera_loop")) {
							m_loopCameras.push_back(ob);
						}
					}
				}

				std::sort(m_loopCameras.begin(), m_loopCameras.end(), [](const BL::Object & l, const BL::Object & r) {
					return const_cast<BL::Object&>(l).name() < const_cast<BL::Object&>(r).name();
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
		} else if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeFrameByFrame &&  m_settings.exporter_type == ExporterType::ExpoterTypeFile) {
			// frame by frame is actually not animation and we need to export current frame only
			m_frameToRender = m_sceneSavedFrame + m_sceneSavedSubframe; // only current frame
			m_animationFrameStep = 0; // no animation
		} else {
			m_frameToRender = m_scene.frame_start() - m_animationFrameStep;
		}
	} else {
		m_animationFrameStep = 0; // we have no animation so dont move
		m_lastFrameToRender = m_frameToRender = (m_sceneSavedFrame + m_sceneSavedSubframe);
	}

	// if we won't use FrameExportManager::forEachExportFrame(), but only export current frame we need to set the current frame
	m_currentFrame = m_frameToRender;
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
	m_currentFrame -= m_mbIntervalStartOffset; // rewind what we exported
	m_lastExportedFrame = std::numeric_limits<float>::lowest(); // remove exported cache
}

void FrameExportManager::reset()
{
	// NOTE: if frame is changed while in preview, the preview will be restarted because of the change in the scene
	if (!m_settings.is_preview) {
		m_scene.frame_set(m_sceneSavedFrame, 0.f);
		updateFromSettings();
	}
}

void FrameExportManager::forEachExportFrame(std::function<bool(FrameExportManager &)> callback)
{
	if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop) {
		// for camera loop we ignore motion blur
		m_currentFrame = m_sceneSavedFrame; // for camera loop we export only the current frame, but from different cameras
		callback(*this);
		m_lastExportedFrame++;
		m_frameToRender++;
	} else {
		m_frameToRender += m_animationFrameStep;

		for (int c = 0; c < m_mbGeomSamples; c++) {
			m_currentFrame = m_frameToRender + m_mbIntervalStartOffset + c * m_mbSampleStep;
			if (!callback(*this)) {
				break;
			}
			m_lastExportedFrame = m_currentFrame;
		}

		const float firstFrame = m_frameToRender + m_mbIntervalStartOffset;
		const float lastFrame = m_frameToRender + m_mbIntervalStartOffset + (m_mbGeomSamples - 1) * m_mbSampleStep;
		const float duration = lastFrame - firstFrame;

		for (int subframes : m_subframes.getSubframeValues()) {
			// set the current subframe export, so exporter can get only object having this subframes value
			m_subframes.setCurrentSubframeDivision(subframes);

			const float keyFrameStep = duration / (subframes - 1); // 3 subframes means we have 2 intervals, so substract 1

			for (int c = 0; c < subframes; c++) {
				m_currentFrame = firstFrame + c * keyFrameStep;

				if (!callback(*this)) {
					break;
				}
			}
		}

		if (m_settings.settings_animation.mode == SettingsAnimation::AnimationModeFrameByFrame) {
			m_lastExportedFrame = std::numeric_limits<float>::lowest(); // remove exported cache since frame by frame will clear all data after render
		}

		m_subframes.setCurrentSubframeDivision(0);
	}
}

BL::Object FrameExportManager::getActiveCamera()
{
	return m_loopCameras[m_frameToRender];
}


static HashSet<std::string> RenderSettingsPlugins = {
	"SettingsOptions",
	"SettingsColorMapping",
	"SettingsDMCSampler",
	"SettingsImageSampler",
	"SettingsGI",
	"SettingsIrradianceMap",
	"SettingsLightCache",
	"SettingsDMCGI",
	"SettingsRaycaster",
	"SettingsRegionsGenerator",
	"SettingsOutput",
	"SettingsRTEngine",
	"SettingsUnitsInfo",
};

static HashSet<std::string> RenderGIPlugins = {
	"SettingsGI",
	"SettingsLightCache",
	"SettingsIrradianceMap",
	"SettingsDMCGI",
};

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

	BL::Object cameraOB = m_view3d ? m_view3d.camera() : scene.camera();
	if (cameraOB.type() == BL::Object::type_CAMERA) {
		m_active_camera = cameraOB;
	}

	m_settings.update(m_context, m_engine, m_data, m_scene, m_view3d);
	m_frameExporter.updateFromSettings();

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
	BL::Object cameraOB = m_view3d ? m_view3d.camera() : m_scene.camera();
	if (cameraOB.type() == BL::Object::type_CAMERA) {
		m_active_camera = cameraOB;
	}

	// make sure we update settings before exporter - it will read from settings
	m_settings.update(m_context, m_engine, m_data, m_scene, m_view3d);
	m_frameExporter.updateFromSettings();

	create_exporter();
	BLI_assert(m_exporter && "Failed to create exporter!");
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
	return true;
}

void SceneExporter::calculate_scene_layers()
{
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
}

void SceneExporter::sync(const bool check_updated)
{
	SCOPED_TRACE_EX("SceneExporter::sync(%d)", static_cast<int>(check_updated));

	if (!m_frameExporter.isCurrentSubframe()) {
		m_data_exporter.syncStart(m_isUndoSync);
	}

	sync_prepass();

	calculate_scene_layers();

	// TODO: this is hack so we can export object dependent on effect before any other objects so we
	// can hide/show them correctly
	m_exporter->set_prepass(true);
	sync_effects(false);
	m_exporter->set_prepass(false);


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

	// for bake view we want objects firts becasue the BakeView plugin references the baked object
	sync_objects(check_updated);
	sync_view(check_updated);

	sync_effects(check_updated);

	if (!m_frameExporter.isCurrentSubframe()) {
		// Sync data (will remove deleted objects)
		m_data_exporter.sync();
		// must be after sync so we update plugins appropriately
		m_data_exporter.exportLightLinker();
	}

	// Sync plugins
	m_exporter->sync();

	if (!m_frameExporter.isCurrentSubframe())
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
	{
		auto lock = m_data_exporter.raiiLock();
		// this object's ID is already synced - skip
		if (override) {
			if (m_data_exporter.m_id_cache.contains(override.id) || (override.useInstancer && m_data_exporter.m_id_cache.contains(ob))) {
				return;
			}
		} else {
			if (m_data_exporter.m_id_cache.contains(ob)) {
				return;
			}
		}

		auto pluginName = override.namePrefix;
		if (DataExporter::isObVrscene(ob)) {
			pluginName += m_data_exporter.getAssetName(ob);
		} else if (DataExporter::isObMesh(ob) || DataExporter::isObGroupInstance(ob)) {
			pluginName += m_data_exporter.getNodeName(ob);
		} else if (DataExporter::isObLamp(ob)) {
			pluginName += m_data_exporter.getLightName(ob);
		}

		if (!pluginName.empty() && !override.isDupli) { // not a duplicate
			m_data_exporter.m_id_track.insert(ob, pluginName);
		}
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
			remove = true;
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

		{
			auto lock = m_data_exporter.raiiLock();
			if (overrideAttr) {
				m_data_exporter.m_id_cache.insert(overrideAttr.id);
				m_data_exporter.m_id_cache.insert(ob);
			} else {
				m_data_exporter.m_id_cache.insert(ob);
			}
		}

		if (!overrideAttr && ob.modifiers.length()) {
			overrideAttr.override = true;
			overrideAttr.visible = m_data_exporter.isObjectVisible(ob);
			overrideAttr.tm = AttrTransformFromBlTransform(ob.matrix_world());
		}
#if 0
		const int data_updated = RNA_int_get(&vrayObject, "data_updated");
		PRINT_INFO_EX("[is_updated = %i | is_updated_data = %i | data_updated = %i | check_updated = %i]: Syncing [%s]\"%s\"...",
						ob.is_updated(), ob.is_updated_data(), data_updated, check_updated,
						override.namePrefix.c_str(), ob.name().c_str());
#endif
		if (DataExporter::isObVrscene(ob)) {
			m_data_exporter.exportAsset(ob, check_updated, override);
		} else if (ob.data() && DataExporter::isObMesh(ob)) {
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
	bool noClipper = !RNA_boolean_get(&vrayClipper, "enabled");

	using OVisibility = DataExporter::ObjectVisibility;

	const int base_visibility = OVisibility::HIDE_VIEWPORT | OVisibility::HIDE_RENDER | OVisibility::HIDE_LAYER;
	const bool skip_export = !m_data_exporter.isObjectVisible(ob, DataExporter::ObjectVisibility(base_visibility));

	if (skip_export && noClipper) {
		const auto exportInstName = "NodeWrapper@Instancer2@" + m_data_exporter.getNodeName(ob);
		m_exporter->remove_plugin(exportInstName);

		PRINT_INFO_EX("Skipping duplication empty %s", ob.name().c_str());

		return;
	}
	MHash maxParticleId = 0;

	enum class InstanceFlags {
		GEOMETRY, HIDDEN, LIGHT, MESH_LIGHT, CLIPPER, FLAGS_COUNT
	};
	using IF = InstanceFlags;
	Blender::FlagsArray<IF, IF::FLAGS_COUNT> instanceFlags;

	AttrInstancer instances;
	instances.frameNumber = m_frameExporter.getCurrentFrame();

	int num_instances = 0;
	int idx_instances = 0;
	if (noClipper) {
		int c = 0;
		for (auto & instance : Blender::collection(ob.dupli_list)) {
			BL::Object      parentOb(instance.object());
			instanceFlags.push_back(false);
			auto flags = instanceFlags.get_flags(c++);

			if (instance.hide() || (!m_exporter->get_is_viewport() && parentOb.hide_render())) {
				flags.set(IF::HIDDEN);
			}

			if (Blender::IsGeometry(parentOb)) {
				flags.set(IF::GEOMETRY);
			}

			if (Blender::IsLight(parentOb)) {
				flags.set(IF::LIGHT);
			} else if (m_data_exporter.objectIsMeshLight(parentOb)) {
				flags.set(IF::MESH_LIGHT);
			}

			// hidden geometries are not exported at all, but hidden mesh lights are
			if (!flags.get(IF::MESH_LIGHT) && flags.get(IF::GEOMETRY) && flags.get(IF::HIDDEN)) {
				continue;
			}

			PointerRNA vrayObject = RNA_pointer_get(&parentOb.ptr, "vray");
			PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");

			if (RNA_boolean_get(&vrayClipper, "enabled")) {
				flags.set(IF::CLIPPER);
			}

			// TODO: consider caching instancer suitable objects - there could be alot of instances of the same object
			// if any of the duplicated objects is clipper or light we cant use instancer
			if (!(flags.get(IF::LIGHT) || flags.get(IF::MESH_LIGHT) || flags.get(IF::CLIPPER))) {
				maxParticleId = std::max(maxParticleId, getParticleID(ob, instance, idx_instances));
				++num_instances;
			}
			++idx_instances;
		}

		if (noClipper) { // this could be removed
			instances.data.resize(num_instances);
		}
	}

	if (is_interrupted()) {
		return;
	}

	int dupliIdx = 0;
	int instancerIdx = 0; // for objects using instancer
	// if parent is empty or it is hidden in some way, do not show base objects
	const bool hide_from_parent = !m_data_exporter.isObjectVisible(ob) || ob.type() == BL::Object::type_EMPTY;

	int c = 0;
	for (auto & instance : Blender::collection(ob.dupli_list)) {
		if (is_interrupted()) {
			return;
		}
		auto flags = instanceFlags.get_flags(c++);
		BL::Object parentOb(instance.object());

		if (!flags.get(IF::MESH_LIGHT) && flags.get(IF::GEOMETRY) && flags.get(IF::HIDDEN)) {
			continue;
		}

		MHash persistendID;
		persistendID = getParticleID(ob, instance, dupliIdx);

		ObjectOverridesAttrs overrideAttrs;
		overrideAttrs.override = true;
		overrideAttrs.isDupli = true;
		overrideAttrs.dupliEmitter = ob;


		// TODO: if we check here it might be faster to skip the redundent call to sync_object
		// node based duplication is for: (light, mesh light, visible clipper)
		if (flags.get(IF::LIGHT) || flags.get(IF::MESH_LIGHT) || (flags.get(IF::CLIPPER) && !flags.get(IF::HIDDEN))) {
			overrideAttrs.useInstancer = false;

			// sync dupli base object
			if (!hide_from_parent) {
				overrideAttrs.visible = !flags.get(IF::HIDDEN);
				overrideAttrs.tm = AttrTransformFromBlTransform(parentOb.matrix_world());
				sync_object(parentOb, check_updated, overrideAttrs);
			}
			overrideAttrs.visible = true;
			overrideAttrs.override = true;
			overrideAttrs.tm = AttrTransformFromBlTransform(instance.matrix());
			overrideAttrs.id = persistendID;

			char namePrefix[255] = {0, };
			snprintf(namePrefix, 250, "Dupli%u@", persistendID);
			overrideAttrs.namePrefix = namePrefix;

			if (flags.get(IF::CLIPPER)) {
				// clipper expects the node to be visible, and will hide it on its own
				overrideAttrs.visible = true;
			}
			// overrideAttrs.visible = true; do this?

			if (flags.get(IF::LIGHT)) {
				// mark the duplication so we can remove in rt
				auto lock = m_data_exporter.raiiLock();
				m_data_exporter.m_id_track.insert(ob, overrideAttrs.namePrefix + m_data_exporter.getLightName(parentOb), IdTrack::DUPLI_LIGHT);
			}
			sync_object(parentOb, check_updated, overrideAttrs);
		} else if (!flags.get(IF::HIDDEN)) {

			overrideAttrs.visible = !flags.get(IF::HIDDEN) && m_data_exporter.isObjectVisible(parentOb, OVisibility::HIDE_LAYER);
			overrideAttrs.tm = AttrTransformFromBlTransform(parentOb.matrix_world());
			overrideAttrs.id = reinterpret_cast<intptr_t>(parentOb.ptr.data);

			float inverted[4][4];
			copy_m4_m4(inverted, ((Object*)parentOb.ptr.data)->obmat);
			invert_m4(inverted);

			float tm[4][4];
			mul_m4_m4m4(tm, ((DupliObject*)instance.ptr.data)->mat, inverted);

			AttrInstancer::Item &instancer_item = (*instances.data)[instancerIdx];
			instancer_item.index = persistendID;
			instancer_item.node = m_data_exporter.getNodeName(parentOb);
			instancer_item.tm = AttrTransformFromBlTransform(tm);
			if (m_settings.use_motion_blur && m_settings.calculate_instancer_velocity) {
				instancer_item.vel = AttrTransformFromBlTransform(static_cast<struct DupliObject*>(instance.ptr.data)->mat);
			} else {
				memset(&instancer_item.vel, 0, sizeof(instancer_item.vel));
			}
			sync_object(parentOb, check_updated, overrideAttrs);

			++instancerIdx;
		}

		dupliIdx++;
	}

	if (noClipper) {
		m_data_exporter.exportVrayInstancer2(ob, instances, IdTrack::DUPLI_INSTACER);
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
	overrideAttrs.dupliEmitter = ob; // array mod is self emitter

	if (!visible) {
		// we have array mod but OB is not rendereable, remove mod
		const auto exportInstName = "NodeWrapper@Instancer2@" + m_data_exporter.getNodeName(ob);
		m_exporter->remove_plugin(exportInstName);
		// export base just in case we need to hide it
		sync_object(ob, check_updated, overrideAttrs);
		return;
	}


	// map of all array modifiers active on the object
	std::vector<int> arrModIndecies;
	// map of dupli cound for each modifier
	std::vector<int> arrModSizes;
	std::vector<bool> modShowStates;
	int instancesCount = 1;
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
				modShowStates.push_back(arrMod.show_viewport());
				arrMod.show_viewport(false);
			} else {
				modShowStates.push_back(arrMod.show_render());
				arrMod.show_render(false);
			}
			const int modSize = arrModData->count;
			// if we have N array modifiers we have N dimentional grid
			// so total objects is product of all dimension's sizes
			instancesCount *= modSize;
			arrModSizes.push_back(modSize);
		}
	}
	const int modCount = arrModIndecies.size();
	// export the node for the base object
	sync_object(ob, check_updated, overrideAttrs);

	std::vector<int> arrCapCount(modCount, 0);
	int capCount = 0; // cap object count
	int dimentionProduct = 1;

	for (int c = 0; c < modCount; c++) {
		auto arrMod = ob.modifiers[arrModIndecies[c]];
		auto arrModData = reinterpret_cast<ArrayModifierData*>(arrMod.ptr.data);

		// this many cap objects per side for this array mod
		const int capObjects = dimentionProduct;
		// count total cap instance objects for this mod
		const int modCapInstances = !!arrModData->start_cap * capObjects + !!arrModData->end_cap * capObjects;

		// save only count for one cap so we dont have to devide by 2 when exporting instances
		arrCapCount[c] = modCapInstances ? capObjects : 0;
		capCount += modCapInstances;

		dimentionProduct *= arrModData->count;

		// restore show states
		if (is_viewport()) {
			arrMod.show_viewport(modShowStates[c]);
		} else {
			arrMod.show_render(modShowStates[c]);
		}
	}

	// reverse these because we traverse mods in reverse order
	std::reverse(arrModIndecies.begin(), arrModIndecies.end());
	std::reverse(arrModSizes.begin(), arrModSizes.end());

	float objectInvertedTm[4][4];
	copy_m4_m4(objectInvertedTm, ((Object*)ob.ptr.data)->obmat);
	invert_m4(objectInvertedTm);

	AttrInstancer instances;
	instances.frameNumber = m_frameExporter.getCurrentFrame();
	instances.data.resize(instancesCount + capCount);

	float m4Identity[4][4];
	unit_m4(m4Identity);

	MHash maxInstanceId = 0;
	for (int c = 0; c < instancesCount; ++c) {
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
				mul_m4_m4m4(dupliLocalTm, (float (*)[4])amdTm, dupliLocalTm);
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

	// TODO: cap objects need to be tracked in m_data_exporter.m_id_track so RT can manage them correctly
	// TODO: cap objects could be "composite" objects (object that is instancer itself) so the name here should be the name of the "root" node

	// add cap object instances
	// we are starting from the last mod, so we need to build up the sizes array
	std::vector<int> capArraySizes;
	int capInstanceIndex = 0;
	// now we need to traverse them in reverse order
	std::reverse(arrModIndecies.begin(), arrModIndecies.end());
	for (int c = 0; c < modCount; c++) {
		auto arrMod = ob.modifiers[arrModIndecies[c]];
		auto arrModData = reinterpret_cast<ArrayModifierData*>(arrMod.ptr.data);
		Object * capObs[2] = {arrModData->start_cap, arrModData->end_cap};

		for (int cap = 0; cap < 2; cap++) {
			if (!capObs[cap]) {
				continue;
			}
			const int capInstanceCount = arrCapCount[c];

			PointerRNA obRNA;
			RNA_id_pointer_create(&capObs[cap]->id, &obRNA);
			BL::Object capOb(obRNA);

			// show/hide override?
			ObjectOverridesAttrs override;
			override.override = true;
			override.useInstancer = true;
			override.tm = AttrTransformFromBlTransform(capOb.matrix_world());
			override.id = reinterpret_cast<intptr_t>(capOb.ptr.data);
			// base of cap object
			sync_object(capOb, check_updated, override);

			// need inverted of cap object
			copy_m4_m4(objectInvertedTm, ((Object*)capOb.ptr.data)->obmat);
			invert_m4(objectInvertedTm);

			float capLocalTm[4][4];
			// start cap is first after all dupli tms
			// end cap is after start cap
			// array is long enough if either of them is present
			const float * capDupliTm = arrModData->dupliTms + ( (arrModData->count + cap) * 16);
			mul_m4_m4m4(capLocalTm, (float (*)[4])capDupliTm, objectInvertedTm);

			// base instance, because it's always there
			MHash instanceId = getParticleID(capOb, capInstanceIndex);
			maxInstanceId = std::max(maxInstanceId, instanceId);

			AttrInstancer::Item &instancer_item = (*instances.data)[instancesCount + capInstanceIndex];
			instancer_item.index = instanceId;
			instancer_item.node = m_data_exporter.getNodeName(capOb);
			instancer_item.tm = AttrTransformFromBlTransform(capLocalTm);
			memset(&instancer_item.vel, 0, sizeof(instancer_item.vel));
			capInstanceIndex++;

			// all instances created because of "next" array modifiers
			for (int r = 1; r < capInstanceCount; r++) {
				auto tmIndecies = unravel_index(r, capArraySizes);
				float capInstanceTm[4][4];
				copy_m4_m4(capInstanceTm, capLocalTm);

				// to get tm of each cap beside base we need to multiply by TM of "next" array mods
				for (int tmIdx = 0; tmIdx < tmIndecies.size(); tmIdx++) {
					auto arrMod = ob.modifiers[arrModIndecies[tmIdx]];
					const auto * amd = reinterpret_cast<ArrayModifierData*>(arrMod.ptr.data);

					// index 0, means object itself, so we offset all indecies by -1 and skip first
					if (tmIndecies[tmIdx] != 0) {
						const float * amdTm = amd->dupliTms + (tmIndecies[tmIdx]-1) * 16;
						mul_m4_m4m4(capInstanceTm, (float (*)[4])amdTm, capInstanceTm);
					}
				}

				MHash instanceId = getParticleID(capOb, capInstanceIndex);
				maxInstanceId = std::max(maxInstanceId, instanceId);

				AttrInstancer::Item &instancer_item = (*instances.data)[instancesCount + capInstanceIndex];
				instancer_item.index = instanceId;
				instancer_item.node = m_data_exporter.getNodeName(capOb);
				instancer_item.tm = AttrTransformFromBlTransform(capInstanceTm);
				memset(&instancer_item.vel, 0, sizeof(instancer_item.vel));

				capInstanceIndex++;
			}
		}

		// push this array mod size so we can calculate "prev" mod cap count
		capArraySizes.push_back(arrModData->count);
	}

	for (int c = 0; c < instancesCount + capCount; ++c) {
		(*instances.data)[c].index = maxInstanceId - (*instances.data)[c].index;
	}

	m_data_exporter.exportVrayInstancer2(ob, instances, IdTrack::DUPLI_MODIFIER, true);
}

void SceneExporter::pre_sync_object(const bool check_updated, BL::Object &ob, CondWaitGroup &wg) {
	if (is_interrupted()) {
		return;
	}

	m_threadManager->addTask([this, check_updated, ob, &wg](int, const volatile bool &) mutable {
		// make wrapper to call wg.done() on function exit
		RAIIWaitGroupTask<CondWaitGroup> doneTask(wg);
		if (is_interrupted()) {
			return;
		}

		const auto obName = ob.name();
		SCOPED_TRACE_EX("Export task for object (%s)", obName.c_str());
		const bool is_updated = (check_updated ? ob.is_updated() : true) || m_data_exporter.hasLayerChanged();
		const bool visible = m_data_exporter.isObjectVisible(ob);

		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
		const bool noClipper = !RNA_boolean_get(&vrayClipper, "enabled");

		bool has_array_mod = false;
		if (noClipper) {
			for (int c = ob.modifiers.length() - 1; c >= 0; --c) {
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

void SceneExporter::sync_objects(const bool check_updated) {
	PRINT_INFO_EX("SceneExporter::sync_objects(%i)", check_updated);

	if (!m_frameExporter.isCurrentSubframe()) {
		CondWaitGroup wg(m_scene.objects.length() - m_frameExporter.countObjectsWithSubframes());
		for (auto & ob : Blender::collection(m_scene.objects)) {
			// If motion blur is enabled, export only object without subframes, theese with will be exported later
			if (!m_settings.use_motion_blur) {
				pre_sync_object(check_updated, ob, wg);
			} else {
				if (!m_frameExporter.hasObjectSubframes(ob)) {
					pre_sync_object(check_updated, ob, wg);
				}
			}
		}

		if (!is_interrupted() && m_threadManager->workerCount()) {
			PRINT_INFO_EX("Started export for all objects - waiting for all.");
			wg.wait();
		}
	}
	else{
		auto range = m_frameExporter.getObjectsWithCurrentSubframes();
		CondWaitGroup wg(m_frameExporter.countObjectsWithCurrentSubframes());
		for (auto obIt = range.first; obIt != range.second; ++obIt) {
			BL::Object ob((*obIt).second);
			pre_sync_object(check_updated, ob, wg);
		}

		if (!is_interrupted() && m_threadManager->workerCount()) {
			PRINT_INFO_EX("Started export for all objects - waiting for all.");
			wg.wait();
		}
	}
}


void SceneExporter::sync_effects(const bool)
{
	NodeContext ctx;
	ctx.isWorldNtree = true;
	m_data_exporter.exportEnvironment(ctx);
}


void SceneExporter::sync_materials()
{
	for (auto & ma : Blender::collection(m_data.materials)) {
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
	if (m_settings.exporter_type == ExporterType::ExpoterTypeFile && !m_settings.is_viewport) {
		return;
	}

	const float sceneFps = m_scene.render().fps() / m_scene.render().fps_base();
	typedef HashMap<std::string, AttrValue> ProperyList;
	typedef HashMap<std::string, ProperyList> PluginOverrideList;
	const PluginOverrideList pluginOverrides = {
		std::make_pair("SettingsUnitsInfo", ProperyList{
			std::make_pair("frames_scale", AttrValue(sceneFps)),
			std::make_pair("seconds_scale", AttrValue(1.f / sceneFps)),
		}),
	};

	const bool lcFromFileOnly = m_settings.is_viewport;
	PluginDesc settingsGI("SettingsGI", "SettingsGI"), settingsLC("SettingsLightCache", "SettingsLightCache");

	PointerRNA vrayObject = RNA_pointer_get(&m_scene.ptr, "vray");
	PointerRNA vrayExporter = RNA_pointer_get(&vrayObject, "Exporter");
	for (const auto &pluginID : RenderSettingsPlugins) {
		if (!RNA_struct_find_property(&vrayObject, pluginID.c_str())) {
			continue;
		}
		PointerRNA propGroup = RNA_pointer_get(&vrayObject, pluginID.c_str());

		PluginDesc pluginDesc(pluginID, pluginID);

		auto plgOverride = pluginOverrides.find(pluginID);
		if (plgOverride != pluginOverrides.end()) {
			for (const auto & propOverride : plgOverride->second) {
				pluginDesc.add(propOverride.first, propOverride.second);
			}
		}

		m_data_exporter.setAttrsFromPropGroupAuto(pluginDesc, &propGroup, pluginID);

		// TODO: rework this to use blender data and not the plugin attrs
		if (pluginID == "SettingsOutput") {
			if (!RNA_boolean_get(&vrayExporter, "auto_save_render")) {
				continue;
			}

			int format = RNA_enum_get(&propGroup, "img_format");
			const char * formatNames[] = {"png", "jpg", "tiff", "tga", "sgi", "exr", "vrimg"};
			const char * imgFormat = format >= 0 && format < ArraySize(formatNames) ? formatNames[format] : "";

			auto * imgFile = pluginDesc.get("img_file");
			auto * imgDir = pluginDesc.get("img_dir");
			if (imgFile || imgDir) {
				// this will call python to try to parse any time expressions so we need to restore the state
				std::lock_guard<PythonGIL> lck(m_pyGIL);

				if (imgFile) {
					imgFile->attrValue.as<AttrSimpleType<std::string>>() = String::ExpandFilenameVariables(
						imgFile->attrValue.as<AttrSimpleType<std::string>>(),
						m_active_camera ? m_active_camera.name() : "Untitled",
						m_scene.name(),
						m_data.filepath(),
						imgFormat);
				}

				if (imgDir) {
					imgDir->attrValue.as<AttrSimpleType<std::string>>() = String::ExpandFilenameVariables(
						imgDir->attrValue.as<AttrSimpleType<std::string>>(),
						m_active_camera ? m_active_camera.name() : "Untitled",
						m_scene.name(),
						m_data.filepath());

					imgDir->attrValue.as<AttrSimpleType<std::string>>() = String::AbsFilePath(imgDir->attrValue.as<AttrSimpleType<std::string>>(), m_data.filepath());
				}
			}
		}

		if (lcFromFileOnly) {
			// if we are in viewport we must get both settingsGi and settingsLC and check
			// that LC is from file or disable it
			if (pluginID == settingsGI.pluginID) {
				settingsGI = pluginDesc;
			} else if (pluginID == settingsLC.pluginID) {
				settingsLC = pluginDesc;
			} else {
				m_exporter->export_plugin(pluginDesc);
			}
		} else {
			m_exporter->export_plugin(pluginDesc);
		}
	}

	if (lcFromFileOnly) {
		bool isLC = false;
		bool isFile = true;

		auto * engineAttr = settingsGI.get("secondary_engine");
		auto * modeAttr = settingsLC.get("mode");
		if (engineAttr) {
			isLC = engineAttr->attrValue.as<AttrSimpleType<int>>() == 3;
		}
		if (modeAttr) {
			// 2 == from file
			isFile = modeAttr->attrValue.as<AttrSimpleType<int>>() == 2;
		}

		if (isLC && !isFile && engineAttr) {
			// disable secondary engine;
			engineAttr->attrValue.as<AttrSimpleType<int>>() = 0;
		}
		m_exporter->export_plugin(settingsGI);
		m_exporter->export_plugin(settingsLC);
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
