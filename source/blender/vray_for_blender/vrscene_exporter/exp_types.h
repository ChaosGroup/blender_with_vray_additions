/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CGR_EXP_TYPES_H
#define CGR_EXP_TYPES_H

#include <Python.h>

#include <algorithm>

#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/join.hpp>

#include "exp_defines.h"
#include "exp_anim.h"
#include "exp_settings.h"

#include "CGR_blender_data.h"
#include "CGR_json_plugins.h"

#include "murmur3.h"

#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"
#include "BLI_path_util.h"

extern "C" {
#  include "DNA_scene_types.h"
#  include "DNA_object_types.h"
#  include "BKE_main.h"
}

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>

#define SKIP_TYPE(attrType) (\
	attrType == "LIST"     || \
	attrType == "INT_LIST" || \
	attrType == "FLOAT_LIST")

#define OUTPUT_TYPE(attrType) (\
	attrType == "OUTPUT_PLUGIN"            || \
	attrType == "OUTPUT_COLOR"             || \
	attrType == "OUTPUT_FLOAT_TEXTURE"     || \
	attrType == "OUTPUT_VECTOR_TEXTURE"    || \
	attrType == "OUTPUT_TRANSFORM_TEXTURE" || \
	attrType == "OUTPUT_TEXTURE")

#define MAPPABLE_TYPE(attrType) (\
	attrType == "BRDF"              || \
	attrType == "MATERIAL"          || \
	attrType == "GEOMETRY"          || \
	attrType == "PLUGIN"            || \
	attrType == "TEXTURE"           || \
	attrType == "TRANSFORM"         || \
	attrType == "TRANSFORM_TEXTURE" || \
	attrType == "FLOAT_TEXTURE"     || \
	attrType == "VECTOR_TEXTURE"    || \
	attrType == "INT_TEXTURE"       || \
	attrType == "VECTOR"            || \
	attrType == "UVWGEN")

#define NOT_ANIMATABLE_TYPE(attrType) (\
	attrType == "BRDF"     || \
	attrType == "MATERIAL" || \
	attrType == "GEOMETRY" || \
	attrType == "PLUGIN")

#define BOOST_FORMAT_STRING(s) boost::str(boost::format("\"%s\"") % s);
#define BOOST_FORMAT_FLOAT(f)  boost::str(boost::format("%.6g") % f)
#define BOOST_FORMAT_TM(tm)    boost::str(boost::format("TransformHex(\"%s\")") % tm)
#define BOOST_FORMAT_INT(i)    boost::str(boost::format("%i") % i)
#define BOOST_FORMAT_BOOL(i)   BOOST_FORMAT_INT(i)

#define BOOST_FORMAT_COLOR(c)   boost::str(boost::format("Color(%.6g,%.6g,%.6g)")       % c[0] % c[1] % c[2]);
#define BOOST_FORMAT_COLOR1(c)  boost::str(boost::format("Color(%.6g,%.6g,%.6g)")       % c    % c    % c);
#define BOOST_FORMAT_ACOLOR(c)  boost::str(boost::format("AColor(%.6g,%.6g,%.6g,%.6g)") % c[0] % c[1] % c[2] % c[3]);
#define BOOST_FORMAT_ACOLOR3(c) boost::str(boost::format("AColor(%.6g,%.6g,%.6g,1.0)")  % c[0] % c[1] % c[2]);
#define BOOST_FORMAT_VECTOR(v)  boost::str(boost::format("Vector(%.6g,%.6g,%.6g)")      % v[0] % v[1] % v[2])

#define BOOST_FORMAT_MATRIX(m) boost::str(boost::format( \
	"Matrix(Vector(%.6g,%.6g,%.6g),Vector(%.6g,%.6g,%.6g),Vector(%.6g,%.6g,%.6g))") \
	% m[0][0] % m[1][0] % m[2][0] % m[0][1] % m[1][1] % m[2][1] % m[0][2] % m[1][2] % m[2][2])

#define BOOST_FORMAT_LIST_JOIN_SEP(data, sep) boost::str(boost::format("%s") % boost::algorithm::join(data, sep))
#define BOOST_FORMAT_LIST_JOIN(data)  BOOST_FORMAT_LIST_JOIN_SEP(data, ",")
#define BOOST_FORMAT_LIST_BASE(type, data) boost::str(boost::format(type"(%s)") % boost::algorithm::join(data, ","))
#define BOOST_FORMAT_LIST(data)       BOOST_FORMAT_LIST_BASE("List",      data)
#define BOOST_FORMAT_LIST_INT(data)   BOOST_FORMAT_LIST_BASE("ListInt",   data)
#define BOOST_FORMAT_LIST_FLOAT(data) BOOST_FORMAT_LIST_BASE("ListFloat", data)

#ifdef WIN32
#  define SIZE_T_FORMAT "%i"
#else
#  define SIZE_T_FORMAT "%zu"
#endif


namespace VRayScene {

typedef std::map<std::string, std::string> AttributeValueMap;

class VRayExportable;


struct NodeAttrs {
	NodeAttrs():dupliHolder(PointerRNA_NULL)
	{
		override = false;

		objectID = 0;
		visible  = true;
		primary_visibility = true;
		nsamples = 0;

		namePrefix = "";
	}

	int  override;

	int  objectID;
	int  visible;
	int  primary_visibility;
	int  nsamples;

