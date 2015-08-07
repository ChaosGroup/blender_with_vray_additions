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
	virtual AttrPlugin  export_plugin(const PluginDesc&) { return AttrPlugin(); }
};

VRayForBlender::PluginExporter* VRayForBlender::ExporterCreate(VRayForBlender::ExpoterType type) {
	return VRayForBlender::ExporterCreate(type, ExporterSettings());
}

VRayForBlender::PluginExporter* VRayForBlender::ExporterCreate(VRayForBlender::ExpoterType type, const ExporterSettings & settings)
{
	PluginExporter *exporter = nullptr;

	switch (type) {
		case ExpoterTypeFile:
			exporter = new VrsceneExporter();
			break;
#ifdef VRAY_USE_CLOUD
		case ExpoterTypeCloud:
			exporter = new NullExporter();
			break;
#endif
#ifdef VRAY_USE_ZMQ
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

	if (exporter) {
		exporter->set_settings(settings);
		exporter->init();
	}

	return exporter;
}


void VRayForBlender::ExporterDelete(VRayForBlender::PluginExporter *exporter)
{
	FreePtr(exporter);
}
