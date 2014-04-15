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

#include "exp_defines.h"
#include "exp_anim.h"

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
#include <set>

#include <Python.h>


typedef std::stringstream sstream;


namespace BL {

typedef Array<float, 16>  Transform;

}


namespace VRayScene {


class VRayExportable;


struct NodeAttrs {
	NodeAttrs() {
		override = false;

		objectID = 0;
		visible  = true;
		primary_visibility = true;
		nsamples = 0;
	}

	int  override;

	int  objectID;
	int  visible;
	int  primary_visibility;
	int  nsamples;
};


struct ExpoterSettings {
	ExpoterSettings(BL::Scene scene, BL::BlendData data, BL::RenderEngine engine):
		b_scene(scene),
		b_data(data),
		b_engine(engine)
	{
		m_sce  = NULL;
		m_main = NULL;

		m_fileObject = NULL;
		m_fileGeom   = NULL;
		m_fileLights = NULL;

		m_activeLayers = true;
		m_altDInstances = false;

		m_useNodeTree = false;
		m_useCameraLoop = false;

		m_customFrame = 0;
	}

	int               DoUpdateCheck();

	Scene            *m_sce;
	Main             *m_main;

	BL::Scene         b_scene;
	BL::BlendData     b_data;
	BL::RenderEngine  b_engine;

	PyObject         *m_fileObject;
	PyObject         *m_fileGeom;
	PyObject         *m_fileLights;

	int               m_activeLayers;
	int               m_altDInstances;
	int               m_useNodeTree;

	int               m_useHideFromView;
	int               m_useCameraLoop;
	int               m_useDisplaceSubdiv;
	int               m_useInstancer;

	int               m_customFrame;
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

// NOTE: Cache all params?

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

	bool needUpdateCheck(const int &frame) {
		return frame > m_sce->r.sfra;
	}

	static void setAnimationMode(int animation, int checkAnimated) {
		m_animation     = animation;
		m_checkAnimated = checkAnimated;
	}

	static void clearCache()  { m_exportNameCache.clear(); }
	static void clearFrames() { m_frameCache.freeData();   }

	static void initPluginDesc(const std::string &dirPath) { m_pluginDesc.init(dirPath); }
	static void freePluginDesc()                           { m_pluginDesc.freeData();    }

	static void initInterpolate(int frame) {
		if(m_animation) {
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
	void writeAttributes(PointerRNA *ptr, boost::property_tree::ptree *pluginDesc, std::stringstream &output, const StrSet &skipAttrs=StrSet());

	// Manual attributes export
	//
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
	static int              m_animation;
	static int              m_checkAnimated;
	static int              m_exportNodes;
	static int              m_exportGeometry;
	static ExpoterSettings *m_exportSettings;

protected:
	static StrSet           m_exportNameCache;
	static ExpCache         m_frameCache;
	static VRayPluginsDesc  m_pluginDesc;

	std::string             m_name;
	MHash                   m_hash;
	std::stringstream       m_plugin;

	Scene                  *m_sce;
	Main                   *m_main;
	Object                 *m_ob;

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
