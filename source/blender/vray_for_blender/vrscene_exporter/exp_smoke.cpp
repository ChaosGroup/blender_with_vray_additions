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

#include "exp_defines.h"
#include "vrscene_api.h"
#include "CGR_vrscene.h"
#include "smoke_API.h"

#define USE_HEAT           0
#define USE_LOCAL_DOMAIN   1

#define DEBUG_GIZMO_SHAPE  0


static void GetDomainBounds(SmokeDomainSettings *sds, float p0[3], float p1[3])
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

	// PRINT_INFO("sds->res_min = %i %i %i", sds->res_min[0], sds->res_min[1], sds->res_min[2]);
	// PRINT_INFO("sds->res_max = %i %i %i", sds->res_max[0], sds->res_max[1], sds->res_max[2]);
	// PRINT_INFO("sds->cell_size = %.3f %.3f %.3f", sds->cell_size[0], sds->cell_size[1], sds->cell_size[2]);
	// PRINT_INFO("sds->p0 = %.3f %.3f %.3f", sds->p0[0], sds->p0[1], sds->p0[2]);
	// PRINT_INFO("sds->p1 = %.3f %.3f %.3f", sds->p1[0], sds->p1[1], sds->p1[2]);
	// PRINT_INFO("p0 = %.3f %.3f %.3f", p0[0], p0[1], p0[2]);
	// PRINT_INFO("p1 = %.3f %.3f %.3f", p1[0], p1[1], p1[2]);
}


// Represents transformation of base domain to adaptive domain
//
static void GetBaseToAdaptiveTransform(SmokeDomainSettings *sds, float tm[4][4])
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

	// PRINT_TM4("adaptive", tm);
}


static void GetDomainTransform(Object *ob, SmokeDomainSettings *sds, float tm[4][4])
{
	unit_m4(tm);

	float domainToAdaptiveTM[4][4];
	GetBaseToAdaptiveTransform(sds, domainToAdaptiveTM);

	mul_m4_m4m4(tm, ob->obmat, domainToAdaptiveTM);

	// PRINT_TM4("object", ob->obmat);
	// PRINT_TM4("domain", tm);
}


static void write_SmokeGizmo(PyObject          *outputFile,
							 Scene             *sce,
							 Object            *ob,
							 SmokeModifierData *smd,
							 const char        *pluginName,
							 const char        *geometryName,
							 const char        *lights)
{
	static char buf[MAX_PLUGIN_NAME];

	SmokeDomainSettings *sds = smd->domain;

	float domainTM[4][4];
	GetDomainTransform(ob, sds, domainTM);

	WRITE_PYOBJECT(outputFile, "\nEnvFogMeshGizmo %s {", pluginName);
	WRITE_PYOBJECT(outputFile, "\n\tgeometry=%s;", geometryName);
	if(strlen(lights)) {
		WRITE_PYOBJECT(outputFile, "\n\tlights=%s;", lights);
	}
	WRITE_PYOBJECT(outputFile, "\n\ttransform=interpolate((%d,", sce->r.cfra);
	WRITE_PYOBJECT_TRANSFORM(outputFile, domainTM);
	WRITE_PYOBJECT(outputFile, "));");
	WRITE_PYOBJECT(outputFile, "\n}\n");

#if DEBUG_GIZMO_SHAPE
	WRITE_PYOBJECT(outputFile, "\nNode Node%s {", geometryName);
	WRITE_PYOBJECT(outputFile, "\n\tgeometry=%s;", geometryName);
	WRITE_PYOBJECT(outputFile, "\n\tmaterial=MANOMATERIALISSET;");
	WRITE_PYOBJECT(outputFile, "\n\ttransform=interpolate((%d,", sce->r.cfra);
	WRITE_PYOBJECT_TRANSFORM(outputFile, domainTM);
	WRITE_PYOBJECT(outputFile, "));");
	WRITE_PYOBJECT(outputFile, "\n}\n");
#endif
}


