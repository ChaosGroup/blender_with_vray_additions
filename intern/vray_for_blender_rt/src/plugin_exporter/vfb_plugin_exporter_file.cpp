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
#include "BLI_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
namespace fs = boost::filesystem;

using namespace VRayForBlender;


VrsceneExporter::VrsceneExporter(const ExporterSettings & settings)
	: PluginExporter(settings)
    , m_Synced(false)
{
	namespace pt = boost::posix_time;
	m_headerString.reserve(1024);

	m_headerString += "// V-Ray For Blender\n";
	m_headerString += "// " + pt::to_simple_string(pt::second_clock::local_time()) + "\n";
	m_headerString += std::string("// Build hash [") + G.main->build_hash  + "] \n\n";
}

std::string getStdString(PyObject *file)
{
	Py_ssize_t len = -1;
	const char * data = PyUnicode_AsUTF8AndSize(file, &len);
	if (data) {
		return std::string(data, len);
	}
	PyErr_Clear();
	return std::string();
}

FILE * VrsceneExporter::getFile(VRayForBlender::ParamDesc::PluginType type, const char *filePath)
{
	const char * mode = "w";
	if (!exporter_settings.export_meshes && type == ParamDesc::PluginType::PluginGeometry) {
		mode = "r";
	}

	auto iter = m_fileMap.find(filePath);
	if (iter == m_fileMap.end()) {
		if (type != VRayForBlender::ParamDesc::PluginChannel &&
			type != VRayForBlender::ParamDesc::PluginFilter &&
			type != VRayForBlender::ParamDesc::PluginSettings &&
			exporter_settings.settings_files.use_separate) {

			m_includesString += "\n#include \"" + fs::basename(filePath) + ".vrscene\"";
		}


		FILE *file = fopen(filePath, mode);
		fwrite(m_headerString.c_str(), 1, m_headerString.size(), file);
		m_fileMap.insert(std::make_pair(filePath, file));
		return file;
	}
	return iter->second;
}

void VrsceneExporter::set_export_file(VRayForBlender::ParamDesc::PluginType type, PyObject *file)
{
	if (file) {
		const std::string &fileName = getStdString(file);
		std::shared_ptr<PluginWriter> writer;
		auto iter = m_fileWritersMap.find(fileName);

		if (iter == m_fileWritersMap.end()) {
			// ensure only one PluginWriter is instantiated for a file
			writer.reset(new PluginWriter(m_threadManager, getFile(type, fileName.c_str()), exporter_settings.export_file_format));
			if (!writer) {
				BLI_assert("Failed to create PluginWriter for python file!");
				return;
			}
			m_fileWritersMap[fileName] = writer;
		} else {
			writer = iter->second;
		}

		switch (type) {
		case VRayForBlender::ParamDesc::PluginChannel:
		case VRayForBlender::ParamDesc::PluginFilter:
		case VRayForBlender::ParamDesc::PluginSettings:
			m_Writers[ParamDesc::PluginFilter] = writer;
			m_Writers[ParamDesc::PluginChannel] = writer;
			m_Writers[ParamDesc::PluginSettings] = writer;
			break;
		case VRayForBlender::ParamDesc::PluginBRDF:
		case VRayForBlender::ParamDesc::PluginMaterial:
			m_Writers[ParamDesc::PluginBRDF] = writer;
			m_Writers[ParamDesc::PluginMaterial] = writer;
			break;
		case VRayForBlender::ParamDesc::PluginTexture:
		case VRayForBlender::ParamDesc::PluginUvwgen:
			m_Writers[ParamDesc::PluginTexture] = writer;
			m_Writers[ParamDesc::PluginUvwgen] = writer;
			break;
		default:
			m_Writers[type] = writer;
		}
	} else {
		PRINT_ERROR("Setting nullptr file for %d", static_cast<int>(type));
	}
}

VrsceneExporter::~VrsceneExporter()
{
}


void VrsceneExporter::init()
{
	PRINT_INFO_EX("Initting VrsceneExporter");
	m_threadManager = ThreadManager::make(2);
	for (auto & w : m_fileWritersMap) {
		w.second->setFormat(exporter_settings.export_file_format);
	}
}


void VrsceneExporter::free()
{
	writeIncludes();
	m_Writers.clear();
	m_fileWritersMap.clear();
}


