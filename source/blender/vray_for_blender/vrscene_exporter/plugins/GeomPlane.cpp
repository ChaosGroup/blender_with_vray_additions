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

#include "GeomPlane.h"


void VRayScene::GeomPlane::preInit()
{
	initName();
	initHash();
}


void VRayScene::GeomPlane::initHash()
{
	// GeomPlane is always the same
	m_hash = 1;
}


void VRayScene::GeomPlane::initName(const std::string &name)
{
	if(NOT(name.empty()))
		m_name = name;
	else
		m_name = "geomPlane";
}


void VRayScene::GeomPlane::writeData(PyObject *output, VRayExportable *prevState, bool keyFrame)
{
	PYTHON_PRINTF(output, "\nGeomPlane %s {}", m_name.c_str());
}
