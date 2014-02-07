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
	eOverideNone,
	eOverideProxy,
	eOveridePlane
};


class Node : public VRayExportable {
public:
	Node(Scene *scene, Main *main, Object *ob, DupliObject *dOb=NULL);

	static int      IsSmokeDomain(Object *ob);
	static int      HasHair(Object *ob);
	static int      DoRenderEmitter(Object *ob);

	virtual        ~Node() { freeData(); }
	virtual void    initHash();
	virtual void    initName(const std::string &name="");
	virtual void    writeData(PyObject *output);
	virtual int     isAnimated();

	void            init(const std::string &mtlOverrideName="");
	int             initGeometry();

	void            freeData();

	void            writeGeometry(PyObject *output, int frame=0);
	void            writeHair(ExpoterSettings *settings);

	MHash           getGeometryHash();
	const char     *getDataName() const { return m_geometry->getName(); }
	Object         *getObject() const   { return m_object; }

	char           *getTransform() const;
	int             getObjectID() const;

	int             isMeshLight();
	int             isSmokeDomain();
	int             hasHair();
	int             doRenderEmitter();

private:
	void            initTransform();
	void            initProperties();

	std::string     writeGeomDisplacedMesh(PyObject *output, const std::string &meshName);
	std::string     writeGeomStaticSmoothedMesh(PyObject *output, const std::string &meshName);

	std::string     writeMtlMulti(PyObject *output);
	std::string     writeMtlWrapper(PyObject *output, const std::string &baseMtl);
	std::string     writeMtlOverride(PyObject *output, const std::string &baseMtl);
	std::string     writeMtlRenderStats(PyObject *output, const std::string &baseMtl);

	Object         *m_object;
	DupliObject    *m_dupliObject;

	std::string     m_materialOverride;

	// Node properties
	int             m_objectID;
	char            m_transform[TRANSFORM_HEX_SIZE];
	std::string     m_materiall;
	VRayExportable *m_geometry;
};

}

#endif // CGR_NODE_H