void VrsceneExporter::sync()
{
	PRINT_INFO_EX("Flushing all data to files");
	m_Synced = true;
	for (auto & writer : m_fileWritersMap) {
		writer.second->blockFlushAll();
	}

	m_threadManager->stop();
}

void VrsceneExporter::writeIncludes()
{
	if (!exporter_settings.settings_files.use_separate) {
		return;
	}
	const auto writerPtr = m_Writers[ParamDesc::PluginSettings];
	if (!writerPtr) {
		PRINT_ERROR("Missing file for PluginSettings");
		return;
	}
	*writerPtr << m_includesString;
}


void VrsceneExporter::start()
{

}


void VrsceneExporter::stop()
{

}


AttrPlugin VrsceneExporter::export_plugin_impl(const PluginDesc &pluginDesc)
{
	const std::string & name = pluginDesc.pluginName;

	AttrPlugin plugin;
	plugin.plugin = name;
	m_Synced = false;

	const ParamDesc::PluginDesc & pluginParamDesc = GetPluginDescription(pluginDesc.pluginID);

	auto writerType = pluginParamDesc.pluginType;

	// redirect all geom plugins which are not static to nodes file
	if (writerType == ParamDesc::PluginGeometry &&
		pluginDesc.pluginID != "GeomStaticMesh" &&
		pluginDesc.pluginID != "GeomMayaHair")
	{
		writerType = ParamDesc::PluginObject;
	}

	auto writerPtr = m_Writers[writerType];
	if (!writerPtr) {
		if (pluginDesc.pluginID == "Node" || pluginDesc.pluginID == "Instancer") {
			writerPtr = m_Writers[ParamDesc::PluginObject];
		} else if (pluginDesc.pluginID.find("Render") != std::string::npos) {
			writerPtr = m_Writers[ParamDesc::PluginSettings];
		} else if (pluginDesc.pluginID.find("Light") != std::string::npos) {
			writerPtr = m_Writers[ParamDesc::PluginLight];
		}

		if (!writerPtr) {
			writerPtr = m_Writers[ParamDesc::PluginSettings];
			if (!writerPtr) {
				PRINT_ERROR("Failed to get plugin writer for type %d exporting %s with id [%s]",
					writerType, name.c_str(), pluginDesc.pluginID.c_str());
				return plugin;
			}
		}
	}

	PluginWriter & writer = *writerPtr;
	// dont set frame for settings file when DR is off and seperate files is on and current file is Settings
	const bool setFrame = writer != *m_Writers[ParamDesc::PluginSettings];

	writer << pluginDesc.pluginID << " " << StripString(pluginDesc.pluginName) << " {\n";
	if (exporter_settings.settings_animation.use || exporter_settings.use_motion_blur) {
		if (setFrame) {
			writer.setAnimationFrame(this->current_scene_frame);
		} else {
			writer.setAnimationFrame(INVALID_FRAME);
		}
	}

	const float writerFrame = writer.getAnimationFrame();
	const ParamDesc::PluginDesc &desc = GetPluginDescription(pluginDesc.pluginID);

	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;
		if (attr.attrValue.type == ValueTypeUnknown) {
			continue;
		}

		bool forceNoFrame = false;
		const auto attrIter = desc.attributes.find(attr.attrName);
		if (attrIter != desc.attributes.end()) {
			// filepaths are not animated
			if (attrIter->second.type == ParamDesc::AttrTypeDirpath || attrIter->second.type == ParamDesc::AttrTypeFilepath) {
				forceNoFrame = true;
			}
			// generic lists different from Instancer2::instances are not animated
			if (attrIter->second.type == ParamDesc::AttrTypeList && pluginDesc.pluginID != "Instancer2") {
				forceNoFrame = true;
			}
		}

		bool restoreFrame = false;
		if (forceNoFrame || (attr.time == INVALID_FRAME && writerFrame != INVALID_FRAME)) {
			writer.setAnimationFrame(INVALID_FRAME);
			restoreFrame = true;
		}

		writer << KVPair<AttrValue>(attr.attrName, attr.attrValue);

		if (restoreFrame) {
			writer.setAnimationFrame(writerFrame);
		}
	}

	writer << "}\n\n";

	return plugin;
}
