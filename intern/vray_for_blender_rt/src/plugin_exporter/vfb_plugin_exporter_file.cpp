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


VrsceneExporter::VrsceneExporter()
    : m_ExportFormat(ExporterSettings::ExportFormatHEX)
    , m_SeparateFiles(false)
    , m_Synced(false)
    , m_isDR(false)
{

}

void VrsceneExporter::set_export_file(VRayForBlender::ParamDesc::PluginType type, PyObject *file)
{
	if (file) {
		std::shared_ptr<PluginWriter> writer;
		auto iter = m_fileWritersMap.find(reinterpret_cast<intptr_t>(file));

		if (iter == m_fileWritersMap.end()) {
			// ensure only one PluginWriter is instantiated for a file
			writer.reset(new PluginWriter(m_threadManager, file, m_ExportFormat));
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

void VrsceneExporter::set_settings(const ExporterSettings &st)
{
	m_ExportFormat = st.export_file_format;
	for (auto & w : m_fileWritersMap) {
		w.second->setFormat(m_ExportFormat);
	}
	animation_settings = st.settings_animation;
	m_SeparateFiles = st.settings_files.use_separate;
	m_isDR = st.settings_dr.use;
}


VrsceneExporter::~VrsceneExporter()
{
}


void VrsceneExporter::init()
{
	PRINT_INFO_EX("Initting VrsceneExporter");
	m_threadManager = ThreadManager::make(2);
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
	bool setFrame = !(!m_isDR && m_SeparateFiles && writer == *m_Writers[ParamDesc::PluginSettings]);

	writer << pluginDesc.pluginID << " " << StripString(pluginDesc.pluginName) << "{\n";
	if (animation_settings.use) {
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
		case ValueTypeMatrix:
			writer << KVPair<AttrMatrix>(attr.attrName, attr.attrValue.valMatrix);
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
			if (animation_settings.use && attr.attrValue.valInstancer.frameNumber != current_scene_frame) {
				PRINT_WARN("Exporting instancer in frame %d, while it has %d frame", static_cast<int>(current_scene_frame), attr.attrValue.valInstancer.frameNumber);
				const_cast<PluginAttr&>(attr).attrValue.valInstancer.frameNumber = current_scene_frame;
			}
			writer << KVPair<AttrInstancer>(attr.attrName, attr.attrValue.valInstancer);
			break;
		default:
			BLI_assert(!"Unsupported attribute type");
			break;
		}
	}

	writer << "}\n\n";

	return plugin;
}
