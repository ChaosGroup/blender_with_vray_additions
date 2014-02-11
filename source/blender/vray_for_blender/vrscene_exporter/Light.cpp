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

#include "DNA_lamp_types.h"

#include "BLI_math_matrix.h"

#include <boost/lexical_cast.hpp>


VRayScene::Light::Light(Scene *scene, Main *main, Object *ob, DupliObject *dOb):
	VRayExportable(scene, main, ob)
{
	m_dupliObject = dOb;
	m_object      = m_dupliObject ? m_dupliObject->ob : m_ob;

	initTransform();
	initType();
	initName();
	initHash();
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


void VRayScene::Light::initTransform()
{
	float tm[4][4];

	if(m_dupliObject)
		copy_m4_m4(tm, m_dupliObject->mat);
	else
		copy_m4_m4(tm, m_object->obmat);

	GetTransformHex(tm, m_transform);
}


void VRayScene::Light::initHash()
{
	std::string lightRnaPath = "vray." + m_vrayPluginID;
	RnaAccess::RnaValue lightRna((ID*)m_object->data, lightRnaPath.c_str());

	// TODO: Hash more params
	m_pluginHash.str("");
	m_pluginHash << m_transform;
	lightRna.writePlugin(m_pluginDesc.getTree(m_vrayPluginID), m_pluginHash);

	m_hash = HashCode(m_pluginHash.str().c_str());
}


void VRayScene::Light::writeKelvinColor()
{

}


void VRayScene::Light::writeData(PyObject *output)
{
	std::string lightRnaPath = "vray." + m_vrayPluginID;
	RnaAccess::RnaValue lightRna((ID*)m_object->data, lightRnaPath.c_str());

	std::stringstream ss;
	ss << "\n" << m_vrayPluginID << " " << m_name << " {";
	lightRna.writePlugin(m_pluginDesc.getTree(m_vrayPluginID), ss, m_interpStart, m_interpEnd);
	ss << "\n\t" << "transform" << "=" << m_interpStart;
	ss << "TransformHex(\"" << m_transform << "\")";
	ss << m_interpEnd << ";";
	ss << "\n}\n";

	PYTHON_PRINT(output, ss.str().c_str());
}


void VRayScene::Light::initType()
{
	Lamp *la = (Lamp*)m_object->data;

	RnaAccess::RnaValue lampRna((ID*)la, "vray");

	switch(la->type) {
		case LA_LOCAL:
			if(lampRna.getEnum("omni_type") == 0)
				if(lampRna.getFloat("radius") > 0.0f)
					m_vrayPluginID = "LightSphere";
				else
					m_vrayPluginID = "LightOmni";
			else
				m_vrayPluginID = "LightAmbient";
			break;
		case LA_SUN:
			if(lampRna.getEnum("direct_type") == 0)
				m_vrayPluginID = "LightDirectMax";
			else
				m_vrayPluginID = "SunLight";
			break;
		case LA_SPOT:
			if(lampRna.getEnum("spot_type") == 0)
				m_vrayPluginID = "LightSpot";
			else
				m_vrayPluginID = "LightIESMax";
			break;
		case LA_HEMI:
			m_vrayPluginID = "LightDome";
			break;
		case LA_AREA:
			m_vrayPluginID = "LightRectangle";
			break;
		default:
			m_vrayPluginID = "LightOmniMax";
			break;
	}
}
