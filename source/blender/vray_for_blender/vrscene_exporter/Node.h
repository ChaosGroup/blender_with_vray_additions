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

enum GeomType {
	eGeometryMesh,
	eGeometryProxy,
	eGeometryPlane
};


struct RenderStats {
	RenderStats() {
		camera_visibility = true;
		gi_visibility = true;
		reflections_visibility = true;
		refractions_visibility = true;
		shadows_visibility = true;
		visibility = true;
	}

	int camera_visibility;
	int gi_visibility;
	int reflections_visibility;
	int refractions_visibility;
	int shadows_visibility;
	int visibility;
};


class Node : public VRayExportable {
public:
	Node(Scene *scene, Main *main, Object *ob);

	static int      IsSmokeDomain(Object *ob);
	static int      HasHair(Object *ob);
	static int      DoRenderEmitter(Object *ob);
	static int      IsUpdated(Object *ob);

	virtual        ~Node() { freeData(); }
	virtual void    preInit() {}
	virtual void    initHash();
	virtual void    initName(const std::string &name="");
	virtual void    writeData(PyObject *output);
	virtual void    writeFakeData(PyObject *output);
	virtual int     isUpdated();

	int             isObjectUpdated();
	int             isObjectDataUpdated();

	void            init(const std::string &mtlOverrideName="");
	int             preInitGeometry(int useDisplaceSubdiv=true);
	void            initGeometry();

	void            freeData();

	void            writeGeometry(PyObject *output, int frame=0);
	void            writeHair(ExpoterSettings *settings);

	char           *getTransform() const;
	int             getObjectID() const;

	int             isMeshLight();
	int             isSmokeDomain();
	int             hasHair();
	int             doRenderEmitter();

	void            setVisiblity(const int &visible=true);
	void            setObjectID(const int &objectID=0);
	void            setHideFromView(const RenderStats &renderStats);

private:
	void            initTransform();

	std::string     writeGeomDisplacedMesh(PyObject *output, const std::string &meshName);
	std::string     writeGeomStaticSmoothedMesh(PyObject *output, const std::string &meshName);

	std::string     writeMtlMulti(PyObject *output);
	std::string     writeMtlWrapper(PyObject *output, const std::string &baseMtl);
	std::string     writeMtlOverride(PyObject *output, const std::string &baseMtl);
	std::string     writeMtlRenderStats(PyObject *output, const std::string &baseMtl);
	std::string     writeHideFromView(PyObject *output, const std::string &baseMtl, const std::string &nodeName);

	const char     *getDataName() const { return m_geometry->getName(); }

	// Export options
	std::string     m_materialOverride;

	// Node properties
	int             m_objectID;
	int             m_visible;
	char            m_transform[CGR_TRANSFORM_HEX_SIZE];
	VRayExportable *m_geometry;
	GeomType        m_geometryType;

	RenderStats     m_renderStatsOverride;

};

}

#endif // CGR_NODE_H
