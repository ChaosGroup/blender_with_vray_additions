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

#include <boost/algorithm/string.hpp>


#define CGR_DEBUG_APPSDK_VALUES  0

#define DUMP_MESSAGE(...) {\
	fprintf(stdout, COLOR_BLUE "V-Ray Core" COLOR_DEFAULT ": "); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout); }


using namespace VRayForBlender;


AppSdkInit AppSdkExporter::vrayInit;


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
	std::string message(msg);
	boost::erase_all(message, "\n");

	if (level <= VRay::MessageError) {
		DUMP_MESSAGE(COLOR_RED "Error: %s" COLOR_DEFAULT, message.c_str());
	}
	else if (level > VRay::MessageError && level <= VRay::MessageWarning) {
		DUMP_MESSAGE(COLOR_YELLOW "Warning: %s" COLOR_DEFAULT, message.c_str());
	}
	else if (level > VRay::MessageWarning && level <= VRay::MessageInfo) {
		DUMP_MESSAGE("%s", message.c_str());
	}
}

AppSDKRenderImage::AppSDKRenderImage(const VRay::VRayImage *image, VRay::RenderElement::PixelFormat pixelFormat)
{
	if (image) {
		w = image->getWidth();
		h = image->getHeight();

		switch (pixelFormat) {
		case VRay::RenderElement::PF_BW_FLOAT:
			channels = 1;
			break;
		case VRay::RenderElement::PF_RGB_FLOAT:
			channels = 3;
			break;
		case VRay::RenderElement::PF_RGBA_FLOAT:
			channels = 4;
			break;
		default:
			break;
		}

		const int pixelCount = w * h;

		pixels = new float[pixelCount * channels];

		const VRay::AColor *imagePixels = image->getPixelData();

		for (int p = 0; p < pixelCount; ++p) {
			const VRay::AColor &imagePixel = imagePixels[p];
			float *bufferPixel = pixels + (p * channels);

			switch (channels) {
			case 4: bufferPixel[3] = imagePixel.alpha;
			case 3: bufferPixel[2] = imagePixel.color.b;
			case 2: bufferPixel[1] = imagePixel.color.g;
			case 1: bufferPixel[0] = imagePixel.color.r;
			}
		}

		delete image;
	}
}


AppSdkExporter::AppSdkExporter()
    : m_vray(nullptr)
    , m_started(false)
{
}


AppSdkExporter::~AppSdkExporter()
{
	if (m_vray) {
		if (m_vray->isRendering()) {
			m_vray->stop();
		}
	}
	free();
}


void AppSdkExporter::init()
{
	if (AppSdkExporter::vrayInit && !m_vray) {
		try {
			VRay::RendererOptions options;
			options.keepRTRunning = true;
			options.noDR = true;
			options.numThreads = BLI_system_thread_count() - 1;
			options.showFrameBuffer = false;
			options.renderMode = VRay::RendererOptions::RENDER_MODE_RT_CPU;

			m_vray = new VRay::VRayRenderer(options);
		}
		catch (std::exception &e) {
			PRINT_ERROR("Error initializing renderer! Error: \"%s\"",
			            e.what());
			m_vray = nullptr;
		}

		if (m_vray) {
			m_vray->setAutoCommit(false);
			// m_vray->setRenderMode(VRay::RendererOptions::RENDER_MODE_RT_GPU);
			// m_vray->setOnBucketReady<AppSdkExporter, &AppSdkExporter::bucket_ready>(*this);

			m_vray->setOnDumpMessage<AppSdkExporter, &AppSdkExporter::CbOnProgress>(*this);
			m_vray->setRTImageUpdateTimeout(200);
		}
	}
}

void AppSdkExporter::CbOnImageReady(VRay::VRayRenderer&, void *userData)
{
	PRINT_INFO_EX("AppSdkExporter::CbOnImageReady");
	if (callback_on_image_ready) {
		callback_on_image_ready.cb();
	}
}


void AppSdkExporter::CbOnRTImageUpdated(VRay::VRayRenderer&, VRay::VRayImage *img, void *userData)
{
	if (!is_viewport) {
		m_bucket_image = AppSDKRenderImage(m_vray->getImage());
		m_bucket_image.flip();
		m_bucket_image.resetAlpha();
		m_bucket_image.clamp();
	}
	if (callback_on_rt_image_updated) {
		callback_on_rt_image_updated.cb();
	}
}


void AppSdkExporter::CbOnProgress(VRay::VRayRenderer &, const char * msg, int, void *)
{
	on_message_update("", msg);
}


void AppSdkExporter::free()
{
	FreePtr(m_vray);
}


void AppSdkExporter::sync()
{
	commit_changes();
}


void AppSdkExporter::start()
{
	PRINT_INFO_EX("AppSdkExporter::start()");
	m_started = true;
	m_vray->setOnBucketReady<AppSdkExporter, &AppSdkExporter::bucket_ready>(*this);
	m_vray->start();
	if (callback_on_rt_image_updated) {
		m_vray->setOnRTImageUpdated<AppSdkExporter, &AppSdkExporter::CbOnRTImageUpdated>(*this);
	}
	if (callback_on_image_ready) {
		m_vray->setOnImageReady<AppSdkExporter, &AppSdkExporter::CbOnImageReady>(*this);
	}
}


void AppSdkExporter::stop()
{
	m_vray->stop();
}


void AppSdkExporter::bucket_ready(VRay::VRayRenderer &renderer, int x, int y, const char *host, VRay::VRayImage *img, void *)
{
	m_bucket_image.updateRegion(reinterpret_cast<const float *>(img->getPixelData()), x, y, img->getWidth(), img->getHeight());

	if (callback_on_rt_image_updated) {
		callback_on_rt_image_updated.cb();
	}
}


