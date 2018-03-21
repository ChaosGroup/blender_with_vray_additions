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

#include "vfb_export_texvoxel.h"
#include "BLI_math.h"
#include "smoke_API.h"

#define CGR_USE_SMOKE_DATA_DEBUG  0

using namespace VRayForBlender;

#define COPY_VECTOR_3_3(a, b) \
	a[0] = b[0];\
	a[1] = b[1];\
	a[2] = b[2];



namespace {

void GetDomainBounds(SmokeDomainSettings *sds, float p0[3], float p1[3])
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
void GetBaseToAdaptiveTransform(SmokeDomainSettings *sds, float tm[4][4]) {
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
}

void VRayForBlender::GetDomainTransform(Object *ob, SmokeDomainSettings *sds, float tm[4][4]) {
	unit_m4(tm);

	float domainToAdaptiveTM[4][4];
	GetBaseToAdaptiveTransform(sds, domainToAdaptiveTM);

	mul_m4_m4m4(tm, ob->obmat, domainToAdaptiveTM);
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

	mul_m4_m4m4(m_uvw_transform, uvwTm, domainTm);
}


void TexVoxelData::initSmoke() {
	SmokeDomainSettings *sds = m_smd->domain;

	if (NOT(sds && sds->fluid)) {
		PRINT_ERROR("Object: %s => Domain and / or fluid data not found!", m_ob->id.name + 2);
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

	if (sds->flags & MOD_SMOKE_HIGHRES) {
		COPY_VECTOR_3_3(m_res_high, sds->res_wt);
#if CGR_USE_HEAT
		COPY_VECTOR_3_3(m_res_low,  sds->res);
#endif
		smoke_turbulence_export(sds->wt, &dens, &react, &flame, &fuel, &r, &g, &b, &tcu, &tcv, &tcw);
	} else {
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

#if CGR_USE_SMOKE_DATA_DEBUG
	float max_flame = 0.0;
	float min_flame = 0.0;
	for(size_t i = 0; i < tot_res_high; ++i) {
		float _fl = flame[i];
		if(_fl < min_flame) {
			min_flame = _fl;
		}
		if(_fl > max_flame) {
			max_flame = _fl;
		}
	}
	PRINT_INFO_EX("Flame range: [%.3f-%.3f]", min_flame, max_flame);

	float max_dens = 0.0;
	float min_dens = 0.0;
	for(size_t i = 0; i < tot_res_high; ++i) {
		float _d = dens[i];
		if(_d < min_dens) {
			min_dens = _d;
		}
		if(_d > max_dens) {
			max_dens = _d;
		}
	}
	PRINT_INFO_EX("Density range: [%.3f-%.3f]", min_dens, max_dens);
#endif
	if (dens) {
		m_dens.resize(tot_res_high);
		std::copy(dens, dens + tot_res_high, m_dens.getData()->begin());
	}
	if (flame) {
		m_flame.resize(tot_res_high);
		std::copy(flame, flame + tot_res_high, m_flame.getData()->begin());
	}
	if (fuel) {
		m_fuel.resize(tot_res_high);
		std::copy(fuel, fuel + tot_res_high, m_fuel.getData()->begin());
	}
#if CGR_USE_HEAT
	if (heat) {
		m_heat.resize(tot_res_low);
		std::copy(heat, heat + tot_res_low, m_heat.getData()->begin());
	}
#endif
}


AttrValue TexVoxelData::export_plugins(std::shared_ptr<PluginExporter> exporter)
{
	PluginDesc uvPlanarWorld("UVW" + m_name, "UVWGenPlanarWorld");
	uvPlanarWorld.add("uvw_transform", AttrTransformFromBlTransform(m_uvw_transform));

	auto uvwGenPlg = exporter->export_plugin(uvPlanarWorld);

	PluginDesc voxelData(m_name, "TexVoxelData");
	voxelData.add("uvwgen", uvwGenPlg);
	voxelData.add("interpolation", p_interpolation);
	voxelData.add("resolution", AttrVector(m_res_high[0], m_res_high[1], m_res_high[2]));
	voxelData.add("density", m_dens);
	voxelData.add("flame", m_flame);
	voxelData.add("fuel", m_fuel);

#if CGR_USE_HEAT
		// Heat is somehow always low res
		if(!m_smd->domain->flags & MOD_SMOKE_HIGHRES) {
			voxelData.add("heat", m_heat);
		}
#endif

	return exporter->export_plugin(voxelData);
}
