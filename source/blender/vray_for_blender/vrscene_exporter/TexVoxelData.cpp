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

#include "TexVoxelData.h"

#include "BLI_math.h"

#include "smoke_API.h"


using namespace VRayScene;


void VRayScene::GetDomainBounds(SmokeDomainSettings *sds, float p0[3], float p1[3])
{
	if(sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) {
		p0[0] = sds->p0[0] + sds->cell_size[0] * sds->res_min[0];
		p0[1] = sds->p0[1] + sds->cell_size[1] * sds->res_min[1];
		p0[2] = sds->p0[2] + sds->cell_size[2] * sds->res_min[2];
		p1[0] = sds->p0[0] + sds->cell_size[0] * sds->res_max[0];
		p1[1] = sds->p0[1] + sds->cell_size[1] * sds->res_max[1];
		p1[2] = sds->p0[2] + sds->cell_size[2] * sds->res_max[2];
	}
	else {
		p0[0] = -1.0f;
		p0[1] = -1.0f;
		p0[2] = -1.0f;
		p1[0] =  1.0f;
		p1[1] =  1.0f;
		p1[2] =  1.0f;
	}
}


// Represents transformation of base domain to adaptive domain
//
void VRayScene::GetBaseToAdaptiveTransform(SmokeDomainSettings *sds, float tm[4][4])
{
	float p0[3];
	float p1[3];

	GetDomainBounds(sds, p0, p1);

	unit_m4(tm);

	tm[0][0] = fabs(p1[0] - p0[0]) / 2.0f;
	tm[1][1] = fabs(p1[1] - p0[1]) / 2.0f;
	tm[2][2] = fabs(p1[2] - p0[2]) / 2.0f;

	tm[3][0] = (p0[0] + p1[0]) / 2.0f;
	tm[3][1] = (p0[1] + p1[1]) / 2.0f;
	tm[3][2] = (p0[2] + p1[2]) / 2.0f;
}


void VRayScene::GetDomainTransform(Object *ob, SmokeDomainSettings *sds, float tm[4][4])
{
	unit_m4(tm);

	float domainToAdaptiveTM[4][4];
	VRayScene::GetBaseToAdaptiveTransform(sds, domainToAdaptiveTM);

	mul_m4_m4m4(tm, ob->obmat, domainToAdaptiveTM);
}


void TexVoxelData::freeData()
{
	if(m_dens) {
		delete [] m_dens;
		m_dens = NULL;
	}
	if(m_flame) {
		delete [] m_flame;
		m_flame = NULL;
	}
	if(m_fuel) {
		delete [] m_fuel;
		m_fuel = NULL;
	}
#if CGR_USE_HEAT
	if(m_heat) {
		delete [] m_heat;
		m_heat = NULL;
	}
#endif
}


void TexVoxelData::setPropGroup(PyObject *propGroup)
{
	m_propGroup = propGroup;
}


void TexVoxelData::initHash()
{
	m_hash = HashCode(m_dens);
}


void TexVoxelData::initName(const std::string &name)
{
	if(NOT(name.empty())) {
		m_name = name;
	}
	else {
		// TODO: Get some name from object and modifier
	}
}


void TexVoxelData::setInterpolation(int value)
{
	p_interpolation = value;
}


void TexVoxelData::init(SmokeModifierData *smd)
{
	m_smd = smd;

	initUvTransform();
	initSmoke();

	initHash();
}


// Blender smoke uses 2.0 x 2.0 x 2.0 mesh domain and then uses object transform to form the final
// smoke domain, there also could be an adaptive domain, so we need to transform UVWs
//
void TexVoxelData::initUvTransform()
{
	SmokeDomainSettings *sds = m_smd->domain;

	float domainTm[4][4];
	GetDomainTransform(m_ob, sds, domainTm);
	invert_m4(domainTm);

	// Remaps [-1.0, 1.0] to [0.0, 1.0]
	float uvwTm[4][4];
	scale_m4_fl(uvwTm, 0.5f);
	copy_v3_fl(uvwTm[3], 0.5f);

	float uvw_transform[4][4];
	mul_m4_m4m4(uvw_transform, uvwTm, domainTm);

	GetTransformHex(uvw_transform, m_uvw_transform);
}


