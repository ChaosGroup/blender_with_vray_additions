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

#include "cgr_json_plugins.h"

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


static void PrintTree(boost::property_tree::ptree &pt)
{
    if(pt.empty())
        return;

    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt) {
        std::cout << "\"" << v.first.data() << "\" : \"" << v.second.data() << "\"" << std::endl;
        PrintTree(v.second);
    }
}


int ReadPluginDesc(const char *pluginIDName)
{
    std::string jsonDirpath("/home/bdancer/devel/vray/vray_json/");

    std::string jsonFilepath(jsonDirpath);
    jsonFilepath.append(pluginIDName);
    jsonFilepath.append(".json");

    std::ifstream file(jsonFilepath.c_str());

    boost::property_tree::ptree pTree;
    boost::property_tree::json_parser::read_json(file, pTree);

    PrintTree(pTree);

    return 0;
}
