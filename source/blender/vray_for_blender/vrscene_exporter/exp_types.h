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

#include "murmur3.h"

extern "C" {
#  include "DNA_scene_types.h"
#  include "DNA_object_types.h"
#  include "BKE_main.h"
}

#include <string>
#include <vector>
#include <set>

#include <Python.h>


namespace VRayScene {


class VRayExportable;

typedef std::vector<std::string>        StringVector;
typedef std::set<std::string>           StrSet;
typedef AnimationCache<VRayExportable>  ExpCache;

class VRayExportable {
public:
	VRayExportable() {
		m_name = "";
		m_hash = 0;

		m_sce  = NULL;
		m_main = NULL;
		m_ob   = NULL;

		m_propGroup = NULL;

		initInterpolate();
	}

	VRayExportable(Scene *scene, Main *main, Object *ob) {
		m_name = "";
		m_hash = 0;

		m_sce  = scene;
		m_main = main;
		m_ob   = ob;

		m_propGroup = NULL;

		initInterpolate();
	}

	virtual      ~VRayExportable() {}

	MHash         getHash() const { return m_hash; }
	const char   *getName() const { return m_name.c_str(); }

	virtual void  initHash()=0;
	virtual void  initName(const std::string &name="")=0;
	virtual void  writeData(PyObject *output)=0;

	virtual void  init() {
		initName();
		initHash();
	}

	// This function will check if there is a cached plugin and export it before
	// the current one to keep animation consistent between interpolate().
	// Is will call virtual writeData() for actual data write.
	// It will also setup interpolate statements prefix and suffix for animation.
	//
	void write(PyObject *output, int frame=INT_MIN) {
		if(NOT(m_animation)) {
			if(m_expCache.find(m_name) != m_expCache.end())
				return;
			m_expCache.insert(m_name);

			writeData(output);
		}
		else {
			MHash currHash = getHash();
			MHash prevHash = m_frameCache.getHash(m_name);

			if(NOT(currHash == prevHash)) {
				// Write previous frame if hash is more then 'frame_step' back.
				// If 'prevHash' is 0 then previous call was for the first frame
				// and no need to reexport.
				//
				if(prevHash) {
					int cacheFrame = m_frameCache.getFrame(m_name);
					int prevFrame  = frame - m_sce->r.frame_step;

					if(cacheFrame < prevFrame) {
						initInterpolate(frame);
						m_frameCache.getData(m_name)->writeData(output);
					}
				}

				initInterpolate(frame);
				writeData(output);

				m_frameCache.update(m_name, currHash, frame, this);
			}
		}
	}

	// Set property group; used to get plugin parameters from Blender Node's propertry groups
	//
	void attachPropGroup(PyObject *propGroup) {
		m_propGroup = propGroup;
	}

	static void setAnimationMode(int animation, int checkAnimated) {
		m_animation     = animation;
		m_checkAnimated = checkAnimated;
	}

	static void clearCache()  { m_expCache.clear();      }
	static void clearFrames() { m_frameCache.freeData(); }

protected:
	void initInterpolate(int frame=INT_MIN) {
		if(m_animation && frame > INT_MIN) {
			sprintf(m_interpStart, "interpolate((%d,", frame);
			sprintf(m_interpEnd,   "))");
		}
		else {
			sprintf(m_interpStart, "%s", "");
			sprintf(m_interpEnd,   "%s", "");
		}
	}

	static StrSet    m_expCache;
	static ExpCache  m_frameCache;
	static int       m_animation;
	static int       m_checkAnimated;

	std::string      m_name;
	MHash            m_hash;

	Scene           *m_sce;
	Main            *m_main;
	Object          *m_ob;

	PyObject        *m_propGroup;

	char             m_interpStart[32];
	char             m_interpEnd[3];

	PYTHON_PRINT_BUF;

};

}

#endif // CGR_EXP_TYPES_H
