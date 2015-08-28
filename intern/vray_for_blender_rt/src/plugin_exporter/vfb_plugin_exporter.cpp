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

#include "vfb_plugin_exporter.h"
#include "vfb_plugin_exporter_appsdk.h"
#include "vfb_plugin_exporter_file.h"
#include "vfb_plugin_exporter_zmq.h"
#include "vfb_export_settings.h"


using namespace VRayForBlender;


PluginExporter::~PluginExporter()
{
}


class NullExporter:
        public PluginExporter
{
public:
	virtual            ~NullExporter() {}
	virtual void        init() {}
	virtual void        free() {}
	virtual AttrPlugin  export_plugin_impl(const PluginDesc&) { return AttrPlugin(); }
};

AttrPlugin PluginExporter::export_plugin(const PluginDesc &pluginDesc)
{
	bool inCache = m_PluginManager.inCache(pluginDesc);
	bool isDifferent = inCache ? m_PluginManager.differs(pluginDesc) : true;
	AttrPlugin plg(pluginDesc.pluginName);

	if (is_viewport) {
		if (inCache && isDifferent) {
			if (is_animation) {
				this->export_plugin_impl(m_PluginManager.fromCache(pluginDesc));
			}
			plg = this->export_plugin_impl(m_PluginManager.differences(pluginDesc));
			m_PluginManager.updateCache(pluginDesc);
		} else if (!inCache) {
			plg = this->export_plugin_impl(pluginDesc);
			m_PluginManager.updateCache(pluginDesc);
		}
	} else {
		if (!inCache) {
			m_PluginManager.updateCache(pluginDesc);
			plg = this->export_plugin_impl(pluginDesc);
		}
	}

	return plg;
}

VRayForBlender::PluginExporter* VRayForBlender::ExporterCreate(VRayForBlender::ExpoterType type)
{
	PluginExporter *exporter = nullptr;

	switch (type) {
		case ExpoterTypeFile:
			exporter = new VrsceneExporter();
			break;

#ifdef USE_BLENDER_VRAY_CLOUD
		case ExpoterTypeCloud:
			exporter = new NullExporter();
			break;
#endif

#ifdef USE_BLENDER_VRAY_ZMQ
		case ExpoterTypeZMQ:
			exporter = new ZmqExporter();
			break;
#endif

#ifdef USE_BLENDER_VRAY_APPSDK
		case ExpoterTypeAppSDK:
			exporter = new AppSdkExporter();
			break;
#endif
		default:
			exporter = new NullExporter();
	}

	return exporter;
}


void VRayForBlender::ExporterDelete(VRayForBlender::PluginExporter *exporter)
{
	FreePtr(exporter);
}