RenderImage AppSdkExporter::get_image()
{
	if (m_started && m_vray->isImageReady() || is_viewport) {
		m_vray->setOnBucketReady(nullptr);
		RenderImage img = AppSDKRenderImage(m_vray->getImage());
		if (!is_viewport) {
			img.flip();
			img.resetAlpha();
			img.clamp();
		}
		return img;
	} else {
		return RenderImage::deepCopy(m_bucket_image);
	}
}


RenderImage AppSdkExporter::get_render_channel(RenderChannelType channelType)
{
	RenderImage renderChannel;

	if (m_vray) {
		try {
			VRay::RenderElements renderElements = m_vray->getRenderElements();
			VRay::RenderElement  renderElement = renderElements.getByType((VRay::RenderElement::Type)channelType);
			if (renderElement) {
				VRay::RenderElement::PixelFormat pixelFormat = renderElement.getDefaultPixelFormat();

				//PRINT_INFO_EX("Found render channel: %i (pixel format %i)",
				//              channelType, pixelFormat);

				renderChannel = AppSDKRenderImage(renderElement.getImage(), pixelFormat);
			}
		}
		catch (VRay::VRayException &e) {
			PRINT_WARN("VRayException %s", e.what());
		}
		catch (...) {
			PRINT_WARN("Ignored exception AppSdkExporter::get_render_channel");
		}
	}

	return renderChannel;
}


void AppSdkExporter::set_render_size(const int &w, const int &h)
{
	if (m_bucket_image.w != w && m_bucket_image.h != h) {
		delete[] m_bucket_image.pixels;
		m_bucket_image.w = w;
		m_bucket_image.h = h;
		m_bucket_image.channels = 4;
		m_bucket_image.pixels = new float[w * h * m_bucket_image.channels];
	}

	m_vray->setImageSize(w, h);
}


void AppSdkExporter::set_callback_on_image_ready(ExpoterCallback cb)
{
	PluginExporter::set_callback_on_image_ready(cb);
}


void AppSdkExporter::set_callback_on_rt_image_updated(ExpoterCallback cb)
{
	PluginExporter::set_callback_on_rt_image_updated(cb);
}


void AppSdkExporter::set_callback_on_message_updated(PluginExporter::UpdateMessageCb cb)
{
	PluginExporter::set_callback_on_message_updated(cb);
	if (on_message_update) {
		// m_vray->setOnProgress<AppSdkExporter, &AppSdkExporter::CbOnProgress>(*this);
	}
}


void AppSdkExporter::set_camera_plugin(const std::string &pluginName)
{
	if (m_vray) {
		VRay::Plugin plugin = m_vray->getPlugin(pluginName, false);
		if (plugin) {
			PRINT_WARN("Setting camera plugin to: %s",
			           plugin.getName());

			m_vray->setCamera(plugin);

			VRay::Error err = m_vray->getLastError();
			if (err != VRay::SUCCESS) {
				PRINT_ERROR("Error setting camera plugin \"%s\" [%s]!",
				            plugin.getName(), err.toString());
			}
		}
	}
}


void AppSdkExporter::set_render_mode(RenderMode renderMode)
{
	if (m_vray) {
		VRay::RendererOptions::RenderMode _curMode = m_vray->getRenderMode();
		VRay::RendererOptions::RenderMode _newMode = (VRay::RendererOptions::RenderMode)renderMode;
		if (_curMode != _newMode) {
			PRINT_WARN("AppSdkExporter::set_render_mode(%i)", _newMode);

			m_vray->setRenderMode(_newMode);
			m_vray->stop();
			m_vray->start();
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


void AppSdkExporter::show_frame_buffer()
{
	if (m_vray) {
		m_vray->vfb.show(true, false);
	}
}


AttrPlugin AppSdkExporter::export_plugin_impl(const PluginDesc &pluginDesc)
{
	AttrPlugin plugin;

	if (pluginDesc.pluginID.empty()) {
		PRINT_WARN("[%s] PluginDesc.pluginID is not set!",
		           pluginDesc.pluginName.c_str());
	}
	else {
		VRay::Plugin plug =  m_vray->newPlugin(pluginDesc.pluginName, pluginDesc.pluginID);
		if (NOT(plug)) {
			PRINT_ERROR("Failed to create plugin: %s [%s]",
			            pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str());
		}
		else {
			plugin.plugin = plug.getName();

			for (const auto &pIt : pluginDesc.pluginAttrs) {
				const PluginAttr &p = pIt.second;
#if 0
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


int AppSdkExporter::remove_plugin_impl(const std::string &pluginName)
{
	bool res = true;

	if (m_vray) {
		PRINT_WARN("Removing plugin: \"%s\"",
		           pluginName.c_str());

		m_pluginManager.remove(pluginName);

		VRay::Plugin plugin = m_vray->getPlugin(pluginName, false);
		if (plugin) {
			res = m_vray->removePlugin(plugin);

			VRay::Error err = m_vray->getLastError();
			if (err != VRay::SUCCESS) {
				PRINT_ERROR("Error removing plugin \"%s\" [%s]!",
				            plugin.getName(), err.toString());
			}
		}
	}

	return res;
}


void AppSdkExporter::export_vrscene(const std::string &filepath)
{
	PRINT_WARN("AppSdkExporter::export_vrscene()");

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
		            err.toString());
	}
}

#endif // USE_BLENDER_VRAY_APPSDK
