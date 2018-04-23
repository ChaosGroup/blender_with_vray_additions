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


class NullExporter:
        public PluginExporter
{
public:
	NullExporter(const ExporterSettings & settings)
		: PluginExporter(settings)
	{}
	virtual            ~NullExporter() {}
	virtual void        init() {}
	virtual void        free() {}
	virtual AttrPlugin  export_plugin_impl(const PluginDesc&) { return AttrPlugin(); }
};


PluginExporter::~PluginExporter() {}


int PluginExporter::remove_plugin(const std::string &name) {
	std::lock_guard<std::recursive_mutex> lock(m_exportMtx);
	int result = 1;
	if (m_pluginManager.inCache(name)) {
		PRINT_INFO_EX("Removing plugin: [%s]", name.c_str());
		m_pluginManager.remove(name);
		result = this->remove_plugin_impl(name);
	}
	return result;
}

void PluginExporter::sync() {
	for (const PluginDesc &desc: delayedPlugins) {
		export_plugin(desc);
	}
}

AttrPlugin PluginExporter::export_plugin(const PluginDesc &pluginDesc, bool replace, bool dontExport)
{
	if (is_prepass || dontExport) {
		return AttrPlugin(pluginDesc.pluginName);
	}

	std::lock_guard<std::recursive_mutex> lock(m_exportMtx);
	const bool hasFrames = exporter_settings.settings_animation.use || exporter_settings.use_motion_blur;

	// force replace off for animation, because repalce will wipe all animation data up until current frame
	replace = hasFrames ? false : replace;

	const bool inCache = m_pluginManager.inCache(pluginDesc);
	const bool isDifferent = inCache ? m_pluginManager.differs(pluginDesc) : true;
	const bool isDifferentId = inCache ? m_pluginManager.differsId(pluginDesc) : false;
	AttrPlugin plg(pluginDesc.pluginName);

	if (!inCache) {
		plg = this->export_plugin_impl(pluginDesc);
		m_pluginManager.updateCache(pluginDesc, current_scene_frame);
	} else if (replace || (inCache && isDifferent)) {

		if (isDifferentId) {
			plg = this->export_plugin_impl(m_pluginManager.differences(pluginDesc));
		} else {
			if (!replace) {
				// We need to export last exported data for the previous frame
				// This is required when an attribute is not animated for several frames and then is
				// If previous frame is not exported then vray will interpolate the animated parameter between
				//    - the last frame data was exported (cached one)
				//    - the current data we export
				// But actually it needs to interpolate between previous frame and current frame
				if (m_pluginManager.storeData()) {
					const auto cachedItem = m_pluginManager.fromCache(pluginDesc.pluginName);
					// if the cached item is from previous frame - we don't need to write extra key frame now
					if (current_scene_frame - cachedItem.frame > 1) {
						// TODO: could this brake for subframes?
						--current_scene_frame;
						this->export_plugin_impl(PluginManager::diffWithPlugin(cachedItem.desc, pluginDesc));
						++current_scene_frame;
					}
				}
				plg = this->export_plugin_impl(m_pluginManager.differences(pluginDesc));
			} else {
				// we need replace when exporting to AppSDK
				const auto state = this->get_commit_state();
				if (state != CommitState::CommitAutoOff) {
					this->set_commit_state(VRayBaseTypes::CommitAction::CommitAutoOff);
				}
				auto pluginCopy = pluginDesc;
				pluginCopy.pluginName += "_internalDest";

				this->export_plugin_impl(pluginCopy);
				this->replace_plugin(pluginDesc.pluginName, pluginCopy.pluginName);
				this->remove_plugin_impl(pluginDesc.pluginName);
				plg = this->export_plugin_impl(pluginDesc);
				this->replace_plugin(pluginCopy.pluginName, pluginDesc.pluginName);
				this->remove_plugin_impl(pluginCopy.pluginName);

				this->commit_changes();

				if (state != CommitState::CommitAutoOff) {
					this->set_commit_state(state);
				}
			}
		}

		m_pluginManager.updateCache(pluginDesc, current_scene_frame);
	}

	return plg;
}

void PluginExporter::set_commit_state(VRayBaseTypes::CommitAction ca)
{
	if (ca == VRayBaseTypes::CommitAutoOff || ca == VRayBaseTypes::CommitAutoOn) {
		commit_state = ca;
	}
}

RenderImage PluginExporter::get_pass(const std::string & name)
{
	RenderImage image;

	if (name == "Combined") {
		image = get_image();
	} else if (name == "Depth") {
		image = get_render_channel(RenderChannelTypeVfbZdepth);
	}

	return image;
}


PluginExporterPtr VRayForBlender::ExporterCreate(VRayForBlender::ExporterType type, const ExporterSettings & settings)
{
	PluginExporterPtr exporter{nullptr};

	switch (type) {
		case ExpoterTypeFile:
			exporter.reset(new VrsceneExporter(settings));
			break;

#ifdef USE_BLENDER_VRAY_CLOUD
		case ExpoterTypeCloud:
			exporter.reset(new NullExporter(settings));
			break;
#endif
		case ExpoterTypeZMQ:
			exporter.reset(new ZmqExporter(settings));
			break;

#ifdef USE_BLENDER_VRAY_APPSDK
		case ExpoterTypeAppSDK:
			exporter.reset(new AppSdkExporter(settings));
			break;
#endif
		case ExporterTypeInvalid:
			/* fall-through */
		default:
			exporter.reset(new NullExporter(settings));
	}

	return exporter;
}
