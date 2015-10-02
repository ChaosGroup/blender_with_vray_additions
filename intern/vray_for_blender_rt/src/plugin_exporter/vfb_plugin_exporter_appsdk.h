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

#ifdef USE_BLENDER_VRAY_APPSDK

#ifndef VRAY_FOR_BLENDER_PLUGIN_EXPORTER_APPSDK_H
#define VRAY_FOR_BLENDER_PLUGIN_EXPORTER_APPSDK_H

#include "vfb_plugin_exporter.h"


namespace VRayForBlender {


struct AppSDKRenderImage:
        public RenderImage
{
	AppSDKRenderImage(const VRay::VRayImage *image);
};


class AppSdkExporter:
        public PluginExporter
{
	struct PluginUsed {
		PluginUsed() {}
		PluginUsed(const VRay::Plugin &p):
		    plugin(p),
		    used(true)
		{}

		VRay::Plugin  plugin;
		int           used;
	};

	typedef std::map<std::string, PluginUsed> PluginUsage;

public:
	AppSdkExporter();
	virtual             ~AppSdkExporter();

public:
	virtual void         init();
	virtual void         free();

	virtual void         sync();
	virtual void         start();
	virtual void         stop();

	virtual AttrPlugin   export_plugin_impl(const PluginDesc &pluginDesc);
	virtual int          remove_plugin(const std::string &pluginName);

	virtual void         export_vrscene(const std::string &filepath);

	virtual RenderImage  get_image();
	virtual void         set_render_size(const int &w, const int &h);

	virtual void         set_callback_on_image_ready(ExpoterCallback cb);
	virtual void         set_callback_on_rt_image_updated(ExpoterCallback cb);

	virtual void         set_camera_plugin(const std::string &pluginName) override;

private:
	void                 reset_used();

	VRay::Plugin         new_plugin(const PluginDesc &pluginDesc);

	VRay::VRayRenderer  *m_vray;

	PluginUsage          m_used_map;

};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_APPSDK_H
#endif // USE_BLENDER_VRAY_APPSDK
