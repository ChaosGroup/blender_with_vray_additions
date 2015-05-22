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

inline VRayBaseTypes::AttrColor AttrColorFromBlColor(const BlAColor &c) {
	VRayBaseTypes::AttrColor color;
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

inline VRayBaseTypes::AttrTransform AttrTransformFromBlTransform(const BlTransform &bl_tm) {
	VRayBaseTypes::AttrTransform tm;
	memcpy(&tm.m.v0, &bl_tm.data[0], VectorBytesCount);
	memcpy(&tm.m.v1, &bl_tm.data[4], VectorBytesCount);
	memcpy(&tm.m.v2, &bl_tm.data[8], VectorBytesCount);
	memcpy(&tm.offs, &bl_tm.data[12], VectorBytesCount);
	return tm;
}

struct AttrValue {
	typedef AttrList<AttrValue> AttrListValue;

	AttrValue():
		type(ValueTypeUnknown) {}

	AttrValue(const AttrValue &other) {
		*this = other;
	}

	AttrValue(const std::string &attrValue) {
		type = ValueTypeString;
		valString = attrValue;
	}

	AttrValue(const char *attrValue) {
		type = ValueTypeString;
		valString = attrValue;
	}

	AttrValue(const AttrPlugin attrValue) {
		type = ValueTypePlugin;
		valPlugin = attrValue;
	}

	AttrValue(const AttrPlugin attrValue, const std::string &output) {
		type = ValueTypePlugin;
		valPlugin = attrValue;
		valPlugin.output = output;
	}

	AttrValue(const AttrColor &c) {
		type = ValueTypeColor;
		valColor = c;
	}

	AttrValue(const AttrAColor &ac) {
		type = ValueTypeAColor;
		valAColor = ac;
	}

	AttrValue(const AttrVector &v) {
		type = ValueTypeVector;
		valVector = v;
	}

	AttrValue(const AttrMatrix &m) {
		type = ValueTypeMatrix;
		valMatrix = m;
	}

	AttrValue(const AttrTransform &attrValue) {
		type = ValueTypeTransform;
		valTransform = attrValue;
	}

	AttrValue(const int &attrValue) {
		type = ValueTypeInt;
		valInt = attrValue;
	}

	AttrValue(const bool &attrValue) {
		type = ValueTypeInt;
		valInt = attrValue;
	}

	AttrValue(const float &attrValue) {
		type = ValueTypeFloat;
		valFloat = attrValue;
	}

	AttrValue(const AttrListInt &attrValue) {
		type = ValueTypeListInt;
		valListInt = attrValue;
	}

	AttrValue(const AttrListFloat &attrValue) {
		type = ValueTypeListFloat;
		valListFloat = attrValue;
	}

	AttrValue(const AttrListVector &attrValue) {
		type = ValueTypeListVector;
		valListVector = attrValue;
	}

	AttrValue(const AttrListColor &attrValue) {
		type = ValueTypeListColor;
		valListColor = attrValue;
	}

	AttrValue(const AttrListPlugin &attrValue) {
		type = ValueTypeListPlugin;
		valListPlugin = attrValue;
	}

	AttrValue(const AttrListValue &attrValue) {
		type = ValueTypeListValue;
		valListValue = attrValue;
	}

	AttrValue(const AttrListString &attrValue) {
		type = ValueTypeListString;
		valListString = attrValue;
	}

	AttrValue(const AttrMapChannels &attrValue) {
		type = ValueTypeMapChannels;
		valMapChannels = attrValue;
	}
	AttrValue(const AttrInstancer &attrValue) {
		type = ValueTypeInstancer;
		valInstancer = attrValue;
	}

	// TODO: Replace with single storage with reinterpret_cast<>
	int                 valInt;
	float               valFloat;
	AttrVector          valVector;
	AttrColor           valColor;
	AttrAColor          valAColor;

	std::string         valString;

	AttrMatrix          valMatrix;
	AttrTransform       valTransform;

	AttrPlugin          valPlugin;

	AttrListInt         valListInt;
	AttrListFloat       valListFloat;
	AttrListVector      valListVector;
	AttrListColor       valListColor;
	AttrListPlugin      valListPlugin;
	AttrListValue       valListValue;
	AttrListString      valListString;

	AttrMapChannels     valMapChannels;
	AttrInstancer       valInstancer;

	ValueType           type;

	const char *getTypeAsString() const {
		switch (type) {
		case ValueTypeInt:           return "Int";
		case ValueTypeFloat:         return "Float";
		case ValueTypeColor:         return "Color";
		case ValueTypeAColor:        return "AColor";
		case ValueTypeVector:        return "Vector";
		case ValueTypeTransform:     return "Transform";
		case ValueTypeString:        return "String";
		case ValueTypePlugin:        return "Plugin";
		case ValueTypeListInt:       return "ListInt";
		case ValueTypeListFloat:     return "ListFloat";
		case ValueTypeListColor:     return "ListColor";
		case ValueTypeListVector:    return "ListVector";
		case ValueTypeListMatrix:    return "ListMatrix";
		case ValueTypeListTransform: return "ListTransform";
		case ValueTypeListString:    return "ListString";
		case ValueTypeListPlugin:    return "ListPlugin";
		case ValueTypeListValue:     return "ListValue";
		case ValueTypeInstancer:     return "Instancer";
		case ValueTypeMapChannels:   return "Map Channels";
		default:
			break;
		}
		return "Unknown";
	}

	operator bool() const {
		bool valid = true;
		if (type == ValueTypeUnknown) {
			valid = false;
		} else if (type == ValueTypePlugin) {
			valid = !!(valPlugin);
		}
		return valid;
	}
};



struct PluginAttr {
	PluginAttr() {}
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


	void add(const PluginAttr &attr, const float &time = 0.0f) {
		pluginAttrs[attr.attrName] = attr;
	}

	void add(const std::string &attrName, const AttrValue &attrValue, const float &time = 0.0f) {
		add(PluginAttr(attrName, attrValue), time);
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
