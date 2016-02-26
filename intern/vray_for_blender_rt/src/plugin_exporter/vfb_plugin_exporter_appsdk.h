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

#include <vraysdk.hpp>

namespace VRayForBlender {

struct AppSdkInit {
	AppSdkInit()
	    : m_vrayInit(nullptr)
	{
		try {
			m_vrayInit = new VRay::VRayInit(true);
		}
		catch (std::exception &e) {
			PRINT_INFO_EX("Error initializing V-Ray library! Error: \"%s\"",
			              e.what());
			m_vrayInit = nullptr;
		}
	}

	~AppSdkInit() {
		FreePtr(m_vrayInit);
	}

	operator bool () const {
		return !!(m_vrayInit);
	}

private:
	AppSdkInit(const AppSdkInit&) = delete;
	AppSdkInit& operator=(const AppSdkInit&) = delete;

	VRay::VRayInit *m_vrayInit;

};


struct AppSDKRenderImage
        : public RenderImage
{
	AppSDKRenderImage(const VRay::VRayImage *image, VRay::RenderElement::PixelFormat pixelFormat = VRay::RenderElement::PF_RGBA_FLOAT);
};


class AppSdkExporter
        : public PluginExporter
{
	static AppSdkInit    vrayInit;

public:
	AppSdkExporter();
	virtual             ~AppSdkExporter();

public:
	virtual void         init() override;
	virtual void         free() override;
	virtual void         sync() override;
	virtual void         start() override;
	virtual void         stop() override;

	virtual void         export_vrscene(const std::string &filepath) override;
	virtual AttrPlugin   export_plugin_impl(const PluginDesc &pluginDesc) override;
	virtual int          remove_plugin_impl(const std::string &pluginName) override;
	virtual void         commit_changes() override;

	virtual RenderImage  get_image() override;
	virtual RenderImage  get_render_channel(RenderChannelType channelType) override;

	virtual void         set_render_size(const int &w, const int &h) override;
	virtual void         set_camera_plugin(const std::string &pluginName) override;
	virtual void         set_render_mode(RenderMode renderMode) override;

	virtual void         set_callback_on_image_ready(ExpoterCallback cb) override;
	virtual void         set_callback_on_rt_image_updated(ExpoterCallback cb) override;
	virtual void         set_callback_on_message_updated(UpdateMessageCb cb) override;

	virtual void         show_frame_buffer() override;

private:
	void                 CbOnImageReady(VRay::VRayRenderer&, void *userData);
	void                 CbOnRTImageUpdated(VRay::VRayRenderer&, VRay::VRayImage*, void *userData);
	void                 CbOnProgress(VRay::VRayRenderer &, const char * msg, int, void *);

	void                 bucket_ready(VRay::VRayRenderer & renderer, int x, int y, const char * host, VRay::VRayImage * img, void * arg);

private:
	VRay::VRayRenderer  *m_vray;

	RenderImage          m_bucket_image;
	bool                 m_started;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_APPSDK_H
#endif // USE_BLENDER_VRAY_APPSDK
