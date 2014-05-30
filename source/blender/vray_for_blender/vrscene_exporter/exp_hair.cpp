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

#include "GeomMayaHair.h"
#include "vrscene_api.h"


int ExportGeomMayaHair(PyObject *outputFile, Scene *sce, Main *main, Object *ob, ParticleSystem *psys, const char *pluginName)
{
	VRayScene::GeomMayaHair *geomMayaHair = new VRayScene::GeomMayaHair(sce, main, ob);
	geomMayaHair->preInit(psys);
	geomMayaHair->init();
	geomMayaHair->initName(pluginName);

	int toDelete = geomMayaHair->write(outputFile,  VRayScene::VRayExportable::m_set->m_frameCurrent);
	if(toDelete)
		delete geomMayaHair;

	return 0;
}
