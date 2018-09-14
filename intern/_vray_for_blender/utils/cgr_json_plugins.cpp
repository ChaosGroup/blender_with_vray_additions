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

#include <boost/filesystem.hpp>

#include "cgr_config.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "cgr_json_plugins.h"

#include "DNA_space_types.h"
#include "BLI_path_util.h"
#include "BLI_fileops_types.h"
#include "BLI_string.h"
#include "PIL_time.h"
#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_timecode.h"
}

#include "../editors/space_file/filelist.h"


static void PrintTree(PluginJson &pt)
{
    if(pt.empty())
        return;

	BOOST_FOREACH(PluginJson::value_type &v, pt) {
        std::cout << "\"" << v.first.data() << "\" : \"" << v.second.data() << "\"" << std::endl;
        PrintTree(v.second);
    }
}


void VRayPluginsDesc::init(const std::string &dirPath)
{
	double timeMeasure = 0.0;
	char   timeMeasureBuf[32];

	PRINT_INFO_LB("Parsing plugin descriptions...");
	timeMeasure = PIL_check_seconds_timer();

	boost::filesystem::recursive_directory_iterator pIt(dirPath);
	boost::filesystem::recursive_directory_iterator end;

	for (; pIt != end; ++pIt) {
		const boost::filesystem::path &path = *pIt;
		if (path.extension() == ".json") {
#ifdef _WIN32
			const std::string &fileName = path.stem().string();
#else
			const std::string &fileName = path.stem().c_str();
#endif

			PluginJson &pTree = m_desc[fileName];

			std::ifstream fileStream(path.c_str());
			try {
				boost::property_tree::json_parser::read_json(fileStream, pTree);
			}
			catch (...) {
				PRINT_ERROR("Parsing error of \"%s\"!", path.c_str())
			}
		}
	}

	BLI_timecode_string_from_time_simple(timeMeasureBuf, sizeof(timeMeasureBuf), PIL_check_seconds_timer()-timeMeasure);
	printf(" done [%s]\n", timeMeasureBuf);
}


void VRayPluginsDesc::freeData()
{
	m_desc.clear();
}


PluginJson* VRayPluginsDesc::getTree(const std::string &name)
{
	if(NOT(m_desc.count(name)))
		return NULL;
	return &m_desc[name];
}
