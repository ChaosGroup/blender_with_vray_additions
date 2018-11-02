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

#include "vfb_params_json.h"
#include "vfb_plugin_exporter.h"
#include "vfb_scene_exporter.h"
#include "vfb_node_exporter.h"
#include "vfb_export_settings.h"
#include "vfb_render_view.h"

#include "utils/vfb_utils_blender.h"
#include "utils/vfb_utils_string.h"

#include "cgr_config.h"

#include <boost/asio/ip/host_name.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>

#include <exception>

namespace fs = boost::filesystem;
using namespace VRayForBlender;

std::string VRaySettingsExporter::pythonPreviewDir;

ExporterSettings::ExporterSettings()
    : export_meshes(true)
    , override_material(PointerRNA_NULL)
    , current_bake_object(PointerRNA_NULL)
    , camera_stereo_left(PointerRNA_NULL)
    , camera_stereo_right(PointerRNA_NULL)
    , background_scene(PointerRNA_NULL)
{}

RenderMode ExporterSettings::getRenderMode()
{
	enum DeviceType {
		deviceTypeCPU = 0,
		deviceTypeGPU,
	};

	/// Matches "VRayExporter.device_type" menu items.
	const DeviceType deviceType =
		static_cast<DeviceType>(RNA_enum_ext_get(&m_vrayExporter, "device_type"));

	RenderMode renderMode = RENDER_MODE_PRODUCTION;

	switch (deviceType) {
		case deviceTypeCPU: {
			renderMode = is_viewport ? RENDER_MODE_RT_CPU : RENDER_MODE_PRODUCTION;
			break;
		}
		case deviceTypeGPU: {
			/// Matches "VRayExporter.device_gpu_type" menu items.
			enum DeviceTypeGPU {
				DeviceTypeGpuCUDA = 0,
				DeviceTypeGpuOpenCL,
			};

			const DeviceTypeGPU deviceGpuType =
				static_cast<DeviceTypeGPU>(RNA_enum_ext_get(&m_vrayExporter, "device_gpu_type"));

			switch (deviceGpuType) {
				case DeviceTypeGpuCUDA: {
					renderMode = is_viewport ? RENDER_MODE_RT_GPU_CUDA : RENDER_MODE_PRODUCTION_CUDA;
					break;
				}
				case DeviceTypeGpuOpenCL: {
					renderMode = is_viewport ? RENDER_MODE_RT_GPU_OPENCL : RENDER_MODE_PRODUCTION_OPENCL;
					break;
				}
			}

			break;
		}
	}

	return renderMode;
}

