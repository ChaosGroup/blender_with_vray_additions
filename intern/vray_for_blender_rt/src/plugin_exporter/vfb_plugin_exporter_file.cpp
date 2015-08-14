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

using namespace VRayForBlender;


VrsceneExporter::VrsceneExporter(): m_Writer("D:/vs.vrscene")
{
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

	if (pDesc.pluginAttrs.size() != pluginDesc.pluginAttrs.size()) {
		// something is filtered out - dont export
		return plugin;
	}

	m_Writer << pluginDesc.pluginID << " " << StripString(pluginDesc.pluginName) << "{\n";

	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;

		PRINT_INFO_EX("Updating: \"%s\" => %s.%s",
			name.c_str(), pluginDesc.pluginID.c_str(), attr.attrName.c_str());


		switch (attr.attrValue.type) {
		case ValueTypeUnknown:
			break;
		case ValueTypeInt:
			m_Writer << KVPair<int>(attr.attrName, attr.attrValue.valInt);
			break;
		case ValueTypeFloat:
			m_Writer << KVPair<float>(attr.attrName, attr.attrValue.valFloat);
			break;
		case ValueTypeString:
			m_Writer << KVPair<std::string>(attr.attrName, attr.attrValue.valString);
			break;
		case ValueTypeColor:
			m_Writer << KVPair<AttrColor>(attr.attrName, attr.attrValue.valColor);
			break;
		case ValueTypeVector:
			m_Writer << KVPair<AttrVector>(attr.attrName, attr.attrValue.valVector);
			break;
		case ValueTypeAColor:
			m_Writer << KVPair<AttrAColor>(attr.attrName, attr.attrValue.valAColor);
			break;
		case ValueTypePlugin:
			m_Writer << KVPair<AttrPlugin>(attr.attrName, attr.attrValue.valPlugin);
			break;
		case ValueTypeTransform:
			m_Writer << KVPair<AttrTransform>(attr.attrName, attr.attrValue.valTransform);
			break;
		case ValueTypeListInt:
			m_Writer << KVPair<AttrListInt>(attr.attrName, attr.attrValue.valListInt);
			break;
		case ValueTypeListFloat:
			m_Writer << KVPair<AttrListFloat>(attr.attrName, attr.attrValue.valListFloat);
			break;
		case ValueTypeListVector:
			m_Writer << KVPair<AttrListVector>(attr.attrName, attr.attrValue.valListVector);
			break;
		case ValueTypeListColor:
			m_Writer << KVPair<AttrListColor>(attr.attrName, attr.attrValue.valListColor);
			break;
		case ValueTypeListPlugin:
			m_Writer << KVPair<AttrListPlugin>(attr.attrName, attr.attrValue.valListPlugin);
			break;
		case ValueTypeListString:
			m_Writer << KVPair<AttrListString>(attr.attrName, attr.attrValue.valListString);
			break;
		case ValueTypeMapChannels:
			m_Writer << KVPair<AttrMapChannels>(attr.attrName, attr.attrValue.valMapChannels);
			break;
		case ValueTypeInstancer:
			m_Writer << KVPair<AttrInstancer>(attr.attrName, attr.attrValue.valInstancer);
			break;
		default:
			PRINT_INFO_EX("--- > UNIMPLEMENTED DEFAULT");
			assert(false);
			break;
		}
	}

	m_Writer << "}\n\n";

	return plugin;
}
