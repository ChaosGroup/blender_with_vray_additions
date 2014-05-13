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

#ifndef CGR_EXPORT_SCENE_H
#define CGR_EXPORT_SCENE_H

#include "CGR_config.h"

#include "exp_defines.h"
#include "exp_types.h"

#include "Node.h"

#include "BKE_depsgraph.h"
#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"

#include <Python.h>


using namespace VRayScene;


typedef std::set<void*> PtrSet;


struct HideFromView {
	void clear() {
		camera_visibility.clear();
		gi_visibility.clear();
		reflections_visibility.clear();
		refractions_visibility.clear();
		shadows_visibility.clear();
		visibility.clear();
	}

	bool affectObject(Object *ob) {
		return (visibility.count(ob)             ||
				gi_visibility.count(ob)          ||
				reflections_visibility.count(ob) ||
				refractions_visibility.count(ob) ||
				shadows_visibility.count(ob)     ||
				camera_visibility.count(ob));
	}

	PtrSet camera_visibility;
	PtrSet gi_visibility;
	PtrSet reflections_visibility;
	PtrSet refractions_visibility;
	PtrSet shadows_visibility;
	PtrSet visibility;
};


struct MyParticle {
	std::string        nodeName;
	size_t             particleId;
	char               transform[CGR_TRANSFORM_HEX_SIZE];
	static const char *velocity;
};
typedef std::vector<MyParticle*> Particles;


class MyPartSystem {
public:
	~MyPartSystem() {
		clear();
	}

	Particles  m_particles;

	void append(MyParticle *pa) {
		m_particles.push_back(pa);
	}

	void clear() {
		for(Particles::const_iterator paIt = m_particles.begin(); paIt != m_particles.end(); ++paIt)
			delete *paIt;
		m_particles.clear();
	}

	const size_t size() const {
		return m_particles.size();
	}
};
typedef std::map<std::string, MyPartSystem*> MyPartSystems;


class MyParticles {
public:
	~MyParticles() {
		clear();
	}

	MyPartSystem* get(const std::string &name) {
		if(NOT(m_systems.count(name)))
			m_systems[name] = new MyPartSystem();
		return m_systems[name];
	}

	void clear() {
		for(MyPartSystems::const_iterator sysIt = m_systems.begin(); sysIt != m_systems.end(); ++sysIt)
			sysIt->second->clear();
		m_systems.clear();
	}

	MyPartSystems m_systems;

};


class VRsceneExporter {
public:
	VRsceneExporter(ExpoterSettings *settings);
	~VRsceneExporter();

	static ExpoterSettings *m_settings;
	static std::string      m_mtlOverride;

	// Used to skip Node creating of gizmo objects
	void                    addSkipObject(void *obPtr);

	// Used for "Hide From View" feature
	void                    addToHideFromViewList(const std::string &listKey, void *obPtr);

	void                    exportScene(const int &exportNodes, const int &exportGeometry);

private:
	void                    init();

	void                    exportObjectBase(Object *ob);
	void                    exportObject(Object *ob, const int &checkUpdated=true, const NodeAttrs &attrs=NodeAttrs());
	void                    exportLight(Object *ob, DupliObject *dOb=NULL);

	void                    initDupli();
	void                    exportDupli();

	void                    exportNode(Object *ob, const int &checkUpdated=true, const NodeAttrs &attrs=NodeAttrs());
	void                    exportNodeFromNodeTree(BL::NodeTree ntree, Object *ob, const int &checkUpdated=true, const NodeAttrs &attrs=NodeAttrs());
	std::string             writeNodeFromNodeTree(BL::NodeTree ntree, BL::Node node);

	StrSet                  m_exportedObject;
	MyParticles             m_psys;
	PtrSet                  m_skipObjects;

	HideFromView            m_hideFromView;

	PYTHON_PRINT_BUF;

};

#endif // CGR_EXPORT_SCENE_H