void TexVoxelData::initSmoke()
{
	SmokeDomainSettings *sds = m_smd->domain;

	if(NOT(sds && sds->fluid)) {
		PRINT_ERROR("Object: %s => Domain and / or fluid data not found!", m_ob->id.name+2);
		return;
	}

	size_t tot_res_high;
#if CGR_USE_HEAT
	size_t tot_res_low;
#endif

	// Store object invert matrix
	float  ob_imat[4][4];
	invert_m4_m4(ob_imat, m_ob->obmat);

	// flame: Use flame temperature as texture data
	// dens: Use smoke density and color as texture data
	// heat: Use smoke heat as texture data. Values from -2.0 to 2.0 are used
	float dt, dx, *dens, *react, *fuel, *flame, *heat, *heatold, *vx, *vy, *vz, *r, *g, *b;
	unsigned char *obstacles;
	float *tcu, *tcv, *tcw;

	if(sds->flags & MOD_SMOKE_HIGHRES) {
		COPY_VECTOR_3_3(m_res_high, sds->res_wt);
#if CGR_USE_HEAT
		COPY_VECTOR_3_3(m_res_low,  sds->res);
#endif
		smoke_turbulence_export(sds->wt, &dens, &react, &flame, &fuel, &r, &g, &b, &tcu, &tcv, &tcw);
	}
	else {
		COPY_VECTOR_3_3(m_res_high, sds->res);
#if CGR_USE_HEAT
		COPY_VECTOR_3_3(m_res_low,  sds->res);
#endif
		smoke_export(sds->fluid, &dt, &dx, &dens, &react, &flame, &fuel, &heat, &heatold, &vx, &vy, &vz, &r, &g, &b, &obstacles);
	}

	tot_res_high = (size_t)m_res_high[0] * (size_t)m_res_high[1] * (size_t)m_res_high[2];

#if CGR_USE_HEAT
	tot_res_low  = (size_t)m_res_low[0]  * (size_t)m_res_low[1]  * (size_t)m_res_low[2];
#endif

	m_dens  = GetFloatArrayZip(dens,  tot_res_high);
	m_flame = GetFloatArrayZip(flame, tot_res_high);
	m_fuel  = GetFloatArrayZip(fuel,  tot_res_high);

#if CGR_USE_HEAT
	m_heat  = CompressFloatArray(heat,  tot_res_low);
#endif
}


