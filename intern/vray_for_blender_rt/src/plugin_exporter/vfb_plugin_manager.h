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

#include "utils/cgr_hash.h"

namespace VRayForBlender {

class PluginManager {
public:
	PluginManager();

	bool inCache(const std::string &name) const;
	bool inCache(const PluginDesc &pluginDesc) const;
	bool differs(const PluginDesc &pluginDesc) const;
	bool differsId(const PluginDesc &pluginDesc) const;

	PluginDesc differences(const PluginDesc &pluginDesc) const;

	// PluginDesc fromCache(const PluginDesc &search) const;
	// plugin must exist in cache - otherwise UB
	// const PluginDesc & operator[](const PluginDesc &search) const;
	void updateCache(const PluginDesc &update);
	void remove(const PluginDesc &pluginDesc);
	void remove(const std::string &pluginName);

	void clear();

	// returns the key in the cache for this pluginDesc (it's name)
	std::string getKey(const PluginDesc &pluginDesc) const;
private:
	struct PluginDescHash {
		std::string                              m_name;
		std::string                              m_id;
		MHash                                    m_allHash;
		boost::unordered_map<std::string, MHash> m_values;
	};

	PluginDescHash makeHash(const PluginDesc &pluginDesc) const;

	std::pair<bool, PluginDesc> diffWithCache(const PluginDesc &pluginDesc, bool buildDiff) const;

	// name -> PluginDesc
	boost::unordered_map<std::string, PluginDescHash> m_cache;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
