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

#ifndef TEX_VOXEL_DATA_H
#define TEX_VOXEL_DATA_H

#include "exp_types.h"

#include "CGR_vrscene.h"

extern "C" {
#  include "DNA_modifier_types.h"
#  include "DNA_smoke_types.h"
}

#define CGR_USE_HEAT           0
#define CGR_DEBUG_GIZMO_SHAPE  0


namespace VRayScene {


void GetDomainTransform(Object *ob, SmokeDomainSettings *sds, float tm[4][4]);
void GetDomainBounds(SmokeDomainSettings *sds, float p0[3], float p1[3]);
void GetBaseToAdaptiveTransform(SmokeDomainSettings *sds, float tm[4][4]);


class TexVoxelData : public VRayExportable {
public:
	TexVoxelData(Scene *scene, Main *main, Object *ob):VRayExportable(scene, main, ob) {
		m_smd = NULL;

		m_dens  = NULL;
		m_flame = NULL;
		m_fuel  = NULL;
#if CGR_USE_HEAT
		m_head  = NULL;
#endif
		p_interpolation = 0;

		m_propGroup = NULL;
		m_asFluid = false;
	}

	virtual           ~TexVoxelData() { freeData(); }
	virtual void       initHash();
	virtual void       initName(const std::string &name="");
	virtual void       writeData(PyObject *output, VRayExportable *prevState, bool keyFrame=false);

	void               init(SmokeModifierData *smd);
	void               freeData();

	void               setPropGroup(PyObject *propGroup);
	void               setInterpolation(int value);

	int                asFluid() const         { return m_asFluid; }
	void               setAsFluid(int asFluid) { m_asFluid = asFluid; }

private:
	void               initUvTransform();
	void               initSmoke();

	SmokeModifierData *m_smd;

	int                m_res_high[3];
#if CGR_USE_HEAT
	int                m_res_low[3];
#endif
	char              *m_dens;
	char              *m_flame;
	char              *m_fuel;
#if CGR_USE_HEAT
	char              *m_heat;
#endif

	char               m_uvw_transform[CGR_TRANSFORM_HEX_SIZE];
	int                p_interpolation;

	PyObject          *m_propGroup;
	int                m_asFluid;

};

}

#endif // TEX_VOXEL_DATA_H
