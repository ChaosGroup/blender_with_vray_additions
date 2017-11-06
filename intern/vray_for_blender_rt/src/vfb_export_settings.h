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

#ifndef VRAY_FOR_BLENDER_EXPORT_SETTINGS_H
#define VRAY_FOR_BLENDER_EXPORT_SETTINGS_H

#include <Python.h>

#include "vfb_rna.h"
#include "vfb_plugin_exporter_types.h"
#include "base_types.h"

namespace VRayForBlender {

struct SettingsDR {
	enum SharingType {
		SharingTypeTransfer = 0,
		SharingTypeShare,
		SharingTypeAbsPath,
	};

	enum NetworkType {
		NetworkTypeWindows = 0,
		NetworkTypeUnix,
	};

	SettingsDR();
	void         init(BL::Scene scene);

	bool         use;
	NetworkType  network_type;
	SharingType  sharing_type;
	std::string  share_path;
	std::string  share_name;
	std::string  hostname;
};


struct SettingsAnimation {
	enum AnimationMode {
		AnimationModeNone = 0,
		AnimationModeFull,
		AnimationModeFullCamera,
		AnimationModeFullNoGeometry,
		AnimationModeCameraLoop,
		AnimationModeFrameByFrame,
	};

	SettingsAnimation():
	    use(false)
	{}

	int           use;
	float         frame_current;
	int           frame_start;
	int           frame_step;
	AnimationMode mode;
};


struct SettingsFiles {
	enum OutputDirType {
		OutputDirTypeUser,
		OutputDirTypeScene,
		OutputDirTypeTmp
	};

	SettingsFiles():
	    use_separate(true),
	    file_main(nullptr),
	    file_object(nullptr),
	    file_environment(nullptr),
	    file_geometry(nullptr),
	    file_lights(nullptr),
	    file_materials(nullptr),
	    file_textures(nullptr)
	{}

	int       use_separate;

	PyObject *file_main;
	PyObject *file_object;
	PyObject *file_environment;
	PyObject *file_geometry;
	PyObject *file_lights;
	PyObject *file_materials;
	PyObject *file_textures;

	OutputDirType output_type;
	std::string   output_dir;
	bool          output_unique;
	std::string   project_path;
};


struct ExporterSettings {
	enum DefaultMapping {
		DefaultMappingObject = 0,
		DefaultMappingCube,
		DefaultMappingChannel
	};

	enum ExportFormat {
		ExportFormatZIP = 0,
		ExportFormatHEX,
		ExportFormatASCII
	};

	enum WorkMode {
		WorkModeRender = 0,
		WorkModeRenderAndExport,
		WorkModeExportOnly,
	};

	enum ActiveLayers {
		ActiveLayersScene = 0,
		ActiveLayersAll,
		ActiveLayersCustom
	};

	enum VRayVerboseLevel {
		LevelNoInfo = 0,
		LevelErrors,
		LevelWarnings,
		LevelProgress,
		LevelAll
	};

	using ImageType = VRayBaseTypes::AttrImage::ImageType;
	using RenderMode = VRayBaseTypes::RenderMode;

	ExporterSettings();
	void              update(BL::Context context, BL::RenderEngine engine, BL::BlendData data, BL::Scene scene, BL::SpaceView3D view3d);

	bool              check_data_updates();
	bool              is_first_frame();

	SettingsAnimation settings_animation;
	SettingsDR        settings_dr;
	SettingsFiles     settings_files;

	ExporterType      exporter_type;
	WorkMode          work_mode;

	// Export options
	BlLayers          active_layers;
	ActiveLayers      use_active_layers;

	DefaultMapping    default_mapping;
	ExportFormat      export_file_format;

	bool              export_meshes;
	bool              export_hair;
	bool              export_fluids;

	bool              use_stereo_camera;

	bool              use_physical_camera;
	bool              use_motion_blur;
	bool              use_hide_from_view;
	bool              use_displace_subdiv;
	bool              use_select_preview;
	bool              use_subsurf_to_osd;

	bool              show_vfb;
	bool              use_bake_view;
	bool              is_viewport;
	bool              is_preview;

	bool              calculate_instancer_velocity;

	int               mb_samples;
	float             mb_duration;
	float             mb_offset;

	VRayVerboseLevel  verbose_level;
	ImageType         viewport_image_type;
	int               viewport_image_quality;
	int               zmq_server_port;
	std::string       zmq_server_address;

	std::string       override_material_name;
	BL::Material      override_material;
	BL::Object        current_bake_object;
	BL::Object        camera_stereo_left;
	BL::Object        camera_stereo_right;

	BL::Scene         background_scene;

	RenderMode        render_mode;

	float                      getViewportResolutionPercentage() const { return m_viewportResolution; }
	int                        getViewportShowAlpha();

private:
	PointerRNA                 m_vrayScene;
	PointerRNA                 m_vrayExporter;


	float                      m_viewportResolution;

};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_EXPORT_SETTINGS_H
