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

#ifndef VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
#define VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H

#include "cgr_config.h"
#include "vfb_util_defines.h"
#include "vfb_rna.h"

#include "BLI_math.h"
#include "MEM_guardedalloc.h"
#include "RNA_types.h"
#include "RNA_access.h"
#include "RNA_blender_cpp.h"

#include <vraysdk.hpp>
#include <map>
#include <set>

#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>


namespace VRayForBlender {


const int VectorBytesCount  = 3 * sizeof(float);
const int Vector2BytesCount = 2 * sizeof(float);


enum ExpoterType {
	ExpoterTypeFile = 0,
	ExpoterTypeCloud,
	ExpoterTypeZMQ,
	ExpoterTypeAppSDK,
};


struct ExpoterCallback {
	typedef boost::function<void(void)> CallbackFunction;

	ExpoterCallback() {}
	ExpoterCallback(CallbackFunction _cb):
	    cb(_cb)
	{}

	operator bool () const {
		return !!(cb);
	}

	CallbackFunction cb;
};


struct RenderImage {
	RenderImage():
	    pixels(nullptr),
	    w(0),
	    h(0)
	{}

	operator bool () const {
		return !!(pixels);
	}

	void free() {
		FreePtrArr(pixels);
	}

	float *pixels;
	int    w;
	int    h;
};


struct AppSDKRenderImage:
        public RenderImage
{
	AppSDKRenderImage(const VRay::VRayImage *image);
};


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
		std::memcpy(&r, &bl_c.data[0], VectorBytesCount);
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
		std::memcpy(&x, &bl_v.data[0], VectorBytesCount);
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

	// Dot product
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
		std::memcpy(&x, &bl_v.data[0], Vector2BytesCount);
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
		std::memcpy(&m.v0, &bl_tm.data[0],  VectorBytesCount);
		std::memcpy(&m.v1, &bl_tm.data[4],  VectorBytesCount);
		std::memcpy(&m.v2, &bl_tm.data[8],  VectorBytesCount);
		std::memcpy(&offs, &bl_tm.data[12], VectorBytesCount);
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
	typedef boost::shared_ptr<T[]> DataArray;

	AttrList():
	    ptr(nullptr),
	    count(0)
	{}

	AttrList(const int &size) {
		setCount(size);
	}

	void setCount(const int &cnt) {
		count = cnt;
		ptr   = DataArray(new T[cnt]);
	}

	int getCount() const {
		return count;
	}

	int getBytesCount() const {
		return count * sizeof(T);
	}

	T* operator * () {
		return ptr.get();
	}

	const T* operator * () const {
		return ptr.get();
	}

	operator bool () const {
		return !!count;
	}

private:
	DataArray  ptr;
	int        count;

};

typedef AttrList<int>         AttrListInt;
typedef AttrList<float>       AttrListFloat;
typedef AttrList<AttrColor>   AttrListColor;
typedef AttrList<AttrVector>  AttrListVector;
typedef AttrList<AttrVector2> AttrListVector2;
typedef AttrList<AttrPlugin>  AttrListPlugin;
typedef AttrList<std::string> AttrListString;


inline VRay::Color to_vray_color(const AttrColor &c)
{
	return VRay::Color(c.r, c.g, c.b);
}


inline VRay::AColor to_vray_acolor(const AttrAColor &ac)
{
	return VRay::AColor(to_vray_color(ac.color),
	                    ac.alpha);
}


inline VRay::Vector to_vray_vector(const AttrVector &v)
{
	return VRay::Vector(v.x, v.y, v.z);
}


inline VRay::Matrix to_vray_matrix(const AttrMatrix &m)
{
	return VRay::Matrix(to_vray_vector(m.v0),
	                    to_vray_vector(m.v1),
	                    to_vray_vector(m.v2));
}


