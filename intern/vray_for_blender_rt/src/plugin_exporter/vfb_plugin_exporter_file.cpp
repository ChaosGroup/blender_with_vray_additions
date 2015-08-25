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

#include "vfb_plugin_exporter_file.h"
#include "vfb_plugin_writer.h"
#include "vfb_params_json.h"
#include "vfb_export_settings.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace VRayForBlender;


VrsceneExporter::VrsceneExporter():
	m_Synced(false)
{

}

void VrsceneExporter::setUpSingleWriter()
{
	auto writer = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + ".vrscene", m_ExportFormat));
	for (int c = ParamDesc::PluginUnknown; c <= ParamDesc::PluginUvwgen; c++) {
		m_Writers[static_cast<ParamDesc::PluginType>(c)] = writer;
	}
}

void VrsceneExporter::setUpSplitWriters()
{
	auto writerScene = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_scene.vrscene", m_ExportFormat));
	auto writerNodes = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_nodes.vrscene", m_ExportFormat));
	auto writerGeometry = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_geometry.vrscene", m_ExportFormat));
	auto writerCamera = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_camera.vrscene", m_ExportFormat));
	auto writerLights = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_lights.vrscene", m_ExportFormat));
	auto writerTextures = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_textures.vrscene", m_ExportFormat));
	auto writerMaterials = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_materials.vrscene", m_ExportFormat));
	auto writerEnvironment = std::shared_ptr<PluginWriter>(new PluginWriter(m_FileDir + "_environment.vrscene", m_ExportFormat));

	m_Writers[ParamDesc::PluginFilter] = writerScene;
	m_Writers[ParamDesc::PluginChannel] = writerScene;
	m_Writers[ParamDesc::PluginSettings] = writerScene;
	m_Writers[ParamDesc::PluginObject] = writerNodes;
	m_Writers[ParamDesc::PluginGeometry] = writerGeometry;
	m_Writers[ParamDesc::PluginCamera] = writerCamera;
	m_Writers[ParamDesc::PluginLight] = writerLights;
	m_Writers[ParamDesc::PluginTexture] = writerTextures;
	m_Writers[ParamDesc::PluginUvwgen] = writerTextures;
	m_Writers[ParamDesc::PluginBRDF] = writerMaterials;
	m_Writers[ParamDesc::PluginMaterial] = writerMaterials;
	m_Writers[ParamDesc::PluginEffect] = writerEnvironment;
}


VrsceneExporter::~VrsceneExporter()
{
}

void VrsceneExporter::set_settings(const ExporterSettings &settings)
{
	m_ExportFormat = settings.export_file_format;

	m_SplitFiles = settings.settings_files.use_separate;
	m_FileDirType = settings.settings_files.output_type;

	m_ReexportMeshes = settings.export_meshes;
	fs::path dir;

	switch (m_FileDirType) {
	case SettingsFiles::OutputDirTypeUser:
		dir = fs::path(settings.settings_files.output_dir);
		break;
	case SettingsFiles::OutputDirTypeScene:
		dir = fs::path(settings.settings_files.project_path);
		break;
	case SettingsFiles::OutputDirTypeTmp:
		/* fall-through */
	default:
		// TODO vrayblender_<USER_NAME>
		dir = fs::temp_directory_path() /  fs::path("vrayblender_");
		break;
	}

	m_FileDir = dir.string();

	char absPath[PATH_MAX];
	strncpy(absPath, m_FileDir.c_str(), PATH_MAX);
	BLI_path_abs(absPath, settings.settings_files.project_path.c_str());
	m_FileDir = absPath;

	if (!fs::exists(m_FileDir)) {
		fs::create_directories(m_FileDir);
	}

	// append the file prefix to the dir
	fs::path fileName = "scene";
	if (settings.settings_files.output_unique && !settings.settings_files.project_path.empty()) {
		fileName = fs::path(settings.settings_files.project_path).filename();
	}

	m_FileDir = (m_FileDir / fileName).string();
}

void VrsceneExporter::init()
{
	if (m_SplitFiles) {
		this->setUpSplitWriters();
	} else {
		this->setUpSingleWriter();
	}
}


void VrsceneExporter::free()
{
	m_Writers.clear();
}


void VrsceneExporter::sync()
{
	m_Synced = true;
	for (auto & writer : m_Writers) {
		if (writer.second) {
			writer.second->flush();
		}
	}
}


