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

#ifndef VRAY_FOR_BLENDER_PLUGIN_DESC_H
#define VRAY_FOR_BLENDER_PLUGIN_DESC_H

#include <string>
#include <map>


namespace VRayForBlender {
namespace ParamDesc {

enum AttrType {
	AttrTypeIgnore = 0,
	AttrTypeBool,
	AttrTypeInt,
	AttrTypeEnum,
	AttrTypeFloat,
	AttrTypeVector,
	AttrTypeColor,
	AttrTypeAColor,
	AttrTypeMatrix,
	AttrTypeTransform,

	AttrTypeString = 50,
	AttrTypeFilepath,
	AttrTypeDirpath,

	AttrTypePlugin = 100,
	AttrTypePluginBRDF,
	AttrTypePluginGeometry,
	AttrTypePluginMaterial,
	AttrTypePluginMatrix,
	AttrTypePluginTexture,
	AttrTypePluginTextureFloat,
	AttrTypePluginTextureInt,
	AttrTypePluginTextureMatrix,
	AttrTypePluginTextureMransform,
	AttrTypePluginTextureTransform,
	AttrTypePluginTextureVector,
	AttrTypePluginTransform,
	AttrTypePluginUvwgen,
	AttrTypePluginVector,
	AttrTypePluginEnd,

	AttrTypeOutputStart,
	AttrTypeOutputPlugin,
	AttrTypeOutputColor,
	AttrTypeOutputFloat,
	AttrTypeOutputTexture,
	AttrTypeOutputTextureFloat,
	AttrTypeOutputTextureInt,
	AttrTypeOutputTextureVector,
	AttrTypeOutputTextureMatrix,
	AttrTypeOutputTextureTransform,
	AttrTypeOutputEnd,

	AttrTypeListInt = 200,
	AttrTypeListFloat,
	AttrTypeListVector,
	AttrTypeListColor,
	AttrTypeListTransform,
	AttrTypeListString,
	AttrTypeListPlugin,
};


inline bool TypeHasSocket(const AttrType &attrType)
{
	return (attrType >= AttrTypePlugin) && (attrType < AttrTypePluginEnd);
}

enum PluginType {
	PluginUnknown = 0,
	PluginBRDF,
	PluginCamera,
	PluginChannel,
	PluginEffect,
	PluginFilter,
	PluginGeometry,
	PluginLight,
	PluginMaterial,
	PluginObject,
	PluginSettings,
	PluginTexture,
	PluginUvwgen,
};

struct AttrDesc {
	std::string  name;
	AttrType     type;
};

typedef std::map<std::string, AttrDesc> MapAttrDesc;


struct PluginDesc {
	PluginDesc():
		pluginType(PluginUnknown)
	{}

	PluginType   pluginType;
	std::string  pluginID;
	MapAttrDesc  attributes;

private:
	PluginDesc(const PluginDesc&) {}

};


PluginType GetPluginTypeFromString(const std::string &typeString);

} // namespace ParamDesc
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_DESC_H
