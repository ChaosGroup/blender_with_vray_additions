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
	virtual            ~NullExporter() {}
	virtual void        init() {}
	virtual void        free() {}
	virtual AttrPlugin  export_plugin_impl(const PluginDesc&) { return AttrPlugin(); }
};


PluginExporter::~PluginExporter() {}


void PluginExporter::set_settings(const ExporterSettings &st)
{
	this->animation_settings = st.settings_animation;
	this->current_scene_frame = this->last_rendered_frame = -1000;
}

int PluginExporter::remove_plugin(const std::string &name) {
	int result = 1;
	if (m_pluginManager.inCache(name)) {
		PRINT_INFO_EX("Removing plugin: [%s]", name.c_str());
		m_pluginManager.remove(name);
		result = this->remove_plugin_impl(name);
	}
	return result;
}

AttrPlugin PluginExporter::export_plugin(const PluginDesc &pluginDesc, bool replace)
{
	if (is_prepass) {
		return AttrPlugin(pluginDesc.pluginName);
	}

	bool inCache = m_pluginManager.inCache(pluginDesc);
	bool isDifferent = inCache ? m_pluginManager.differs(pluginDesc) : true;
	AttrPlugin plg(pluginDesc.pluginName);

	if (is_viewport || !animation_settings.use) {
		if (!inCache) {
			plg = this->export_plugin_impl(pluginDesc);
			m_pluginManager.updateCache(pluginDesc);
		} else if (inCache && isDifferent) {
			const auto &cachedPlugin = m_pluginManager[pluginDesc];

			if (cachedPlugin.pluginID != pluginDesc.pluginID) {
				// copy the name, since cachedPlugin is reference from inside the manager
				// and when we remove it, it will reference invalid memory!
				auto name = cachedPlugin.pluginName;
				this->remove_plugin(name);
				plg = this->export_plugin_impl(pluginDesc);
			} else {
				if (!replace) {
					plg = this->export_plugin_impl(m_pluginManager.differences(pluginDesc));
				} else {
					this->set_commit_state(VRayBaseTypes::CommitAction::CommitAutoOff);
					auto pluginCopy = pluginDesc;
					pluginCopy.pluginName += "_internalDest";

					this->export_plugin_impl(pluginCopy);
					this->replace_plugin(pluginDesc.pluginName, pluginCopy.pluginName);
					this->remove_plugin_impl(pluginDesc.pluginName);
					plg = this->export_plugin_impl(pluginDesc);
					this->replace_plugin(pluginCopy.pluginName, pluginDesc.pluginName);
					this->remove_plugin_impl(pluginCopy.pluginName);

					this->commit_changes();
					this->set_commit_state(VRayBaseTypes::CommitAction::CommitAutoOn);
				}
			}

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


RenderImage PluginExporter::get_pass(BL::RenderPass::type_enum passType)
{
	RenderImage image;

	switch (passType) {
		case BL::RenderPass::type_COMBINED: image = get_image(); break;
		case BL::RenderPass::type_Z: image = get_render_channel(RenderChannelTypeVfbZdepth); break;
		case BL::RenderPass::type_COLOR: image = get_render_channel(RenderChannelTypeVfbRealcolor); break;
		// case BL::RenderPass::type_DIFFUSE: image = get_render_channel(RenderChannelTypeVfbDiffuse); break;
		// case BL::RenderPass::type_SPECULAR: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_SHADOW: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_AO: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_REFLECTION: image = get_render_channel(RenderChannelType); break;
		case BL::RenderPass::type_NORMAL: image = get_render_channel(RenderChannelTypeVfbNormal); break;
		// case BL::RenderPass::type_VECTOR: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_REFRACTION: image = get_render_channel(RenderChannelType); break;
		case BL::RenderPass::type_OBJECT_INDEX: image = get_render_channel(RenderChannelTypeVfbRenderID); break;
		// case BL::RenderPass::type_UV: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_MIST: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_EMIT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_ENVIRONMENT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_MATERIAL_INDEX: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_DIFFUSE_DIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_DIFFUSE_INDIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_DIFFUSE_COLOR: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_GLOSSY_DIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_GLOSSY_INDIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_GLOSSY_COLOR: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_TRANSMISSION_DIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_TRANSMISSION_INDIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_TRANSMISSION_COLOR: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_SUBSURFACE_DIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_SUBSURFACE_INDIRECT: image = get_render_channel(RenderChannelType); break;
		// case BL::RenderPass::type_SUBSURFACE_COLOR: image = get_render_channel(RenderChannelType); break;
		default:
			break;
	}

	return image;
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
		case ExpoterTypeZMQ:
			exporter = new ZmqExporter();
			break;

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
