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

#include <mutex>
#include "utils/cgr_hash.h"

namespace VRayForBlender {
/// Class that keeps track of what data is exported last, it keeps hashes for all plugins properties
/// All plugins that are exported first go trough this class to check if any/all properties need to be exported
class PluginManager {
public:
	PluginManager();

	//// Check if a plugin with a given name is in the cache
	bool inCache(const std::string &name) const;
	/// Check if a plugin with a plugin description is in the cache
	bool inCache(const PluginDesc &pluginDesc) const;
	/// Check if the plugin desc passed to the method differs from the cached data
	bool differs(const PluginDesc &pluginDesc) const;
	/// Check if the plugin desc passed to the method has differen plugin ID that the cached one
	bool differsId(const PluginDesc &pluginDesc) const;

	/// Get new PluginDesc containing only the properties that are different in the cache or are missing
	PluginDesc differences(const PluginDesc &pluginDesc) const;

	/// Update the cache with the given PluginDesc
	void updateCache(const PluginDesc &update);
	/// Remove data from the cache for a plugin
	void remove(const PluginDesc &pluginDesc);
	/// Remove data from the cache for a plugin
	void remove(const std::string &pluginName);

	/// Clear everything from the cache
	void clear();

	// returns the key in the cache for this pluginDesc (it's name)
	std::string getKey(const PluginDesc &pluginDesc) const;
private:
	/// Hash data kept for a single PluginDesc
	struct PluginDescHash {
		std::string                              m_name; ///< the name of the plugin
		std::string                              m_id; ///< the ID of the plugin
		MHash                                    m_allHash; ///< hash of all the properties
		HashMap<std::string, MHash>              m_values; ///< map of property name to property value hash
	};

	/// Calculate the hash of a given PluginDesc
	PluginDescHash makeHash(const PluginDesc &pluginDesc) const;

	/// Check the difference of a PluginDesc with the cached data
	/// @pluginDesc - the plugin description we want to filter/check
	/// @uildDiff - if true the second member of the returned pair will contain only the different parameters
	///             else it will be empty PluginDesc with only name and ID set
	std::pair<bool, PluginDesc> diffWithCache(const PluginDesc &pluginDesc, bool buildDiff) const;

	// name -> PluginDesc
	HashMap<std::string, PluginDescHash> m_cache; ///< map a plugin name to it's hash
	mutable std::mutex                   m_cacheLock; ///< lock protecting @m_cache
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
