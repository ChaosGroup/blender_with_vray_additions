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
