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

#include "vfb_export_settings.h"

#include <boost/asio/ip/host_name.hpp>


using namespace VRayForBlender;


ExporterSettings::ExporterSettings():
    export_meshes(true)
{}


void ExporterSettings::update(BL::Context context, BL::RenderEngine engine, BL::BlendData data, BL::Scene _scene)
{
	BL::Scene scene(_scene);
	if (engine && engine.is_preview()) {
		scene = context.scene();
	}

	m_vrayScene    = RNA_pointer_get(&scene.ptr, "vray");
	m_vrayExporter = RNA_pointer_get(&m_vrayScene, "Exporter");

	export_hair         = RNA_boolean_get(&m_vrayExporter, "use_hair");
	export_fluids       = RNA_boolean_get(&m_vrayExporter, "use_smoke");
	use_displace_subdiv = RNA_boolean_get(&m_vrayExporter, "use_displace");
	use_select_preview  = RNA_boolean_get(&m_vrayExporter, "select_node_preview");
	use_subsurf_to_osd  = RNA_boolean_get(&m_vrayExporter, "subsurf_to_osd");
	default_mapping     = (DefaultMapping)RNA_enum_ext_get(&m_vrayExporter, "default_mapping");
	export_file_format  = (ExportFormat)RNA_enum_ext_get(&m_vrayExporter, "data_format");
	export_meshes       = RNA_boolean_get(&m_vrayExporter, "auto_meshes");

	settings_files.use_separate  = RNA_boolean_get(&m_vrayExporter, "useSeparateFiles");
	settings_files.output_type   = (SettingsFiles::OutputDirType)RNA_enum_ext_get(&m_vrayExporter, "output");
	settings_files.output_dir    = RNA_std_string_get(&m_vrayExporter, "output_dir");
	settings_files.output_unique = RNA_boolean_get(&m_vrayExporter, "output_unique");
	settings_files.project_path  = data.filepath();

	// Read layers if custom are specified
	use_active_layers = (ActiveLayers)RNA_enum_get(&m_vrayExporter, "activeLayers");
	if(use_active_layers == ActiveLayersCustom) {
		RNA_boolean_get_array(&m_vrayExporter, "customRenderLayers", active_layers.data);
	}

	settings_animation.mode = (SettingsAnimation::AnimationMode)RNA_enum_get(&m_vrayExporter, "animation_mode");
	settings_animation.use  = settings_animation.mode != SettingsAnimation::AnimationMode::AnimationModeNone && !engine.is_preview();
	if (settings_animation.use) {
		settings_animation.frame_start   = scene.frame_start();
		settings_animation.frame_current = scene.frame_current();
		settings_animation.frame_step    = scene.frame_step();
	}

	// Find if we need hide from view
	if (settings_animation.mode == SettingsAnimation::AnimationModeCameraLoop) {
		BL::BlendData::cameras_iterator caIt;
		for (data.cameras.begin(caIt); caIt != data.cameras.end(); ++caIt) {
			BL::Camera camera_data = *caIt;

			PointerRNA vrayCamera = RNA_pointer_get(&camera_data.ptr, "vray");

			if (RNA_boolean_get(&vrayCamera, "use_camera_loop")) {
				if (RNA_boolean_get(&vrayCamera, "hide_from_view")) {
					use_hide_from_view = true;
					break;
				}
			}
		}
	}
	else {
		BL::Object camera(scene.camera());
		// NOTE: Could happen if scene has no camera and we initing exporter for
		// proxy export, for example.
		if (camera) {
			BL::Camera camera_data(camera.data());

			PointerRNA vrayCamera = RNA_pointer_get(&camera_data.ptr, "vray");

			use_hide_from_view = RNA_boolean_get(&vrayCamera, "hide_from_view");
		}
	}

#if 0
	m_mtlOverrideName.clear();
	m_mtlOverride = BL::Material(PointerRNA_NULL);
	PointerRNA settingsOptions = RNA_pointer_get(&vrayScene, "SettingsOptions");
	if(RNA_boolean_get(&settingsOptions, "mtl_override_on")) {
		const std::string &overrideName = RNA_std_string_get(&settingsOptions, "mtl_override");
		if(NOT(overrideName.empty())) {
			BL::BlendData::materials_iterator maIt;
			for(b_data.materials.begin(maIt); maIt != b_data.materials.end(); ++maIt) {
				BL::Material ma = *maIt;
				if(ma.name() == overrideName) {
					m_mtlOverride     = ma;
					m_mtlOverrideName = Node::GetMaterialName((Material*)ma.ptr.data);
					break;
				}
			}
		}
	}
#endif

	exporter_type = (ExpoterType)RNA_enum_get(&m_vrayExporter, "backend");
	if (engine.is_preview()) {
		exporter_type = ExpoterTypeZMQ;
	}
	work_mode     = (WorkMode)RNA_enum_get(&m_vrayExporter, "work_mode");

	zmq_server_port    = RNA_int_get(&m_vrayExporter, "zmq_port");
	zmq_server_address = RNA_std_string_get(&m_vrayExporter, "zmq_address");
	if (zmq_server_address.empty()) {
		zmq_server_address = "127.0.0.1";
	}

	m_renderMode = (VRayBaseTypes::RenderMode)RNA_enum_ext_get(&m_vrayExporter, "rendering_mode");
	if (engine.is_preview()) {
		m_renderMode = VRayBaseTypes::RenderModeRtCpu;
	}

	m_renderModeViewport = (VRayBaseTypes::RenderMode)RNA_enum_ext_get(&m_vrayExporter, "viewport_rendering_mode");

	m_viewportResolution = RNA_int_get(&m_vrayExporter, "viewport_resolution") / 100.0f;
	viewportQuality = RNA_int_get(&m_vrayExporter, "viewport_jpeg_quality");
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

	share_name = RNA_std_string_get(&vrayDR, "share_name");

	network_type = (NetworkType)RNA_enum_get(&vrayDR, "networkType");
	sharing_type = (SharingType)RNA_enum_get(&vrayDR, "assetSharing");
}
