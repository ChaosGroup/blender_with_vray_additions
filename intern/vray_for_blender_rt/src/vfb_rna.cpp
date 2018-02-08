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

#include "DNA_ID.h"
#include "BLI_path_util.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "rna_internal_types.h"


using namespace VRayForBlender;


std::string VRayForBlender::RNA_path_get(PointerRNA *ptr, const std::string &attrName)
{
	char filepath[FILE_MAX] = "";

	RNA_string_get(ptr, attrName.c_str(), filepath);
	BLI_path_abs(filepath, ID_BLEND_PATH(G.main, ((ID*)ptr->id.data)));

	return filepath;
}


std::string VRayForBlender::RNA_std_string_get(PointerRNA *ptr, const std::string &attrName)
{
	std::string result;

	const int len = RNA_string_length(ptr, attrName.c_str());
	char * dest = new char[len + 1];
	RNA_string_get(ptr, attrName.c_str(), dest);
	result = dest;
	delete[] dest;

	return result;
}


const EnumPropertyItem *VRayForBlender::RNA_enum_item(PointerRNA *ptr, const char *attrName)
{
	int enum_item_index = RNA_enum_get(ptr, attrName);

	const EnumPropertyRNA *enum_prop = (const EnumPropertyRNA*)RNA_struct_find_property(ptr, attrName);
	if (enum_prop)
		return &enum_prop->item[enum_item_index];

	return NULL;
}


std::string VRayForBlender::RNA_enum_identifier_get(PointerRNA *ptr, const char *attrName)
{
	const EnumPropertyItem *enum_item = RNA_enum_item(ptr, attrName);
	if (enum_item->identifier)
		return enum_item->identifier;
	return "";
}


std::string VRayForBlender::RNA_enum_name_get(PointerRNA *ptr, const char *attrName)
{
	const EnumPropertyItem *enum_item = RNA_enum_item(ptr, attrName);
	if (enum_item->name)
		return enum_item->name;
	return "";
}


int VRayForBlender::RNA_enum_ext_get(PointerRNA *ptr, const char *attrName)
{
	int enum_item_index = RNA_enum_get(ptr, attrName);

	const EnumPropertyItem *enum_item = RNA_enum_item(ptr, attrName);
	if (!enum_item->identifier) {
		PropertyRNA *prop = RNA_struct_find_property(ptr, attrName);
		if (prop) {
			PRINT_ERROR("Property \"%s\": Enum identifier is not found!",
						prop->name);
		}
	}
	else {
		// If enum item is digit, return it as int
		int tmp_int = 0;
		if (sscanf(enum_item->identifier, "%i", &tmp_int) == 1) {
			enum_item_index = tmp_int;
		}
	}

	return enum_item_index;
}