void ExporterSettings::update(BL::Context context, BL::RenderEngine engine, BL::BlendData data, BL::Scene _scene, BL::SpaceView3D view3d)
{
	getLog().setRenderEngine(engine);

	is_viewport = !!view3d;
	is_preview = engine && engine.is_preview();

	BL::Scene scene(_scene);
	if (is_preview) {
		scene = context.scene();
	}

	settings_dr.init(scene);
	settings_dr.use = settings_dr.use && !is_preview; // && !isViewport; viewport DR?

	background_scene = BL::Scene(RNA_pointer_get(&scene.ptr, "background_set"));

	m_vrayScene    = RNA_pointer_get(&scene.ptr, "vray");
	m_vrayExporter = RNA_pointer_get(&m_vrayScene, "Exporter");

	calculate_instancer_velocity = RNA_boolean_get(&m_vrayExporter, "calculate_instancer_velocity");
	export_hair         = RNA_boolean_get(&m_vrayExporter, "use_hair");
	export_fluids       = RNA_boolean_get(&m_vrayExporter, "use_smoke");
	use_displace_subdiv = RNA_boolean_get(&m_vrayExporter, "use_displace");
	use_select_preview  = RNA_boolean_get(&m_vrayExporter, "select_node_preview");
	use_subsurf_to_osd  = RNA_boolean_get(&m_vrayExporter, "subsurf_to_osd");
	auto_save_render    = RNA_boolean_get(&m_vrayExporter, "auto_save_render");
	default_mapping     = (DefaultMapping)RNA_enum_ext_get(&m_vrayExporter, "default_mapping");
	export_meshes       = is_preview ? true : RNA_boolean_get(&m_vrayExporter, "auto_meshes");
	export_file_format  = (ExportFormat)RNA_enum_ext_get(&m_vrayExporter, "data_format");
	if (is_preview) {
		// force zip for preview so it can be faster if we are writing to file
		export_file_format = ExportFormat::ExportFormatZIP;
	}

	PointerRNA BakeView = RNA_pointer_get(&m_vrayScene, "BakeView");
	use_bake_view = RNA_boolean_get(&BakeView, "use") && !is_viewport && !is_preview; // no bake in viewport
	if (use_bake_view) {
		PointerRNA bakeObj = RNA_pointer_get(&m_vrayExporter, "currentBakeObject");
		current_bake_object = BL::Object(bakeObj);
	}

	if (!current_bake_object) {
		use_bake_view = false;
	}

	settings_files.use_separate  = RNA_boolean_get(&m_vrayExporter, "useSeparateFiles");
	settings_files.output_type   = (SettingsFiles::OutputDirType)RNA_enum_ext_get(&m_vrayExporter, "output");
	settings_files.output_dir    = RNA_std_string_get(&m_vrayExporter, "output_dir");
	settings_files.output_unique = RNA_boolean_get(&m_vrayExporter, "output_unique");
	settings_files.project_path  = data.filepath();

	use_active_layers = static_cast<ActiveLayers>(RNA_enum_get(&m_vrayExporter, "activeLayers"));
	if (is_preview) {
		// preview scene's layers are actually the different objects that the scene is showinge
		use_active_layers = ActiveLayersScene;
	}
	// Read layers if custom are specified
	if(use_active_layers == ActiveLayersCustom) {
		RNA_boolean_get_array(&m_vrayExporter, "customRenderLayers", active_layers.data);
	}

	if (is_preview || is_viewport || use_bake_view) {
		settings_animation.mode = SettingsAnimation::AnimationMode::AnimationModeNone;
	} else {
		settings_animation.mode = (SettingsAnimation::AnimationMode)RNA_enum_get(&m_vrayExporter, "animation_mode");
	}

	settings_animation.use = settings_animation.mode != SettingsAnimation::AnimationModeNone;

	settings_animation.frame_start   = scene.frame_start();
	settings_animation.frame_current = scene.frame_current();
	settings_animation.frame_step    = scene.frame_step();

	use_stereo_camera = false;
	use_motion_blur = false;
	use_physical_camera = false;

	// Find if we need hide from view
	if (settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop) {
		for (auto ob : Blender::collection(scene.objects)) {
			if (ob.type() == BL::Object::type_CAMERA) {
				auto dataPtr = ob.data().ptr;
				PointerRNA vrayCamera = RNA_pointer_get(&dataPtr, "vray");

				if (RNA_boolean_get(&vrayCamera, "use_camera_loop")) {
					if (RNA_boolean_get(&vrayCamera, "hide_from_view")) {
						use_hide_from_view = true;
						break;
					}
				}
			}
		}
	} else {
		BL::Object camera(scene.camera());
		// NOTE: Could happen if scene has no camera and we initing exporter for
		// proxy export, for example.
		if (camera && camera.type() == BL::Object::type_CAMERA) {
			BL::Camera camera_data(camera.data());

			PointerRNA vrayCamera = RNA_pointer_get(&camera_data.ptr, "vray");
			PointerRNA physCamera = RNA_pointer_get(&vrayCamera, "CameraPhysical");
			use_physical_camera = RNA_boolean_get(&physCamera, "use");

			if (!is_preview) { // no stereo camera for preview
				PointerRNA stereoSettings = RNA_pointer_get(&m_vrayScene, "VRayStereoscopicSettings");
				PointerRNA cameraStereo = RNA_pointer_get(&vrayCamera, "CameraStereoscopic");
				use_stereo_camera = (stereoSettings.data && RNA_boolean_get(&stereoSettings, "use")) && (cameraStereo.data && RNA_boolean_get(&cameraStereo, "use"));
				if (use_stereo_camera) {
					const auto leftCamName = RNA_std_string_get(&cameraStereo, "LeftCam");
					const auto rightCamName = RNA_std_string_get(&cameraStereo, "RightCam");

					BL::BlendData::objects_iterator obIt;
					int found = 0;
					for (data.objects.begin(obIt); found < 2 && obIt != data.objects.end(); ++obIt) {
						const auto camName = obIt->name();
						if (camName == leftCamName) {
							camera_stereo_left = *obIt;
							found++;
						} else if (camName == rightCamName) {
							camera_stereo_right = *obIt;
							found++;
						}
					}
					if (found != 2) {
						use_stereo_camera = false;
						getLog().error("Failed to find cameras for stereo camera!");
					}
				}
			}

			use_hide_from_view = RNA_boolean_get(&vrayCamera, "hide_from_view");
			PointerRNA mbSettings = RNA_pointer_get(&vrayCamera, "SettingsMotionBlur");
			mb_samples = RNA_int_get(&mbSettings, "geom_samples");

			if (use_physical_camera) {
				use_motion_blur = RNA_boolean_get(&physCamera, "use_moblur");
				enum PhysicalCameraType {
					Still = 0, Cinematic = 1, Video = 2,
				};
				const auto cameraType = static_cast<PhysicalCameraType>(RNA_enum_ext_get(&physCamera, "type"));
				const float frameDuration = 1.0 / (scene.render().fps() / scene.render().fps_base());

				if (cameraType == Still) {
					mb_duration = 1.0 / (RNA_float_get(&physCamera, "shutter_speed") * frameDuration);
					mb_offset = mb_duration * 0.5;
				} else if (cameraType == Cinematic) {
					mb_duration = RNA_float_get(&physCamera, "shutter_angle") / 360.0;
					mb_offset = RNA_float_get(&physCamera, "shutter_offset") / 360.0 + mb_duration * 0.5;
				} else if (cameraType == Video) {
					mb_duration = 1.0 + RNA_float_get(&physCamera, "latency") / frameDuration;
					mb_offset = -mb_duration * 0.5;
				} else {
					use_motion_blur = false;
				}
			} else {
				if (RNA_boolean_get(&mbSettings, "on")) {
					use_motion_blur = true;
					mb_duration = RNA_float_get(&mbSettings, "duration");
					mb_offset = RNA_float_get(&mbSettings, "interval_center");
				}
			}
		}
	}

	// disable motion blur for bake render
	use_motion_blur = use_motion_blur && !use_bake_view && !is_preview;

	std::string overrideName;
	PointerRNA settingsOptions = RNA_pointer_get(&m_vrayScene, "SettingsOptions");
	if(RNA_boolean_get(&settingsOptions, "mtl_override_on")) {
		overrideName = RNA_std_string_get(&settingsOptions, "mtl_override");
	}

	if(overrideName != override_material_name) {
		override_material_name = overrideName;
		if (override_material_name.empty()) {
			override_material = BL::Material(PointerRNA_NULL);
		} else {
			for (auto & mat : Blender::collection(data.materials)) {
				if (mat.name() == overrideName) {
					override_material = mat;
					break;
				}
			}
		}
	}

	exporter_type = (ExporterType)RNA_enum_get(&m_vrayExporter, "backend");
	if (exporter_type != ExporterType::ExpoterTypeFile) {
		// there is no sense to skip meshes for other than file export
		export_meshes = true;
	}

	work_mode = (WorkMode)RNA_enum_get(&m_vrayExporter, "work_mode");

	zmq_server_port    = RNA_int_get(&m_vrayExporter, "zmq_port");
	zmq_server_address = RNA_std_string_get(&m_vrayExporter, "zmq_address");
	if (zmq_server_address.empty()) {
		zmq_server_address = "127.0.0.1";
	}

	render_mode = getRenderMode();

	is_gpu = render_mode != RENDER_MODE_PRODUCTION &&
	         render_mode != RENDER_MODE_RT_CPU;

	m_viewportResolution = RNA_int_get(&m_vrayExporter, "viewport_resolution") / 100.0f;
	viewport_image_quality = RNA_int_get(&m_vrayExporter, "viewport_jpeg_quality");
	if (is_viewport) {
		viewport_image_type = static_cast<ImageType>(RNA_enum_ext_get(&m_vrayExporter, "viewport_image_type"));
	} else {
		viewport_image_type = ImageType::RGBA_REAL;
	}
	show_vfb = !is_viewport && work_mode != WorkMode::WorkModeExportOnly && !is_preview && RNA_boolean_get(&m_vrayExporter, "display");
	close_on_stop = RNA_boolean_get(&m_vrayExporter, "autoclose");

	verbose_level = static_cast<VRayVerboseLevel>(RNA_enum_ext_get(&m_vrayExporter, "verboseLevel"));
	switch (verbose_level) {
		case LevelNoInfo:   getLog().setLogLevel(LogLevel::none); break;
		case LevelErrors:   getLog().setLogLevel(LogLevel::error);break;
		case LevelWarnings: getLog().setLogLevel(LogLevel::warning); break;
		case LevelProgress: getLog().setLogLevel(LogLevel::progress); break;
		case LevelAll:      getLog().setLogLevel(LogLevel::debug);break;
		default: ;
	}
}


