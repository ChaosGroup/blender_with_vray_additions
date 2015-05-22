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

namespace VRayForBlender {


struct ZmqRenderImage:
	public RenderImage {
	void update(const VRayMessage &);
};


class ZmqExporter:
        public PluginExporter
{
public:
	ZmqExporter();
	virtual            ~ZmqExporter();

public:
	virtual void        init();
	virtual void        free();
	virtual void        sync();
	virtual void        start();
	virtual void        stop();

	virtual RenderImage  get_image();
	virtual void set_render_size(const int &w, const int &h);

	virtual AttrPlugin  export_plugin(const PluginDesc &pluginDesc);
private:
	ZmqClient * m_Client;
	ZmqRenderImage m_CurrentImage;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_ZMQ_H
