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

#ifndef CGR_JSON_PLUGINS_H
#define CGR_JSON_PLUGINS_H

#include <Python.h>

#ifdef _MSC_VER
#include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

#include <cassert>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <map>

typedef boost::property_tree::ptree         PluginJson;
typedef std::map<std::string, PluginJson*>  PluginDesc;


void  PrintTree(PluginJson &pt);

class VRayPluginsDesc {
public:
	void        init(const std::string &dirPath);
	void        freeData();

	PluginJson *getTree(const std::string &name);

private:
	PluginDesc  m_desc;

};

#endif // CGR_JSON_PLUGINS_H