	// For DupliGroup without Instancer
	std::string  namePrefix;
	BL::Object   dupliHolder;
	float        tm[4][4];

};


struct MyColor {
	MyColor(float r, float g, float b):m_r(r),m_g(g),m_b(b) {}
	MyColor(float c[3]):m_r(c[0]),m_g(c[1]),m_b(c[2])       {}

	float m_r;
	float m_g;
	float m_b;
};

typedef std::vector<std::string>        StrVector;
typedef std::set<std::string>           StrSet;

typedef AnimationCache<VRayExportable>  ExpCache;


class VRayExportable {
public:
	VRayExportable();
	VRayExportable(Scene *scene, Main *main, Object *ob);

	virtual      ~VRayExportable()=0;

	// Inits plugin name
	virtual void  initName(const std::string &name="")=0;

	// Inits plugin hash for tracking actual data changes
	//
	virtual void  initHash()=0;

	// Writes plugin data
	//
	virtual void  writeData(PyObject *output, VRayExportable *prevState, bool keyFrame=false)=0;

	// Checks if object was recalculated by Blender.
	//
	virtual int   isUpdated() { return m_ob->id.pad2; }

	// Inits some very basic settings like object name, geometry type, etc.
	//
	virtual void preInit() {
		initName();
		initHash();
	}

	// Inits actual data, like building vertex list etc.
	//
	virtual void init() {
		preInit();
	}

	MHash         getHash() const { return m_hash; }
	const char   *getName() const { return m_name.c_str(); }

	// This function will check if there is a cached plugin and export it before
	// the current one to keep animation consistent between interpolate().
	// Is will call virtual writeData() for actual data write.
	// It will also setup interpolate statements prefix and suffix for animation.
	//
	int write(PyObject *output, int frame=INT_MIN);

	// Set property group; used to get plugin parameters from Blender Node's propertry groups
	//
	void setPropGroup(PyObject *propGroup) {
		m_propGroup = propGroup;
	}

	static void clearCache()  { m_exportNameCache.clear(); }
	static void clearFrames() { m_frameCache.freeData();   }

	static void initPluginDesc(const std::string &dirPath) { m_pluginDesc.init(dirPath); }
	static void freePluginDesc()                           { m_pluginDesc.freeData();    }

	static void initInterpolate(int frame) {
		if(ExpoterSettings::gSet.m_isAnimation) {
			sprintf(m_interpStart, "interpolate((%d,", frame);
			sprintf(m_interpEnd,   "))");
		}
		else {
			sprintf(m_interpStart, "%s", "");
			sprintf(m_interpEnd,   "%s", "");
		}
	}

	void writeHeader(const char *pluginID, const char *pluginName) {
		m_plugin << "\n" << pluginID << " " << pluginName << " {";
	}

	void writeFooter() {
		m_plugin << "\n}\n";
	}

	// Generic function for automatic attribute export
	//   @propName    - name of the attribute in the plugin description; could match RNA name
	//   @rnaPropName - override for the RNA name if RNA name doesn't match the plugin attribute name
	//
	void writeAttribute(PointerRNA *ptr, const char *propName, const char *rnaPropName=NULL);

	// Write all attibutes from RNA pointer
	void writeAttributes(PointerRNA *ptr);

	// Write all attibutes based on JSON plugin description
	void writeAttributes(PointerRNA *ptr, PluginJson *pluginDesc, std::stringstream &output, const StrSet &skipAttrs=StrSet());

	// Manual attributes export
	//
	std::string getAttributeValue(PointerRNA *ptr, PropertyRNA *prop, const char *propName);
	
	void writeAttribute(const char *name, const int &value) {
		m_plugin << "\n\t" << name << "=" << m_interpStart;
		m_plugin << value;
		m_plugin << m_interpEnd << ";";
	}

	void writeAttribute(const char *name, const float &value) {
		m_plugin << "\n\t" << name << "=" << m_interpStart;
		m_plugin << std::setprecision(6) << value;
		m_plugin << m_interpEnd << ";";
	}

	void writeAttribute(const char *name, const double &value) {
		m_plugin << "\n\t" << name << "=" << m_interpStart;
		m_plugin << std::setprecision(12) << value;
		m_plugin << m_interpEnd << ";";
	}

	void writeAttribute(const char *name, const MyColor &value) {
		m_plugin << "\n\t" << name << "=" << m_interpStart;
		m_plugin << "Color(" << value.m_r << "," << value.m_g << "," << value.m_b << ")";
		m_plugin << m_interpEnd << ";";
	}

	void writeAttribute(const char *name, const char *value, const int &quotes=false) {
		m_plugin << "\n\t" << name << "=" << m_interpStart;
		if(quotes)
			m_plugin << "\"";
		m_plugin << value;
		if(quotes)
			m_plugin << "\"";
		m_plugin << m_interpEnd << ";";
	}

	static char             m_interpStart[32];
	static char             m_interpEnd[3];

	static VRayPluginsDesc  m_pluginDesc;

protected:
	static StrSet           m_exportNameCache;
	static ExpCache         m_frameCache;

	std::string             m_name;
	MHash                   m_hash;
	std::stringstream       m_plugin;

	Scene                  *m_sce;
	Main                   *m_main;
	Object                 *m_ob;
	
	BL::Object              m_bl_ob;

	PyObject               *m_propGroup;

	PYTHON_PRINT_BUF;

};


// Base class for geometry
// This will override some exporting functions
// for more efficient transfer over http
//
class VRayGeom : public VRayExportable {
};

}

#endif // CGR_EXP_TYPES_H
