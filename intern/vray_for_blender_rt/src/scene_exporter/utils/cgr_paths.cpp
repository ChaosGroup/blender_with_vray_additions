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

// XXX: This will fix compilation fail on OS X:
//   error: 'std::toupper' cannot appear in a constant-expression
//   error: parse error in template argument list
//
// TODO: Find out how to compile with boost without such hack
//
#include <Python.h>

#include "cgr_paths.h"
#include "cgr_config.h"
#include "vfb_export_settings.h"

#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BKE_Global.h"
#include "BKE_library.h"
#include "BKE_Main.h"

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format/free_funcs.hpp>

namespace bfs {
using namespace boost::filesystem;
}

typedef bfs::path bpath;


std::string BlenderUtils::GetFullFilepath(const std::string &filepath, ID *holder)
{
	char absFilepath[FILE_MAX];
	BLI_strncpy(absFilepath, filepath.c_str(), FILE_MAX);

	BLI_path_abs(absFilepath, (holder ? (ID_BLEND_PATH(G.main, holder)) : G.main->name));

	// Convert UNC filepath "\\" to "/" on *nix
	// User then have to mount share "\\MyShare" to "/MyShare"
#ifndef _WIN32
	if (absFilepath[0] == '\\' && absFilepath[1] == '\\') {
		absFilepath[1] = '/';
		return absFilepath+1;
	}
#endif

	return absFilepath;
}


std::string BlenderUtils::CopyDRAsset(VRayForBlender::ExporterSettings &settings, const std::string &filepath)
{
	if (settings.settings_dr.share_path.empty()) {
		return filepath;
	}

	bpath srcFilepath(filepath);
	bpath fileName = srcFilepath.filename();

	if(!bfs::exists(srcFilepath)) {
		PRINT_ERROR("File \"%s\" doesn't exist!", srcFilepath.string().c_str());
		return filepath;
	}

	std::string fileExt = srcFilepath.extension().string();
	boost::algorithm::to_lower(fileExt);

	std::string assetSubdir = "textures";
	if(fileExt == "ies")
		assetSubdir = "ies";
	else if(fileExt == "vrmesh")
		assetSubdir = "proxy";
	else if(fileExt == "vrmap" || fileExt == "vrst" || fileExt == "vrsm")
		assetSubdir = "lightmaps";

	bpath drRoot(settings.settings_dr.share_path);

	bpath dstDirpath = drRoot / assetSubdir;

	if(NOT(bfs::exists(dstDirpath))) {
		try {
			bfs::create_directories(dstDirpath);
		}
		catch(const bfs::filesystem_error &ex) {
			PRINT_ERROR("Exception %s: Error creating directory path \"%s\"!",
						ex.what(), dstDirpath.string().c_str());
			return filepath;
		}
	}

	bpath dstFilepath = dstDirpath / fileName;

	bool needCopy = true;
#if 0
	if(bfs::exists(dstFilepath)) {
		if(bfs::file_size(srcFilepath) == bfs::file_size(dstFilepath)) {
			needCopy = false;
		}
	}
#endif
	if(needCopy) {
		PRINT_INFO_EX("Copying \"%s\" to \"%s\"...",
					  fileName.string().c_str(), dstDirpath.string().c_str());

		try {
			bfs::copy_file(srcFilepath, dstFilepath, bfs::copy_option::overwrite_if_exists);
		}
		catch(const bfs::filesystem_error &ex) {
			PRINT_ERROR("Exception %s: Error copying \"%s\" file to \"%s\"!",
						ex.what(), fileName.string().c_str(), dstDirpath.string().c_str());
		}
	}

	std::string finalFilepath = dstFilepath.string();

	if (settings.settings_dr.network_type == VRayForBlender::SettingsDR::NetworkTypeWindows &&
		settings.settings_dr.sharing_type == VRayForBlender::SettingsDR::SharingTypeShare) {
		
		char uncPrefix[2048] = {0,};
		snprintf(uncPrefix, 2048, "\\\\%s\\%s", settings.settings_dr.hostname.c_str(), settings.settings_dr.share_name.c_str());

		boost::replace_all(finalFilepath, settings.settings_dr.share_path, uncPrefix);
	}

	return finalFilepath;
}
