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

#ifndef GEOM_MAYA_HAIR_H
#define GEOM_MAYA_HAIR_H

#include "exp_types.h"


namespace VRayScene {

class GeomMayaHair : public VRayExportable {
public:
	GeomMayaHair(Scene *scene, Main *main, Object *ob);

	virtual           ~GeomMayaHair() { freeData(); }
	virtual void       initHash();
	virtual void       initName(const std::string &name="");

	virtual void       writeData(PyObject *output, VRayExportable *prevState, bool keyFrame=false);
	void               writeNode(PyObject *output, int frame=INT_MIN, const NodeAttrs &attrs=NodeAttrs());

	virtual void       init();
	void               preInit(ParticleSystem *psys);
	void               freeData();

	char              *getHairVertices() const    { return hair_vertices; }
	char              *getNumHairVertices() const { return num_hair_vertices;}
	char              *getWidths() const          { return widths; }
	char              *getTransparency() const    { return transparency; }
	char              *getStrandUVW() const       { return strand_uvw; }

	MHash              getHairVerticesHash() const    { return m_hashHairVertices; }
	MHash              getNumHairVerticesHash() const { return m_hashNumHairVertices; }
	MHash              getWidthsHash() const          { return m_hashWidths; }
	MHash              getTransparencyHash() const    { return m_hashTransparency; }
	MHash              getStrandUvwHash() const       { return m_hashStrandUVW; }

private:
	void               initData();
	void               initAttributes();

	std::string        getHairMaterialName() const;

	ParticleSystem    *m_psys;

	// Experimental option to make hair thinner to the end
	int                use_width_fade;

	// Node for hair
	std::string        m_nodeName;
	char               m_nodeTm[CGR_TRANSFORM_HEX_SIZE];

	// Additional hashes
	// Allows exporting only the changed parts
	MHash              m_hashHairVertices;
	MHash              m_hashNumHairVertices;
	MHash              m_hashWidths;
	MHash              m_hashTransparency;
	MHash              m_hashStrandUVW;

	// GeomMayaHair params
	char              *hair_vertices;
	char              *num_hair_vertices;
	char              *widths;
	char              *transparency;
	char              *strand_uvw;

	int                use_global_hair_tree;
	int                geom_splines;
	float              geom_tesselation_mult;
	float              opacity;

};

}

#endif // GEOM_MAYA_HAIR_H
