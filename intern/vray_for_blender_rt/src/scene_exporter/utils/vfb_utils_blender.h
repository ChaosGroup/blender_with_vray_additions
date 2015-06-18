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

#ifndef VRAY_FOR_BLENDER_UTILS_BLENDER_H
#define VRAY_FOR_BLENDER_UTILS_BLENDER_H

#include "vfb_rna.h"


namespace VRayForBlender {
namespace Blender {

std::string   GetFilepath(const std::string &filepath, ID *holder=nullptr);

BL::Object    GetObjectByName(BL::BlendData data, const std::string &name);
BL::Material  GetMaterialByName(BL::BlendData data, const std::string &name);

std::string   GetIDName(BL::ID id, const std::string &prefix="");
std::string   GetIDNameAuto(BL::ID id);

int           GetMaterialCount(BL::Object ob);

template <typename T>
T GetDataFromProperty(PointerRNA *ptr, const std::string &attr) {
	PropertyRNA *ntreeProp = RNA_struct_find_property(ptr, attr.c_str());
	if (ntreeProp) {
		if  (RNA_property_type(ntreeProp) == PROP_POINTER) {
			return T(RNA_pointer_get(ptr, attr.c_str()));
		}
	}
	return T(PointerRNA_NULL);
}

} // namespace Blender
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_BLENDER_H
