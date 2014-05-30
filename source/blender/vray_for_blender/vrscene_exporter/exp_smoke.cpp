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

#include "exp_defines.h"
#include "vrscene_api.h"
#include "CGR_vrscene.h"

#include "DNA_smoke_types.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "TexVoxelData.h"


void ExportTexVoxelData(PyObject          *output,
						Scene             *sce,
						Object            *ob,
						SmokeModifierData *smd,
						const char        *pluginName,
						short              interpolation)
{
	VRayScene::TexVoxelData texVoxelData(sce, NULL, ob);
	texVoxelData.initName(pluginName);
	texVoxelData.init(smd);
	texVoxelData.setInterpolation(interpolation);

	if(NOT(texVoxelData.getHash()))
		return;

	texVoxelData.write(output, sce->r.cfra);
}


static void write_SmokeGizmo(PyObject          *outputFile,
							 Scene             *sce,
							 Object            *ob,
							 SmokeModifierData *smd,
							 const char        *pluginName,
							 const char        *geometryName,
							 const char        *lights)
{
	PYTHON_PRINT_BUF;

	SmokeDomainSettings *sds = smd->domain;

	float domainTM[4][4];
	VRayScene::GetDomainTransform(ob, sds, domainTM);

	PYTHON_PRINTF(outputFile, "\nEnvFogMeshGizmo %s {", pluginName);
	PYTHON_PRINTF(outputFile, "\n\tgeometry=%s;", geometryName);
	if(strlen(lights)) {
		PYTHON_PRINTF(outputFile, "\n\tlights=%s;", lights);
	}
	PYTHON_PRINTF(outputFile, "\n\ttransform=interpolate((%d,", sce->r.cfra);
	PYTHON_PRINT_TRANSFORM(outputFile, domainTM);
	PYTHON_PRINTF(outputFile, "));");
	PYTHON_PRINTF(outputFile, "\n}\n");
}


void ExportSmokeDomain(PyObject          *outputFile,
					   Scene             *sce,
					   Object            *ob,
					   SmokeModifierData *smd,
					   const char        *pluginName,
					   const char        *lights)
{
	PYTHON_PRINT_BUF;

	char geomteryPluginName[CGR_MAX_PLUGIN_NAME];
	sprintf(geomteryPluginName, "Geom%s", pluginName);

	// Topology is always the same:
	//   2.0 x 2.0 x 2.0 box
	//
	PYTHON_PRINTF(outputFile, "\nGeomStaticMesh %s {", geomteryPluginName);
	PYTHON_PRINTF(outputFile,
				   "\n\tvertices=ListVectorHex(\""
				   "000080BF000080BF000080BF000080BF000080BF0000803F000080BF0000803F0000803F000080BF0000803F000080BF"
				   "0000803F000080BF000080BF0000803F000080BF0000803F0000803F0000803F0000803F0000803F0000803F000080BF\");");
	PYTHON_PRINTF(outputFile,
				   "\n\tfaces=ListIntHex(\""
				   "000000000100000002000000020000000300000000000000020000000100000005000000050000000600000002000000"
				   "070000000600000005000000050000000400000007000000000000000300000007000000070000000400000000000000"
				   "030000000200000006000000060000000700000003000000010000000000000004000000040000000500000001000000\");");
	PYTHON_PRINTF(outputFile, "\n}\n");

	write_SmokeGizmo(outputFile, sce, ob, smd, pluginName, geomteryPluginName, lights);
}


void  ExportVoxelDataAsFluid(PyObject *output, Scene *sce, Object *ob, SmokeModifierData *smd, PyObject *propGroup, const char *pluginName)
{
	VRayScene::TexVoxelData *texVoxelData = new VRayScene::TexVoxelData(sce, NULL, ob);
	texVoxelData->initName(pluginName);
	texVoxelData->init(smd);
	texVoxelData->setPropGroup(propGroup);
	texVoxelData->setAsFluid(true);

	if(NOT(texVoxelData->getHash()))
		return;

	int toDelete = texVoxelData->write(output, VRayScene::VRayExportable::m_set->m_frameCurrent);
	if(toDelete)
		delete texVoxelData;
}
