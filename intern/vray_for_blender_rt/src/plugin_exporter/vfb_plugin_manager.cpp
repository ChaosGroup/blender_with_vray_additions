
#include "vfb_plugin_manager.h"
#include "vfb_plugin_exporter.h"
#include "utils/cgr_hash.h"

using namespace VRayForBlender;
using namespace std;

namespace {

template <typename T>
MHash getValueHash(const AttrList<T> & val, const MHash seed = 42) {
	MHash id;
	MurmurHash3_x86_32(val.getData()->data(), val.getBytesCount(), seed, &id);
	return id;
}

template <typename T>
MHash getValueHash(const T & val, const MHash seed = 42) {
	MHash id;
	MurmurHash3_x86_32(&val, sizeof(val), seed, &id);
	return id;
}

template <>
MHash getValueHash(const std::string & val, const MHash seed) {
	MHash id;
	MurmurHash3_x86_32(val.c_str(), val.size(), seed, &id);
	return id;
}

MHash getAttrHash(const AttrValue & value, const MHash seed = 42) {
	MHash valHash = seed;
	switch (value.type) {
		case ValueTypeInt:
			return getValueHash(value.valInt);
		case ValueTypeFloat:
			return getValueHash(value.valFloat);
		case ValueTypeString:
			return getValueHash(value.valString);
		case ValueTypeColor:
			return getValueHash(value.valColor);
		case ValueTypeAColor:
			return getValueHash(value.valAColor);
		case ValueTypeVector:
			return getValueHash(value.valVector);
		case ValueTypePlugin:
			valHash = getValueHash(value.valPlugin.plugin);
			if (!value.valPlugin.output.empty()) {
				valHash = getValueHash(value.valPlugin.output, valHash);
			}
			return valHash;
		case ValueTypeTransform:
			return getValueHash(value.valTransform);
		case ValueTypeListInt:
			return getValueHash(value.valListInt);
		case ValueTypeListFloat:
			return getValueHash(value.valListFloat);
		case ValueTypeListVector:
			return getValueHash(value.valListVector);
		case ValueTypeListColor:
			return getValueHash(value.valListColor);
		case ValueTypeInstancer:
			valHash = getValueHash(value.valInstancer.frameNumber);
			for (int c = 0; c < value.valInstancer.data.getCount(); c++) {
				const auto &rd = (*value.valInstancer.data)[c];

				valHash = getValueHash(rd.tm, valHash);
				valHash = getValueHash(rd.vel, valHash);
				valHash = getValueHash(rd.index, valHash);
				valHash = getValueHash(rd.node, valHash);
			}
			return valHash;
		case ValueTypeListPlugin:
			for (int c = 0; c < value.valListPlugin.getCount(); ++c) {
				const auto & rPlugin = value.valListPlugin.getData()->at(c);
				valHash = getValueHash(rPlugin.plugin, valHash);
				if (!rPlugin.output.empty()) {
					valHash = getValueHash(rPlugin.output, valHash);
				}
			}
			return valHash;
		case ValueTypeListString:
			for (int c = 0; c < value.valListString.getCount(); ++c) {
				valHash = getValueHash(value.valListString.getData()->at(c), valHash);
			}
			return valHash;
		case ValueTypeMapChannels:
			for (const auto & iter : value.valMapChannels.data) {
				auto & map = iter.second;
				valHash = getValueHash(map.name, valHash);
				valHash = getValueHash(map.faces, valHash);
				valHash = getValueHash(map.vertices, valHash);
			}
			return valHash;
		default:
			break;
	}
	return false;
}
}


PluginManager::PluginManager()
{}

std::string PluginManager::getKey(const PluginDesc &pluginDesc) const
{
	return pluginDesc.pluginName;
}

bool PluginManager::inCache(const std::string &name) const
{
	return m_cache.find(name) != m_cache.end();
}

bool PluginManager::inCache(const PluginDesc &pluginDesc) const
{
	return m_cache.find(getKey(pluginDesc)) != m_cache.end();
}

