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

std::string GetUniqueName(StrSet &namesSet, const std::string &name);

std::string StripString(std::string &str);

/// Expand some variables from the provided string
/// $C is camera name, $S is scene name, $F is blend file name
/// also supports python's datetime placeholders like %H %M %S, etc.
std::string ExpandFilenameVariables(const std::string & expr, const std::string & camera, const std::string & scene, const std::string & blendPath, const std::string & ext);

} // namespace String
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_STRING_H
