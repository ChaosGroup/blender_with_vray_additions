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

#include "vfb_params_desc.h"


using namespace VRayForBlender;


VRayForBlender::ParamDesc::PluginType VRayForBlender::ParamDesc::GetPluginTypeFromString(const std::string &typeString)
{
	ParamDesc::PluginType pluginType = ParamDesc::PluginUnknown;

	if (typeString == "BRDF") {
		pluginType = ParamDesc::PluginBRDF;
	}
	else if (typeString == "MATERIAL") {
		pluginType = ParamDesc::PluginMaterial;
	}
	else if (typeString == "UVWGEN") {
		pluginType = ParamDesc::PluginUvwgen;
	}
	else if (typeString == "TEXTURE") {
		pluginType = ParamDesc::PluginTexture;
	}
	else if (typeString == "GEOMETRY") {
		pluginType = ParamDesc::PluginGeometry;
	}
	else if (typeString == "RENDERCHANNEL") {
		pluginType = ParamDesc::PluginChannel;
	}
	else if (typeString == "EFFECT") {
		pluginType = ParamDesc::PluginEffect;
	}
	else if (typeString == "SETTINGS") {
		pluginType = ParamDesc::PluginSettings;
	}
	else if (typeString == "CAMERA") {
		pluginType = ParamDesc::PluginCamera;
	}

	return pluginType;
}