bool ExporterSettings::check_data_updates()
{
	bool do_update_check = false;
	if(settings_animation.use && settings_animation.frame_current > settings_animation.frame_start) {
		do_update_check = true;
	}
	return do_update_check;
}


bool ExporterSettings::is_first_frame()
{
	bool is_first_frame = false;
	if(!settings_animation.use) {
		is_first_frame = true;
	}
	else if(settings_animation.frame_current == settings_animation.frame_start) {
		is_first_frame = true;
	}
	return is_first_frame;
}


int ExporterSettings::getViewportShowAlpha()
{
	return RNA_boolean_get(&m_vrayExporter, "viewport_alpha");
}


SettingsDR::SettingsDR():
    use(false)
{
	hostname = boost::asio::ip::host_name();
}

void SettingsDR::init(BL::Scene scene)
{
	PointerRNA vrayScene = RNA_pointer_get(&scene.ptr, "vray");
	PointerRNA vrayDR = RNA_pointer_get(&vrayScene, "VRayDR");

	use = RNA_boolean_get(&vrayDR, "on");

	share_name = RNA_std_string_get(&vrayDR, "share_name");

	network_type = (NetworkType)RNA_enum_get(&vrayDR, "networkType");
	sharing_type = (SharingType)RNA_enum_get(&vrayDR, "assetSharing");

	renderOnlyOnHodes = RNA_boolean_get(&vrayDR, "renderOnlyOnNodes");

	if (use) {
		const int nodesCount = RNA_collection_length(&vrayDR, "nodes");
		PropertyRNA *nodesCollection = RNA_struct_find_property(&vrayDR, "nodes");
		if (nodesCollection && nodesCount) {
			CollectionPropertyIterator nodesIter;
			RNA_property_collection_begin(&vrayDR, nodesCollection, &nodesIter);
			while (nodesIter.valid) {
				if (RNA_boolean_get(&nodesIter.ptr, "use")) {
					std::string addr = RNA_std_string_get(&nodesIter.ptr, "address");
					addr.push_back(':');

					if (!addr.empty()) {
						const bool overridePort = RNA_boolean_get(&nodesIter.ptr, "port_override");
						if (overridePort) {
							const int portNum = RNA_int_get(&nodesIter.ptr, "port");
							char portNumBuff[32] = {0};
							snprintf(portNumBuff, sizeof(portNumBuff), "%d", portNum);
							addr += portNumBuff;
						} else {
							addr += "20207";
						}
						hosts.push_back(addr);
					}
				}
				RNA_property_collection_next(&nodesIter);
			}
			RNA_property_collection_end(&nodesIter);
		}
	}
}