void write_SmokeDomain(PyObject          *outputFile,
					   Scene             *sce,
					   Object            *ob,
					   SmokeModifierData *smd,
					   const char        *pluginName,
					   const char        *lights)
{
	static char buf[MAX_PLUGIN_NAME];

	char geomteryPluginName[MAX_PLUGIN_NAME];
	sprintf(geomteryPluginName, "Geom%s", pluginName);

	// Topology is always the same:
	//   2.0 x 2.0 x 2.0 box
	//
	WRITE_PYOBJECT(outputFile, "\nGeomStaticMesh %s {", geomteryPluginName);
	WRITE_PYOBJECT(outputFile,
				   "\n\tvertices=ListVectorHex(\""
				   "000080BF000080BF000080BF000080BF000080BF0000803F000080BF0000803F0000803F000080BF0000803F000080BF"
				   "0000803F000080BF000080BF0000803F000080BF0000803F0000803F0000803F0000803F0000803F0000803F000080BF\");");
	WRITE_PYOBJECT(outputFile,
				   "\n\tfaces=ListIntHex(\""
				   "000000000100000002000000020000000300000000000000020000000100000005000000050000000600000002000000"
				   "070000000600000005000000050000000400000007000000000000000300000007000000070000000400000000000000"
				   "030000000200000006000000060000000700000003000000010000000000000004000000040000000500000001000000\");");
	WRITE_PYOBJECT(outputFile, "\n}\n");

	write_SmokeGizmo(outputFile, sce, ob, smd, pluginName, geomteryPluginName, lights);
}


// Blender smoke uses 2.0 x 2.0 x 2.0 mesh domain and then uses object transform to form the final
// smoke domain, there also could be an adaptive domain, so we need to transform UVWs
//
static void write_SmokeUVWGen(PyObject          *outputFile,
							  Scene             *sce,
							  Object            *ob,
							  SmokeModifierData *smd,
							  const char        *pluginName)
{
	static char buf[MAX_PLUGIN_NAME];

	SmokeDomainSettings *sds = smd->domain;

	float domainTm[4][4];
	GetDomainTransform(ob, sds, domainTm);
	invert_m4(domainTm);

	// Remaps [-1.0, 1.0] to [0.0, 1.0]
	float uvwTm[4][4];
	scale_m4_fl(uvwTm, 0.5f);
	copy_v3_fl(uvwTm[3], 0.5f);

	float uvw_transform[4][4];
	mul_m4_m4m4(uvw_transform, uvwTm, domainTm);

	WRITE_PYOBJECT(outputFile, "\nUVWGenPlanarWorld %s {", pluginName);
	WRITE_PYOBJECT(outputFile, "\n\tuvw_transform=interpolate((%d,", sce->r.cfra);
	WRITE_PYOBJECT_TRANSFORM(outputFile, uvw_transform);
	WRITE_PYOBJECT(outputFile, "));");
	WRITE_PYOBJECT(outputFile, "\n}\n");
}


