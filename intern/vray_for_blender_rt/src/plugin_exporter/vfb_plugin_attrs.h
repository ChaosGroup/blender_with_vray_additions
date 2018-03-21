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

#include <cstdio>

namespace VRayForBlender {

// Invalid value for frame number, if set to a property, interpolate will not be used
const float INVALID_FRAME = -FLT_MAX;

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

inline void BlTransformFromAttrTransform(float bl[4][4], const VRayBaseTypes::AttrTransform & tm) {
	memset(bl, 0, sizeof(float[4][4]));
	bl[0][0] = tm.m.v0.x;
	bl[0][1] = tm.m.v0.y;
	bl[0][2] = tm.m.v0.z;

	bl[1][0] = tm.m.v1.x;
	bl[1][1] = tm.m.v1.y;
	bl[1][2] = tm.m.v1.z;

	bl[2][0] = tm.m.v2.x;
	bl[2][1] = tm.m.v2.y;
	bl[2][2] = tm.m.v2.z;

	bl[3][0] = tm.offs.x;
	bl[3][1] = tm.offs.y;
	bl[3][2] = tm.offs.z;
	bl[3][3] = 1;
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

inline AttrMatrix operator-(const AttrMatrix & left, const AttrMatrix & right) {
	AttrMatrix mtx;
	mtx.v0 = left.v0 - right.v0;
	mtx.v1 = left.v1 - right.v1;
	mtx.v2 = left.v2 - right.v2;
	return mtx;
}

inline AttrVector operator/(const AttrVector & v, const float value) {
	return AttrVector(v.x / value, v.y / value, v.z / value);
}

inline AttrMatrix operator/(const AttrMatrix & v, const float value) {
	AttrMatrix mtx;
	mtx.v0 = v.v0 / value;
	mtx.v1 = v.v1 / value;
	mtx.v2 = v.v2 / value;
	return mtx;
}

inline AttrTransform operator/(const AttrTransform & left, const float value) {
	AttrTransform tm;
	tm.m = left.m / value;
	tm.offs = left.offs / value;
	return tm;
}

inline AttrTransform operator-(const AttrTransform & left, const AttrTransform & right) {
	AttrTransform tm;
	tm.m = left.m - right.m;
	tm.offs = left.offs - right.offs;
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
typedef HashMap<std::string, PluginAttr> PluginAttrs;

struct PluginDesc {
	typedef HashMap<std::string, PluginDesc> PluginAttrsCache;
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
