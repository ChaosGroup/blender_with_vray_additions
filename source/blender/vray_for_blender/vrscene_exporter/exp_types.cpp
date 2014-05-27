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


void VRayExportable::writeAttribute(PointerRNA *ptr, const char *propName, const char *rnaPropName)
{
	const char *rnaPropetryName = rnaPropName ? rnaPropName : propName;

	PropertyRNA *prop = RNA_struct_find_property(ptr, rnaPropetryName);
	if(NOT(prop))
		return;

	PropertyType propType = RNA_property_type(prop);

	m_plugin << "\n\t" << propName << "=" << m_interpStart;

	if(propType == PROP_BOOLEAN) {
		m_plugin << RNA_boolean_get(ptr, rnaPropetryName);
	}
	else if(propType == PROP_INT) {
		m_plugin << RNA_int_get(ptr, rnaPropetryName);
	}
	else if(propType == PROP_FLOAT) {
		if(RNA_property_array_check(prop)) {
			PropertySubType propSubType = RNA_property_subtype(prop);
			if(propSubType == PROP_COLOR) {
				if(RNA_property_array_length(ptr, prop) == 4) {
					float acolor[4];
					RNA_float_get_array(ptr, rnaPropetryName, acolor);
					m_plugin << "AColor(" << acolor[0] << "," << acolor[1] << "," << acolor[2] << "," << acolor[3] << ")";
				}
				else {
					float color[3];
					RNA_float_get_array(ptr, rnaPropetryName, color);
					m_plugin << "Color(" << color[0] << "," << color[1] << "," << color[2] << ")";
				}
			}
			else {
				float vector[3];
				RNA_float_get_array(ptr, rnaPropetryName, vector);
				m_plugin << "Vector(" << vector[0] << "," << vector[1] << "," << vector[2] << ")";
			}
		}
		else {
			m_plugin << RNA_float_get(ptr, rnaPropetryName);
		}
	}
	else if(propType == PROP_ENUM) {
		m_plugin << RNA_enum_get(ptr, rnaPropetryName);
	}
	else if(propType == PROP_STRING) {
		char value[FILE_MAX] = "";

		RNA_string_get(ptr, rnaPropetryName, value);

		PropertySubType propSubType = RNA_property_subtype(prop);
		if(propSubType == PROP_FILEPATH || propSubType == PROP_DIRPATH) {
			BLI_path_abs(value, G.main->name);
			m_plugin << "\"" << value << "\"";
		}
		else if(propSubType == PROP_FILENAME) {
			m_plugin << "\"" << value << "\"";
		}
		else {
			m_plugin << value;
		}
	}
	m_plugin << m_interpEnd << ";";
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

		if(attrType == "LIST"      ||
		   attrType == "COLOR"     ||
		   attrType == "VECTOR"    ||
		   attrType == "TRANSFORM")
			continue;

		PRINT_INFO("  Processing attribute: %s [%s]", attrName.c_str(), attrType.c_str());

		if(attrType == "STRING" || attrType == "MATERIAL" || attrType == "TEXTURE") {
			char value[FILE_MAX] = "";
			RNA_string_get(ptr, attrName.c_str(), value);

			if(strlen(value) == 0)
				continue;

			output << "\n\t" << attrName << "=" << m_interpStart;

			if(attrType == "MATERIAL") {
				output << "MA" << value;
			}
			else if(attrType == "TEXTURE") {
				output << "TE" << value;
			}
			else {
				if(v.second.count("subtype")) {
					std::string subType = v.second.get_child("subtype").data();
					if(subType == "FILE_PATH" || subType == "DIR_PATH") {
						BLI_path_abs(value, ID_BLEND_PATH(G.main, ((ID*)ptr->id.data)));
					}
				}
				output << "\"" << value << "\"";
			}

			output << m_interpEnd << ";";
		}
		else {
			output << "\n\t" << attrName << "=" << m_interpStart;

			if(attrType == "BOOL") {
				output << RNA_boolean_get(ptr, attrName.c_str());
			}
			else if(attrType == "INT") {
				output << RNA_int_get(ptr, attrName.c_str());
			}
			else if(attrType == "FLOAT") {
				output << RNA_float_get(ptr, attrName.c_str());
			}

			output << m_interpEnd << ";";
		}
	}
}