void write_TexVoxelData(PyObject          *outputFile,
						Scene             *sce,
						Object            *ob,
						SmokeModifierData *smd,
						const char        *pluginName,
						short              interpolation)
{
	static char buf[MAX_PLUGIN_NAME];

	SmokeDomainSettings *sds = smd->domain;

	char uvwPluginName[MAX_PLUGIN_NAME];
	sprintf(uvwPluginName, "UVW%s", pluginName);

	size_t i;
	size_t tot_res_high;
	size_t tot_res_low;

	int    res_high[3];
	int    res_low[3];

	float  ob_imat[4][4];

	if(NOT(sds && sds->fluid)) {
		PRINT_ERROR("Domain and / or fluid data not found!");
		return;
	}

	// Store object invert matrix
	invert_m4_m4(ob_imat, ob->obmat);

	// flame: Use flame temperature as texture data
	// dens: Use smoke density and color as texture data
	// heat: Use smoke heat as texture data. Values from -2.0 to 2.0 are used
	float dt, dx, *dens, *react, *fuel, *flame, *heat, *heatold, *vx, *vy, *vz, *r, *g, *b;
	float _d, _fu, _fl;
	unsigned char *obstacles;
	float *tcu, *tcv, *tcw;

	if(sds->flags & MOD_SMOKE_HIGHRES) {
		COPY_VECTOR_3_3(res_high, sds->res_wt);
		COPY_VECTOR_3_3(res_low,  sds->res);

		smoke_turbulence_export(sds->wt, &dens, &react, &flame, &fuel, &r, &g, &b, &tcu, &tcv, &tcw);
	}
	else {
		COPY_VECTOR_3_3(res_high, sds->res);
		COPY_VECTOR_3_3(res_low,  sds->res);

		smoke_export(sds->fluid, &dt, &dx, &dens, &react, &flame, &fuel, &heat, &heatold, &vx, &vy, &vz, &r, &g, &b, &obstacles);
	}

	// PRINT_INFO("sds->res      = %i %i %i", sds->res[0],    sds->res[1],    sds->res[2]);
	// PRINT_INFO("sds->res_wt   = %i %i %i", sds->res_wt[0], sds->res_wt[1], sds->res_wt[2]);
	// PRINT_INFO("sds->base_res = %i %i %i", sds->base_res[0], sds->base_res[1], sds->base_res[2]);

	tot_res_high = (size_t)res_high[0] * (size_t)res_high[1] * (size_t)res_high[2];
	tot_res_low  = (size_t)res_low[0]  * (size_t)res_low[1]  * (size_t)res_low[2];

	write_SmokeUVWGen(outputFile, sce, ob, smd, uvwPluginName);

	WRITE_PYOBJECT(outputFile, "\nTexVoxelData %s {", pluginName);
	WRITE_PYOBJECT(outputFile, "\n\tuvwgen=%s;", uvwPluginName);
	WRITE_PYOBJECT(outputFile, "\n\tinterpolation=%i;", interpolation);
	WRITE_PYOBJECT(outputFile, "\n\tresolution=Vector(%i,%i,%i);", res_high[0], res_high[1], res_high[2]);
#if USE_HEAT
	WRITE_PYOBJECT(outputFile, "\n\tresolution_low=Vector(%i,%i,%i);", res_low[0], res_low[1], res_low[2]);
#endif

	WRITE_PYOBJECT(outputFile, "\n\tdensity=interpolate((%d, ListFloatHex(\"", sce->r.cfra);
	for(i = 0; i < tot_res_high; ++i) {
		_d = dens ? dens[i] : 0.0f;
		WRITE_PYOBJECT_HEX_VALUE(outputFile, _d);
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	WRITE_PYOBJECT(outputFile, "\n\tflame=interpolate((%d, ListFloatHex(\"", sce->r.cfra);
	for(i = 0; i < tot_res_high; ++i) {
		_fl = flame ? flame[i] : 0.0f;
		WRITE_PYOBJECT_HEX_VALUE(outputFile, _fl);
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	WRITE_PYOBJECT(outputFile, "\n\tfuel=interpolate((%d, ListFloatHex(\"", sce->r.cfra);
	for(i = 0; i < tot_res_high; ++i) {
		_fu = fuel ? fuel[i] : 0.0f;
		WRITE_PYOBJECT_HEX_VALUE(outputFile, _fu);
	}
	WRITE_PYOBJECT(outputFile, "\")));");

#if USE_HEAT
	if(NOT(sds->flags & MOD_SMOKE_HIGHRES)) {
		WRITE_PYOBJECT(outputFile, "\n\theat=interpolate((%d, ListFloatHex(\"", sce->r.cfra);
		// Heat is somehow always low res
		for(i = 0; i < tot_res_low; ++i) {
			_h = heat ? heat[i] : 0.0f;
			WRITE_PYOBJECT_HEX_VALUE(outputFile, _h);
		}
		WRITE_PYOBJECT(outputFile, "\")));");
	}
#endif

	WRITE_PYOBJECT(outputFile, "\n}\n");
}
