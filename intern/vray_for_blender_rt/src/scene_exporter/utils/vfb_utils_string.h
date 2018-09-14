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


#ifndef VRAY_FOR_BLENDER_UTILS_STRING_H
#define VRAY_FOR_BLENDER_UTILS_STRING_H

#include "vfb_typedefs.h"


namespace VRayForBlender {
namespace String {

/// Expected max length of a name for a plugin we can generate
const int MAX_PLG_LEN = 256;

std::string GetUniqueName(HashSet<std::string> &namesSet, const std::string &name);

/// Replace all invalid chars in @str
/// @param str - the in/out param
void StripString(char *str);

/// Replace all invalid chars in @str, but ignores any found in @ignore
/// @param str - the in/out param
/// @param ignore - null terminated string containing all characters to skip replacing
/// NOTE: instead of passing nullptr or empty string - call the overload
void StripString(char *str, const char * ignore);


/// Replace all invalid chars in @str and return copy
/// @param str - the in/out param
std::string StripString(const std::string &str);

/// Replace all invalid chars in @str and return copy, but ignores any found in @ignore
/// @param str - the in/out param
/// @param ignore - null terminated string containing all characters to skip replacing
/// NOTE: instead of passing nullptr or empty string - call the overload
std::string  StripString(const std::string &str, const char * ignore);

bool IsStdStringDigit(const std::string &str);

/// Expand some variables from the provided string
/// $C is camera name, $S is scene name, $F is blend file name
/// @param expr - path containing variables and/or starting with //
/// @param context - the current blender context
/// @return - full path string with variables expanded
std::string ExpandFilenameVariables(const std::string & expr, BL::Context & context);

/// If path starts with // then prepend it with current blend file name
std::string AbsFilePath(const std::string & path, const std::string & blendPath);

/// Get full filepath from relative and ID holder
std::string GetFullFilepath(const std::string &filepath, ID *holder=nullptr);

/// Split a string by a list of delimiters
std::vector<std::string> SplitString(const std::string & input, const std::string & delimiters, bool preserveEmpty = false);

/// Check if str starts with some prefix
/// @param str - the search string
/// @param prefix - the prefix we want to check
/// @return - true if str starts with prefix
inline bool StartsWith(const std::string &str, const char *prefix)
{
	return str.find(prefix) == 0;
}

/// Check if str starts with some prefix
/// @param str - the search string
/// @param prefix - the prefix we want to check
/// @return - true if str starts with prefix
inline bool StartsWith(const std::string &str, const std::string &prefix)
{
	return StartsWith(str, prefix.c_str());
}

} // namespace String
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_STRING_H
