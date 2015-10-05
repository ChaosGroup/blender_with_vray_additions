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

#include "vfb_plugin_exporter_appsdk.h"
#include "BLI_threads.h"

#define CGR_DEBUG_APPSDK_VALUES  0


using namespace VRayForBlender;


inline VRay::Color to_vray_color(const AttrColor &c)
{
	return VRay::Color(c.r, c.g, c.b);
}


inline VRay::AColor to_vray_acolor(const AttrAColor &ac)
{
	return VRay::AColor(to_vray_color(ac.color),
	                    ac.alpha);
}


inline VRay::Vector to_vray_vector(const AttrVector &v)
{
	return VRay::Vector(v.x, v.y, v.z);
}


inline VRay::Matrix to_vray_matrix(const AttrMatrix &m)
{
	return VRay::Matrix(to_vray_vector(m.v0),
	                    to_vray_vector(m.v1),
	                    to_vray_vector(m.v2));
}


inline VRay::Transform to_vray_transform(const AttrTransform &tm)
{
	return VRay::Transform(to_vray_matrix(tm.m),
	                       to_vray_vector(tm.offs));
}


static void CbDumpMessage(VRay::VRayRenderer&, const char *msg, int level, void*)
{
	if (level <= VRay::MessageError) {
		PRINT_INFO_EX("V-Ray: Error: %s", msg);
	}
#if 0
	else if (level > VRay::MessageError && level <= VRay::MessageWarning) {
		PRINT_INFO_EX("V-Ray: Warning: %s", msg);
	}
	else if (level > VRay::MessageWarning && level <= VRay::MessageInfo) {
		PRINT_INFO_EX("V-Ray: %s", msg);
	}
#endif
}


static void CbOnImageReady(VRay::VRayRenderer&, void *userData)
{
	ExpoterCallback *cb = (ExpoterCallback*)userData;
	cb->cb();
}


static void CbOnRTImageUpdated(VRay::VRayRenderer&, VRay::VRayImage*, void *userData)
{
	ExpoterCallback *cb = (ExpoterCallback*)userData;
	cb->cb();
}


AppSDKRenderImage::AppSDKRenderImage(const VRay::VRayImage *image)
{
	if (image) {
#if 0
		PRINT_INFO_EX("Has some pixels %p (%i x %i)",
		              image->getPixelData(), image->getWidth(), image->getHeight());
#endif
		w = image->getWidth();
		h = image->getHeight();

		pixels = new float[w * h * 4];
		std::memcpy(pixels, image->getPixelData(), w * h * 4 * sizeof(float));

		delete image;
	}
}


AppSdkExporter::AppSdkExporter():
    m_vray(nullptr)
{
}


AppSdkExporter::~AppSdkExporter()
{
	free();
}


void AppSdkExporter::init()
{
	if (!m_vray) {
		try {
			VRay::RendererOptions options;
			options.noDR = true;
			options.numThreads = BLI_system_thread_count() - 1;

			m_vray = new VRay::VRayRenderer(options);
		}
		catch (std::exception &e) {
			PRINT_ERROR("Error initializing renderer! Error: \"%s\"",
			            e.what());
			m_vray = nullptr;
		}

		if (m_vray) {
			VRay::RendererOptions options(m_vray->getOptions());
			options.keepRTRunning = true;
			m_vray->setOptions(options);

			m_vray->setRenderMode(VRay::RendererOptions::RENDER_MODE_RT_CPU);
			m_vray->setOnDumpMessage(CbDumpMessage);
			m_vray->setRTImageUpdateTimeout(200);
		}
	}
}


void AppSdkExporter::free()
{
	FreePtr(m_vray);
}


void AppSdkExporter::sync()
{
	typedef std::set<std::string> RemoveKeys;
	RemoveKeys removeKeys;

	for (auto &pIt : m_used_map) {
		if (NOT(pIt.second.used)) {
			removeKeys.insert(pIt.first);

			bool res = m_vray->removePlugin(pIt.second.plugin);

			PRINT_WARN("Removing: %s [%i]",
			           pIt.second.plugin.getName().c_str(), res);

			VRay::Error err = m_vray->getLastError();
			if (err != VRay::SUCCESS) {
				PRINT_ERROR("Error removing plugin: %s",
				            err.toString().c_str());
			}
		}
	}

	if (removeKeys.size()) {
		for (RemoveKeys::iterator kIt = removeKeys.begin(); kIt != removeKeys.end(); ++kIt) {
			m_used_map.erase(*kIt);
		}
	}

	commit_changes();
}


void AppSdkExporter::start()
{
	m_vray->start();
}


void AppSdkExporter::stop()
{
	m_vray->stop();
}


