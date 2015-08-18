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

using namespace VRayForBlender;


VrsceneExporter::VrsceneExporter()
{
	this->setUpSplitWriters();
}

void VrsceneExporter::setUpSingleWriter()
{
	auto writer = std::shared_ptr<PluginWriter>(new PluginWriter("D:/vs.vrscene"));
	for (int c = ParamDesc::PluginUnknown; c <= ParamDesc::PluginUvwgen; c++) {
		m_Writers[static_cast<ParamDesc::PluginType>(c)] = writer;
	}
}

void VrsceneExporter::setUpSplitWriters()
{
	auto writerScene = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_scene.vrscene"));
	auto writerNodes = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_nodes.vrscene"));
	auto writerGeometry = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_geometry.vrscene"));
	auto writerCamera = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_camera.vrscene"));
	auto writerLights = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_lights.vrscene"));
	auto writerTextures = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_textures.vrscene"));
	auto writerMaterials = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_materials.vrscene"));
	auto writerEnvironment = std::shared_ptr<PluginWriter>(new PluginWriter("D:/scene_environment.vrscene"));

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


void VrsceneExporter::init()
{

}


void VrsceneExporter::free()
{

}


void VrsceneExporter::sync()
{

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

	const ParamDesc::PluginDesc & pluginParamDesc = GetPluginDescription(pluginDesc.pluginID);
	auto writerPtr = m_Writers[pluginParamDesc.pluginType];
	if (!writerPtr) {
		PRINT_ERROR("No PluginWriter for type %d exproting %s with id [%s]",
			pluginParamDesc.pluginType, name.c_str(), pluginDesc.pluginID.c_str());
		return plugin;
	}

	PluginWriter & writer = *writerPtr;
	m_Writers[ParamDesc::PluginSettings]->include(writer.getName());

	if (pDesc.pluginAttrs.size() != pluginDesc.pluginAttrs.size()) {
		// something is filtered out - dont export
		return plugin;
	}

	writer << pluginDesc.pluginID << " " << StripString(pluginDesc.pluginName) << "{\n";

	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;

		PRINT_INFO_EX("Updating: \"%s\" => %s.%s",
			name.c_str(), pluginDesc.pluginID.c_str(), attr.attrName.c_str());


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
