
#include "vfb_plugin_manager.h"
#include "vfb_plugin_exporter.h"
#include "utils/cgr_hash.h"

using namespace VRayForBlender;
using namespace std;

namespace {

template <typename T>
MHash getListHash(const AttrList<T> & val) {
	MHash id;
	MurmurHash3_x86_32(val.getData()->data(), val.getBytesCount(), 42, &id);
	return id;
}

bool compare(const AttrValue & left, const AttrValue & right) {
	if (left.type == right.type) {
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
			default:
				break;
		}
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
	lock_guard<mutex> l(cacheLock);
	return cache.find(name) != cache.end();
}

bool PluginManager::inCache(const PluginDesc &pluginDesc) const
{
	lock_guard<mutex> l(cacheLock);
	return cache.find(getKey(pluginDesc)) != cache.end();
}

void PluginManager::remove(const std::string &pluginName)
{
	lock_guard<mutex> l(cacheLock);
	cache.erase(pluginName);
}

void PluginManager::remove(const PluginDesc &pluginDesc)
{
	lock_guard<mutex> l(cacheLock);
	cache.erase(getKey(pluginDesc));
}

std::pair<bool, PluginDesc> PluginManager::diffWithCache(const PluginDesc &pluginDesc, bool buildDiff) const 
{
	lock_guard<mutex> l(cacheLock);
	const auto key = getKey(pluginDesc);
	auto cacheEntry = cache.find(key);

	PluginDesc res(pluginDesc.pluginName, pluginDesc.pluginID);

	if (cacheEntry == cache.end()) {
		return std::make_pair(true, res);
	}

	const auto &cacheAttributes = cacheEntry->second.pluginAttrs;

	if (cacheAttributes.size() != pluginDesc.pluginAttrs.size() && !buildDiff) {
		return std::make_pair(true, res);
	}

	for (const auto &attribute : pluginDesc.pluginAttrs) {
		auto cAttr = cacheAttributes.find(attribute.first);

		// attribute present in the cached PluginDesc?
		if (cAttr == cacheAttributes.end()) {
			if (buildDiff) {
				res.add(attribute.second.attrName, attribute.second.attrValue);
				continue;
			} else {
				return std::make_pair(true, res);
			}
		}

		// attribute in cache but different value?
		if (!compare(attribute.second.attrValue, cAttr->second.attrValue)) {
			if (buildDiff) {
				res.add(attribute.second.attrName, attribute.second.attrValue);
			} else {
				return std::make_pair(true, res);
			}
		}
	}

	return std::make_pair(false, res);;
}

bool PluginManager::differs(const PluginDesc &pluginDesc) const
{
	return diffWithCache(pluginDesc, false).first;
}

PluginDesc PluginManager::differences(const PluginDesc &pluginDesc) const
{
	return diffWithCache(pluginDesc, true).second;
}

const PluginDesc & PluginManager::operator[](const PluginDesc &search) const
{
	lock_guard<mutex> l(cacheLock);
	return cache.find(getKey(search))->second;
}

PluginDesc PluginManager::fromCache(const PluginDesc &search) const
{
	lock_guard<mutex> l(cacheLock);
	auto iter = cache.find(getKey(search));
	if (iter != cache.cend()) {
		return iter->second;
	}
	return PluginDesc(search.pluginName, search.pluginID);
}

void PluginManager::updateCache(const PluginDesc &update)
{
	lock_guard<mutex> l(cacheLock);
	const auto key = getKey(update);
	auto iter = cache.find(key);

	if (iter == cache.end()) {
		cache.insert(make_pair(key, update));
	} else {
		iter->second.pluginAttrs = update.pluginAttrs;
		iter->second.pluginID = update.pluginID;
	}
}

void PluginManager::clear()
{
	lock_guard<mutex> l(cacheLock);
	cache.clear();
}