void PluginManager::remove(const std::string &pluginName)
{
	m_cache.erase(pluginName);
}

void PluginManager::remove(const PluginDesc &pluginDesc)
{
	m_cache.erase(getKey(pluginDesc));
}

std::pair<bool, PluginDesc> PluginManager::diffWithCache(const PluginDesc &pluginDesc, bool buildDiff) const 
{
	const auto key = getKey(pluginDesc);
	auto cacheEntry = m_cache.find(key);

	PluginDesc res(pluginDesc.pluginName, pluginDesc.pluginID);

	if (cacheEntry == m_cache.end()) {
		return std::make_pair(true, res);
	}

	if (cacheEntry->second.m_values.size() != pluginDesc.pluginAttrs.size() && !buildDiff) {
		return std::make_pair(true, res);
	}

	const auto descHash = makeHash(pluginDesc);

	if (descHash.m_allHash == cacheEntry->second.m_allHash) {
		return make_pair(true, res);
	} else if (!buildDiff) {
		return std::make_pair(true, res);
	}

	BLI_assert(descHash.m_name == pluginDesc.pluginName && "PluginManager::diffWithCache called with desc to different plugin!");

	const auto & cacheAttrMap = cacheEntry->second.m_values;
	const auto & descAttrMap = descHash.m_values;

	for (const auto &attrHash : descAttrMap) {
		auto cacheHash = cacheAttrMap.find(attrHash.first);

		// attribute is not in cache at all
		if (cacheHash == cacheAttrMap.end()) {
			if (buildDiff) {
				// we are sure it is here, since descAttrMap was build from pluginDesc
				const auto & attr = pluginDesc.pluginAttrs.find(attrHash.first)->second;
				res.add(attr.attrName, attr.attrValue);
				continue;
			} else {
				return std::make_pair(true, res);
			}
		}

		if (attrHash.second != cacheHash->second) {
			if (buildDiff) {
				// we are sure it is here, since descAttrMap was build from pluginDesc
				const auto & attr = pluginDesc.pluginAttrs.find(attrHash.first)->second;
				res.add(attr.attrName, attr.attrValue);
			} else {
				return std::make_pair(true, res);
			}
		}
	}

	return std::make_pair(false, res);
}

bool PluginManager::differsId(const PluginDesc &pluginDesc) const
{
	auto iter = m_cache.find(getKey(pluginDesc));
	if (iter == m_cache.end()) {
		return false;
	}

	return iter->second.m_id != pluginDesc.pluginID;
}

bool PluginManager::differs(const PluginDesc &pluginDesc) const
{
	return diffWithCache(pluginDesc, false).first;
}

PluginDesc PluginManager::differences(const PluginDesc &pluginDesc) const
{
	return diffWithCache(pluginDesc, true).second;
}

PluginManager::PluginDescHash PluginManager::makeHash(const PluginDesc &pluginDesc) const
{
	PluginDescHash hash;
	hash.m_id = pluginDesc.pluginID;
	hash.m_name = pluginDesc.pluginName;
	hash.m_allHash = 42;

	for (const auto & attr : pluginDesc.pluginAttrs) {
		const auto aHash = getAttrHash(attr.second.attrValue);
		hash.m_allHash = getValueHash(aHash, hash.m_allHash);
		hash.m_values[attr.second.attrName] = aHash;
	}

	return hash;
}

//const PluginDesc & PluginManager::operator[](const PluginDesc &search) const {
//	return m_cache.find(getKey(search))->second;
//}

//PluginDesc PluginManager::fromCache(const PluginDesc &search) const
//{
//	auto iter = cache.find(getKey(search));
//	if (iter != cache.cend()) {
//		return iter->second;
//	}
//	return PluginDesc(search.pluginName, search.pluginID);
//}

void PluginManager::updateCache(const PluginDesc &update)
{
	const auto key = getKey(update);
	auto hash = makeHash(update);

	m_cache.insert(make_pair(key, hash));
}

void PluginManager::clear() {
	m_cache.clear();
}
