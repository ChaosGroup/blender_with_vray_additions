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

#include "Light.h"
#include "CGR_rna.h"
#include "CGR_string.h"
#include "CGR_vrscene.h"

#include <boost/lexical_cast.hpp>


VRayScene::Light::Light(Scene *scene, Main *main, Object *ob, DupliObject *dOb):
	VRayExportable(scene, main, ob)
{
	m_dupliObject = dOb;
	m_object      = m_dupliObject ? m_dupliObject->ob : m_ob;
}


void VRayScene::Light::initName(const std::string &name)
{
	if(NOT(name.empty())) {
		m_name = name;
	}
	else {
		m_name.clear();

		// If base object is a duplicator also add it's name
		if(m_ob->transflag & OB_DUPLI)
			m_name = GetIDName((ID*)m_ob);

		m_name.append(GetIDName((ID*)m_object));

		// Add unique dupli index
		if(m_dupliObject)
			m_name.append(boost::lexical_cast<std::string>(m_dupliObject->persistent_id[0]));
	}
}


void VRayScene::Light::initHash()
{
	RnaAccess::RnaValue rna((ID*)m_ob->data, "vray");

	std::string lightPluginType = "OMNI";

	rna.getEnum("omni_type");
	rna.getEnum("direct_type");
	rna.getEnum("spot_type");

	m_plugin.str("");
	m_plugin << "\n"   << "GeomMeshFile" << " " << m_name << " {";
	m_plugin << "\n\t" << "file"        << "=\"" << rna.getPath("file") << "\";";
	m_plugin << "\n\t" << "anim_type"   << "=" << rna.getEnum("anim_type") << ";";
	m_plugin << "\n\t" << "anim_speed"  << "=" << rna.getFloat("anim_speed") << ";";
	m_plugin << "\n\t" << "anim_offset" << "=" << rna.getFloat("anim_offset") << ";";
	m_plugin << "\n}\n";

	m_hash = HashCode(m_plugin.str().c_str());
}


void VRayScene::Light::writeData(PyObject *output)
{
	PYTHON_PRINT(output, m_plugin.str().c_str());
}
