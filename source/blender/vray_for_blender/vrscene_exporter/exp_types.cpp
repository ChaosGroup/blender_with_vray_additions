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

#include "exp_types.h"


using namespace VRayScene;


StrSet           VRayExportable::m_expCache;
ExpCache         VRayExportable::m_frameCache;
int              VRayExportable::m_animation     = 0;
int              VRayExportable::m_checkAnimated = 0;
VRayPluginsDesc  VRayExportable::m_pluginDesc;

char             VRayExportable::m_interpStart[32];
char             VRayExportable::m_interpEnd[3];


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
		m_plugin << RNA_boolean_get(ptr, propName);
	}
	else if(propType == PROP_INT) {
		m_plugin << RNA_int_get(ptr, propName);
	}
	else if(propType == PROP_FLOAT) {
		if(RNA_property_array_check(prop)) {
			PropertySubType propSubType = RNA_property_subtype(prop);
			if(propSubType == PROP_COLOR) {
				if(RNA_property_array_length(ptr, prop) == 4) {
					float acolor[4];
					RNA_float_get_array(ptr, propName, acolor);
					m_plugin << "AColor(" << acolor[0] << "," << acolor[1] << "," << acolor[2] << "," << acolor[3] << ")";
				}
				else {
					float color[3];
					RNA_float_get_array(ptr, propName, color);
					m_plugin << "Color(" << color[0] << "," << color[1] << "," << color[2] << ")";
				}
			}
			else {
				float vector[3];
				RNA_float_get_array(ptr, propName, vector);
				m_plugin << "Vector(" << vector[0] << "," << vector[1] << "," << vector[2] << ")";
			}
		}
		else {
			m_plugin << RNA_float_get(ptr, propName);
		}
	}
	else if(propType == PROP_ENUM) {
		m_plugin << RNA_enum_get(ptr, propName);
	}
	else if(propType == PROP_STRING) {
		char value[FILE_MAX] = "";

		RNA_string_get(ptr, propName, value);

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


void VRayExportable::write(PyObject *output, int frame) {
	if(NOT(getHash()))
		return;

	if(NOT(m_animation) || (m_animation && m_checkAnimated == ANIM_CHECK_NONE)) {
		// TODO: Do this in animation mode also to prevent data reexport of dupli objects
		//
		if(m_expCache.find(m_name) != m_expCache.end())
			return;
		m_expCache.insert(m_name);

		initInterpolate(frame);
		writeData(output);
	}
	else {
		if(m_checkAnimated == ANIM_CHECK_SIMPLE) {
			if(checkUpdated(frame) && NOT(isUpdated()))
				return;

			initInterpolate(frame);
			writeData(output);
		}
		else if(m_checkAnimated == ANIM_CHECK_HASH || m_checkAnimated == ANIM_CHECK_BOTH) {
			if(m_checkAnimated == ANIM_CHECK_BOTH)
				if(checkUpdated(frame) && NOT(isUpdated()))
					return;

			MHash currHash = getHash();
			MHash prevHash = m_frameCache.getHash(m_name);

			if(currHash != prevHash) {
				int cacheFrame = m_frameCache.getFrame(m_name);
				int prevFrame  = frame - m_sce->r.frame_step;

				if(prevHash == 0) {
#if 0
					// prevHash 0 could mean that object have appeared at some frame of
					// animation; so we need to set some fake data for previous state
					// Let's say invisible at the first frame.
					if(frame > m_sce->r.sfra) {
						initInterpolate(m_sce->r.sfra);
						writeFakeData(output);
					}
#endif
				}
				else {
					// Write previous frame if hash is more then 'frame_step' back.
					if(cacheFrame < prevFrame) {
						initInterpolate(prevFrame);
						m_frameCache.getData(m_name)->writeData(output);
					}
				}

				initInterpolate(frame);
				writeData(output);

				m_frameCache.update(m_name, currHash, frame, this);
			}
		}
	}
}


void VRayExportable::writeAttributes(PointerRNA *ptr) {
	PropertyRNA *iterprop = RNA_struct_iterator_property(ptr->type);
	RNA_PROP_BEGIN(ptr, itemptr, iterprop) {
		PropertyRNA *prop = (PropertyRNA*)itemptr.data;

		writeAttribute(ptr, RNA_property_identifier(prop));
	}
	RNA_PROP_END;
}


void VRayExportable::writeAttributes(PointerRNA *ptr, boost::property_tree::ptree *pluginDesc, std::stringstream &output, const StrSet &skipAttrs)
{
	if(NOT(pluginDesc))
		return;

	PRINT_INFO("Processing attributes for: %s ", pluginDesc->get_child("ID").data().c_str());

	BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pluginDesc->get_child("Parameters")) {
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
