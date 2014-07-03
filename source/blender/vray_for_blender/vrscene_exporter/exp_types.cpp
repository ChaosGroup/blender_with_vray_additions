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

#include "exp_types.h"


using namespace VRayScene;


StrSet           VRayExportable::m_exportNameCache;
ExpCache         VRayExportable::m_frameCache;
VRayPluginsDesc  VRayExportable::m_pluginDesc;

char             VRayExportable::m_interpStart[32] = "";
char             VRayExportable::m_interpEnd[3]    = "";

ExpoterSettings *VRayExportable::m_set = NULL;


VRayExportable::VRayExportable() {
	m_name = "";
	m_hash = 0;

	m_sce  = NULL;
	m_main = NULL;
	m_ob   = NULL;

	m_propGroup = NULL;

	initInterpolate(0);
}


VRayExportable::VRayExportable(Scene *scene, Main *main, Object *ob) {
	m_name = "";
	m_hash = 0;

	m_sce  = scene;
	m_main = main;
	m_ob   = ob;

	m_propGroup = NULL;

	initInterpolate(0);
}


VRayExportable::~VRayExportable() {}


std::string VRayExportable::getAttributeValue(PointerRNA *ptr, PropertyRNA *prop, const char *propName)
{
	PropertyType propType = RNA_property_type(prop);

	if(propType == PROP_BOOLEAN) {
		return BOOST_FORMAT_BOOL(RNA_boolean_get(ptr, propName));
	}
	else if(propType == PROP_INT) {
		return BOOST_FORMAT_INT(RNA_int_get(ptr, propName));
	}
	else if(propType == PROP_FLOAT) {
		if(RNA_property_array_check(prop)) {
			PropertySubType propSubType = RNA_property_subtype(prop);
			if(propSubType == PROP_COLOR) {
				if(RNA_property_array_length(ptr, prop) == 4) {
					float acolor[4];
					RNA_float_get_array(ptr, propName, acolor);
					return BOOST_FORMAT_ACOLOR(acolor);
				}
				else {
					float color[3];
					RNA_float_get_array(ptr, propName, color);
					return BOOST_FORMAT_COLOR(color);
				}
			}
			else {
				float vector[3];
				RNA_float_get_array(ptr, propName, vector);
				return BOOST_FORMAT_VECTOR(vector);
			}
		}
		else {
			return BOOST_FORMAT_FLOAT(RNA_float_get(ptr, propName));
		}
	}
	else if(propType == PROP_ENUM) {
		return BOOST_FORMAT_INT(RNA_enum_get(ptr, propName));
	}
	else if(propType == PROP_STRING) {
		char value[FILE_MAX] = "";

		RNA_string_get(ptr, propName, value);

		PropertySubType propSubType = RNA_property_subtype(prop);
		if(propSubType == PROP_FILEPATH || propSubType == PROP_DIRPATH) {
			BLI_path_abs(value, G.main->name);
			return BOOST_FORMAT_STRING(value);
		}
		else if(propSubType == PROP_FILENAME) {
			return BOOST_FORMAT_STRING(value);
		}
		else {
			return BOOST_FORMAT_STRING(value);
		}
	}

	return "NULL";
}

void VRayExportable::writeAttribute(PointerRNA *ptr, const char *propName, const char *rnaPropName)
{
	const char *rnaPropetryName = rnaPropName ? rnaPropName : propName;

	PropertyRNA *prop = RNA_struct_find_property(ptr, rnaPropetryName);
	if(NOT(prop))
		return;

	m_plugin << "\n\t" << propName << "=" << m_interpStart << getAttributeValue(ptr, prop, rnaPropetryName) << m_interpEnd << ";";
}


int VRayExportable::write(PyObject *output, int frame) {
	if(NOT(getHash()))
		return 1;

	// Allows to skip already exported data,
	// useful when using dupli.
	//
	if(m_exportNameCache.find(m_name) != m_exportNameCache.end())
		return 1;
	m_exportNameCache.insert(m_name);

	if(NOT(m_set->m_isAnimation)) {
		writeData(output, NULL);
	}
	else {
		if(NOT(m_set->DoUpdateCheck())) {
			writeData(output, NULL);
		}
		else {
			MHash currentHash = getHash();

			if(frame == m_set->m_frameCurrent) {
				initInterpolate(frame);
				writeData(output, NULL);
				m_frameCache.update(m_name, currentHash, frame, this);
				return 0;
			}
			else {
				if(NOT(isUpdated()))
					return 1;

				MHash           prevHash  = m_frameCache.getHash(m_name);
				VRayExportable *prevState = m_frameCache.getData(m_name);

				if(currentHash == prevHash) {
					return 1;
				}
				else {
					int cacheFrame = m_frameCache.getFrame(m_name);
					int prevFrame  = frame - m_set->m_frameStep;

					int needKeyFrame = cacheFrame < prevFrame;

					initInterpolate(frame);
					writeData(output, prevState, needKeyFrame);

					m_frameCache.update(m_name, currentHash, frame, this);

					return 0;
				}
			}
		}
	}

	return 1;
}


void VRayExportable::writeAttributes(PointerRNA *ptr) {
	PropertyRNA *iterprop = RNA_struct_iterator_property(ptr->type);
	RNA_PROP_BEGIN(ptr, itemptr, iterprop) {
		PropertyRNA *prop = (PropertyRNA*)itemptr.data;

		writeAttribute(ptr, RNA_property_identifier(prop));
	}
	RNA_PROP_END;
}


// This function is used only to export simple attribute values
//
void VRayExportable::writeAttributes(PointerRNA *ptr, PluginJson *pluginDesc, std::stringstream &output, const StrSet &skipAttrs)
{
	if(NOT(pluginDesc))
		return;

	PRINT_INFO("Processing attributes for: %s ", pluginDesc->get_child("ID").data().c_str());

	BOOST_FOREACH(PluginJson::value_type &v, pluginDesc->get_child("Parameters")) {
		std::string attrName = v.second.get_child("attr").data();
		std::string attrType = v.second.get_child("type").data();

		if(v.second.count("skip"))
			if(v.second.get<bool>("skip"))
				continue;

		if(skipAttrs.size())
			if(skipAttrs.count(attrName))
				continue;

		PropertyRNA *prop = RNA_struct_find_property(ptr, attrName.c_str());
		if(NOT(prop))
			continue;

		if(MAPPABLE_TYPE(attrType))
			continue;

		PRINT_INFO("  Processing attribute: %s [%s]", attrName.c_str(), attrType.c_str());

		output << "\n\t" << attrName << "=" << m_interpStart << getAttributeValue(ptr, prop, attrName.c_str()) << m_interpEnd << ";";
	}
}
