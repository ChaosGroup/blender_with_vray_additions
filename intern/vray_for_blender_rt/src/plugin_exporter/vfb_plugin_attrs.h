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

const int VectorBytesCount  = 3 * sizeof(float);
const int Vector2BytesCount = 2 * sizeof(float);


<<<<<<< HEAD
struct AttrValue;


struct AttrColor {
	AttrColor():
	    r(0.0f),
	    g(0.0f),
	    b(0.0f)
	{}

	AttrColor(const float &r, const float &g, const float &b):
	    r(r),
	    g(g),
	    b(b)
	{}
=======
struct AttrColor : public VRayBaseTypes::AttrColorBase {
	AttrColor(const AttrColorBase & o): AttrColorBase(o) {}
	AttrColor(): AttrColorBase() {}
>>>>>>> add: Initial integration with zmqlib & remote vray

	AttrColor(const BlColor &c) {
		memcpy(&r, &c.data[0], VectorBytesCount);
	}

	AttrColor(const BlAColor &ac) {
		memcpy(&r, &ac.data[0], VectorBytesCount);
	}

	AttrColor(const float &r, const float &g, const float &b): AttrColorBase(r, g, b) {}

	AttrColor(float c): AttrColorBase(c) {}

	AttrColor(float color[4]): AttrColorBase(color) {}
};


struct AttrAColor: public VRayBaseTypes::AttrAColorBase {
	AttrAColor(): AttrAColorBase() {}

	AttrAColor(const AttrColor &c, const float &a = 1.0f): AttrAColorBase(c, a) {}
};


struct AttrVector: public VRayBaseTypes::AttrVectorBase {
	AttrVector(const AttrVectorBase & o): AttrVectorBase(o) {}

	AttrVector(): AttrVectorBase() {}

	AttrVector(const BlVector &bl_v) {
		memcpy(&x, &bl_v.data[0], VectorBytesCount);
	}

	AttrVector(const BlVector2 &bl_v) {
		x = bl_v.data[0];
		y = bl_v.data[1];
		z = 0.0f;
	}

	float operator * (const AttrVector other) {
		return x * other.x + y * other.y + z * other.z;
	}

	AttrVector operator - (const AttrVector other) {
		return AttrVector(x - other.x, y - other.y, z - other.z);
	}

	bool operator == (const AttrVector other) {
		return (x == other.x) && (y == other.y) && (z == other.z);
	}

	AttrVector(float vector[3]): AttrVectorBase(vector) {}

	AttrVector(const float &_x, const float &_y, const float &_z): AttrVectorBase(x, y, z) {}
};


struct AttrVector2: public VRayBaseTypes::AttrVector2Base {
	AttrVector2(): AttrVector2Base() {}

	AttrVector2(const BlVector2 &bl_v) {
		memcpy(&x, &bl_v.data[0], Vector2BytesCount);
	}

	AttrVector2(float vector[2]): AttrVector2Base(vector) {}
};


struct AttrMatrix: public VRayBaseTypes::AttrMatrixBase {
	AttrMatrix(const AttrMatrixBase & o): AttrMatrixBase(o) {}

	AttrMatrix(): VRayBaseTypes::AttrMatrixBase() {}

	AttrMatrix(float tm[3][3]): AttrMatrixBase(tm) {}

	AttrMatrix(float tm[4][4]): AttrMatrixBase(tm) {}
};


struct AttrTransform: public VRayBaseTypes::AttrTransformBase {
	AttrTransform(): AttrTransformBase() {}
	
	AttrTransform(const BlTransform &bl_tm) {
		memcpy(&m.v0, &bl_tm.data[0],  VectorBytesCount);
		memcpy(&m.v1, &bl_tm.data[4],  VectorBytesCount);
		memcpy(&m.v2, &bl_tm.data[8],  VectorBytesCount);
		memcpy(&offs, &bl_tm.data[12], VectorBytesCount);
	}

	AttrTransform(float tm[4][4]): AttrTransformBase(tm) {}
};


struct AttrPlugin: public VRayBaseTypes::AttrPluginBase {
	AttrPlugin(): AttrPluginBase() {}

<<<<<<< HEAD
	AttrPlugin& operator=(const std::string &name) {
		plugin = name;
		return *this;
	}

	AttrPlugin& operator=(const AttrValue &attrValue);

