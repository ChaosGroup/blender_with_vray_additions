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


#ifndef VRAY_FOR_BLENDER_PLUGIN_EXPORTER_ZMQ_H
#define VRAY_FOR_BLENDER_PLUGIN_EXPORTER_ZMQ_H

#include "vfb_plugin_exporter.h"
#include "vfb_utils_object.h"

#include "zmq_wrapper.hpp"
#include "zmq_message.hpp"

#include <stack>
#include <vector>

namespace VRayForBlender {

typedef std::unique_ptr<ZmqClient> ClientPtr;


class ZmqServer {
public:
	static bool isRunning();
	static bool start(const char * addr);
	static bool stop();
private:
	static std::mutex clientMtx;
	static ClientPtr serverCheck;
};


class ZmqExporter:
        public PluginExporter
{
public:

	struct ZmqRenderImage:
		public RenderImage {
		void update(const VRayBaseTypes::AttrImage &img, ZmqExporter * exp, bool fixImage);
	};

	typedef HashMap<RenderChannelType, ZmqRenderImage, std::hash<int>> ImageMap;

	ZmqExporter(const ExporterSettings & settings);
	virtual            ~ZmqExporter();

public:
	virtual void        init();

	virtual void        export_vrscene(const std::string &filepath);
	virtual void        clear_frame_data(float upTo);
	virtual void        free();
	virtual void        sync();
	virtual void        start();
	virtual void        stop();
	virtual void        reset();

	virtual bool        is_running() const { return m_started; }

	virtual RenderImage get_image();
	virtual RenderImage get_render_channel(RenderChannelType channelType);
	virtual void        set_render_size(const int &w, const int &h);
	virtual void        set_camera_plugin(const std::string &pluginName);
	virtual void        set_commit_state(VRayBaseTypes::CommitAction ca);

	virtual void        set_current_frame(float frame);
	virtual bool        is_aborted() const { return m_isAborted; }

	virtual void        replace_plugin(const std::string & oldPlugin, const std::string & newPlugin);
	virtual int         remove_plugin_impl(const std::string&);
	virtual AttrPlugin  export_plugin_impl(const PluginDesc &pluginDesc);
private:
	void                checkZmqClient();
	void                zmqCallback(const VRayMessage & message, ZmqClient * client);

private:
	using ImageType = VRayBaseTypes::AttrImage::ImageType;

	struct ValueCache {
		int renderWidth;
		int renderHeight;
		std::string activeCamera;

		int viewport_image_quality;
		ImageType viewport_image_type;
		bool show_vfb;
		RenderMode render_mode;
	};

	ClientPtr           m_client;

	// some cached values to reduce trafix
	bool                m_isDirty; ///< if true we have some change sent to server after last commit, so next commit will go trough
	bool                m_isAborted;
	bool                m_started;

	// ensures the image is not changed while it is read
	std::mutex          m_imgMutex;
	std::mutex          m_zmqClientMutex;
	ZmqRenderImage      m_currentImage;
	ImageMap            m_layerImages;

	ValueCache          m_cachedValues;
};
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_ZMQ_H
