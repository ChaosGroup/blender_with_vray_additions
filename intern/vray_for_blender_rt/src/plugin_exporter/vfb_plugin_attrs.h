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

#ifndef VRAY_FOR_BLENDER_PLUGIN_ATTRS_H
#define VRAY_FOR_BLENDER_PLUGIN_ATTRS_H

#include "base_types.h"

#include "cgr_config.h"
#include "vfb_util_defines.h"
#include "vfb_typedefs.h"
#include "vfb_rna.h"

#include "utils/cgr_hash.h"

#include "BLI_math.h"

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>


namespace VRayForBlender {

// import everything, as these are types used extensively in this namespace
using namespace VRayBaseTypes;

const int VectorBytesCount  = 3 * sizeof(float);
const int Vector2BytesCount = 2 * sizeof(float);

// convert functions from BL types to vray base types
inline VRayBaseTypes::AttrColor AttrColorFromBlColor(const BlColor &c) {
	VRayBaseTypes::AttrColor color;
	memcpy(&color, c.data, VectorBytesCount);
	return color;
}

inline VRayBaseTypes::AttrAColor AttrAColorFromBlColor(const BlAColor &c) {
	VRayBaseTypes::AttrAColor color;
	memcpy(&color, c.data, VectorBytesCount + sizeof(float));
	return color;
}

inline VRayBaseTypes::AttrVector AttrVectorFromBlVector(const BlVector &bl_v) {
	VRayBaseTypes::AttrVector vector;
	memcpy(&vector, bl_v.data, VectorBytesCount);
	return vector;
}

inline VRayBaseTypes::AttrVector AttrVectorFromBlVector(const BlVector2 &bl_v) {
	VRayBaseTypes::AttrVector vector;
	memcpy(&vector, bl_v.data, Vector2BytesCount);
	vector.z = 0.0f;
	return vector;
}

inline VRayBaseTypes::AttrVector2 AttrVector2FromBlVector(const BlVector2 &bl_v) {
	VRayBaseTypes::AttrVector2 vector;
	memcpy(&vector, bl_v.data, Vector2BytesCount);
	return vector;
}

inline VRayBaseTypes::AttrTransform AttrTransformFromBlTransform(const float data[4][4]) {
	VRayBaseTypes::AttrTransform tm;
	tm.m.v0.x = data[0][0];
	tm.m.v0.y = data[0][1];
	tm.m.v0.z = data[0][2];

	tm.m.v1.x = data[1][0];
	tm.m.v1.y = data[1][1];
	tm.m.v1.z = data[1][2];

	tm.m.v2.x = data[2][0];
	tm.m.v2.y = data[2][1];
	tm.m.v2.z = data[2][2];

	tm.offs.x = data[3][0];
	tm.offs.y = data[3][1];
	tm.offs.z = data[3][2];
	return tm;
}

inline VRayBaseTypes::AttrTransform AttrTransformFromBlTransform(const BlTransform &bl_tm) {
	VRayBaseTypes::AttrTransform tm;
	memcpy(&tm.m.v0, &bl_tm.data[0], VectorBytesCount);
	memcpy(&tm.m.v1, &bl_tm.data[4], VectorBytesCount);
	memcpy(&tm.m.v2, &bl_tm.data[8], VectorBytesCount);
	memcpy(&tm.offs, &bl_tm.data[12], VectorBytesCount);
	return tm;
}


struct PluginAttr {
	PluginAttr(): time(0) {}
	PluginAttr(const std::string &attrName, const AttrValue &attrValue, double time=0.0):
	    attrName(attrName),
	    attrValue(attrValue),
	    time(time)
	{}
	std::string  attrName;
	AttrValue    attrValue;
	double       time;
};
typedef boost::unordered_map<std::string, PluginAttr> PluginAttrs;

struct PluginDesc {
	typedef boost::unordered_map<std::string, PluginDesc> PluginAttrsCache;
	static PluginAttrsCache cache;

	std::string  pluginName;
	std::string  pluginID;
	PluginAttrs  pluginAttrs;

	//PluginDesc() {}
	PluginDesc(const std::string &plugin_name, const std::string &plugin_id, const std::string &prefix = ""):
		pluginName(plugin_name),
		pluginID(plugin_id) {
		if (!prefix.empty()) {
			pluginName.insert(0, prefix);
		}
	}

	bool contains(const std::string &paramName) const {
		if (get(paramName)) {
			return true;
		}
		return false;
	}

	const PluginAttr *get(const std::string &paramName) const {
		if (pluginAttrs.count(paramName)) {
			const auto pIt = pluginAttrs.find(paramName);
			return &pIt->second;
		}
		return nullptr;
	}

	PluginAttr *get(const std::string &paramName) {
		if (pluginAttrs.count(paramName)) {
			return &pluginAttrs[paramName];
		}
		return nullptr;
	}

	void add(const PluginAttr &attr) {
		pluginAttrs[attr.attrName] = attr;
	}

	void add(const std::string &attrName, const AttrValue &attrValue, const float &time=0.0f) {
		add(PluginAttr(attrName, attrValue, time));
	}

	void del(const std::string &attrName) {
		auto delIt = pluginAttrs.find(attrName);
		pluginAttrs.erase(delIt);
	}

	void showAttributes() const {
		PRINT_INFO_EX("Plugin \"%s.%s\" parameters:",
			pluginID.c_str(), pluginName.c_str());

		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt.second;
			PRINT_INFO_EX("  %s at %.3f [%s]",
			              p.attrName.c_str(), p.time, p.attrValue.getTypeAsString());
		}
	}
};


} // namespace VRayForBlender
#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
