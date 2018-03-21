/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vfb_utils_string.h"
#include "cgr_config.h"
#include "vfb_utils_blender.h"

#include "BLI_string.h"
#include "BLI_path_util.h"
#include "DNA_ID.h"
#include "BKE_main.h"
#include "BKE_global.h"

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <Python.h>


std::string VRayForBlender::String::GetUniqueName(HashSet<std::string> &namesSet, const std::string &name)
{
	std::string uniqueName(name);

	int uniqueSuffix = 0;
	while (namesSet.count(uniqueName)) {
		uniqueSuffix++;
		char nameBuff[String::MAX_PLG_LEN] = {0, };
		snprintf(nameBuff, sizeof(nameBuff), "%s.%03i", name.c_str(), uniqueSuffix);
		uniqueName = nameBuff;
	}

	namesSet.insert(uniqueName);

	return uniqueName;
}


std::string VRayForBlender::String::StripString(std::string &str)
{
	for (int i = 0; i < str.length(); ++i) {
		const char &c = str[i];

		// Valid characters
		if ((c >= 'A' && c <= 'Z') ||
		    (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') ||
		    (c == '|')             ||
		    (c == '@'))
		{
			continue;
		}

		if(c == '+') {
			str[i] = 'p';
		}
		else if(c == '-') {
			str[i] = 'm';
		}
		else {
			// Replace illigal chars with "_"
			str[i] = '_';
		}
	}

	return str;
}

namespace {
/// Apply datetime's strftime to expression:
/// import datetime
/// t = datetime.datetime.now()
/// replaced = t.strftime(expr)
std::string doPythonTimeReplace(const std::string & expr)
{
	return expr; // TODO: crash on 2nd call when freeing timeModuleDict
	std::string replaced = expr;
	using namespace VRayForBlender::Blender;
	auto timeModule = toPyPTR(PyImport_ImportModule("datetime"));
	auto timeModuleDict = toPyPTR(PyModule_GetDict(timeModule.get()));
	auto dtObject = toPyPTR(PyDict_GetItemString(timeModuleDict.get(), "datetime"));

	if (!timeModule || !timeModuleDict || !dtObject) {
		return replaced;
	}

	auto nowOb = toPyPTR(PyObject_CallMethod(dtObject.get(), "now", nullptr));
	auto replacedStr = toPyPTR(PyObject_CallMethod(nowOb.get(), "strftime", "s", expr.c_str()));
	if (PyUnicode_Check(replacedStr.get())) {
		auto byteRepr = toPyPTR(PyUnicode_AsEncodedString(replacedStr.get(), "ASCII", "strict"));
		if (byteRepr) {
			const char * resStr = PyBytes_AsString(byteRepr.get());
			if (resStr) {
				replaced = resStr;
			}
		}
	}
	return replaced;
}

}


std::string VRayForBlender::String::ExpandFilenameVariables(const std::string & expr, BL::Context & context)
{
	namespace fs = boost::filesystem;
	namespace alg = boost::algorithm;

	const std::string & blendPath = context.blend_data().filepath();

	std::string expandedPath;
	if (expr.find("//") == 0) {
		fs::path prefixPath;
		if (blendPath.empty()) {
			prefixPath = fs::temp_directory_path();
		} else {
			prefixPath = fs::path(blendPath).parent_path();
		}

		expandedPath = (prefixPath / fs::path(expr.substr(2))).string();
	} else {
		expandedPath = expr;
	}

	alg::replace_all(expandedPath, "$F", fs::path(blendPath).filename().string());
	alg::replace_all(expandedPath, "$C", context.scene().camera().name());
	alg::replace_all(expandedPath, "$S", context.scene().name());

	return expandedPath;
}


std::string VRayForBlender::String::AbsFilePath(const std::string & path, const std::string & blendPath)
{
	if (path.size() >= 2 && path[0] == '/' && path[1] == '/') {
		boost::filesystem::path base = blendPath;
		base.remove_filename();
		base /= path.substr(2);
		base.remove_trailing_separator();
		base.normalize();
		return base.string();
	} else {
		return path;
	}
}

std::string VRayForBlender::String::GetFullFilepath(const std::string &filepath, ID *holder)
{
	char absFilepath[FILE_MAX];
	BLI_strncpy(absFilepath, filepath.c_str(), FILE_MAX);

	BLI_path_abs(absFilepath, holder ? ID_BLEND_PATH(G.main, holder) : G.main->name);

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

std::vector<std::string> VRayForBlender::String::SplitString(const std::string & input, const std::string & delimiters, bool preserveEmpty)
{
	std::vector<std::string> parts;

	size_t current = 0, next = -1;
	do {
		current = next + 1;
		next = input.find_first_of(delimiters);
		if (preserveEmpty && current == next) {
			parts.push_back("");
		} else {
			parts.push_back(input.substr(current, next - current));
		}
	} while (next != std::string::npos);

	return parts;
}
