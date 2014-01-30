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

#ifndef CGR_UTILS_RNA_H
#define CGR_UTILS_RNA_H

#include "RNA_access.h"
#include "CGR_json_plugins.h"

#include <string>
#include <sstream>


namespace RnaAccess {

class RnaValue {
public:
	RnaValue(ID *id, const char *rnaPointerPath);

	int          GetValue(const char *propName, int   &value);
	int          GetValue(const char *propName, bool  &value);
	int          GetValue(const char *propName, float &value);
	int          GetValue(const char *propName, char  *value);
	int          GetValue(const char *propName, float  value[3]);

	int          getInt(const char *propName);
	int          getEnum(const char *propName);
	float        getFloat(const char *propName);
	int          getBool(const char *propName);
	std::string  getString(const char *propName);
	void         getChar(const char *propName, char *buf);
	std::string  getPath(const char *propName);

	int          hasProperty(const char *propName);

	void         writePlugin(boost::property_tree::ptree *pluginDesc, std::stringstream &ss);

private:
	int          checkProperty(const char *propName);

	std::string  m_path;
	PointerRNA   m_pointer;

};

}

#endif // CGR_UTILS_RNA_H
