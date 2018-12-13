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

#include "cgr_config.h"
#include "vfb_rna.h"
#include "vfb_log.h"

#include "DNA_ID.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "rna_internal_types.h"

using namespace VRayForBlender;

std::string VRayForBlender::RNA_path_get(PointerRNA *ptr, const std::string &attrName)
{
	char buf[FILE_MAX] = "";
	RNA_string_get(ptr, attrName.c_str(), buf);
	BLI_path_abs(buf, ID_BLEND_PATH(G.main, ((ID*)ptr->id.data)));
	return std::string(buf);
}

std::string VRayForBlender::RNA_std_string_get(PointerRNA *ptr, const std::string &attrName)
{
	char buf[PATH_MAX] = "";
	RNA_string_get(ptr, attrName.c_str(), buf);
	return std::string(buf);
}

const EnumPropertyItem *VRayForBlender::RNA_enum_item(PointerRNA *ptr, const char *attrName)
{
	const EnumPropertyRNA *enumProp =
		reinterpret_cast<const EnumPropertyRNA*>(RNA_struct_find_property(ptr, attrName));
	if (enumProp) {
		const int enumItemIndex = RNA_enum_get(ptr, attrName);
		return &enumProp->item[enumItemIndex];
	}
	return NULL;
}

std::string VRayForBlender::RNA_enum_identifier_get(PointerRNA *ptr, const char *attrName)
{
	const EnumPropertyItem *enumItem = RNA_enum_item(ptr, attrName);
	if (enumItem->identifier && *enumItem->identifier) {
		return enumItem->identifier;
	}
	return std::string("");
}

std::string VRayForBlender::RNA_enum_name_get(PointerRNA *ptr, const char *attrName)
{
	const EnumPropertyItem *enumItem = RNA_enum_item(ptr, attrName);
	if (enumItem->name && *enumItem->name) {
		return enumItem->name;
	}
	return std::string("");
}

int VRayForBlender::RNA_enum_ext_get(PointerRNA *ptr, const char *attrName)
{
	int enumItemIndex = RNA_enum_get(ptr, attrName);

	const EnumPropertyItem *enumItem = RNA_enum_item(ptr, attrName);
	if (!enumItem->identifier) {
		PropertyRNA *prop = RNA_struct_find_property(ptr, attrName);
		if (prop) {
			getLog().error("Property \"%s\": Enum identifier is not found!",
				prop->name);
		}
	}
	else {
		// If enum item is digit, return it as int
		int tmp_int = 0;
		if (sscanf(enumItem->identifier, "%i", &tmp_int) == 1) {
			enumItemIndex = tmp_int;
		}
	}

	return enumItemIndex;
}
