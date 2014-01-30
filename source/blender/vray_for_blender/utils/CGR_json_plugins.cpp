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
#include "CGR_json_plugins.h"

#include "DNA_space_types.h"
#include "BLI_path_util.h"
#include "BLI_fileops_types.h"
#include "MEM_guardedalloc.h"

#include "../editors/space_file/filelist.h"


void PrintTree(boost::property_tree::ptree &pt)
{
    if(pt.empty())
        return;

    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt) {
        std::cout << "\"" << v.first.data() << "\" : \"" << v.second.data() << "\"" << std::endl;
        PrintTree(v.second);
    }
}


void VRayPluginsDesc::init(const std::string &dirPath)
{
	FileList *files = filelist_new(FILE_UNIX);

	filelist_setdir(files, dirPath.c_str());
	filelist_readdir(files);

	int nFiles = filelist_numfiles(files);

	for(int i = 0; i < nFiles; ++i) {
		struct direntry *file = filelist_file(files, i);
		if(NOT(file && (S_ISREG(file->type))))
			continue;

		std::string fileName(BLI_path_basename(file->path));
		fileName.erase(fileName.find_last_of("."), std::string::npos);

		boost::property_tree::ptree *pTree = new boost::property_tree::ptree();

		std::ifstream fileStream(file->path);
		boost::property_tree::json_parser::read_json(fileStream, *pTree);

		m_desc[fileName] = pTree;
	}

	filelist_free(files);
	MEM_freeN(files);
}


void VRayPluginsDesc::freeData()
{
	for(PluginDesc::iterator it = m_desc.begin(); it != m_desc.end(); ++it)
		delete it->second;
	m_desc.clear();
}


boost::property_tree::ptree *VRayPluginsDesc::getTree(const std::string &name)
{
	if(NOT(m_desc.count(name)))
		return NULL;
	return m_desc[name];
}
