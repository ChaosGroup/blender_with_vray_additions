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


void LightLinker::getObject(const std::string &name, StrSet &list)
{
	BL::Scene::objects_iterator obIt;
	for (m_sce.objects.begin(obIt); obIt != m_sce.objects.end(); ++obIt) {
		BL::Object ob = *obIt;
		if (ob.name() == name) {
			list.insert(GetIDName(ob));
			break;
		}
	}
}


void LightLinker::getGroupObjects(const std::string &name, StrSet &list)
{
	BL::BlendData::groups_iterator grIt;
	for (m_data.groups.begin(grIt); grIt != m_data.groups.end(); ++grIt) {
		BL::Group gr = *grIt;
		if (gr.name() == name) {
			BL::Group::objects_iterator grObIt;
			for (gr.objects.begin(grObIt); grObIt != gr.objects.end(); ++grObIt) {
				BL::Object ob = *grObIt;
				list.insert(GetIDName(ob));
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

		int includeExclude   = RNA_enum_get(&ptr, "include_exclude");
		int includeIllumShad = RNA_enum_get(&ptr, "illumination_shadow");

		if (includeExclude == 0)
			continue;

		const std::string &lightName = GetIDName((ID*)ob.ptr.data, "LA");

		m_include_exclude[lightName].obListType = includeExclude;
		m_include_exclude[lightName].flags      = includeIllumShad;

		char buf_object[MAX_ID_NAME];
		RNA_string_get(&ptr, "exclude_objects", buf_object);
		std::string obName = buf_object;
		if(NOT(obName.empty())) {
			getObject(obName, m_include_exclude[lightName].obList);
		}

		char buf_group[MAX_ID_NAME];
		RNA_string_get(&ptr, "exclude_groups", buf_group);
		std::string groupName = buf_group;
		if(NOT(groupName.empty())) {
			getGroupObjects(groupName, m_include_exclude[lightName].obList);
		}
	}
}


void LightLinker::excludePlugin(const std::string &refObName, const std::string &pluginName)
{
	LightLink::iterator linkIt;
	for (linkIt = m_include_exclude.begin(); linkIt != m_include_exclude.end(); ++linkIt) {
		StrSet &obList = linkIt->second.obList;

		if (obList.count(refObName)) {
			obList.insert(pluginName);
		}
	}
}


void LightLinker::write(PyObject *output)
{
	std::stringstream plugin;

	StrSet::const_iterator nameIt;
	for (nameIt = m_scene_nodes->begin(); nameIt != m_scene_nodes->end(); ++nameIt) {
		std::cout << "Scene plugin " << *nameIt << std::endl;
	}

	LightLink::const_iterator incExclIt;
	for (incExclIt = m_include_exclude.begin(); incExclIt != m_include_exclude.end(); ++incExclIt) {
		const std::string &lightName = incExclIt->first;
		const LightList   &lightList = incExclIt->second;

		const StrSet &obList = lightList.obList;

		if (lightList.obListType == 1) {
			std::copy(obList.begin(), obList.end(),
					  std::inserter(m_ignored_lights[lightName], m_ignored_lights[lightName].begin()));
		}
		else if (lightList.obListType == 2) {
			boost::set_difference(*m_scene_nodes, obList,
								  std::inserter(m_ignored_lights[lightName], m_ignored_lights[lightName].begin()));
		}
	}

	plugin << "\nSettingsLightLinker SettingsLightLinker {";
	plugin << "\n\t" << "ignored_lights=List(";
	LightIgnore::const_iterator linkIt;
	for (linkIt = m_ignored_lights.begin(); linkIt != m_ignored_lights.end(); ++linkIt) {
		const std::string &lightName   = linkIt->first;
		const StrSet      &obNamesList = linkIt->second;

		if (NOT(obNamesList.size()))
			continue;

		plugin << "List(" << lightName << "," << BOOST_FORMAT_LIST_JOIN(obNamesList) << ")";
		if (linkIt != --m_ignored_lights.end())
			plugin << ",";
	}

	plugin << ");";
	plugin << "\n}\n";
	
	PYTHON_PRINT(output, plugin.str().c_str());
}