const HashSet<std::string> VRaySettingsExporter::IgnoredPlugins = {
	// TODO: These plugins have to be implemented
	"SettingsPtexBaker",
	"SettingsVertexBaker",
	"SettingsImageFilter",
	// These plugins will be exported manually
	"Includer",
	"SettingsEnvironment",
	"OutputDeepWriter",
	// These plugins are exported from camera export
	"BakeView",
	"VRayStereoscopicSettings",
	// Unused plugins for now
	"SettingsCurrentFrame",
	"SettingsLightTree",
	"SettingsColorMappingModo",
	"SettingsDR",
	"OutputTest",
	// Deprecated
	"SettingsPhotonMap",
	"RTEngine",
	"EffectLens",
	// Manually exported
	"SettingsGI",
	"SettingsLightCache",
	"SettingsLightLinker"
};

const HashSet<std::string> VRaySettingsExporter::DelayPlugins = {
	"SettingsPNG",
	"SettingsJPEG",
	"SettingsTIFF",
	"SettingsTGA",
	"SettingsSGI",
	"SettingsEXR",
	"SettingsVRST",
	"SettingsImageSampler",
	"SettingsOutput",
};


void VRaySettingsExporter::init(PluginExporterPtr pluginExporter, BL::Scene &scene, BL::Context &context)
{
	this->pluginExporter = pluginExporter;
	this->scene = scene;
	this->context = context;

	vrayScene = RNA_pointer_get(&scene.ptr, "vray");
	vrayExporter = RNA_pointer_get(&vrayScene, "Exporter");
}