void TexVoxelData::writeData(PyObject *output)
{
	if(m_asFluid) {
		int interpolation_type = m_propGroup ? GetPythonAttrInt(m_propGroup, "interpolation_type") : 0;

		PYTHON_PRINTF(output, "\nTexMayaFluid %s@Data@Density {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tsize_x=%i;", m_res_high[0]);
		PYTHON_PRINTF(output, "\n\tsize_y=%i;", m_res_high[1]);
		PYTHON_PRINTF(output, "\n\tsize_z=%i;", m_res_high[2]);
		PYTHON_PRINTF(output, "\n\tinterpolation_type=%i;", interpolation_type);
		PYTHON_PRINTF(output, "\n\tvalues=%sListFloatHex(\"", m_interpStart);
		PYTHON_PRINT(output, m_dens);
		PYTHON_PRINTF(output, "\")%s;", m_interpEnd);
		PYTHON_PRINTF(output, "\n}\n");
		PYTHON_PRINTF(output, "\nTexMayaFluidTransformed %s@Density {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tfluid_tex=%s@Data@Density;", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tfluid_value_scale=1.0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_x=0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_y=0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_z=0;");
		PYTHON_PRINTF(output, "\n}\n");

		PYTHON_PRINTF(output, "\nTexMayaFluid %s@Data@Flame {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tsize_x=%i;", m_res_high[0]);
		PYTHON_PRINTF(output, "\n\tsize_y=%i;", m_res_high[1]);
		PYTHON_PRINTF(output, "\n\tsize_z=%i;", m_res_high[2]);
		PYTHON_PRINTF(output, "\n\tinterpolation_type=%i;", interpolation_type);
		PYTHON_PRINTF(output, "\n\tvalues=%sListFloatHex(\"", m_interpStart);
		PYTHON_PRINT(output, m_flame);
		PYTHON_PRINTF(output, "\")%s;", m_interpEnd);
		PYTHON_PRINTF(output, "\n}\n");
		PYTHON_PRINTF(output, "\nTexMayaFluidTransformed %s@Flame {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tfluid_tex=%s@Data@Flame;", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tfluid_value_scale=1.0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_x=0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_y=0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_z=0;");
		PYTHON_PRINTF(output, "\n}\n");

		PYTHON_PRINTF(output, "\nTexMayaFluid %s@Data@Fuel {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tsize_x=%i;", m_res_high[0]);
		PYTHON_PRINTF(output, "\n\tsize_y=%i;", m_res_high[1]);
		PYTHON_PRINTF(output, "\n\tsize_z=%i;", m_res_high[2]);
		PYTHON_PRINTF(output, "\n\tinterpolation_type=%i;", interpolation_type);
		PYTHON_PRINTF(output, "\n\tvalues=%sListFloatHex(\"", m_interpStart);
		PYTHON_PRINT(output, m_fuel);
		PYTHON_PRINTF(output, "\")%s;", m_interpEnd);
		PYTHON_PRINTF(output, "\n}\n");
		PYTHON_PRINTF(output, "\nTexMayaFluidTransformed %s@Fuel {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tfluid_tex=%s@Data@Fuel;", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tfluid_value_scale=1.0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_x=0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_y=0;");
		PYTHON_PRINTF(output, "\n\tdynamic_offset_z=0;");
		PYTHON_PRINTF(output, "\n}\n");
	}
	else {
		PYTHON_PRINTF(output, "\nUVWGenPlanarWorld UVW%s {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tuvw_transform=%sTransformHex(\"%s\")%s;", m_interpStart, m_uvw_transform, m_interpEnd);
		PYTHON_PRINTF(output, "\n}\n");

		PYTHON_PRINTF(output, "\nTexVoxelData %s {", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tuvwgen=UVW%s;", m_name.c_str());
		PYTHON_PRINTF(output, "\n\tinterpolation=%i;", p_interpolation);
		PYTHON_PRINTF(output, "\n\tresolution=Vector(%i,%i,%i);", m_res_high[0], m_res_high[1], m_res_high[2]);
#if CGR_USE_HEAT
		PYTHON_PRINTF(output, "\n\tresolution_low=Vector(%i,%i,%i);", m_res_low[0], m_res_low[1], m_res_low[2]);
#endif
		PYTHON_PRINTF(output, "\n\tdensity=%sListFloatHex(\"", m_interpStart);
		PYTHON_PRINT(output, m_dens);
		PYTHON_PRINTF(output, "\")%s;", m_interpEnd);
		PYTHON_PRINTF(output, "\n\tflame=%sListFloatHex(\"", m_interpStart);
		PYTHON_PRINT(output, m_flame);
		PYTHON_PRINTF(output, "\")%s;", m_interpEnd);
		PYTHON_PRINTF(output, "\n\tfuel=%sListFloatHex(\"", m_interpStart);
		PYTHON_PRINT(output, m_fuel);
		PYTHON_PRINTF(output, "\")%s;", m_interpEnd);
#if CGR_USE_HEAT
		// Heat is somehow always low res
		if(NOT(sds->flags & MOD_SMOKE_HIGHRES)) {
			PYTHON_PRINTF(output, "\n\theat=%sListFloatHex(\"", m_interpStart);
			PYTHON_PRINT(output, m_heat);
			PYTHON_PRINTF(output, "\")%s;", m_interpEnd);
		}
#endif
		PYTHON_PRINTF(output, "\n}\n");
	}
}
