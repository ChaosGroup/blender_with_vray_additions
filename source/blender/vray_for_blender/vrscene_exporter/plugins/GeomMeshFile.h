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

#ifndef GEOM_MESH_FILE_H
#define GEOM_MESH_FILE_H

#include "exp_types.h"


namespace VRayScene {

class GeomMeshFile : public VRayExportable {
public:
	GeomMeshFile(Scene *scene, Main *main, Object *ob):VRayExportable(scene, main, ob) {}

	virtual      ~GeomMeshFile() {}
	virtual void  preInit();
	virtual void  initHash();
	virtual void  initName(const std::string &name="");
	virtual void  writeData(PyObject *output, VRayExportable *prevState, bool keyFrame=false);


private:
	std::stringstream  m_plugin;

};

}

#endif // GEOM_MESH_FILE_H
