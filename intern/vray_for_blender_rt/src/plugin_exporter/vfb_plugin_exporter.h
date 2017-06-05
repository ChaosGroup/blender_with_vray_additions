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

#include <mutex>

#include <boost/function.hpp>

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
	typedef std::shared_ptr<PluginExporter> Ptr;
	typedef boost::function<void(const char *, const char *)> UpdateMessageCb;
	typedef boost::function<void(const VRayBaseTypes::AttrImage &)> BucketReadyCb;
	typedef VRayBaseTypes::CommitAction CommitState;

	PluginExporter(const ExporterSettings & settings)
	    : exporter_settings(settings)
	    , last_rendered_frame(-1.f)
	    , current_scene_frame(-1.f)
	    , render_progress(0.f)
	    , is_viewport(false)
	    , is_prepass(false)
	    , commit_state(CommitState::CommitNone)
	{}

	virtual             ~PluginExporter()=0;

	virtual void         init()=0;
	virtual void         free()=0;

	virtual void         clear_frame_data(float upTo) {};
	virtual void         sync()  {}
	virtual void         start() {}
	virtual void         stop()  {}
	virtual bool         is_running() const { return true; }

	virtual void         export_vrscene(const std::string&) {}

	virtual AttrPlugin   export_plugin_impl(const PluginDesc &pluginDesc)=0;
	AttrPlugin           export_plugin(const PluginDesc &pluginDesc, bool replace = false);
	virtual void         replace_plugin(const std::string &, const std::string &) {};

	virtual int          remove_plugin_impl(const std::string&) { return 0; }
	int                  remove_plugin(const std::string&);

	virtual float        get_last_rendered_frame() const { return last_rendered_frame; }
	virtual void         set_current_frame(float val)    { current_scene_frame = val; }
	virtual bool         is_aborted() const { return false; }

	virtual RenderImage  get_image() { return RenderImage(); }
	virtual RenderImage  get_render_channel(RenderChannelType) { return RenderImage(); }

	RenderImage          get_pass(const std::string & name);

	virtual void         show_frame_buffer() {}
	virtual void         hide_frame_buffer() {}
	virtual void         set_render_mode(RenderMode) {}

	virtual void         set_render_size(const int&, const int&) {}
	virtual void         set_viewport_quality(int) {}

	virtual void         set_callback_on_image_ready(ExpoterCallback cb) { callback_on_image_ready = cb; }
	virtual void         set_callback_on_rt_image_updated(ExpoterCallback cb) { callback_on_rt_image_updated = cb; }
	virtual void         set_callback_on_message_updated(UpdateMessageCb cb) { callback_on_message_update = cb; }
	virtual void         set_callback_on_bucket_ready(BucketReadyCb cb) { callback_on_bucket_ready = cb; }

	virtual void         set_camera_plugin(const std::string &) {}
	virtual void         commit_changes() { set_commit_state(VRayBaseTypes::CommitAction::CommitNow); }
	virtual void         set_commit_state(VRayBaseTypes::CommitAction ca);
	CommitState          get_commit_state() const { return commit_state; }

	virtual void         set_export_file(VRayForBlender::ParamDesc::PluginType, PyObject *) {}

	void                 set_is_viewport(bool flag)  { is_viewport = flag; }
	bool                 get_is_viewport() const { return is_viewport; }

	float                get_progress() const { return render_progress; }
	const std::string &  get_progress_message() const { return progress_message; }

	// if prepass is true - no exporting is done
	void                 set_prepass(bool flag) { is_prepass = flag; }
	bool                 get_is_prepass() const { return is_prepass; }

	PluginManager       &getPluginManager() { return m_pluginManager; }

protected:
	const ExporterSettings &exporter_settings;

	ExpoterCallback      callback_on_image_ready;
	ExpoterCallback      callback_on_rt_image_updated;
	UpdateMessageCb      callback_on_message_update;
	BucketReadyCb        callback_on_bucket_ready;
	float                last_rendered_frame;
	float                current_scene_frame;
	float                render_progress;
	std::string          progress_message;
	bool                 is_viewport;
	bool                 is_prepass;
	CommitState          commit_state;

	PluginManager        m_pluginManager;
	std::recursive_mutex m_exportMtx;

};

PluginExporter::Ptr ExporterCreate(ExporterType type, const ExporterSettings & settings);

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_EXPORTER_H