void VRaySettingsExporter::exportPlugins()
{
	delayPlugins.clear();
	delayPlugins.reserve(DelayPlugins.size());

	const PluginParamDescList &settingsPlugins = GetPluginsOfType(ParamDesc::PluginType::PluginSettings);
	for (const ParamDesc::PluginParamDesc * const desc : settingsPlugins) {
		if (IgnoredPlugins.find(desc->pluginID) != IgnoredPlugins.end()) {
			continue;
		}

		if (DelayPlugins.find(desc->pluginID) != DelayPlugins.end()) {
			delayPlugins.push_back(desc);
			continue;
		}

		exportSettingsPlugin(*desc);
	}
}

void VRaySettingsExporter::exportDelayedPlugins() {
	for (const ParamDesc::PluginParamDesc * const desc : delayPlugins) {
		exportSettingsPlugin(*desc);
	}

	exportLCGISettings();
}


struct PropNotFound
	: std::runtime_error
{
	PropNotFound(const char * const prop)
		: std::runtime_error(prop)
	{}
};

/// Generic get for some type of property from RNA pointer
/// If the the type is wrong it will just the property value's memory as the requested type
/// @throw - PropNotFound if the property does not exist
template <typename T>
T get(PointerRNA &ptr, const char *name)
{
	// static_assert(false, "Not implemented type for get<T>(PointerRNA&, const char *)");
	return T();
}

template <>
bool get(PointerRNA &ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(&ptr, name);
	if (prop) {
		return RNA_property_boolean_get(&ptr, prop);
	}
	throw PropNotFound(name);
}

template <>
int get(PointerRNA &ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(&ptr, name);
	if (prop) {
		return RNA_property_int_get(&ptr, prop);
	}
	throw PropNotFound(name);
}

template <>
float get(PointerRNA &ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(&ptr, name);
	if (prop) {
		return RNA_property_float_get(&ptr, prop);
	}
	throw PropNotFound(name);
}

template <>
PointerRNA get(PointerRNA &ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(&ptr, name);
	if (prop) {
		return RNA_property_pointer_get(&ptr, prop);
	}
	throw PropNotFound(name);
}

template <>
std::string get(PointerRNA &ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(&ptr, name);
	if (prop) {
		std::string result;
		result = "";
		result.resize(RNA_property_string_length(&ptr, prop));
		RNA_property_string_get(&ptr, prop, &result[0]);
		return result;
	}
	throw PropNotFound(name);
}

template <typename T>
T get(PointerRNA &ptr, const std::string & name)
{
	return get<T>(ptr, name.c_str());
}

