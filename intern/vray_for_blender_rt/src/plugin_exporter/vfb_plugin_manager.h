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

#include "vfb_plugin_attrs.h"
#include "base_types.h"

#include "utils/cgr_hash.h"

#include <mutex>

namespace VRayForBlender {
/// Class that keeps track of what data is exported last, it keeps hashes for all plugin's properties
/// All plugins that are exported first go trough this class to check if any/all properties need to be exported
/// Check PluginExporter::export_plugin for details
class PluginManager {
	using HashAttr = VRayBaseTypes::AttrSimpleType<int>;

public:
	PluginManager(bool storeData)
	    : m_storeData(storeData)
	{}

	PluginManager(const PluginManager &) = delete;
	PluginManager &operator=(const PluginManager &) = delete;

	//// Check if a plugin with a given name is in the cache
	bool inCache(const std::string &name) const;
	/// Check if a plugin with a plugin description is in the cache
	bool inCache(const PluginDesc &pluginDesc) const;
	/// Check if the plugin desc passed to the method differs from the cached data
	bool differs(const PluginDesc &pluginDesc) const;
	/// Check if the plugin desc passed to the method has differen plugin ID that the cached one
	bool differsId(const PluginDesc &pluginDesc) const;

	/// Check if the PluginManager is storing data or only hash
	bool storeData() const {
		return m_storeData;
	}

	/// Get new PluginDesc containing only the properties that are different in the cache or are missing
	PluginDesc differences(const PluginDesc &pluginDesc) const;


	/// Wrapper over reference to plugin description and a frame, use this to avoid 2 cache lookups for frame and desc
	struct FramePluginDesc {
		std::reference_wrapper<PluginDesc> desc;
		float frame;
	};

	/// Get PluginDesc for a given name, it must exist in the cache
	FramePluginDesc fromCache(const std::string &name) {
		auto iter = m_cache.find(name);
		const bool foundItem = iter != m_cache.end();
		VFB_Assert(foundItem && "PluginManager::fromCache() called with NON cache plugin name!");
		VFB_Assert(m_storeData && "PluginManager::fromCache called when m_storeData == false");
		if (foundItem) {
			return { iter->second.m_desc, iter->second.m_frame };
		} else {
			auto & item = m_cache[name];
			return { item.m_desc, item.m_frame };
		}
	}

	/// Update the cache with the given PluginDesc
	void updateCache(const PluginDesc &desc, float frame);
	/// Remove data from the cache for a plugin
	void remove(const PluginDesc &pluginDesc);
	/// Remove data from the cache for a plugin
	void remove(const std::string &pluginName);

	/// Get plugin desc with attrs subset of source, that are different in filter
	/// @param source - the input plugin desc
	/// @param filter - plugin desc to compare values with
	/// @return - new plugin desc, with attributes from source that have different value in filter
	static PluginDesc diffWithPlugin(const PluginDesc &source, const PluginDesc &filter);

	/// Clear everything from the cache
	void clear();
private:
	/// Hash data kept for a single PluginDesc
	struct PluginDescHash {
		MHash m_allHash; ///< hash of all the properties
		PluginDesc m_desc; ///< Either the full plugin desc or values are hashes of the real data
		HashMap<std::string, MHash> m_attrHashes; ///< Hashes for the attributes in m_desc
		float m_frame; ///< The frame which this plugin was cached

		PluginDescHash()
			: m_desc("", "")
		{}
	};

	/// Calculate the hash of a given PluginDesc
	PluginDescHash makeHash(const PluginDesc &pluginDesc) const;

	/// Check the difference of a PluginDesc with the cached data
	/// @pluginDesc - the plugin description we want to filter/check
	/// @uildDiff - if true the second member of the returned pair will contain only the different parameters
	///             else it will be empty PluginDesc with only name and ID set
	std::pair<bool, PluginDesc> diffWithCache(const PluginDesc &pluginDesc, bool buildDiff) const;

	HashMap<std::string, PluginDescHash> m_cache; ///< map a plugin name to it's hash
	mutable std::mutex m_cacheLock; ///< lock protecting @m_cache
	const bool m_storeData; ///< True if we are storing real data in PluginDescHash::m_desc or just hashes
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
