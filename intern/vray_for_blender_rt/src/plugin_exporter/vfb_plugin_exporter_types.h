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

#ifndef VRAY_FOR_BLENDER_PLUGIN_EXPORTER_TYPES_H
#define VRAY_FOR_BLENDER_PLUGIN_EXPORTER_TYPES_H

#include "vfb_util_defines.h"

namespace VRayForBlender {


struct EnumItem {
	const char *key;
	const char *name;
	const char *desc;
};


static const EnumItem ExporterTypesList[] = {
	{"STD",    "V-Ray Standalone", ""},
	{"ZMQ",    "V-Ray ZMQ Server", ""},

#ifdef USE_BLENDER_VRAY_CLOUD
	{"CLOUD",  "V-Ray Cloud", ""},
#endif

#ifdef USE_BLENDER_VRAY_APPSDK
	{"APPSDK", "V-Ray Application SDK", ""},
#endif
};


enum ExporterType {
	ExpoterTypeFile = 0,
	ExpoterTypeZMQ,

#ifdef USE_BLENDER_VRAY_CLOUD
	ExpoterTypeCloud,
#endif

#ifdef USE_BLENDER_VRAY_APPSDK
	ExpoterTypeAppSDK,
#endif
	ExpoterTypeLast,
	ExporterTypeInvalid = ExpoterTypeLast,
};


static_assert(ExpoterTypeLast == ArraySize(ExporterTypesList), "ExporterType / ExporterTypeInfo size must match!");

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_TYPES_H
