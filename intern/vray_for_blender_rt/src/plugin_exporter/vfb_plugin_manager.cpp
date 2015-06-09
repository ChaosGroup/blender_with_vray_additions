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
			return getListHash(left.valInstancer.data) == getListHash(right.valInstancer.data);
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

	const std::string & key = pluginDesc.pluginName + pluginDesc.pluginID;

	auto & item = cache.find(key);

	if (item == cache.end()) {
		cache.insert(make_pair(key, pluginDesc.pluginAttrs));
		return pluginDesc;
	}

	PluginDesc filteredDesc(pluginDesc.pluginName, pluginDesc.pluginID);

	for (auto & cacheIt : item->second) {
		auto inputIt = pluginDesc.pluginAttrs.find(cacheIt.first);

		if (inputIt == pluginDesc.pluginAttrs.end()) {
			continue;
		}

		bool same = compare(cacheIt.second.attrValue, inputIt->second.attrValue);

		if (!same) {
			cacheIt.second = inputIt->second;
			filteredDesc.add(cacheIt.second.attrName, cacheIt.second.attrValue);
		}
	}

	return filteredDesc;
}

void PluginManager::clear() {
	cache.clear();
}
