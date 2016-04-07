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

#include "cgr_config.h"

#include "exp_defines.h"
#include "exp_types.h"

#include "Node.h"
#include "LightLinker.h"

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

	bool hasData() {
		return (visibility.size()             ||
				gi_visibility.size()          ||
				reflections_visibility.size() ||
				refractions_visibility.size() ||
				shadows_visibility.size()     ||
				camera_visibility.size());
	}

	PtrSet camera_visibility;
	PtrSet gi_visibility;
	PtrSet reflections_visibility;
	PtrSet refractions_visibility;
	PtrSet shadows_visibility;
	PtrSet visibility;
};


struct InstancerItem {
	std::string        nodeName;
	MHash              particleId;
	char               transform[CGR_TRANSFORM_HEX_SIZE];
	static const char *velocity;
};
typedef std::vector<InstancerItem*> InstancerItems;


class InstancerSystem {
public:
	~InstancerSystem() {
		clear();
	}

	InstancerItems  m_instances;

	InstancerItem* add(const std::string &node, const MHash idx, float obmat[4][4], float dupmat[4][4]);

	void append(InstancerItem *pa) {
		m_instances.push_back(pa);
	}

	void clear() {
		for(InstancerItems::const_iterator paIt = m_instances.begin(); paIt != m_instances.end(); ++paIt) {
			delete *paIt;
		}
		m_instances.clear();
	}

	const size_t size() const {
		return m_instances.size();
	}
};
typedef std::map<BL::ID, InstancerSystem*> MyPartSystems;


class MyParticles {
public:
	~MyParticles() {
		clear();
	}

	InstancerSystem *get(BL::ID ob) {
		InstancerSystem *myPsys = NULL;

		MyPartSystems::iterator it = m_systems.find(ob);
		if (it != m_systems.end()) {
			myPsys = it->second;
		}
		else {
			myPsys = new InstancerSystem();
			m_systems.insert(std::make_pair(ob, myPsys));
		}

		return myPsys;
	}

	void clear() {
		for (MyPartSystems::iterator sysIt = m_systems.begin(); sysIt != m_systems.end(); ++sysIt) {
			InstancerSystem *myPsys = sysIt->second;
			myPsys->clear();
			delete myPsys;
		}
		m_systems.clear();
	}

	MyPartSystems m_systems;
};

typedef std::set<BL::Object> ObjectSet;
typedef std::map<int, ObjectSet> SubframeObjects;

class VRsceneExporter {
public:
	VRsceneExporter();
	~VRsceneExporter();

	// Used to skip Node creating of gizmo objects
	void                    addSkipObject(void *obPtr);

	// Used for "Hide From View" feature
	void                    addToHideFromViewList(const std::string &listKey, void *obPtr);

	void                    exportSceneInit();
	int                     exportScene(const int &exportNodes, const int &exportGeometry);
	void                    exportClearCaches();

	void                    exportObjectsPre();
	void                    exportObjectBase(BL::Object ob);
	void                    exportObjectsPost();

	int                     is_interrupted();

private:
	void                    exportNodeEx(BL::Object ob, const NodeAttrs &attrs=NodeAttrs());

	void                    exportObject(BL::Object ob, const NodeAttrs &attrs=NodeAttrs());
	void                    exportLamp(BL::Object ob, const NodeAttrs &attrs=NodeAttrs());
	void                    exportVRayAsset(BL::Object ob, const NodeAttrs &attrs=NodeAttrs());
	void                    exportVRayClipper(BL::Object ob, const NodeAttrs &attrs=NodeAttrs());

	void                    exportDupliFromArray(BL::Object ob, BL::ArrayModifier arrayModifier);

	void                    initDupli();
	void                    exportDupli();

	void                    exportNode(Object *ob, const NodeAttrs &attrs=NodeAttrs());
	void                    exportNodeFromNodeTree(BL::NodeTree ntree, Object *ob, const NodeAttrs &attrs=NodeAttrs());
	std::string             writeNodeFromNodeTree(BL::NodeTree ntree, BL::Node node);

	StrSet                  m_exportedObjects;
	MyParticles             m_psys;
	PtrSet                  m_skipObjects;

	HideFromView            m_hideFromView;

	LightLinker             m_lightLinker;

	SubframeObjects         m_subframeObjects;

	PYTHON_PRINT_BUF;

};

#endif // CGR_EXPORT_SCENE_H