inline VRay::Transform to_vray_transform(const AttrTransform &tm)
{
	return VRay::Transform(to_vray_matrix(tm.m),
	                       to_vray_vector(tm.offs));
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


#define USE_MAP 1
#if USE_MAP
typedef std::map<std::string, PluginAttr> PluginAttrs;
#else
typedef std::vector<PluginAttr>           PluginAttrs;
#endif


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
#if USE_MAP
		if (pluginAttrs.count(paramName)) {
			const auto pIt = pluginAttrs.find(paramName);
			return &pIt->second;
		}
#else
		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			if (paramName == p.attrName) {
				return &p;
			}
		}
#endif
		return nullptr;
	}

	PluginAttr *get(const std::string &paramName) {
#if USE_MAP
		if (pluginAttrs.count(paramName)) {
			return &pluginAttrs[paramName];
		}
#else
		for (auto &pIt : pluginAttrs) {
			PluginAttr &p = pIt;
			if (paramName == p.attrName) {
				return &p;
			}
		}
#endif
		return nullptr;
	}

	void add(const PluginAttr &attr, const float &time=0.0f) {
		PluginAttr *_attr = get(attr.attrName);
		if (_attr) {
			*_attr = attr;
		}
		else {
#if USE_MAP
			pluginAttrs[attr.attrName] = attr;
#else
			pluginAttrs.push_back(attr);
#endif
		}
	}

	void add(const std::string &attrName, const AttrValue &attrValue, const float &time=0.0f) {
		add(PluginAttr(attrName, attrValue), time);
	}

	void del(const std::string &attrName) {
#if USE_MAP
		auto delIt = pluginAttrs.find(attrName);
		pluginAttrs.erase(delIt);
#else
		// ...
#endif
	}

	void showAttributes() const {
		PRINT_INFO_EX("Plugin \"%s.%s\" parameters:",
		              pluginID.c_str(), pluginName.c_str());

		for (const auto &pIt : pluginAttrs) {
#if USE_MAP
			const PluginAttr &p = pIt.second;
#else
			const PluginAttr &p = pIt;
#endif
			PRINT_INFO_EX("  %s [%s]",
			              p.attrName.c_str(), p.attrValue.getTypeAsString());
		}
	}
};


class PluginExporter
{
public:
	virtual             ~PluginExporter()=0;

public:
	virtual void         init()=0;
	virtual void         free()=0;

	virtual void         sync()  {}
	virtual void         start() {}
	virtual void         stop()  {}

	virtual AttrPlugin   export_plugin(const PluginDesc &pluginDesc)=0;

	virtual RenderImage  get_image() { return RenderImage(); }
	virtual void         set_render_size(const int &w, const int &h) {}

	virtual void         set_callback_on_image_ready(ExpoterCallback cb)      { callback_on_image_ready = cb; }
	virtual void         set_callback_on_rt_image_updated(ExpoterCallback cb) { callback_on_rt_image_updated = cb; }

protected:
	ExpoterCallback      callback_on_image_ready;
	ExpoterCallback      callback_on_rt_image_updated;

};


class AppSdkExporter:
        public PluginExporter
{
	struct PluginUsed {
		PluginUsed() {}
		PluginUsed(const VRay::Plugin &p):
		    plugin(p),
		    used(true)
		{}

		VRay::Plugin  plugin;
		int           used;
	};

	typedef std::map<std::string, PluginUsed> PluginUsage;

public:
	AppSdkExporter();
	virtual             ~AppSdkExporter();

public:
	virtual void         init();
	virtual void         free();

	virtual void         sync();
	virtual void         start();
	virtual void         stop();

	virtual AttrPlugin   export_plugin(const PluginDesc &pluginDesc);

	virtual RenderImage  get_image();
	virtual void         set_render_size(const int &w, const int &h);

	virtual void         set_callback_on_image_ready(ExpoterCallback cb);
	virtual void         set_callback_on_rt_image_updated(ExpoterCallback cb);


private:
	void                 reset_used();

	VRay::Plugin         new_plugin(const PluginDesc &pluginDesc);

	VRay::VRayRenderer  *m_vray;

	PluginUsage          m_used_map;

};


PluginExporter* ExporterCreate(ExpoterType type);
void            ExporterDelete(PluginExporter *exporter);

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
