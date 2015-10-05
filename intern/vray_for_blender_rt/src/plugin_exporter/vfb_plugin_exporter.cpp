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

void PluginExporter::set_settings(const ExporterSettings &st)
{
	this->animation_settings = st.settings_animation;
	if (this->animation_settings.use) {
		this->current_scene_frame = st.settings_animation.frame_current;
		this->last_rendered_frame = this->animation_settings.frame_start - 1;
	} else {
		this->last_rendered_frame = 0;
	}
}

AttrPlugin PluginExporter::export_plugin(const PluginDesc &pluginDesc)
{
	bool inCache = m_pluginManager.inCache(pluginDesc);
	bool isDifferent = inCache ? m_pluginManager.differs(pluginDesc) : true;
	AttrPlugin plg(pluginDesc.pluginName);

	if (is_viewport || !animation_settings.use) {
		if (!inCache) {
			plg = this->export_plugin_impl(pluginDesc);
			m_pluginManager.updateCache(pluginDesc);
		} else if (inCache && isDifferent) {
			plg = this->export_plugin_impl(m_pluginManager.differences(pluginDesc));
			m_pluginManager.updateCache(pluginDesc);
		}
	} else {
		if (inCache && isDifferent) {
			this->export_plugin_impl(m_pluginManager.fromCache(pluginDesc));
			plg = this->export_plugin_impl(m_pluginManager.differences(pluginDesc));
			m_pluginManager.updateCache(pluginDesc);
		} else if (!inCache) {
			plg = this->export_plugin_impl(pluginDesc);
			m_pluginManager.updateCache(pluginDesc);
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
		case ExporterTypeInvalid:
			/* fall-through */
		default:
			exporter = new NullExporter();
	}

	return exporter;
}


void VRayForBlender::ExporterDelete(VRayForBlender::PluginExporter *exporter)
{
	FreePtr(exporter);
}


void RenderImage::flip()
{
	if (pixels && w && h) {
		const int _half_h = h / 2;
		const int half_h = h % 2 ? _half_h - 1 : _half_h;

		const int row_items = w * 4;
		const int row_bytes = row_items * sizeof(float);

		float *buf = new float[row_items];

		for (int i = 0; i < half_h; ++i) {
			float *to_row   = pixels + (i       * row_items);
			float *from_row = pixels + ((h - i) * row_items);

			memcpy(buf,      to_row,   row_bytes);
			memcpy(to_row,   from_row, row_bytes);
			memcpy(from_row, buf,      row_bytes);
		}

		FreePtr(buf);
	}
}
