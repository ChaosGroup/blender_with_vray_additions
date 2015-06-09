#ifndef VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
#define VRAY_FOR_BLENDER_PLUGIN_MANAGER_H

#include "vfb_plugin_exporter.h"
#include "base_types.h"


#include <unordered_map>

namespace VRayForBlender {
class PluginManager {
public:
	PluginManager();

	PluginDesc filterPlugin(const PluginDesc & pluginDesc);

	void clear();

private:
	std::unordered_map<std::string, PluginAttrs> cache;
};
} // VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
