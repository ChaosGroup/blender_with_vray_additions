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

#ifndef VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
#define VRAY_FOR_BLENDER_PLUGIN_MANAGER_H

#include <vfb_plugin_attrs.h>
#include <boost/unordered_map.hpp>

namespace VRayForBlender {

class PluginManager {
public:
	PluginManager();

	bool inCache(const PluginDesc &pluginDesc) const;
	bool differs(const PluginDesc &pluginDesc) const;
	PluginDesc differences(const PluginDesc &pluginDesc) const;

	PluginDesc fromCache(const PluginDesc &search) const;
	void updateCache(const PluginDesc &update);
	void remove(const PluginDesc &pluginDesc);
	void remove(const std::string &pluginName);

	void clear();

private:

	std::pair<bool, PluginDesc> diffWithCache(const PluginDesc &pluginDesc, bool buildDiff) const;
	std::string getKey(const PluginDesc &pluginDesc) const;

	// id + name -> PluginDesc
	boost::unordered_map<std::string, PluginDesc> cache;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
