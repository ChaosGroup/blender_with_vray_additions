#include "vfb_plugin_manager.h"
#include "utils/cgr_hash.h"

using namespace VRayForBlender;

namespace {
	template <typename T>
	MHash getListHash(const AttrList<T> & val) {
		MHash id;
		MurmurHash3_x86_32(val.getData()->data(), val.getBytesCount(), 42, &id);
		return id;
	}

	bool compare(const AttrValue & left, const AttrValue & right) {
		if (left.type != right.type) {
			assert(false);
		}

		switch (left.type) {
		case ValueTypeInt:
			return left.valInt == right.valInt;
		case ValueTypeFloat:
			return left.valFloat == right.valFloat;
		case ValueTypeString:
			return left.valString == right.valString;
		case ValueTypeColor:
			return 0 == memcmp(&left.valColor, &right.valColor, sizeof(left.valColor));
		case ValueTypeAColor:
			return 0 == memcmp(&left.valAColor, &right.valAColor, sizeof(left.valAColor));
		case ValueTypeVector:
			return 0 == memcmp(&left.valVector, &right.valVector, sizeof(left.valVector));
		case ValueTypePlugin:
			return left.valPlugin.plugin == right.valPlugin.plugin && left.valPlugin.output == right.valPlugin.output;
		case ValueTypeTransform:
			return 0 == memcmp(&left.valTransform, &right.valTransform, sizeof(left.valTransform));
		case ValueTypeListInt:
			return getListHash(left.valListInt) == getListHash(right.valListInt);
		case ValueTypeListFloat:
			return getListHash(left.valListFloat) == getListHash(right.valListFloat);
		case ValueTypeListVector:
			return getListHash(left.valListVector) == getListHash(right.valListVector);
		case ValueTypeListColor:
			return getListHash(left.valListColor) == getListHash(right.valListColor);
		case ValueTypeInstancer:
			if (left.valInstancer.data.getCount() != right.valInstancer.data.getCount() ||
				left.valInstancer.frameNumber != right.valInstancer.frameNumber) {
				return false;
			}
			for (int c = 0; c < left.valInstancer.data.getCount(); c++) {
				const auto &rd = (*right.valInstancer.data)[c],
					       &ld = (*left.valInstancer.data)[c];

				if (memcmp(&ld.tm, &rd.tm, sizeof(ld.tm)) != 0 ||
					memcmp(&ld.vel, &rd.vel, sizeof(ld.vel)) != 0 ||
					ld.index != rd.index ||
					ld.node != rd.node)
				{
					return false;
				}
			}
			return true;
		case ValueTypeListPlugin:
			if (left.valListPlugin.getCount() != right.valListPlugin.getCount()) {
				return false;
			}
			for (int c = 0; c < left.valListPlugin.getCount(); ++c) {
				const auto & lPlugin = left.valListPlugin.getData()->at(c),
					&rPlugin = right.valListPlugin.getData()->at(c);
				if (lPlugin.plugin != rPlugin.plugin || lPlugin.output != rPlugin.output) {
					return false;
				}
			}
			return true;
		case ValueTypeListString:
			if (left.valListString.getCount() != right.valListString.getCount()) {
				return false;
			}
			for (int c = 0; c < left.valListString.getCount(); ++c) {
				if (left.valListString.getData()->at(c) != right.valListString.getData()->at(c)) {
					return false;
				}
			}
			return true;
		case ValueTypeMapChannels:
			if (left.valMapChannels.data.size() != right.valMapChannels.data.size()) {
				return false;
			}
			for (const auto & lIter : left.valMapChannels.data) {
				const auto & rIter = right.valMapChannels.data.find(lIter.first);
				if (rIter == right.valMapChannels.data.end()) {
					return false;
				}

				if (rIter->second.name != lIter.second.name ||
					getListHash(rIter->second.faces) != getListHash(lIter.second.faces) ||
					getListHash(rIter->second.vertices) != getListHash(lIter.second.vertices)) {
					return false;
				}
			}
			return true;
		}
		return false;
	}
}


PluginManager::PluginManager() {}

PluginDesc PluginManager::filterPlugin(const PluginDesc & pDesc) {
	auto & pluginDesc = const_cast<PluginDesc&>(pDesc);

	const auto & key = pluginDesc.pluginName + pluginDesc.pluginID;

	auto cacheEntry = cache.find(key);

	if (cacheEntry == cache.end()) {
		cache.insert(make_pair(key, pluginDesc.pluginAttrs));
		return pluginDesc;
	}

	PluginDesc filteredDesc(pluginDesc.pluginName, pluginDesc.pluginID);

	// all input keys will be stored here, any cache entry with key not here must be removed
	std::set<std::string> validCacheKeys;

	for (const auto & inputItem : pluginDesc.pluginAttrs) {
		// add input key to validate cache keys
		validCacheKeys.insert(inputItem.first);

		auto cacheItem = cacheEntry->second.find(inputItem.first);

		// the input item is not present in cache - add it to both cache and output
		if (cacheItem == cacheEntry->second.end()) {
			cacheEntry->second.insert(inputItem);
			pluginDesc.add(inputItem.second.attrName, inputItem.second.attrValue);
			continue;
		}

		// item is both in cache and input
		if (!compare(inputItem.second.attrValue, cacheItem->second.attrValue)) {
			cacheItem->second = inputItem.second;
			filteredDesc.add(inputItem.second.attrName, inputItem.second.attrValue);
		}
	}

	// remove cache entries which are not in the input
	if (cacheEntry->second.size() != validCacheKeys.size()) {
		for (auto iter = cacheEntry->second.cbegin(), end = cacheEntry->second.cend(); iter != end; ++iter) {
			if (validCacheKeys.find(iter->first) == validCacheKeys.end()) {
				cacheEntry->second.erase(iter);
			}
		}
	}

	return filteredDesc;
}

void PluginManager::clear() {
	cache.clear();
}