bool VRaySettingsExporter::checkPluginOverrides(const std::string &pluginId, PointerRNA &propertyGroup, PluginDesc &pluginDesc)
{
	propertyGroup = get<PointerRNA>(vrayScene, pluginId);

	if (pluginId == "SettingsRegionsGenerator") {
		if (get<bool>(propertyGroup, "lock_size")) {
			pluginDesc.add("yc", AttrValue(get<int>(propertyGroup, "xc")));
		}
	} else if (pluginId.find("Filter") == 0) {
		const char * imageSampler = "SettingsImageSampler";
		PointerRNA sampler = get<PointerRNA>(vrayScene, imageSampler);
		const std::string &selectedFilter = RNA_enum_identifier_get(&sampler, "filter_type");
		if (selectedFilter.empty() || pluginId.find(selectedFilter) == std::string::npos) {
			return false;
		}
	} else if (pluginId == "SphericalHarmonicsExporter" || pluginId == "SphericalHarmonicsRenderer") {
		const char * settingsGi = "SettingsGI";
		PointerRNA gi = get<PointerRNA>(vrayScene, settingsGi);
		if (static_cast<ExporterSettings::GIEngine>(RNA_enum_ext_get(&gi, "primary_engine")) != ExporterSettings::GIEngine::EngineSphericalharmonics) {
			return false;
		}

		if (RNA_enum_get(&vrayExporter, "spherical_harmonics") == 0) {
			if (pluginId == "SphericalHarmonicsRenderer") {
				return false;
			}
		} else {
			if (pluginId == "SphericalHarmonicsExporter") {
				return false;
			}
		}
	} else if (pluginId == "SettingsUnitsInfo") {
		const float sceneFps = scene.render().fps() / scene.render().fps_base();
		pluginDesc.add("frames_scale", AttrValue(sceneFps));
		pluginDesc.add("seconds_scale", AttrValue(1.f / sceneFps));

		PointerRNA unitSettings = get<PointerRNA>(scene.ptr, "unit_settings");
		if (RNA_enum_get(&unitSettings, "system") != 0) {
			pluginDesc.add("meters_scale", get<float>(unitSettings, "scale_length"));
		}
	} else if (pluginId == "SettingsColorMapping") {
		BL::Scene mainScene = context.scene();

		PointerRNA vrayObj = get<PointerRNA>(mainScene.ptr, "vray");
		PointerRNA settingsCM = get<PointerRNA>(vrayObj, pluginId.c_str());

		// override preview collor mapping with parent scene's collor mapping
		if (settings.is_preview && get<bool>(settingsCM, "preview_use_scene_cm")) {
			propertyGroup = settingsCM;
		}
	} else if (pluginId == "SettingsImageSampler") {
		if (get<bool>(propertyGroup, "use_dmc_treshhold")) {
			PointerRNA dmcSampler = get<PointerRNA>(vrayScene, "SettingsDMCSampler");
			pluginDesc.add("dmc_threshold", AttrValue(get<float>(dmcSampler, "adaptive_threshold")));
		}

		{
			const int minSubdiv = get<int>(propertyGroup, "progressive_minSubdivs");
			const int maxSubdiv = get<int>(propertyGroup, "progressive_maxSubdivs");

			if (minSubdiv > maxSubdiv) {
				pluginDesc.add("progressive_minSubdivs", maxSubdiv);
				pluginDesc.add("progressive_maxSubdivs", minSubdiv);
			}
		}

		{
			const int maxRate = get<int>(propertyGroup, "subdivision_maxRate");
			const int minRate = get<int>(propertyGroup, "subdivision_minRate");

			if (minRate > maxRate) {
				pluginDesc.add("subdivision_minRate", maxRate);
				pluginDesc.add("subdivision_maxRate", minRate);
			}
		}

		enum class RenderMaskMode { Disable, None, Objects, ObjectId };
		const RenderMaskMode maskMode = static_cast<RenderMaskMode>(RNA_enum_ext_get(&propertyGroup, "render_mask_mode"));

		if (maskMode == RenderMaskMode::Objects) {
			AttrListPlugin selectedObjectsList;
			if (get<bool>(propertyGroup, "render_mask_objects_selected")) {
				// for each selected object, get all exported plugins
				for (const BL::Object &ob : selectedObjects) {
					for (const std::string &pluginName : dataExporter.m_id_track.getAllObjectPlugins(ob)) {
						selectedObjectsList.append(pluginName);
					}
				}
			} else {
				// for each object in the group, get all exported plugins
				for (const BL::Object &object: dataExporter.getObjectList("", get<std::string>(propertyGroup, "render_mask_objects"))) {
					for (const std::string &pluginName : dataExporter.m_id_track.getAllObjectPlugins(object)) {
						selectedObjectsList.append(pluginName);
					}
				}
			}

			if (selectedObjectsList.empty()) {
				pluginDesc.add("render_mask_mode", static_cast<int>(RenderMaskMode::Disable));
			} else {
				pluginDesc.add("render_mask_objects", selectedObjectsList);
			}
		} else if (maskMode == RenderMaskMode::ObjectId) {
			std::string objectIds = get<std::string>(propertyGroup, "render_mask_object_ids");
			if (objectIds.empty()) {
				pluginDesc.add("render_mask_mode", static_cast<int>(RenderMaskMode::Disable));
			} else {
				AttrListInt objectIdList;
				typedef boost::split_iterator<std::string::iterator> SplitIter;
				auto tokenFinder = boost::token_finder(boost::algorithm::is_any_of(";"), boost::token_compress_on);
				for (auto iter = boost::make_split_iterator(objectIds, tokenFinder); iter != SplitIter(); ++iter) {
					int id;
					if (boost::conversion::try_lexical_convert(*iter, id)) {
						objectIdList.append(id);
					}
				}
				pluginDesc.add("render_mask_object_ids", objectIdList);
			}
		}
	} else if (pluginId == "SettingsIrradianceMap") {
		const int minRate = get<int>(propertyGroup, "min_rate");
		const int maxRate = get<int>(propertyGroup, "max_rate");
		if (minRate > maxRate) {
			getLog().warning("SettingsIrradianceMap: \"Min. Rate\" is more than \"Max. Rate\"");
			pluginDesc.add("min_rate", maxRate);
			pluginDesc.add("max_rate", minRate);
		}

		if (get<bool>(vrayExporter, "draft")) {
			pluginDesc.add("subdivs", static_cast<int>(get<int>(propertyGroup, "subdivs") / 5.0));
		}

		if (!get<bool>(propertyGroup, "auto_save")) {
			pluginDesc.add("auto_save_file", AttrIgnore());
		}

		// There is no file prop when we are calculating it
		enum Mode {SingleFrame, MultiFrame, FromFile, AddToMap, IncrementalToMap, Bucket, AnimationPrepass, AnimationRender};
		const Mode mode = static_cast<Mode>(RNA_enum_ext_get(&propertyGroup, "mode"));
		if (mode != FromFile) {
			pluginDesc.add("file", AttrIgnore());
		}
	} else if (pluginId == "SettingsLightCache") {
		if (get<bool>(propertyGroup, "num_passes_auto")) {
			pluginDesc.add("num_passes", scene.render().threads());
		}

		if (get<bool>(vrayExporter, "draft")) {
			pluginDesc.add("subdivs", static_cast<int>(get<int>(propertyGroup, "subdivs") / 10.0));
		}

		if (!get<bool>(propertyGroup, "auto_save")) {
			pluginDesc.add("auto_save_file", AttrIgnore());
		}

		// There is no file prop when we are calculating it
		enum Mode { SingleFrame, FlyTrough, FromFile, ProgressivePathTracing };
		const Mode mode = static_cast<Mode>(RNA_enum_ext_get(&propertyGroup, "mode"));
		if (mode != FromFile) {
			pluginDesc.add("file", AttrIgnore());
		}
	} else if (pluginId == "SettingsOptions") {
		if (settings.settings_dr.use && settings.settings_dr.sharing_type == SettingsDR::SharingTypeTransfer) {
			pluginDesc.add("misc_transferAssets", true);
		}
		// remove mtl_override
	} else if (pluginId == "SettingsOutput") {
		int width = viewParams.renderSize.w;
		int height = viewParams.renderSize.h;

		if (BL::Object camera = scene.camera()) {
			auto cameraData = camera.data();
			PointerRNA vrayCamera = get<PointerRNA>(cameraData.ptr, "vray");

			if (settings.is_gpu) {
				if (settings.use_stereo_camera) {
					width *= 2;
				}
			} else {
				PointerRNA stereoSettings = get<PointerRNA>(vrayScene, "VRayStereoscopicSettings");

				if (settings.use_stereo_camera && get<bool>(stereoSettings, "adjust_resolution")) {
					if (get<bool>(stereoSettings, "adjust_resolution")) {
						width *= 2;
					}
				}
			}
		}

		if (settings.use_bake_view) {
			height = width;
		}

		static const char *widths[] = {"img_width", "bmp_width", "rgn_width", "r_width"};
		static const char *heights[] = {"img_height", "bmp_height", "rgn_height", "r_height"};

		for (int c = 0; c < ArraySize(widths); c++) {
			pluginDesc.add(widths[c], width);
			pluginDesc.add(heights[c], height);
		}

		if (!settings.is_preview && !settings.auto_save_render) {
			pluginDesc.add("img_file", AttrIgnore());
			pluginDesc.add("img_dir", AttrIgnore());
		} else {
			std::string imgDir, imgFile;

			if (settings.is_preview) {
				imgDir = pythonPreviewDir;
				imgFile = "preview.exr";
				pluginDesc.add("img_file_needFrameNumber", false);
			} else {
				imgDir = String::ExpandFilenameVariables(get<std::string>(propertyGroup, "img_dir"), context);
				imgFile = String::ExpandFilenameVariables(get<std::string>(propertyGroup, "img_file"), context);
				enum ImageFormat {PNG, JPG, TIFF, TGA, SGI, EXR, VRIMG};
				const ImageFormat format = static_cast<ImageFormat>(RNA_enum_ext_get(&propertyGroup, "img_format"));
				const char *extensions[] = {".png", ".jpg", ".tiff", ".tga", ".sgi", ".exr", ".vrimg"};

				if (format >= PNG && format <= VRIMG) {
					imgFile += extensions[format];
				} else {
					imgFile += extensions[PNG];
				}

				// EXR == 5
				if (format == EXR && !get<bool>(propertyGroup, "relements_separateFiles")) {
					pluginDesc.add("img_rawFile", true);
				}
			}
			if (imgDir.back() != '/' && imgDir.back() != '\\') {
				imgDir.push_back('/');
			}

			// make sure the directory exists
			fs::create_directories(imgDir);

			pluginDesc.add("img_dir", imgDir);
			pluginDesc.add("img_file", imgFile);
		}

		pluginDesc.add("anim_start", frameExporter.getFirstFrame());
		pluginDesc.add("anim_end", frameExporter.getLastFrame());
		pluginDesc.add("frame_start", frameExporter.getFirstFrame());

		if (settings.is_preview) {
			pluginDesc.add("img_noAlpha", true);
		}
	} else if (pluginId == "SettingsRTEngine") {
		if (settings.is_viewport) {
			pluginDesc.add("cpu_samples_per_pixel", 1);
			pluginDesc.add("cpu_bundle_size", 64);
			pluginDesc.add("gpu_samples_per_pixel", 1);
			pluginDesc.add("gpu_bundle_size ", 128);
			pluginDesc.add("undersampling",  4);
			pluginDesc.add("progressive_samples_per_pixel", 0);

			pluginDesc.add("noise_threshold", 0.f);
		} else {
			pluginDesc.add("gpu_samples_per_pixel", 16);
			pluginDesc.add("gpu_bundle_size", 256);
			pluginDesc.add("undersampling", 0);
			pluginDesc.add("progressive_samples_per_pixel", 0);

			pluginDesc.add("noise_threshold", get<float>(propertyGroup, "noise_threshold"));
		}
	} else if (pluginId == "SettingsVFB") {
		if (!get<bool>(propertyGroup, "use")) {
			return false;
		}
	} else if (pluginId == "SettingsCaustics") {
		if (!get<bool>(propertyGroup, "auto_save")) {
			pluginDesc.add("auto_save_file", AttrIgnore());
		}

		// There is no file prop when we are calculating it
		enum Mode { New, FromFile };
		const Mode mode = static_cast<Mode>(RNA_enum_ext_get(&propertyGroup, "mode"));
		if (mode == New) {
			pluginDesc.add("file", AttrIgnore());
		}
	}

	return true;
}

