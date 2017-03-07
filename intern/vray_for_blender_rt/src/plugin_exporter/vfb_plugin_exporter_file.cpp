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

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace VRayForBlender;


VrsceneExporter::VrsceneExporter(const ExporterSettings & settings)
	: PluginExporter(settings)
    , m_Synced(false)
{

}

void VrsceneExporter::set_export_file(VRayForBlender::ParamDesc::PluginType type, PyObject *file)
{
	if (file) {
		std::shared_ptr<PluginWriter> writer;
		auto iter = m_fileWritersMap.find(reinterpret_cast<intptr_t>(file));

		if (iter == m_fileWritersMap.end()) {
			// ensure only one PluginWriter is instantiated for a file
			writer.reset(new PluginWriter(m_threadManager, file, exporter_settings.export_file_format));
			if (!writer) {
				BLI_assert("Failed to create PluginWriter for python file!");
				return;
			}
			m_fileWritersMap[reinterpret_cast<intptr_t>(file)] = writer;
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
	m_Writers.clear();
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
	if (writerType == ParamDesc::PluginGeometry && pluginDesc.pluginID != "GeomStaticMesh") {
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
	bool setFrame = !(
	    !exporter_settings.settings_dr.use              &&
	    exporter_settings.settings_files.use_separate   &&
	    writer == *m_Writers[ParamDesc::PluginSettings]
	);

	writer << pluginDesc.pluginID << " " << StripString(pluginDesc.pluginName) << "{\n";
	if (exporter_settings.settings_animation.use || exporter_settings.use_motion_blur) {
		if (setFrame) {
			writer.setAnimationFrame(this->current_scene_frame);
		} else {
			writer.setAnimationFrame(-1);
		}
	}

	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;

		switch (attr.attrValue.type) {
		case ValueTypeUnknown:
			break;
		case ValueTypeInt:
			writer << KVPair<int>(attr.attrName, attr.attrValue.as<AttrSimpleType<int>>());
			break;
		case ValueTypeFloat:
			writer << KVPair<float>(attr.attrName, attr.attrValue.as<AttrSimpleType<float>>());
			break;
		case ValueTypeString:
			writer << KVPair<std::string>(attr.attrName, attr.attrValue.as<AttrSimpleType<std::string>>());
			break;
		case ValueTypeColor:
			writer << KVPair<AttrColor>(attr.attrName, attr.attrValue.as<AttrColor>());
			break;
		case ValueTypeVector:
			writer << KVPair<AttrVector>(attr.attrName, attr.attrValue.as<AttrVector>());
			break;
		case ValueTypeAColor:
			writer << KVPair<AttrAColor>(attr.attrName, attr.attrValue.as<AttrAColor>());
			break;
		case ValueTypePlugin:
			writer << KVPair<AttrPlugin>(attr.attrName, attr.attrValue.as<AttrPlugin>());
			break;
		case ValueTypeTransform:
			writer << KVPair<AttrTransform>(attr.attrName, attr.attrValue.as<AttrTransform>());
			break;
		case ValueTypeMatrix:
			writer << KVPair<AttrMatrix>(attr.attrName, attr.attrValue.as<AttrMatrix>());
			break;
		case ValueTypeListInt:
			writer << KVPair<AttrListInt>(attr.attrName, attr.attrValue.as<AttrListInt>());
			break;
		case ValueTypeListFloat:
			writer << KVPair<AttrListFloat>(attr.attrName, attr.attrValue.as<AttrListFloat>());
			break;
		case ValueTypeListVector:
			writer << KVPair<AttrListVector>(attr.attrName, attr.attrValue.as<AttrListVector>());
			break;
		case ValueTypeListColor:
			writer << KVPair<AttrListColor>(attr.attrName, attr.attrValue.as<AttrListColor>());
			break;
		case ValueTypeListPlugin:
			writer << KVPair<AttrListPlugin>(attr.attrName, attr.attrValue.as<AttrListPlugin>());
			break;
		case ValueTypeListString:
			writer << KVPair<AttrListString>(attr.attrName, attr.attrValue.as<AttrListString>());
			break;
		case ValueTypeMapChannels:
			writer << KVPair<AttrMapChannels>(attr.attrName, attr.attrValue.as<AttrMapChannels>());
			break;
		case ValueTypeInstancer:
			BLI_assert(attr.attrValue.as<AttrInstancer>().frameNumber == current_scene_frame && "Instancer's frame mismatching scene frame");
			writer << KVPair<AttrInstancer>(attr.attrName, attr.attrValue.as<AttrInstancer>());
			break;
		default:
			BLI_assert(!"Unsupported attribute type");
			break;
		}
	}

	writer << "}\n\n";

	return plugin;
}