	std::string  plugin;
	std::string  output;
=======
	AttrPlugin(const std::string &name): AttrPluginBase(name) {}
>>>>>>> add: Initial integration with zmqlib & remote vray
};


template <typename T>
struct AttrList: public VRayBaseTypes::AttrListBase<T> {
	AttrList(): AttrListBase<T>() {}

	VRayBaseTypes::ValueType getType() const;

	VRayBaseTypes::AttrListBase<T> toBase() const {
		return VRayBaseTypes::AttrListBase<T>(*this);
	}

	AttrList(const int &size): AttrListBase<T>(size) {}
};

typedef AttrList<int>         AttrListInt;
typedef AttrList<float>       AttrListFloat;
typedef AttrList<AttrColor>   AttrListColor;
typedef AttrList<AttrVector>  AttrListVector;
typedef AttrList<AttrVector2> AttrListVector2;
typedef AttrList<AttrPlugin>  AttrListPlugin;
typedef AttrList<std::string> AttrListString;


inline VRayBaseTypes::ValueType AttrListInt::getType() const {
	return VRayBaseTypes::ValueType::ValueTypeListInt;
}

inline VRayBaseTypes::ValueType AttrListFloat::getType() const {
	return VRayBaseTypes::ValueType::ValueTypeListFloat;
}

inline VRayBaseTypes::ValueType AttrListColor::getType() const {
	return VRayBaseTypes::ValueType::ValueTypeListColor;
}

inline VRayBaseTypes::ValueType AttrListVector::getType() const {
	return VRayBaseTypes::ValueType::ValueTypeListVector;
}

inline VRayBaseTypes::ValueType AttrListVector2::getType() const {
	return VRayBaseTypes::ValueType::ValueTypeListVector2;
}

inline VRayBaseTypes::ValueType AttrListPlugin::getType() const {
	return VRayBaseTypes::ValueType::ValueTypeListPlugin;
}

inline VRayBaseTypes::ValueType AttrListString::getType() const {
	return VRayBaseTypes::ValueType::ValueTypeListString;
}

enum ValueType {
	ValueTypeUnknown = 0,

	ValueTypeInt,
	ValueTypeFloat,
	ValueTypeColor,
	ValueTypeAColor,
	ValueTypeVector,
	ValueTypeMatrix,
	ValueTypeTransform,
	ValueTypeString,
	ValueTypePlugin,

	ValueTypeListInt,
	ValueTypeListFloat,
	ValueTypeListColor,
	ValueTypeListVector,
	ValueTypeListMatrix,
	ValueTypeListTransform,
	ValueTypeListString,
	ValueTypeListPlugin,

	ValueTypeListValue,

	ValueTypeInstancer,
	ValueTypeMapChannels,
};


struct AttrMapChannels: public VRayBaseTypes::AttrMapChannelsBase {
	struct AttrMapChannel {
		AttrListVector vertices;
		AttrListInt    faces;
		std::string    name;
	};
	typedef boost::unordered_map<std::string, AttrMapChannel> MapChannelsMap;

	MapChannelsMap data;
};


struct AttrInstancer: public VRayBaseTypes::AttrInstancerBase {
	struct Item {
		int            index;
		AttrTransform  tm;
		AttrTransform  vel;
		AttrPlugin     node;
	};
	typedef AttrList<Item> Items;

	Items data;
};


struct AttrValue {
	typedef AttrList<AttrValue> AttrListValue;

	AttrValue():
	    type(ValueTypeUnknown)
	{}

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

	operator bool () const {
		bool valid = true;
		if (type == ValueTypeUnknown) {
			valid = false;
		}
		else if (type == ValueTypePlugin) {
			valid = !!(valPlugin);
		}
		return valid;
	}
};


inline AttrPlugin& AttrPlugin::operator=(const AttrValue &attrValue)
{
	if (attrValue.type == ValueTypePlugin) {
		*this = attrValue.valPlugin;
	}

	return *this;
}


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
	PluginDesc(const std::string &plugin_name, const std::string &plugin_id, const std::string &prefix=""):
		pluginName(plugin_name),
		pluginID(plugin_id)
	{
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

namespace VRayBaseTypes {
	inline VRayBaseTypes::ValueType VRayBaseTypes::AttrListBase<VRayForBlender::AttrVector>::getType() const {
		return VRayBaseTypes::ValueType::ValueTypeListVector;
	}
	inline VRayBaseTypes::ValueType VRayBaseTypes::AttrListBase<VRayForBlender::AttrColor>::getType() const {
		return VRayBaseTypes::ValueType::ValueTypeListColor;
	}
}
#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
