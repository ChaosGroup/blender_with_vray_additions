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

#ifndef GEOM_MAYA_HAIR_H
#define GEOM_MAYA_HAIR_H

#include "exp_types.h"
#include "CGR_vrscene.h"


namespace VRayScene {

class GeomMayaHair : public VRayExportable {
public:
	GeomMayaHair(Scene *scene, Main *main, Object *ob);

	virtual        ~GeomMayaHair() { freeData(); }
	virtual void    preInit() {}
	virtual void    initHash();
	virtual void    initName(const std::string &name="");

	virtual void    writeData(PyObject *output);
	void            writeNode(PyObject *output, int frame=INT_MIN);

	void            init(ParticleSystem *psys);
	void            freeData();

	char           *getHairVertices() const    { return hair_vertices; }
	char           *getNumHairVertices() const { return num_hair_vertices;}
	char           *getWidths() const          { return widths; }
	char           *getTransparency() const    { return transparency; }


private:
	void            initData();
	void            initAttributes();

	std::string     getHairMaterialName() const;

	ParticleSystem *m_psys;

	std::stringstream  m_nodePlugin;
	std::string        m_nodeName;
	char               m_nodeTm[TRANSFORM_HEX_SIZE];

	int             use_width_fade;

	char           *hair_vertices;
	char           *num_hair_vertices;
	char           *widths;
	char           *transparency;
	char           *strand_uvw;

	float           opacity;

	int             use_global_hair_tree;

	int             geom_splines;
	float           geom_tesselation_mult;
};

}

#endif // GEOM_MAYA_HAIR_H
