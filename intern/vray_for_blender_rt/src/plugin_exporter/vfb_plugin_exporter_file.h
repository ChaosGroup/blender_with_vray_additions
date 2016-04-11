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

#ifndef VRAY_FOR_BLENDER_PLUGIN_EXPORTER_FILE_H
#define VRAY_FOR_BLENDER_PLUGIN_EXPORTER_FILE_H

#include "vfb_plugin_exporter.h"
#include "vfb_plugin_writer.h"
#include "vfb_params_json.h"
#include "vfb_export_settings.h"

#include <unordered_map>

namespace VRayForBlender {


class VrsceneExporter:
        public PluginExporter
{
public:
	VrsceneExporter();
	virtual            ~VrsceneExporter();

public:
	virtual void        init();
	virtual void        free();
	virtual void        sync();
	virtual void        start();
	virtual void        stop();

	virtual AttrPlugin  export_plugin_impl(const PluginDesc &pluginDesc);
	virtual void        set_export_file(VRayForBlender::ParamDesc::PluginType type, PyObject *file);
	virtual void        set_settings(const ExporterSettings &st);
private:

private:
	std::unordered_map<ParamDesc::PluginType, std::shared_ptr<PluginWriter>, std::hash<int>> m_Writers;

	ExporterSettings::ExportFormat m_ExportFormat;

	bool                          m_ReexportMeshes;
	bool                          m_Synced;
	bool                          m_SplitFiles;
	SettingsFiles::OutputDirType  m_FileDirType;
	std::string                   m_FileDir;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_FILE_H
