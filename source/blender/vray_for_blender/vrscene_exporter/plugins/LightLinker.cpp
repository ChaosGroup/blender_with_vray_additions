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

#include "LightLinker.h"


LightLinker::LightLinker():
    m_sce(PointerRNA_NULL),
    m_data(PointerRNA_NULL)
{}


void LightLinker::init(BL::BlendData data, BL::Scene sce)
{
	m_data = data;
	m_sce  = sce;
}


void LightLinker::getObject(const std::string &name, ObjectList &list)
{
	BL::Scene::objects_iterator obIt;
	for (m_sce.objects.begin(obIt); obIt != m_sce.objects.end(); ++obIt) {
		BL::Object ob = *obIt;
		if (ob.name() == name) {
			list.insert(ob);
			break;
		}
	}
}


void LightLinker::getGroupObjects(const std::string &name, ObjectList &list)
{
	BL::BlendData::groups_iterator grIt;
	for (m_data.groups.begin(grIt); grIt != m_data.groups.end(); ++grIt) {
		BL::Group gr = *grIt;
		if (gr.name() == name) {
			BL::Group::objects_iterator grObIt;
			for (gr.objects.begin(grObIt); grObIt != gr.objects.end(); ++grObIt) {
				list.insert(*grObIt);
			}
			break;
		}
	}
}


void LightLinker::prepass()
{
	BL::Scene::objects_iterator obIt;
	for(m_sce.objects.begin(obIt); obIt != m_sce.objects.end(); ++obIt) {
		BL::Object ob = *obIt;
		
		if(ob.type() != BL::Object::type_LAMP)
			continue;
		
		BL::Lamp lamp(ob.data());
		
		PointerRNA ptr = RNA_pointer_get(&lamp.ptr, "vray");
		
		if(NOT(RNA_boolean_get(&ptr, "use_include_exclude")))
			continue;

		const std::string &lightName = GetIDName((ID*)ob.ptr.data, "LA");

		char buf[MAX_ID_NAME];
		
		std::string obName;
		std::string groupName;
		
		if(RNA_boolean_get(&ptr, "use_exclude")) {
			RNA_string_get(&ptr, "exclude_objects", buf);
			obName = buf;
			if(NOT(obName.empty())) {
				getObject(obName, m_exclude[lightName]);
			}
			
			RNA_string_get(&ptr, "exclude_groups", buf);
			groupName = buf;
			if(NOT(groupName.empty())) {
				getGroupObjects(obName, m_exclude[lightName]);
			}
		}

		// NOTE: Include means add all other objects to exclude list
		if(RNA_boolean_get(&ptr, "use_include")) {
			RNA_string_get(&ptr, "include_objects", buf);
			obName = buf;
			if(NOT(obName.empty())) {
				BL::Scene::objects_iterator obIt;
				for (m_sce.objects.begin(obIt); obIt != m_sce.objects.end(); ++obIt) {
					BL::Object ob = *obIt;
					if (ob.name() != obName) {
						m_exclude[lightName].insert(ob);
					}
				}
			}

			RNA_string_get(&ptr, "include_groups", buf);
			groupName = buf;
			BL::BlendData::groups_iterator grIt;
			for (m_data.groups.begin(grIt); grIt != m_data.groups.end(); ++grIt) {
				BL::Group gr = *grIt;
				if (gr.name() == groupName) {
					BL::Scene::objects_iterator obIt;
					for (m_sce.objects.begin(obIt); obIt != m_sce.objects.end(); ++obIt) {
						BL::Object ob = *obIt;

						bool excludeObject = true;
						BL::Group::objects_iterator grObIt;
						for (gr.objects.begin(grObIt); grObIt != gr.objects.end(); ++grObIt) {
							BL::Object grOb = *grObIt;

							// If object is in include group then don't exclude it
							if (ob == grOb) {
								excludeObject = false;
								break;
							}
						}

						if (excludeObject)
							m_exclude[lightName].insert(ob);
					}
					break;
				}
			}
		}
	}

	LightLink::const_iterator linkIt;
	for (linkIt = m_exclude.begin(); linkIt != m_exclude.end(); ++linkIt) {
		const std::string &lightName = linkIt->first;
		const ObjectList  &obList    = linkIt->second;

		ObjectList::const_iterator obIt;
		for (obIt = obList.begin(); obIt != obList.end(); ++obIt) {
			m_ignored_lights[lightName].insert(GetIDName((ID*)obIt->ptr.data));
		}
	}
}


void LightLinker::excludePlugin(BL::Object ob, const std::string &obName)
{
	LightLink::const_iterator linkIt;
	for (linkIt = m_exclude.begin(); linkIt != m_exclude.end(); ++linkIt) {
		const std::string &lightName = linkIt->first;
		const ObjectList  &obList    = linkIt->second;

		if (obList.count(ob)) {
			m_ignored_lights[lightName].insert(obName);
		}
	}
}


void LightLinker::write(PyObject *output)
{
	std::stringstream plugin;

	plugin << "\nSettingsLightLinker SettingsLightLinker {";
	plugin << "\n\t" << "ignored_lights=List(";
	LightIgnore::const_iterator linkIt;
	for (linkIt = m_ignored_lights.begin(); linkIt != m_ignored_lights.end(); ++linkIt) {
		const std::string &lightName   = linkIt->first;
		const StrSet      &obNamesList = linkIt->second;

		plugin << "List(" << lightName << "," << BOOST_FORMAT_LIST_JOIN(obNamesList) << ")";
		if (linkIt != --m_ignored_lights.end())
			plugin << ",";
	}

	plugin << ");";
	plugin << "\n}\n";
	
	PYTHON_PRINT(output, plugin.str().c_str());
}
