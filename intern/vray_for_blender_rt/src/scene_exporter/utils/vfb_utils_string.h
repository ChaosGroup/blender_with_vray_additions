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

std::string GetUniqueName(StrSet &namesSet, const std::string &name);

std::string StripString(std::string &str);

std::string ExpandFilenameVariables(const std::string & expr, const std::string & camera, const std::string & scene, const std::string & blendPath, const std::string & ext);

} // namespace String
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_STRING_H
