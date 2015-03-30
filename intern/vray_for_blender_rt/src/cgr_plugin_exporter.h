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

#ifndef VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
#define VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H

#include "cgr_config.h"
#include "cgr_util_defines.h"

#include "BLI_math.h"
#include "MEM_guardedalloc.h"
#include "RNA_types.h"
#include "RNA_access.h"
#include "RNA_blender_cpp.h"

#include <vraysdk.hpp>
#include <map>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>


namespace VRayForBlender {


typedef BL::Array<float, 16>  BlTransform;
typedef BL::Array<float, 3>   BlVector;
typedef BlVector              BlColor;
typedef BL::Array<int, 20>    BlLayers;

typedef BL::Array<int, 4>     BlFace;
typedef BL::Array<float, 2>   BlVertUV;
typedef BL::Array<float, 3>   BlVertCol;


const int VectorBytesCount = 3 * sizeof(float);


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
		return pixels;
	}

	void free() {
		FreePtr(pixels);
	}

	float *pixels;
	int    w;
	int    h;
};


struct AppSDKRenderImage:
        public RenderImage
{
	AppSDKRenderImage(VRay::VRayImage *image);
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

	float x;
	float y;
	float z;
};


struct AttrMatrix {
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

	AttrMatrix m;
	AttrVector offs;
};


struct AttrPlugin {
	AttrPlugin() {}
	AttrPlugin(const std::string &_name):
	    name(_name)
	{}

	operator bool () const {
		return !name.empty();
	}

	std::string  name;
};


template <typename T>
struct AttrList {
	typedef boost::shared_ptr<T[]> DataArray;

	AttrList():
	    ptr(nullptr),
	    count(0)
	{}

	AttrList(const int &size) {
		init(size);
	}

	void init(const int &size) {
		count = size;
		ptr = DataArray(new T[count]);
	}

	T* operator * () {
		return ptr.get();
	}

	const T* operator * () const {
		return ptr.get();
	}

	DataArray  ptr;
	int        count;

};

typedef AttrList<int>         AttrListInt;
typedef AttrList<float>       AttrListFloat;
typedef AttrList<AttrColor>   AttrListColor;
typedef AttrList<AttrVector>  AttrListVector;


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


struct PluginAttr {
	enum AttrType {
		AttrTypeUnknown = 0,
		AttrTypeIgnore,
		AttrTypeInt,
		AttrTypeFloat,
		AttrTypeVector,
		AttrTypeColor,
		AttrTypeAColor,
		AttrTypeMatrix,
		AttrTypeTransform,
		AttrTypeString,
		AttrTypePlugin,
		AttrTypeListInt,
		AttrTypeListFloat,
		AttrTypeListVector,
		AttrTypeListColor,
		AttrTypeListTransform,
		AttrTypeListString,
		AttrTypeListPlugin,
		AttrTypeListValue,
	};

	struct AttrValue {
		int                 valInt;
		float               valFloat;
		AttrVector          valVector;
		AttrColor           valColor;
		AttrAColor          valAColor;

		std::string         valString;

		AttrMatrix          valMatrix;
		AttrTransform       valTransform;

		AttrListInt         valListInt;
		AttrListFloat       valListFloat;
		AttrListVector      valListVector;
		AttrListColor       valListColor;

		// AttrListValue    valListValue;

		AttrPlugin          valPlugin;
		std::string         valPluginOutput;
	};

	PluginAttr() {
		paramName.clear();
		paramType = PluginAttr::AttrTypeUnknown;
	}

	PluginAttr(const std::string &attrName, const AttrType attrType) {
		paramName = attrName;
		paramType = attrType;
	}

	PluginAttr(const std::string &attrName, const std::string &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeString;
		paramValue.valString = attrValue;
	}

	PluginAttr(const std::string &attrName, const char *attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeString;
		paramValue.valString = attrValue;
	}


	PluginAttr(const std::string &attrName, const AttrPlugin attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypePlugin;
		paramValue.valPlugin = attrValue;
	}

	PluginAttr(const std::string &attrName, const AttrPlugin attrValue, const std::string &output) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypePlugin;
		paramValue.valPlugin = attrValue;
		paramValue.valPluginOutput = output;
	}

	PluginAttr(const std::string &attrName, const AttrColor &c) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeColor;
		paramValue.valColor = c;
	}

	PluginAttr(const std::string &attrName, const AttrAColor &ac) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeAColor;
		paramValue.valAColor = ac;
	}

	PluginAttr(const std::string &attrName, const AttrVector &v) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeVector;
		paramValue.valVector = v;
	}

	PluginAttr(const std::string &attrName, const AttrMatrix &m) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeMatrix;
		paramValue.valMatrix = m;
	}

	PluginAttr(const std::string &attrName, const AttrTransform attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeTransform;
		paramValue.valTransform = attrValue;
	}


	PluginAttr(const std::string &attrName, const int &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const bool &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const float &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeFloat;
		paramValue.valFloat = attrValue;
	}

	//	PluginAttr(const std::string &attrName, const AttrListValue &attrValue) {
	//		paramName = attrName;
	//		paramType = PluginAttr::AttrTypeListValue;
	//		paramValue.valListValue = attrValue;
	//	}

	PluginAttr(const std::string &attrName, const AttrListInt &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListInt;
		paramValue.valListInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const AttrListFloat &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListFloat;
		paramValue.valListFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const AttrListVector &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListVector;
		paramValue.valListVector = attrValue;
	}

	PluginAttr(const std::string &attrName, const AttrListColor &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListColor;
		paramValue.valListColor = attrValue;
	}

	const char *get_type() const {
		switch (paramType) {
			case AttrTypeInt: return "Int";
			case AttrTypeFloat: return "Float";
			case AttrTypeVector: return "Vector";
			case AttrTypeColor: return "Color";
			case AttrTypeAColor: return "AColor";
			case AttrTypeTransform: return "Transform";
			case AttrTypeString: return "String";
			case AttrTypePlugin: return "Plugin";
			case AttrTypeListInt: return "ListInt";
			case AttrTypeListFloat: return "ListFloat";
			case AttrTypeListVector: return "ListVector";
			case AttrTypeListColor: return "ListColor";
			case AttrTypeListTransform: return "ListTransform";
			case AttrTypeListString: return "ListString";
			case AttrTypeListPlugin: return "ListPlugin";
			case AttrTypeListValue: return "ListValue";
			default:
				break;
		}
		return "AttrTypeUnknown";
	}

	std::string  paramName;
	AttrType     paramType;
	AttrValue    paramValue;

};

typedef std::vector<PluginAttr> PluginAttrs;


struct PluginDesc {
	std::string  pluginName;
	std::string  pluginID;
	PluginAttrs  pluginAttrs;

	PluginDesc()
	{}

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
		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}

	PluginAttr *get(const std::string &paramName) {
		for (auto &pIt : pluginAttrs) {
			PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}

	void add(const PluginAttr &attr) {
		PluginAttr *_attr = get(attr.paramName);
		if (_attr) {
			*_attr = attr;
		}
		else {
			pluginAttrs.push_back(attr);
		}
	}

	void showAttributes() const {
		PRINT_INFO_EX("Plugin \"%s.%s\" parameters:",
		              pluginID.c_str(), pluginName.c_str());

		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			PRINT_INFO_EX("  %s [%s]",
			              p.paramName.c_str(), p.get_type());
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

#endif // VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
