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

#ifndef CGR_NODE_H
#define CGR_NODE_H

extern "C" {
#  include "DNA_mesh_types.h"
}

#include "BKE_main.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "exp_types.h"

#include "utils/CGR_vrscene.h"
#include "utils/murmur3.h"

#include <string>
#include <vector>


namespace VRayScene {


enum GeomOverride {
	eOverideNode,
	eOverideProxy,
	eOveridePlane
};


class Node : public VRayExportable {
public:
	Node(Scene *sce, Main *main, Object *ob, DupliObject *dOb=NULL);

	virtual      ~Node() { freeData(); }
	virtual void  buildHash();
	virtual void  write(PyObject *output, int frame=0);
	void          writeGeometry(PyObject *output, int frame=0);

	void          init();
	void          freeData();

	const char   *getName() const     { return name.c_str(); }
	const char   *getDataName() const { return dataName.c_str(); }
	MHash         getHash() const     { return hash; }
	Object       *getObject() const   { return object; }

	char         *getTransform() const;
	int           getObjectID() const;

private:
	void          initName();
	void          initOverride();
	void          initTransform();
	void          initProperties();

	std::string   writeGeomStaticMesh(PyObject *output);
	std::string   writeGeomDisplacedMesh(PyObject *output, const std::string &meshName);
	std::string   writeGeomStaticSmoothedMesh(PyObject *output, const std::string &meshName);

	std::string   writeGeomMeshFile(PyObject *output);
	std::string   writeGeomPlane(PyObject *output);

	std::string   writeMtlMulti(PyObject *output);
	std::string   writeMtlWrapper(PyObject *output, const std::string &baseMtl);
	std::string   writeMtlOverride(PyObject *output, const std::string &baseMtl);
	std::string   writeMtlRenderStats(PyObject *output, const std::string &baseMtl);

	std::string   name;
	std::string   dataName;
	MHash         hash;

	Scene        *m_sce;
	Main         *m_main;
	Object       *object;
	DupliObject  *dupliObject;

	// Node properties
	int           objectID;
	char          transform[TRANSFORM_HEX_SIZE];
	std::string   material;

	// Additional properties
	GeomOverride  useGeomOverride;

	int           useMtlWrapper;
	int           useMtlOverride;
	int           useMtlRenderStats;

};

}

#endif // CGR_NODE_H
