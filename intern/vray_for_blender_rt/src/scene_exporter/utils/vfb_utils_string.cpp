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

#include "BLI_path_util.h"

#include <Python.h>


std::string VRayForBlender::String::GetUniqueName(StrSet &namesSet, const std::string &name)
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


std::string VRayForBlender::String::ExpandFilenameVariables(
	const std::string & expr,
	const std::string & camera,
	const std::string & scene,
	const std::string & blendPath,
	const std::string & ext)
{
	std::string timeReplace = doPythonTimeReplace(expr);
	std::string result = "";

	for (int c = 0; c < timeReplace.length(); ++c) {
		if (timeReplace[c] == '$' && c + 1 < timeReplace.length()) {
			char type = timeReplace[++c];
			switch (type) {
			case 'C':
				result.append(camera);
				break;
			case 'S':
				result.append(scene);
				break;
			case 'F':
				if (blendPath == "") {
					result.append("default");
				} else {
					// basename(blendPath) + remove extension
					const auto nameStart = blendPath.find_last_of("/\\");
					const auto lastDot = blendPath.find_last_of(".");
					std::string name;
					if (nameStart != std::string::npos) {
						name = blendPath.substr(nameStart + 1, lastDot - nameStart - 1);
					} else {
						name = blendPath.substr(0, lastDot);
					}
					result.append(name);
				}
				break;
			default:
				result.push_back('_');
				result.push_back(type);
				PRINT_WARN("Unknown format variable \"$%c\" in img_file", type);
			}
		} else {
			result.push_back(timeReplace[c]);
		}
	}
	if (ext != "") {
		result.push_back('.');
		return result + ext;
	} else {
		return result;
	}
}


std::string VRayForBlender::String::AbsFilePath(const std::string & path, const std::string & blendPath)
{
	char result[FILE_MAX];
	strcpy(result, blendPath.c_str());

	if (BLI_path_abs(result, blendPath.c_str())) {
		return result;
	}

	return path;
}
