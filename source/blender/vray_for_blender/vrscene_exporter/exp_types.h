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

#ifndef CGR_EXP_TYPES_H
#define CGR_EXP_TYPES_H

#include "exp_defines.h"
#include "murmur3.h"

#include <string>
#include <vector>
#include <Python.h>


namespace VRayScene {


typedef std::vector<std::string> StringVector;


class VRayExportable {
public:
	virtual      ~VRayExportable() {}

	MHash         getHash() const { return hash; }
	const char   *getName() const { return name.c_str(); }

	virtual void  buildHash()=0;
	virtual void  write(PyObject *output, int frame=0)=0;

protected:
	std::string   name;
	MHash         hash;

	char          m_interpStart[32];
	char          m_interpEnd[3];

	PYTHON_PRINT_BUF;

};

}

#endif // CGR_EXP_TYPES_H
