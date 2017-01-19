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
#include "cgr_string.h"

using namespace VRayScene;

boost::format vrsceneStrFmt("%s");
boost::format vrsceneQuotedStrFmt("\"%s\"");
boost::format vrsceneFloatFmt("%.6g");
boost::format vrsceneTmFmt("TransformHex(\"%s\")");
boost::format vrsceneIntFmt("%i");
boost::format vrsceneUIntFmt("%u");
boost::format vrsceneColorFmt("Color(%.6g,%.6g,%.6g)");
boost::format vrsceneAColorFmt("AColor(%.6g,%.6g,%.6g,%.6g)");
boost::format vrsceneAColorNoAlphaFmt("AColor(%.6g,%.6g,%.6g,1.0)");
boost::format vrsceneVectorFmt("Vector(%.6g,%.6g,%.6g)");
boost::format vrsceneAsciiMatrixFmt("Matrix(Vector(%.6g,%.6g,%.6g),Vector(%.6g,%.6g,%.6g),Vector(%.6g,%.6g,%.6g))");

StrSet           VRayExportable::m_exportNameCache;
ExpCache         VRayExportable::m_frameCache;
VRayPluginsDesc  VRayExportable::m_pluginDesc;

char             VRayExportable::m_interpStart[32] = "";
char             VRayExportable::m_interpEnd[3]    = "";


VRayExportable::VRayExportable():
	m_bl_ob(PointerRNA_NULL)
{
	m_name = "";
	m_hash = 0;

	m_sce  = NULL;
	m_main = NULL;
	m_ob   = NULL;

	m_propGroup = NULL;

	initInterpolate(0);
}


VRayExportable::VRayExportable(Scene *scene, Main *main, Object *ob):
	m_bl_ob(PointerRNA_NULL)
{
	m_name = "";
	m_hash = 0;

	m_sce  = scene;
	m_main = main;
	m_ob   = ob;

	m_propGroup = NULL;

	PointerRNA objectRNA;
	RNA_id_pointer_create((ID*)ob, &objectRNA);
	m_bl_ob = BL::Object(objectRNA);

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


int VRayExportable::write(PyObject *output, float frame)
{
	VRayScene::WriteFlag exitCode = VRayScene::eFreeData;

	if(NOT(getHash()))
		return exitCode;

	if (ExporterSettings::gSet.m_anim_check_cache) {
		// Allows to skip already exported data,
		// useful when using dupli.
		//
		if(m_exportNameCache.find(m_name) != m_exportNameCache.end())
			return exitCode;
		m_exportNameCache.insert(m_name);
	}

	if(NOT(ExporterSettings::gSet.m_isAnimation)) {
		writeData(output, NULL);
	}
	else {
		MHash currentHash = getHash();

		if(frame == ExporterSettings::gSet.m_frameStart) {
			initInterpolate(frame);
			writeData(output, NULL);

			m_frameCache.update(m_name, currentHash, frame, this);

			exitCode = VRayScene::eKeedData;
		}
		else {
			if(isUpdated()) {
				MHash           prevHash  = m_frameCache.getHash(m_name);
				VRayExportable *prevState = m_frameCache.getData(m_name);

				if(currentHash != prevHash) {
					// Cached frame
					const float cacheFrame = m_frameCache.getFrame(m_name);

					// We need a keyframe if stored value is more then "frame step"
					// behind the current value
					const int needKeyFrame = cacheFrame < (frame - ExporterSettings::gSet.m_frameStep);

					initInterpolate(frame);
					writeData(output, prevState, needKeyFrame);

					m_frameCache.update(m_name, currentHash, frame, this);

					exitCode = VRayScene::eKeedData;
				}
			}
		}
	}

	return exitCode;
}


std::string VRayExportable::GetHairName(BL::Object ob, BL::ParticleSystem psys, const NodeAttrs &attrs)
{
	std::string name = attrs.namePrefix + "HAIR";
	name.append(psys.name());
	name.append(psys.settings().name());

	return StripString(name);
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


char* GetExportString(const u_int8_t *buf, unsigned bufLen)
{
	if (ExporterSettings::gSet.m_dataExportFormat == ExporterSettings::FormatZIP) {
		return GetStringZip(buf, bufLen);
	}
	return GetHex(buf, bufLen);
}
