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

#ifndef VRAY_LIGHTS_H
#define VRAY_LIGHTS_H

#include "exp_types.h"

#include "CGR_vrscene.h"

#include <sstream>
#include <map>


namespace VRayScene {

class Light : public VRayExportable {
public:
	Light(Scene *scene, Main *main, Object *ob, DupliObject *dOb=NULL);

	virtual           ~Light() {}
	virtual void       preInit() {}
	virtual void       initHash();
	virtual void       initName(const std::string &name="");
	virtual void       writeData(PyObject *output);

private:
	void               initType();
	void               initTransform();

	void               writeKelvinColor(const std::string &name, const int &temp);
	void               writePlugin();

	Object            *m_object;
	DupliObject       *m_dupliObject;

	std::string        m_vrayPluginID;
	char               m_transform[CGR_TRANSFORM_HEX_SIZE];

	const char       **m_paramDesc;

};

}

#endif // VRAY_LIGHTS_H
