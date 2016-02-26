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

#include "zmq_wrapper.h"
#include "zmq_message.hpp"

#include <unordered_map>
#include <stack>
#include <vector>

namespace VRayForBlender {

typedef std::unique_ptr<ZmqClient> ClientPtr;

class ZmqWorkerPool {
public:

	static ZmqWorkerPool & getInstance();
	ClientPtr getClient();
	void returnClient(ClientPtr cl);

private:
	std::stack<ClientPtr, std::vector<ClientPtr>> m_Clients;

private:
	~ZmqWorkerPool();
	ZmqWorkerPool();
};


class ZmqExporter:
        public PluginExporter
{
public:

	struct ZmqRenderImage:
		public RenderImage {
		void update(const VRayBaseTypes::AttrImage &img, ZmqExporter * exp, bool fixImage);
	};

	typedef std::unordered_map<RenderChannelType, ZmqRenderImage, std::hash<int>> ImageMap;

	ZmqExporter();
	virtual            ~ZmqExporter();

public:
	virtual void        init();
	virtual void        set_settings(const ExporterSettings &);

	virtual void        export_vrscene(const std::string &filepath);

	virtual void        free();
	virtual void        sync();
	virtual void        start();
	virtual void        stop();
	virtual bool        is_running() const { return m_Started; }

	virtual RenderImage get_image();
	virtual RenderImage get_render_channel(RenderChannelType channelType);
	virtual void        set_render_size(const int &w, const int &h);
	virtual void        set_viewport_quality(int quality);
	virtual bool        is_aborted() const { return m_IsAborted; }


	virtual int         remove_plugin_impl(const std::string&);
	virtual AttrPlugin  export_plugin_impl(const PluginDesc &pluginDesc);
private:
	void                checkZmqClient();
	void                zmqCallback(VRayMessage & message, ZmqWrapper * client);

private:
	RenderMode          m_RenderMode;

	int                 m_ServerPort;
	std::string         m_ServerAddress;
	ClientPtr           m_Client;

	std::mutex          m_ImgMutex;
	std::mutex          m_ZmqClientMutex;
	ZmqRenderImage      m_CurrentImage;
	ImageMap            m_LayerImages;
	float               m_LastExportedFrame;
	bool                m_IsAborted;
	bool                m_Started;

	int                 m_RenderQuality;
	int                 m_RenderWidth;
	int                 m_RenderHeight;
};
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_ZMQ_H
