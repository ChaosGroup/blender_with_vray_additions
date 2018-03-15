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
			return value.as<AttrSimpleType<int>>().value;
		case ValueTypeFloat:
			return *reinterpret_cast<const MHash*>(&value.as<AttrSimpleType<float>>().value);
		case ValueTypeString:
			return getValueHash(value.as<AttrSimpleType<std::string>>().value);
		case ValueTypeColor:
			return getValueHash(value.as<AttrColor>());
		case ValueTypeAColor:
			return getValueHash(value.as<AttrAColor>());
		case ValueTypeVector:
			return getValueHash(value.as<AttrVector>());
		case ValueTypePlugin:
			valHash = getValueHash(value.as<AttrPlugin>().plugin);
			if (!value.as<AttrPlugin>().output.empty()) {
				valHash = getValueHash(value.as<AttrPlugin>().output, valHash);
			}
			return valHash;
		case ValueTypeTransform:
			return getValueHash(value.as<AttrTransform>());
		case ValueTypeMatrix:
			return getValueHash(value.as<AttrMatrix>());
		case ValueTypeListInt:
			return getValueHash(value.as<AttrListInt>());
		case ValueTypeListFloat:
			return getValueHash(value.as<AttrListFloat>());
		case ValueTypeListVector:
			return getValueHash(value.as<AttrListVector>());
		case ValueTypeListColor:
			return getValueHash(value.as<AttrListColor>());
		case ValueTypeInstancer:
			valHash = getValueHash(value.as<AttrInstancer>().frameNumber);
			for (int c = 0; c < value.as<AttrInstancer>().data.getCount(); c++) {
				const auto &rd = (*value.as<AttrInstancer>().data)[c];

				valHash = getValueHash(rd.tm, valHash);
				valHash = getValueHash(rd.vel, valHash);
				valHash = getValueHash(rd.index, valHash);
				valHash = getValueHash(rd.node, valHash);
			}
			return valHash;
		case ValueTypeListPlugin:
			for (int c = 0; c < value.as<AttrListPlugin>().getCount(); ++c) {
				const auto & rPlugin = value.as<AttrListPlugin>().getData()->at(c);
				valHash = getValueHash(rPlugin.plugin, valHash);
				if (!rPlugin.output.empty()) {
					valHash = getValueHash(rPlugin.output, valHash);
				}
			}
			return valHash;
		case ValueTypeListString:
			for (int c = 0; c < value.as<AttrListString>().getCount(); ++c) {
				valHash = getValueHash(value.as<AttrListString>().getData()->at(c), valHash);
			}
			return valHash;
		case ValueTypeMapChannels:
			for (const auto & iter : value.as<AttrMapChannels>().data) {
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
	lock_guard<mutex> l(m_cacheLock);
	return m_cache.find(name) != m_cache.end();
}

bool PluginManager::inCache(const PluginDesc &pluginDesc) const
{
	lock_guard<mutex> l(m_cacheLock);
	return m_cache.find(getKey(pluginDesc)) != m_cache.end();
}

void PluginManager::remove(const std::string &pluginName)
{
	lock_guard<mutex> l(m_cacheLock);
	m_cache.erase(pluginName);
}

void PluginManager::remove(const PluginDesc &pluginDesc)
{
	lock_guard<mutex> l(m_cacheLock);
	m_cache.erase(getKey(pluginDesc));
}

std::pair<bool, PluginDesc> PluginManager::diffWithCache(const PluginDesc &pluginDesc, bool buildDiff) const 
{
	lock_guard<mutex> l(m_cacheLock);
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

	if (descHash.m_allHash != cacheEntry->second.m_allHash) {
		if (!buildDiff) {
			return std::make_pair(true, res);
		}
	} else {
		return std::make_pair(false, res);
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

void PluginManager::updateCache(const PluginDesc &update)
{
	lock_guard<mutex> l(m_cacheLock);
	const auto key = getKey(update);
	auto hash = makeHash(update);

	m_cache[key] = hash;
}

void PluginManager::clear()
{
	lock_guard<mutex> l(m_cacheLock);
	m_cache.clear();
}
