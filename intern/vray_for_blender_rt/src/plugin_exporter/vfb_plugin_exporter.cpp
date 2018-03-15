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

AttrPlugin PluginExporter::export_plugin(const PluginDesc &pluginDesc, bool replace, bool dontExport)
{
	if (is_prepass || dontExport) {
		return AttrPlugin(pluginDesc.pluginName);
	}

	std::lock_guard<std::recursive_mutex> lock(m_exportMtx);
	const bool hasFrames = exporter_settings.settings_animation.use || exporter_settings.use_motion_blur;

	// force replace off for animation, because repalce will wipe all animation data up until current frame
	replace = hasFrames ? false : replace;

	bool inCache = m_pluginManager.inCache(pluginDesc);
	bool isDifferent = inCache ? m_pluginManager.differs(pluginDesc) : true;
	bool isDifferentId = inCache ? m_pluginManager.differsId(pluginDesc) : false;
	AttrPlugin plg(pluginDesc.pluginName);

	if (is_viewport || hasFrames) {
		if (!inCache) {
			plg = this->export_plugin_impl(pluginDesc);
			m_pluginManager.updateCache(pluginDesc);
		} else if (replace || (inCache && isDifferent)) {

			if (isDifferentId) {
				// copy the name, since cachedPlugin is reference from inside the manager
				// and when we remove it, it will reference invalid memory!
				this->remove_plugin(pluginDesc.pluginName);
				plg = this->export_plugin_impl(pluginDesc);
			} else {
				if (!replace) {
					plg = this->export_plugin_impl(m_pluginManager.differences(pluginDesc));
				} else {
					auto state = this->get_commit_state();
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

			m_pluginManager.updateCache(pluginDesc);
		}
	} else {
		if (inCache && isDifferent) {
			// TODO: do we really need to export previous state first?
			// this->export_plugin_impl(m_pluginManager.fromCache(pluginDesc));
			plg = this->export_plugin_impl(m_pluginManager.differences(pluginDesc));
			m_pluginManager.updateCache(pluginDesc);
		} else if (!inCache) {
			plg = this->export_plugin_impl(pluginDesc);
			m_pluginManager.updateCache(pluginDesc);
		}
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


std::shared_ptr<PluginExporter> VRayForBlender::ExporterCreate(VRayForBlender::ExporterType type, const ExporterSettings & settings)
{
	std::shared_ptr<PluginExporter> exporter{nullptr};

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