RenderImage AppSdkExporter::get_image()
{
	return AppSDKRenderImage(m_vray->getImage());
}


void AppSdkExporter::set_render_size(const int &w, const int &h)
{
	m_vray->setImageSize(w, h);
}


void AppSdkExporter::set_callback_on_image_ready(ExpoterCallback cb)
{
	PluginExporter::set_callback_on_image_ready(cb);
	if (callback_on_image_ready) {
		m_vray->setOnImageReady(CbOnImageReady, (void*)&callback_on_image_ready);
	}
}


void AppSdkExporter::set_callback_on_rt_image_updated(ExpoterCallback cb)
{
	PluginExporter::set_callback_on_rt_image_updated(cb);
	if (callback_on_rt_image_updated) {
		m_vray->setOnRTImageUpdated(CbOnRTImageUpdated, (void*)&callback_on_rt_image_updated);
	}
}


void AppSdkExporter::set_camera_plugin(const std::string &pluginName)
{
	VRay::Plugin plugin = m_vray->getPlugin(pluginName, false);
	if (plugin) {
		PRINT_WARN("Setting camera plugin to: %s",
		           plugin.getName().c_str());

		m_vray->setCamera(plugin);

		VRay::Error err = m_vray->getLastError();
		if (err != VRay::SUCCESS) {
			PRINT_ERROR("Error setting camera plugin \"%s\" [%s]!",
			            plugin.getName().c_str(), err.toString().c_str());
		}
	}
}


void AppSdkExporter::commit_changes()
{
	PRINT_WARN("AppSdkExporter::commit_changes()");

	if (m_vray) {
		m_vray->commit();
	}
}


void AppSdkExporter::reset_used()
{
	for (auto &pair : m_used_map) {
		pair.second.used = false;
	}
}


// TODO: Support exporting empty list data, could happen with "Build" modifier, for example
//
AttrPlugin AppSdkExporter::export_plugin_impl(const PluginDesc &pluginDesc)
{
	AttrPlugin plugin;

	if (pluginDesc.pluginID.empty()) {
		PRINT_WARN("[%s] PluginDesc.pluginID is not set!",
		           pluginDesc.pluginName.c_str());
	}
	else {
		VRay::Plugin plug = new_plugin(pluginDesc);
		if (NOT(plug)) {
			PRINT_ERROR("Failed to create plugin: %s [%s]",
			            pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str());
		}
		else {
			plugin.plugin = plug.getName();

			for (const auto &pIt : pluginDesc.pluginAttrs) {
				const PluginAttr &p = pIt.second;
#if 1
				PRINT_INFO_EX("Updating: \"%s\" => %s.%s",
				              pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str(), p.attrName.c_str());
#endif
				if (p.attrValue.type == ValueTypeUnknown) {
					continue;
				}
				else if (p.attrValue.type == ValueTypeInt) {
					plug.setValue(p.attrName, p.attrValue.valInt);
				}
				else if (p.attrValue.type == ValueTypeFloat) {
					plug.setValue(p.attrName, p.attrValue.valFloat);
				}
				else if (p.attrValue.type == ValueTypeColor) {
					plug.setValue(p.attrName, to_vray_color(p.attrValue.valColor));
				}
				else if (p.attrValue.type == ValueTypeVector) {
					plug.setValue(p.attrName, to_vray_vector(p.attrValue.valVector));
				}
				else if (p.attrValue.type == ValueTypeAColor) {
					plug.setValue(p.attrName, to_vray_acolor(p.attrValue.valAColor));
				}
				else if (p.attrValue.type == ValueTypePlugin) {
					std::string pluginName = p.attrValue.valPlugin.plugin;
					if (NOT(p.attrValue.valPlugin.output.empty())) {
						pluginName.append("::");
						pluginName.append(p.attrValue.valPlugin.output);
					}

					plug.setValueAsString(p.attrName, pluginName);
				}
				else if (p.attrValue.type == ValueTypeTransform) {
					plug.setValue(p.attrName, to_vray_transform(p.attrValue.valTransform));
				}
				else if (p.attrValue.type == ValueTypeString) {
					plug.setValue(p.attrName, p.attrValue.valString);
				}
				else if (p.attrValue.type == ValueTypeListInt) {
					plug.setValue(p.attrName,
					              (void*)*p.attrValue.valListInt,
					              p.attrValue.valListInt.getBytesCount());
				}
				else if (p.attrValue.type == ValueTypeListFloat) {
					plug.setValue(p.attrName,
					              (void*)*p.attrValue.valListFloat,
					              p.attrValue.valListFloat.getBytesCount());
				}
				else if (p.attrValue.type == ValueTypeListVector) {
					plug.setValue(p.attrName,
					              (void*)*p.attrValue.valListVector,
					              p.attrValue.valListVector.getBytesCount());
				}
				else if (p.attrValue.type == ValueTypeListColor) {
					plug.setValue(p.attrName,
					              (void*)*p.attrValue.valListColor,
					              p.attrValue.valListColor.getBytesCount());
				}
				else if (p.attrValue.type == ValueTypeListPlugin) {
					VRay::ValueList pluginList;
					for (int i = 0; i < p.attrValue.valListPlugin.getCount(); ++i) {
						pluginList.push_back(VRay::Value((*p.attrValue.valListPlugin)[i].plugin));
					}
					plug.setValue(p.attrName, VRay::Value(pluginList));
				}
				else if (p.attrValue.type == ValueTypeListString) {
					VRay::ValueList string_list;
					for (int i = 0; i < p.attrValue.valListString.getCount(); ++i) {
						string_list.push_back(VRay::Value((*p.attrValue.valListString)[i]));
					}
					plug.setValue(p.attrName, VRay::Value(string_list));
				}
				else if (p.attrValue.type == ValueTypeMapChannels) {
					VRay::ValueList map_channels;

					int i = 0;
					for (const auto &mcIt : p.attrValue.valMapChannels.data) {
						const AttrMapChannels::AttrMapChannel &map_channel_data = mcIt.second;

						VRay::ValueList map_channel;
						map_channel.push_back(VRay::Value(i++));

						VRay::IntList faces;
						faces.resize(map_channel_data.faces.getCount());
						memcpy(&faces[0], *map_channel_data.faces, map_channel_data.faces.getBytesCount());

						VRay::VectorList vertices;
						vertices.resize(map_channel_data.vertices.getCount());
						memcpy(&vertices[0], *map_channel_data.vertices, map_channel_data.vertices.getBytesCount());

						map_channel.push_back(VRay::Value(vertices));
						map_channel.push_back(VRay::Value(faces));

						map_channels.push_back(VRay::Value(map_channel));
					}

					plug.setValue(p.attrName, VRay::Value(map_channels));
				}
				else if (p.attrValue.type == ValueTypeInstancer) {
					VRay::ValueList instancer;
					instancer.push_back(VRay::Value(p.attrValue.valInstancer.frameNumber));

					for (int i = 0; i < p.attrValue.valInstancer.data.getCount(); ++i) {
						const AttrInstancer::Item &item = (*p.attrValue.valInstancer.data)[i];

						VRay::ValueList instance;
						instance.push_back(VRay::Value(item.index));
						instance.push_back(VRay::Value(to_vray_transform(item.tm)));
						instance.push_back(VRay::Value(to_vray_transform(item.vel)));
						instance.push_back(VRay::Value(m_vray->getPlugin(item.node.plugin)));

						instancer.push_back(VRay::Value(instance));
					}

					plug.setValue(p.attrName, VRay::Value(instancer));
				}
			}
		}
	}

	return plugin;
}


