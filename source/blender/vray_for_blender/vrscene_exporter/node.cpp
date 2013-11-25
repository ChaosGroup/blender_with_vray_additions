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

#include "utils/CGR_string.h"
#include "vrscene.h"
#include <set>
#include <string>


static std::set<std::string> exportedMeshes;


static void  write_ObjectNode(PyObject   *nodeFile,
							  PyObject   *geomFile,
							  Scene      *sce,
							  Main       *main,
							  Object     *ob,
							  float       tm[4][4],
							  const char *pluginName)
{
	static char buf[MAX_PLUGIN_NAME];

	char material[MAX_PLUGIN_NAME];
	char geometry[MAX_PLUGIN_NAME];

	// TODO:
	//   [ ] Add type checking and sync with Python naming
	//
	sprintf(material, "RS%s", ob->id.name);
	sprintf(geometry, "ME%s", ob->id.name+2);

	StripString(material);
	StripString(geometry);

	if(exportedMeshes.find(geometry) == exportedMeshes.end()) {
		exportedMeshes.insert(geometry);

		write_Mesh(geomFile, sce, ob, main, geometry, NULL);
	}

	WRITE_PYOBJECT(nodeFile, "\nNode %s {", pluginName);
	WRITE_PYOBJECT(nodeFile, "\n\tobjectID=%i;", ob->index);
	WRITE_PYOBJECT(nodeFile, "\n\tmaterial=%s;", material);
	WRITE_PYOBJECT(nodeFile, "\n\tgeometry=%s;", geometry);
	WRITE_PYOBJECT(nodeFile, "\n\ttransform=interpolate((%d,", sce->r.cfra);
	WRITE_PYOBJECT_TRANSFORM(nodeFile, tm);
	WRITE_PYOBJECT(nodeFile, "));");
	WRITE_PYOBJECT(nodeFile, "\n}\n");
}


void  write_Node(PyObject   *outputFile,
				 Scene      *sce,
				 Object     *ob,
				 const char *pluginName,
				 const char *transform,
				 const char *geometry,
				 const char *material,
				 const char *volume,
				 const int   nsamples,
				 const char *lights,
				 const char *user_attributes,
				 const int   visible,
				 const int   objectID,
				 const int   primary_visibility)
{
}


static void free_duplilist(Object *ob)
{
	if(ob->duplilist) {
		free_object_duplilist(ob->duplilist);
		ob->duplilist = NULL;
	}
}


void write_Dupli(PyObject   *nodeFile,
				 PyObject   *geomFile,
				 Scene      *sce,
				 Main       *main,
				 Object     *ob)
{
	DupliObject *dob;
	char         pluginName[MAX_PLUGIN_NAME];

	exportedMeshes.clear();

	// Free duplilist if a user forgets to
	free_duplilist(ob);

	ob->duplilist = object_duplilist(sce, ob, TRUE);

	for(dob = (DupliObject*)ob->duplilist->first; dob; dob = dob->next) {
		sprintf(pluginName, "%s_%.5i", dob->ob->id.name, dob->persistent_id[0]);
		StripString(pluginName);

		write_ObjectNode(nodeFile, geomFile, sce, main, dob->ob, dob->mat, pluginName);
	}

	// Free our duplilist
	free_duplilist(ob);
}
