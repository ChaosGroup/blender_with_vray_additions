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

#ifndef VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
#define VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H

#include "cgr_config.h"
#include "vfb_util_defines.h"
#include "vfb_rna.h"
#include "vfb_plugin_attrs.h"
#include "vfb_util_defines.h"
#include "vfb_export_settings.h"
#include "vfb_params_desc.h"
#include "vfb_plugin_exporter_types.h"
#include "vfb_plugin_manager.h"
#include "vfb_render_image.h"

#include "RNA_blender_cpp.h"

#include "base_types.h"

#include <boost/function.hpp>
#include <boost/unordered_map.hpp>

namespace VRayForBlender {

struct ExporterSettings;

struct ExpoterCallback {
	typedef boost::function<void(void)> CallbackFunction;

	ExpoterCallback() {}
	ExpoterCallback(CallbackFunction _cb):
	    cb(_cb)
	{}

	operator bool () const {
		return !!(cb);
	}

	CallbackFunction cb;
};

class PluginExporter
{
public:
	typedef boost::function<void(const char *, const char *)> UpdateMessageCb;

	PluginExporter()
	    : is_viewport(false)
	    , is_prepass(false)
	{}

	virtual             ~PluginExporter()=0;

	virtual void         init()=0;
	virtual void         free()=0;

	virtual void         set_settings(const ExporterSettings &st);

	virtual void         sync()  {}
	virtual void         start() {}
	virtual void         stop()  {}
	virtual bool         is_running() const { return true; }

	virtual void         export_vrscene(const std::string&) {}

	virtual AttrPlugin   export_plugin_impl(const PluginDesc &pluginDesc)=0;
	AttrPlugin           export_plugin(const PluginDesc &pluginDesc);

	virtual int          remove_plugin_impl(const std::string&) { return 0; }
	int                  remove_plugin(const std::string&);

	virtual float        get_last_rendered_frame() const { return last_rendered_frame; }
	void                 set_current_frame(float val)    { current_scene_frame = val; }
	virtual bool         is_aborted() const { return false; }

	virtual RenderImage  get_image() { return RenderImage(); }
	virtual RenderImage  get_render_channel(RenderChannelType channelType) { return RenderImage(); }

	RenderImage          get_pass(BL::RenderPass::type_enum passType);

	virtual void         show_frame_buffer() {}
	virtual void         hide_frame_buffer() {}
	virtual void         set_render_mode(RenderMode renderMode) {}

	virtual void         set_render_size(const int&, const int&) {}
	virtual void         set_viewport_quality(int) {}

	virtual void         set_callback_on_image_ready(ExpoterCallback cb)      { callback_on_image_ready = cb; }
	virtual void         set_callback_on_rt_image_updated(ExpoterCallback cb) { callback_on_rt_image_updated = cb; }
	virtual void         set_callback_on_message_updated(UpdateMessageCb cb)  { on_message_update = cb; }

	virtual void         set_camera_plugin(const std::string &pluginName) {}
	virtual void         commit_changes() { set_commit_state(VRayBaseTypes::CommitAction::CommitNow); }
	virtual void         set_commit_state(VRayBaseTypes::CommitAction ca) {};

	virtual void         set_export_file(VRayForBlender::ParamDesc::PluginType, PyObject *) {}

	void                 set_is_viewport(bool flag)  { is_viewport = flag; }
	bool                 get_is_viewport() const { return is_viewport; }

	// if prepass is true - no exporting is done
	void                 set_prepass(bool flag) { is_prepass = flag; }
	bool                 get_is_prepass() const { return is_prepass; }

	PluginManager       &getPluginManager() { return m_pluginManager; }

protected:
	ExpoterCallback      callback_on_image_ready;
	ExpoterCallback      callback_on_rt_image_updated;
	UpdateMessageCb      on_message_update;
	float                last_rendered_frame;
	float                current_scene_frame;
	SettingsAnimation    animation_settings;
	bool                 is_viewport;
	bool                 is_prepass;

	PluginManager        m_pluginManager;

};

PluginExporter* ExporterCreate(ExpoterType type);
void            ExporterDelete(PluginExporter *exporter);

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
