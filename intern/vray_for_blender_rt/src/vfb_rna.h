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

#ifndef VRAY_FOR_BLENDER_RNA_H
#define VRAY_FOR_BLENDER_RNA_H

#include "MEM_guardedalloc.h"
#include "RNA_types.h"
#include "RNA_access.h"
#include "RNA_blender_cpp.h"

#include <vector>

namespace VRayForBlender {

enum EvalMode {
	EvalModePreview = 1,
	EvalModeRender  = 2,
};

typedef BL::Array<float, 16>  BlTransform;
typedef BL::Array<float, 3>   BlVector;
typedef BL::Array<float, 2>   BlVector2;
typedef BL::Array<float, 4>   BlAColor;
typedef BlVector              BlColor;
typedef BL::Array<bool, 20>   BlLayers;

typedef BL::Array<int, 4>     BlFace;
typedef BlVector2             BlVertUV;
typedef BlVector              BlVertCol;

typedef std::vector<BL::Object>   ObList;


std::string  RNA_path_get(PointerRNA *ptr, const std::string &attrName);
std::string  RNA_std_string_get(PointerRNA *ptr, const std::string &attrName);

int          RNA_enum_ext_get(PointerRNA *ptr, const char *attrName);
std::string  RNA_enum_identifier_get(PointerRNA *ptr, const char *attrName);
std::string  RNA_enum_name_get(PointerRNA *ptr, const char *attrName);

const EnumPropertyItem *RNA_enum_item(PointerRNA *ptr, const char *attrName);

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_RNA_H
