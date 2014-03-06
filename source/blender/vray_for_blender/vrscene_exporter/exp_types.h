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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
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


namespace BL {

typedef Array<float, 16>  Transform;

}


namespace VRayScene {


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

		m_exportNodes    = true;
		m_exportGeometry = true;

		m_animation = false;
		m_checkAnimated = ANIM_CHECK_BOTH;

		m_activeLayers = true;
		m_altDInstances = false;

		m_useNodes = false;
	}

	int checkUpdates() {
		if(m_animation && m_checkAnimated != ANIM_CHECK_NONE)
			return m_sce->r.cfra > m_sce->r.sfra;
		return 0;
	}

	Scene            *m_sce;
	Main             *m_main;

	BL::Scene         b_scene;
	BL::BlendData     b_data;
	BL::RenderEngine  b_engine;

	PyObject         *m_fileObject;
	PyObject         *m_fileGeom;
	PyObject         *m_fileLights;

	int               m_exportNodes;
	int               m_exportGeometry;

	int               m_animation;
	int               m_checkAnimated;

	int               m_activeLayers;
	int               m_altDInstances;

	int               m_useNodes;
};


class VRayExportable;

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
	VRayExportable() {
		m_name = "";
		m_hash = 0;

		m_sce  = NULL;
		m_main = NULL;
		m_ob   = NULL;

		m_propGroup = NULL;
		m_checkUpdated = true;

		initInterpolate(0);
	}

	VRayExportable(Scene *scene, Main *main, Object *ob) {
		m_name = "";
		m_hash = 0;

		m_sce  = scene;
		m_main = main;
		m_ob   = ob;

		m_propGroup = NULL;
		m_checkUpdated = true;

		initInterpolate(0);
	}

	virtual      ~VRayExportable()=0;

	MHash         getHash() const { return m_hash; }
	const char   *getName() const { return m_name.c_str(); }
	const std::string getMName() const { return m_name; }

	virtual void  initHash()=0;
	virtual void  initName(const std::string &name="")=0;
	virtual void  writeData(PyObject *output)=0;

	virtual void  writeFakeData(PyObject *output) {}

	virtual int isUpdated() {
		return m_ob->id.pad2;
	}

	virtual void preInit() {
		initName();
		initHash();
	}

	virtual void init() {
		preInit();
	}

	// This function will check if there is a cached plugin and export it before
	// the current one to keep animation consistent between interpolate().
	// Is will call virtual writeData() for actual data write.
	// It will also setup interpolate statements prefix and suffix for animation.
	//
	void write(PyObject *output, int frame=INT_MIN) {
		if(NOT(getHash()))
			return;

		if(NOT(m_animation) || (m_animation && m_checkAnimated == ANIM_CHECK_NONE)) {
			// TODO: Do this in animation mode also to prevent data reexport of dupli objects
			//
			if(m_expCache.find(m_name) != m_expCache.end())
				return;
			m_expCache.insert(m_name);

			initInterpolate(frame);
			writeData(output);
		}
		else {
			if(m_checkAnimated == ANIM_CHECK_SIMPLE) {
				if(checkUpdated(frame) && NOT(isUpdated()))
					return;

				initInterpolate(frame);
				writeData(output);
			}
			else if(m_checkAnimated == ANIM_CHECK_HASH || m_checkAnimated == ANIM_CHECK_BOTH) {
				if(m_checkAnimated == ANIM_CHECK_BOTH)
					if(checkUpdated(frame) && NOT(isUpdated()))
						return;

				MHash currHash = getHash();
				MHash prevHash = m_frameCache.getHash(m_name);

				if(currHash != prevHash) {
					int cacheFrame = m_frameCache.getFrame(m_name);
					int prevFrame  = frame - m_sce->r.frame_step;

					if(prevHash == 0) {
#if 0
						// prevHash 0 could mean that object have appeared at some frame of
						// animation; so we need to set some fake data for previous state
						// Let's say invisible at the first frame.
						if(frame > m_sce->r.sfra) {
							initInterpolate(m_sce->r.sfra);
							writeFakeData(output);
						}
#endif
					}
					else {
						// Write previous frame if hash is more then 'frame_step' back.
						if(cacheFrame < prevFrame) {
							initInterpolate(prevFrame);
							m_frameCache.getData(m_name)->writeData(output);
						}
					}

					initInterpolate(frame);
					writeData(output);

					m_frameCache.update(m_name, currHash, frame, this);
				}
			}
		}
	}

	// Set property group; used to get plugin parameters from Blender Node's propertry groups
	//
	void setPropGroup(PyObject *propGroup) {
		m_propGroup = propGroup;
	}

	bool checkUpdated(const int &frame) {
		// Data is exported first time - force export
		if(m_expCache.count(m_name) == 0)
			return false;
		m_expCache.insert(m_name);
		return frame > m_sce->r.sfra;
	}

	static void setAnimationMode(int animation, int checkAnimated) {
		m_animation     = animation;
		m_checkAnimated = checkAnimated;
	}

	static void clearCache()  { m_expCache.clear();      }
	static void clearFrames() { m_frameCache.freeData(); }

	static void initPluginDesc(const std::string &dirPath) { m_pluginDesc.init(dirPath); }
	static void freePluginDesc()                           { m_pluginDesc.freeData(); }

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

	static char m_interpStart[32];
	static char m_interpEnd[3];

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
	void writeAttributes(PointerRNA *ptr, boost::property_tree::ptree *pluginDesc, std::stringstream &output);

	// Manual attributes export
	//
	void writeAttribute(const char *name, const int &value) {
		m_plugin << "\n\t" << name << "=" << m_interpStart;
		m_plugin << value;
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

protected:
	static StrSet           m_expCache;
	static ExpCache         m_frameCache;
	static int              m_animation;
	static int              m_checkAnimated;
	static VRayPluginsDesc  m_pluginDesc;

	std::string             m_name;
	MHash                   m_hash;
	std::stringstream       m_plugin;

	Scene                  *m_sce;
	Main                   *m_main;
	Object                 *m_ob;

	PyObject               *m_propGroup;

	int                     m_checkUpdated;

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
