#ifndef VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
#define VRAY_FOR_BLENDER_PLUGIN_MANAGER_H

#include "vfb_plugin_exporter.h"
#include "base_types.h"


#include <boost/unordered_map.hpp>

namespace VRayForBlender {
class PluginManager {
public:
	PluginManager();

	PluginDesc filterPlugin(const PluginDesc & pluginDesc);

	void clear();

private:
	boost::unordered_map<std::string, PluginAttrs> cache;
};
} // VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_MANAGER_H