void VRaySettingsExporter::exportLCGISettings()
{
	PluginDesc settingsGI("SettingsGI", "SettingsGI");
	PluginDesc settingsLC("SettingsLightCache", "SettingsLightCache");

	PointerRNA giPropGroup;
	PointerRNA lcPropGroup;

	try {
		checkPluginOverrides(settingsGI.pluginID, giPropGroup, settingsGI);
	} catch (PropNotFound &ex) {
		getLog().error("Property \"%s\" not found when exporting SettingsGI", ex.what());
	}

	try {
		checkPluginOverrides(settingsLC.pluginID, lcPropGroup, settingsLC);
	} catch (PropNotFound &ex) {
		getLog().error("Property \"%s\" not found when exporting SettingsLightCache", ex.what());
	}

	dataExporter.setAttrsFromPropGroupAuto(settingsLC, &lcPropGroup, settingsLC.pluginID);
	dataExporter.setAttrsFromPropGroupAuto(settingsGI, &giPropGroup, settingsGI.pluginID);
	
	/// for viewport rendering Light Cache can be only 'From File' mode
	/// if it is not we should disable it
	if (settings.is_viewport) {
		bool isLC = false;
		bool isFile = true;
		PluginAttr * engineAttr = settingsGI.get("secondary_engine");
		PluginAttr * modeAttr = settingsLC.get("mode");
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
	}

	pluginExporter->export_plugin(settingsGI);
	pluginExporter->export_plugin(settingsLC);
}

void VRaySettingsExporter::exportSettingsPlugin(const ParamDesc::PluginParamDesc &desc)
{
	PointerRNA propGroup;
	PluginDesc pluginDesc(desc.pluginID, desc.pluginID);
	try {
		if (!checkPluginOverrides(desc.pluginID, propGroup, pluginDesc)) {
			return;
		}
		dataExporter.setAttrsFromPropGroupAuto(pluginDesc, &propGroup, desc.pluginID);
		pluginExporter->export_plugin(pluginDesc);
	} catch (PropNotFound &ex) {
		getLog().error("Property \"%s\" not found when exporting %s", ex.what(), desc.pluginID.c_str());
	}

}
