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

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "cgr_paths.h"
#include "exp_types.h"

#include <BLI_string.h>
#include <BLI_path_util.h>


#define bfs   boost::filesystem
#define bpath bfs::path


std::string BlenderUtils::GetFullFilepath(const std::string &filepath, ID *holder)
{
	char absFilepath[FILE_MAX];
	BLI_strncpy(absFilepath, filepath.c_str(), FILE_MAX);

	BLI_path_abs(absFilepath, ID_BLEND_PATH_EX(holder));

	return absFilepath;
}


std::string BlenderUtils::CopyDRAsset(const std::string &filepath)
{
	if(VRayScene::VRayExportable::m_set->m_drSharePath.empty())
		return filepath;

	bpath srcFilepath(filepath);
	bpath fileName = srcFilepath.filename();

	if(NOT(bfs::exists(srcFilepath))) {
		PRINT_ERROR("File \"%s\" doesn't exist!",
					srcFilepath.c_str());
		return filepath;
	}

	std::string fileExt = srcFilepath.extension().string();
	boost::algorithm::to_lower(fileExt);

	std::string assetSubdir = "textures";
	if(fileExt == "ies")
		assetSubdir = "ies";
	else if(fileExt == "vrmesh")
		assetSubdir = "proxy";
	else if(fileExt == "vrmap")
		assetSubdir = "lightmaps";

	bpath drRoot(VRayScene::VRayExportable::m_set->m_drSharePath);

	bpath dstDirpath = drRoot / assetSubdir;

	if(NOT(bfs::exists(dstDirpath))) {
		try {
			bfs::create_directories(dstDirpath);
		}
		catch(const bfs::filesystem_error &ex)
		{
			PRINT_ERROR("Exception %s: Error creating directory path: %s!",
						ex.what(), dstDirpath.c_str());
			return filepath;
		}
	}

	bpath dstFilepath = dstDirpath / fileName;

	bool needCopy = true;

	if(bfs::exists(dstFilepath)) {
		if(bfs::file_size(srcFilepath) == bfs::file_size(dstFilepath)) {
			needCopy = false;
		}
	}

	if(needCopy) {
		PRINT_INFO_EX("Copying \"%s\" to \"%s\"...",
					  fileName.c_str(), dstDirpath.c_str());

		try {
			bfs::copy_file(srcFilepath, dstFilepath, bfs::copy_option::overwrite_if_exists);
		}
		catch(const bfs::filesystem_error &ex)
		{
			PRINT_ERROR("Exception %s: Error copying \"%s\" file to \"%s\"!",
						ex.what(), fileName.c_str(), dstDirpath.c_str());
			return filepath;
		}
	}

	return dstFilepath.string();
}
