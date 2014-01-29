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
#include "GeomStaticMesh.h"
#include "vrscene_api.h"


using namespace VRayScene;


static void WritePythonAttribute(PyObject *outputFile, PyObject *propGroup, const char *attrName)
{
	static char buf[CGR_MAX_PLUGIN_NAME];

	PyObject *attr      = NULL;
	PyObject *attrValue = NULL;

	if(propGroup == Py_None)
		return;

	attr      = PyObject_GetAttrString(propGroup, attrName);
	attrValue = PyNumber_Long(attr);

	if(attrValue) {
		PYTHON_PRINTF(outputFile, "\n\t%s=%li;", attrName, PyLong_AsLong(attrValue));
	}
}


void write_Mesh(PyObject *outputFile, Scene *sce, Object *ob, Main *main, const char *pluginName, PyObject *propGroup)
{
	GeomStaticMesh geomStaticMesh;
	geomStaticMesh.init(sce, main, ob);
	geomStaticMesh.initName(pluginName);

	if(NOT(geomStaticMesh.getHash()))
		return;

	geomStaticMesh.write(outputFile, sce->r.cfra);

//	// Custom attibutes
//	if(propGroup) {
//		WritePythonAttribute(outputFile, propGroup, "dynamic_geometry");
//		WritePythonAttribute(outputFile, propGroup, "osd_subdiv_level");
//	}

}