void VrsceneExporter::start()
{

}


void VrsceneExporter::stop()
{

}


AttrPlugin VrsceneExporter::export_plugin(const PluginDesc &pDesc)
{
	const auto pluginDesc = m_PluginManager.filterPlugin(pDesc);
	const std::string & name = pluginDesc.pluginName;

	AttrPlugin plugin;
	plugin.plugin = name;

	if (pDesc.pluginAttrs.size() != pluginDesc.pluginAttrs.size()) {
		// something is filtered out - dont export
		return plugin;
	}
	m_Synced = false;

	const ParamDesc::PluginDesc & pluginParamDesc = GetPluginDescription(pluginDesc.pluginID);
	auto writerPtr = m_Writers[pluginParamDesc.pluginType];
	if (!writerPtr) {
		if (pluginDesc.pluginID == "Node" || pluginDesc.pluginID == "Instancer") {
			writerPtr = m_Writers[ParamDesc::PluginObject];
		} else if (pluginDesc.pluginID.find("Render") != std::string::npos) {
			writerPtr = m_Writers[ParamDesc::PluginSettings];
		} else if (pluginDesc.pluginID.find("Light") != std::string::npos) {
			writerPtr = m_Writers[ParamDesc::PluginLight];
		} else {
			PRINT_ERROR("No PluginWriter for type %d exproting %s with id [%s]",
				pluginParamDesc.pluginType, name.c_str(), pluginDesc.pluginID.c_str());
			return plugin;
		}
	}

	PluginWriter & writer = *writerPtr;
	if (m_Writers[ParamDesc::PluginSettings]) {
		m_Writers[ParamDesc::PluginSettings]->include(writer.getName());
	}

	writer << pluginDesc.pluginID << " " << StripString(pluginDesc.pluginName) << "{\n";

	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;

		switch (attr.attrValue.type) {
		case ValueTypeUnknown:
			break;
		case ValueTypeInt:
			writer << KVPair<int>(attr.attrName, attr.attrValue.valInt);
			break;
		case ValueTypeFloat:
			writer << KVPair<float>(attr.attrName, attr.attrValue.valFloat);
			break;
		case ValueTypeString:
			writer << KVPair<std::string>(attr.attrName, attr.attrValue.valString);
			break;
		case ValueTypeColor:
			writer << KVPair<AttrColor>(attr.attrName, attr.attrValue.valColor);
			break;
		case ValueTypeVector:
			writer << KVPair<AttrVector>(attr.attrName, attr.attrValue.valVector);
			break;
		case ValueTypeAColor:
			writer << KVPair<AttrAColor>(attr.attrName, attr.attrValue.valAColor);
			break;
		case ValueTypePlugin:
			writer << KVPair<AttrPlugin>(attr.attrName, attr.attrValue.valPlugin);
			break;
		case ValueTypeTransform:
			writer << KVPair<AttrTransform>(attr.attrName, attr.attrValue.valTransform);
			break;
		case ValueTypeListInt:
			writer << KVPair<AttrListInt>(attr.attrName, attr.attrValue.valListInt);
			break;
		case ValueTypeListFloat:
			writer << KVPair<AttrListFloat>(attr.attrName, attr.attrValue.valListFloat);
			break;
		case ValueTypeListVector:
			writer << KVPair<AttrListVector>(attr.attrName, attr.attrValue.valListVector);
			break;
		case ValueTypeListColor:
			writer << KVPair<AttrListColor>(attr.attrName, attr.attrValue.valListColor);
			break;
		case ValueTypeListPlugin:
			writer << KVPair<AttrListPlugin>(attr.attrName, attr.attrValue.valListPlugin);
			break;
		case ValueTypeListString:
			writer << KVPair<AttrListString>(attr.attrName, attr.attrValue.valListString);
			break;
		case ValueTypeMapChannels:
			writer << KVPair<AttrMapChannels>(attr.attrName, attr.attrValue.valMapChannels);
			break;
		case ValueTypeInstancer:
			writer << KVPair<AttrInstancer>(attr.attrName, attr.attrValue.valInstancer);
			break;
		default:
			PRINT_INFO_EX("--- > UNIMPLEMENTED DEFAULT");
			assert(false);
			break;
		}
	}

	writer << "}\n\n";

	return plugin;
}