int AppSdkExporter::remove_plugin(const std::string &pluginName)
{
	bool res = true;

	if (m_vray) {
		PRINT_WARN("Removing plugin: \"%s\"",
		           pluginName.c_str());

		m_pluginManager.remove(pluginName);

		res = remove_plugin_impl(m_vray->getPlugin(pluginName, false));
	}

	return res;
}


int AppSdkExporter::remove_plugin_impl(VRay::Plugin plugin)
{
	int res = true;

	if (m_vray) {
		if (plugin) {
			res = m_vray->removePlugin(plugin);

			VRay::Error err = m_vray->getLastError();
			if (err != VRay::SUCCESS) {
				PRINT_ERROR("Error removing plugin \"%s\" [%s]!",
				            plugin.getName().c_str(), err.toString().c_str());
			}
		}
	}

	return res;
}


void AppSdkExporter::export_vrscene(const std::string &filepath)
{
	VRay::VRayExportSettings exportParams;
	exportParams.useHexFormat = false;
	exportParams.compressed = false;

	int res = m_vray->exportScene(filepath, &exportParams);
	if (res) {
		PRINT_ERROR("Error exporting scene!");
	}

	VRay::Error err = m_vray->getLastError();
	if (err != VRay::SUCCESS) {
		PRINT_ERROR("Error: %s",
		            err.toString().c_str());
	}
}


VRay::Plugin AppSdkExporter::new_plugin(const PluginDesc &pluginDesc)
{
	VRay::Plugin plug = m_vray->newPlugin(pluginDesc.pluginName, pluginDesc.pluginID);

	if (!pluginDesc.pluginName.empty()) {
		m_used_map[pluginDesc.pluginName] = PluginUsed(plug);
	}

	return plug;
}

#endif // USE_BLENDER_VRAY_APPSDK
