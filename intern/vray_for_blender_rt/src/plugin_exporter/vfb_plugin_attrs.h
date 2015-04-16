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

#include "cgr_config.h"
#include "vfb_util_defines.h"
#include "vfb_typedefs.h"
#include "vfb_rna.h"

#include "BLI_math.h"

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>


namespace VRayForBlender {

const int VectorBytesCount  = 3 * sizeof(float);
const int Vector2BytesCount = 2 * sizeof(float);


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

	AttrColor(const BlColor &bl_c) {
		memcpy(&r, &bl_c.data[0], VectorBytesCount);
	}

	AttrColor(float c):
		r(c),
		g(c),
		b(c)
	{}

	AttrColor(float color[4]):
		r(color[0]),
		g(color[1]),
		b(color[2])
	{}

	float r;
	float g;
	float b;
};


struct AttrAColor {
	AttrAColor():
	    alpha(1.0f)
	{}

	AttrAColor(const AttrColor &c, const float &a):
	    color(c),
	    alpha(a)
	{}

	AttrColor  color;
	float      alpha;
};


struct AttrVector {
	AttrVector():
	    x(0.0f),
	    y(0.0f),
	    z(0.0f)
	{}

	AttrVector(const BlVector &bl_v) {
		memcpy(&x, &bl_v.data[0], VectorBytesCount);
	}

	AttrVector(const BlVector2 &bl_v):
		x(bl_v.data[0]),
		y(bl_v.data[1]),
		z(0.0f)
	{}

	AttrVector(float vector[3]):
		x(vector[0]),
		y(vector[1]),
		z(vector[2])
	{}

	AttrVector(const float &_x, const float &_y, const float &_z):
		x(_x),
		y(_y),
		z(_z)
	{}

	float operator * (const AttrVector other) {
		return x * other.x + y * other.y + z * other.z;
	}

	AttrVector operator - (const AttrVector other) {
		return AttrVector(x - other.x, y - other.y, z - other.z);
	}

	bool operator == (const AttrVector other) {
		return (x == other.x) && (y == other.y) && (z == other.z);
	}

	void set(const float &_x, const float &_y, const float &_z) {
		x = _x;
		y = _y;
		z = _z;
	}

	void set(float vector[3]) {
		x = vector[0];
		y = vector[1];
		z = vector[2];
	}

	float x;
	float y;
	float z;
};


struct AttrVector2 {
	AttrVector2():
	    x(0.0f),
	    y(0.0f)
	{}

	AttrVector2(const BlVector2 &bl_v) {
		memcpy(&x, &bl_v.data[0], Vector2BytesCount);
	}

	AttrVector2(float vector[2]):
		x(vector[0]),
		y(vector[1])
	{}

	float x;
	float y;
};


struct AttrMatrix {
	AttrMatrix() {}

	AttrMatrix(float tm[3][3]):
	    v0(tm[0]),
	    v1(tm[1]),
	    v2(tm[2])
	{}

	AttrMatrix(float tm[4][4]):
	    v0(tm[0]),
	    v1(tm[1]),
	    v2(tm[2])
	{}

	AttrVector v0;
	AttrVector v1;
	AttrVector v2;
};


struct AttrTransform {
	AttrTransform() {}
	AttrTransform(const BlTransform &bl_tm) {
		memcpy(&m.v0, &bl_tm.data[0],  VectorBytesCount);
		memcpy(&m.v1, &bl_tm.data[4],  VectorBytesCount);
		memcpy(&m.v2, &bl_tm.data[8],  VectorBytesCount);
		memcpy(&offs, &bl_tm.data[12], VectorBytesCount);
	}
	AttrTransform(float tm[4][4]):
	    m(tm),
	    offs(tm[3])
	{}

	AttrMatrix m;
	AttrVector offs;
};


struct AttrPlugin {
	AttrPlugin() {}
	AttrPlugin(const std::string &name):
	    plugin(name)
	{}

	operator bool () const {
		return !plugin.empty();
	}

	std::string  plugin;
};


template <typename T>
struct AttrList {
	typedef std::vector<T>              DataType;
	typedef boost::shared_ptr<DataType> DataArray;

	AttrList() {
		init();
	}

	AttrList(const int &size) {
		init();
		resize(size);
	}

	void init() {
		ptr = DataArray(new DataType);
	}

	void resize(const int &cnt) {
		ptr.get()->resize(cnt);
	}

	void append(const T &value) {
		ptr.get()->push_back(value);
	}

	void prepend(const T &value) {
		ptr.get()->insert(0, value);
	}

	int getCount() const {
		return ptr.get()->size();
	}

	int getBytesCount() const {
		return getCount() * sizeof(T);
	}

	T* operator * () {
		return &ptr.get()->at(0);
	}

	const T* operator * () const {
		return &ptr.get()->at(0);
	}

	operator bool () const {
		return ptr && ptr.get()->size();
	}

private:
	DataArray ptr;

};

typedef AttrList<int>         AttrListInt;
typedef AttrList<float>       AttrListFloat;
typedef AttrList<AttrColor>   AttrListColor;
typedef AttrList<AttrVector>  AttrListVector;
typedef AttrList<AttrVector2> AttrListVector2;
typedef AttrList<AttrPlugin>  AttrListPlugin;
typedef AttrList<std::string> AttrListString;


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


struct AttrMapChannels {
	struct AttrMapChannel {
		AttrListVector vertices;
		AttrListInt    faces;
		std::string    name;
	};
	typedef boost::unordered_map<std::string, AttrMapChannel> MapChannelsMap;

	MapChannelsMap data;
};


struct AttrValue {
	typedef AttrList<AttrValue> AttrListValue;

	AttrValue():
	    type(ValueTypeUnknown)
	{}

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
		valPluginOutput = output;
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
	std::string         valPluginOutput;

	AttrListInt         valListInt;
	AttrListFloat       valListFloat;
	AttrListVector      valListVector;
	AttrListColor       valListColor;
	AttrListPlugin      valListPlugin;
	AttrListValue       valListValue;
	AttrListString      valListString;

	AttrMapChannels     valMapChannels;

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
		if (type == ValueTypePlugin) {
			return !!(valPlugin);
		}
		return true;
	}
};


struct PluginAttr {
	PluginAttr() {}
	PluginAttr(const std::string &attrName, const AttrValue &attrValue):
	    attrName(attrName),
	    attrValue(attrValue)
	{}

	std::string  attrName;
	AttrValue    attrValue;

};
typedef boost::unordered_map<std::string, PluginAttr> PluginAttrs;


struct PluginDesc {
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

	void add(const PluginAttr &attr, const float &time=0.0f) {
		PluginAttr *_attr = get(attr.attrName);
		if (_attr) {
			*_attr = attr;
		}
		else {
			pluginAttrs[attr.attrName] = attr;
		}
	}

	void add(const std::string &attrName, const AttrValue &attrValue, const float &time=0.0f) {
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
			PRINT_INFO_EX("  %s [%s]",
			              p.attrName.c_str(), p.attrValue.getTypeAsString());
		}
	}
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
