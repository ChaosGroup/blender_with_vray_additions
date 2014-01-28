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

#include "CGR_config.h"
#include "CGR_rna.h"

#include "DNA_ID.h"
#include "BLI_path_util.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>


using namespace RnaAccess;


typedef std::vector<std::string> PathRNA;


RnaValue::RnaValue(ID *id, const char *rnaPointerPath, const char *pluginID)
{
    m_path = rnaPointerPath;

    PathRNA rnaPath;
    boost::split(rnaPath, m_path, boost::is_any_of("."));

    DEBUG_PRINT(0, "Initing RnaValue for path = %s", rnaPointerPath);

    RNA_id_pointer_create(id, &m_pointer);

    size_t nTokens = rnaPath.size();
    for(size_t t = 0; t < nTokens; ++t) {
        if(NOT(RNA_struct_find_property(&m_pointer, rnaPath[t].c_str()))) {
            m_pointer = PointerRNA_NULL;
            break;
        }
        m_pointer = RNA_pointer_get(&m_pointer, rnaPath[t].c_str());
    }

	if(pluginID)
		ReadPluginDesc(pluginID, m_pluginDesc);
}


int RnaValue::checkProperty(const char *propName)
{
    if(m_pointer.data == NULL) {
        PRINT_ERROR("Property pointer not found!");
        return 1;
    }

    if(NOT(RNA_struct_find_property(&m_pointer, propName))) {
        PRINT_ERROR("Property "COLOR_YELLOW"%s"COLOR_DEFAULT" not found!", propName);
        return 2;
    }

    return 0;
}


int RnaValue::GetValue(const char *propName, int &value)
{
	if(checkProperty(propName)) {
        return 1;
    }

    value = RNA_int_get(&m_pointer, propName);

    DEBUG_PRINT(0,
                COLOR_BLUE"%s"COLOR_DEFAULT"."COLOR_GREEN"%s"COLOR_DEFAULT" = "COLOR_YELLOW"%i"COLOR_DEFAULT,
                m_path.c_str(), propName, value);

    return 0;
}


int RnaValue::GetValue(const char *propName, bool &value)
{
	if(checkProperty(propName)) {
        return 1;
    }

    value = RNA_boolean_get(&m_pointer, propName);

    DEBUG_PRINT(0,
                COLOR_BLUE"%s"COLOR_DEFAULT"."COLOR_GREEN"%s"COLOR_DEFAULT" = "COLOR_YELLOW"%s"COLOR_DEFAULT,
                m_path.c_str(), propName, value ? "True" : "False");

    return 0;
}


int RnaValue::GetValue(const char *propName, float &value)
{
	if(checkProperty(propName)) {
        return 1;
    }

    value = RNA_float_get(&m_pointer, propName);

    DEBUG_PRINT(0,
                COLOR_BLUE"%s"COLOR_DEFAULT"."COLOR_GREEN"%s"COLOR_DEFAULT" = "COLOR_YELLOW"%.3f"COLOR_DEFAULT,
                m_path.c_str(), propName, value);

    return 0;
}


// Usage:
//   char value[MAX_ID_NAME - 2];
//
int RnaValue::GetValue(const char *propName, char *value)
{
	if(checkProperty(propName)) {
        return 1;
    }

    RNA_string_get(&m_pointer, propName, value);

    DEBUG_PRINT(0,
                COLOR_BLUE"%s"COLOR_DEFAULT"."COLOR_GREEN"%s"COLOR_DEFAULT" = "COLOR_YELLOW"%s"COLOR_DEFAULT,
                m_path.c_str(), propName, value);

    return 0;
}


int RnaValue::GetValue(const char *propName, float value[])
{
	if(checkProperty(propName)) {
        return 1;
    }

    RNA_float_get_array(&m_pointer, propName, value);

	return 0;
}


int RnaValue::getInt(const char *propName)
{
	if(checkProperty(propName))
		return 0;
	return RNA_int_get(&m_pointer, propName);
}


int RnaValue::getEnum(const char *propName)
{
	if(checkProperty(propName))
		return 0;
	return RNA_enum_get(&m_pointer, propName);
}


float RnaValue::getFloat(const char *propName)
{
	if(checkProperty(propName))
		return 0.0f;
	return RNA_float_get(&m_pointer, propName);
}


int RnaValue::getBool(const char *propName)
{
	if(checkProperty(propName))
		return 0;
	return RNA_boolean_get(&m_pointer, propName);
}


std::string RnaValue::getString(const char *propName)
{
	if(checkProperty(propName))
		return "";

	char value[MAX_ID_NAME];
	RNA_string_get(&m_pointer, propName, value);

	return std::string(value);
}

void RnaValue::getChar(const char *propName, char *buf)
{
	if(checkProperty(propName))
		return;
	RNA_string_get(&m_pointer, propName, buf);
}


std::string RnaValue::getPath(const char *propName)
{
	if(checkProperty(propName))
		return "";

	char resultPath[FILE_MAX] = "";
	char buf[FILE_MAX] = "";

	RNA_string_get(&m_pointer, propName, buf);

	BLI_path_abs(resultPath, buf);

	return std::string(resultPath);
}
