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

#include "vfb_typedefs.h"

namespace VRayForBlender {
namespace ParamDesc {

enum AttrType {
	AttrTypeInvalid = -1,
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

	AttrTypeString,
	AttrTypeFilepath,
	AttrTypeDirpath,

	AttrTypePlugin,
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

	AttrTypeWidgetStart,
	AttrTypeWidgetRamp,
	AttrTypeWidgetCurve,
	AttrTypeWidgetEnd,

	AttrTypeList,
	AttrTypeListInt,
	AttrTypeListFloat,
	AttrTypeListVector,
	AttrTypeListColor,
	AttrTypeListTransform,
	AttrTypeListString,
	AttrTypeListPlugin,
	AttrTypeListEnd,
};


inline bool TypeHasSocket(const AttrType &attrType)
{
	return ((attrType >= AttrTypePlugin) && (attrType < AttrTypePluginEnd)) ||
		attrType == AttrTypeMatrix ||
		attrType == AttrTypeTransform ||
		attrType == AttrTypeVector;
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

enum AttrOptions {
	AttrOptionNone          = 0,
	AttrOptionExportAsColor = 1 << 0,
};

/// Descriptor of a plugin's attribute
struct AttrDesc {
	struct Options {
		AttrOptions optionData;
		Options & operator|=(AttrOptions o) {
			optionData = static_cast<AttrOptions>(optionData | o);
			return *this;
		}

		AttrOptions operator&(AttrOptions o) const {
			return static_cast<AttrOptions>(optionData & o);
		}

		Options & operator=(AttrOptions o) {
			optionData = o;
			return *this;
		}
	};

	struct ParmRampDesc {
		std::string  colors;
		std::string  positions;
		std::string  interpolations;
	} descRamp;

	struct ParmCurveDesc {
		std::string  positions;
		std::string  values;
		std::string  interpolations;
	} descCurve;

	std::string  name; ///< The name of the attribute
	AttrType     type; ///< The type of the attribute
	Options      options; ///< Options for exporter
};

typedef HashMap<std::string, AttrDesc> MapAttrDesc;

/// Descriptor of a vray plugin, and all of it's attributes
struct PluginParamDesc {
	PluginParamDesc():
		pluginType(PluginUnknown)
	{}

	PluginType   pluginType; ///< The type of the plugin
	std::string  pluginID; ///< The plugin ID (not to be confused with instance ID)
	MapAttrDesc  attributes; ///< The plugin attributes (parameters)
};

/// Convert string upper case plugin type to PluginType
/// @param typeString - the type as string
/// @return - the plugin type or PluginUnknown
PluginType GetPluginTypeFromString(const std::string &typeString);

} // namespace ParamDesc
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_DESC_H
