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

#ifndef VRAY_FOR_BLENDER_JSON_PLUGINS_H
#define VRAY_FOR_BLENDER_JSON_PLUGINS_H

#include "vfb_params_desc.h"

namespace VRayForBlender {

typedef std::vector<const ParamDesc::PluginParamDesc *> PluginParamDescList;

/// Load all plugin descriptions from json files from the given path
/// @param dirPath - path to the root of json descriptions
void InitPluginDescriptions(const std::string &dirPath);

/// Get plugin description by it's plugin id
/// @param pluginID - the string ID of the plugin
/// @return - const ref to the plugin description for that plugin
const ParamDesc::PluginParamDesc& GetPluginDescription(const std::string &pluginID);

/// Get list of all loaded plugins of certain type
/// @param type - the plugin type we want
/// @return - ref to const vector of const pointers to plugin descriptions
const PluginParamDescList & GetPluginsOfType(ParamDesc::PluginType type);

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_JSON_PLUGINS_H
