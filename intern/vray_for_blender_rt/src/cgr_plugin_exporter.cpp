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

#include "cgr_plugin_exporter.h"


using namespace VRayForBlender;


static float* FlipImageRows(const VRay::VRayImage *image)
{
	const int imgWidth  = image->getWidth();
	const int imgHeight = image->getHeight();

	const int rowSize = 4 * imgWidth;
	const int imageSize = rowSize * imgHeight;

	float *buf = new float[rowSize];

	float *myImage = new float[imageSize];

	std::memcpy(myImage, image->getPixelData(), imageSize * sizeof(float));

	const int halfHeight = imgHeight / 2;
	int bottomRow = 0;

	for(int row = 0; row < halfHeight; ++row) {
		bottomRow = imgHeight - row - 1;

		const int topRowStart    = row       * rowSize;
		const int bottomRowStart = bottomRow * rowSize;

		float *topRowPtr    = myImage + topRowStart;
		float *bottomRowPtr = myImage + bottomRowStart;

		std::memcpy(buf,          topRowPtr,    rowSize * sizeof(float));
		std::memcpy(topRowPtr,    bottomRowPtr, rowSize * sizeof(float));
		std::memcpy(bottomRowPtr, buf,          rowSize * sizeof(float));
	}

	delete [] buf;

	return myImage;
}


static void CbDumpMessage(VRay::VRayRenderer&, const char *msg, int level, void*)
{
	if (level <= VRay::MessageError) {
		PRINT_PREFIX("V-Ray", "Error: %s", msg);
	}
#if 0
	else if (level > VRay::MessageError && level <= VRay::MessageWarning) {
		PRINT_PREFIX("V-Ray", "Warning: %s", msg);
	}
	else if (level > VRay::MessageWarning && level <= VRay::MessageInfo) {
		PRINT_PREFIX("V-Ray", "%s", msg);
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


AppSDKRenderImage::AppSDKRenderImage(VRay::VRayImage *image)
{
	w = image->getWidth();
	h = image->getHeight();
	pixels = FlipImageRows(image);
}


PluginExporter::~PluginExporter()
{
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
			m_vray = new VRay::VRayRenderer();
		}
		catch (...) {
			m_vray = nullptr;
		}

		if (!m_vray) {
			PRINT_ERROR("Error initializing renderer!");
		}
		else {
			m_vray->setRenderMode(VRay::RendererOptions::RENDER_MODE_RT_CPU);
			m_vray->setOnDumpMessage(CbDumpMessage);

			VRay::RendererOptions options = m_vray->getOptions();
			// options.numThreads = 4;

			m_vray->setOptions(options);
		}
	}
}


void AppSdkExporter::free()
{
	FreePtr(m_vray);
}


void AppSdkExporter::sync()
{
#if 0
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
		m_vray->stop();

		for (RemoveKeys::iterator kIt = removeKeys.begin(); kIt != removeKeys.end(); ++kIt) {
			m_used_map.erase(*kIt);
		}

		m_vray->start();
	}
#endif

#if 0
	int res = m_vray->exportScene("/tmp/vrayblender_bdancer/scene_app_sdk.vrscene");
	if (res) {
		PRINT_ERROR("Error exporting scene!");
	}
	VRay::Error err = m_vray->getLastError();
	if (err != VRay::SUCCESS) {
		PRINT_ERROR("Error: %s",
		            err.toString().c_str());
	}
#endif
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
	RenderImage renderImage;

	VRay::VRayImage *image = m_vray->getImage();
	if (image && image->getPixelData()) {
		PRINT_INFO_EX("Have some pixels (%i x %i)",
		              image->getWidth(), image->getHeight());

		renderImage = AppSDKRenderImage(image);
	}

	return renderImage;
}


void AppSdkExporter::set_render_size(const int &w, const int &h)
{
	m_vray->setWidth(w);
	m_vray->setHeight(h);
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


void AppSdkExporter::reset_used()
{
	for (auto &pair : m_used_map) {
		pair.second.used = false;
	}
}


AttrPlugin AppSdkExporter::export_plugin(const PluginDesc &pluginDesc)
{
	AttrPlugin plugin;

#define CGR_DEBUG_APPSDK_VALUES  0
	if (pluginDesc.pluginID.empty()) {
		// NOTE: Could be done intentionally to skip plugin creation
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
			plugin.name = plug.getName();

			for (const auto &pIt : pluginDesc.pluginAttrs) {
				const PluginAttr &p = pIt;
#if CGR_DEBUG_APPSDK_VALUES
				PRINT_INFO("Setting plugin parameter: \"%s\" %s.%s",
				           pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str(), p.paramName.c_str());
#endif
				if (p.paramType == PluginAttr::AttrTypeIgnore) {
					continue;
				}

				if (p.paramType == PluginAttr::AttrTypeInt) {
					plug.setValue(p.paramName, p.paramValue.valInt);
				}
				else if (p.paramType == PluginAttr::AttrTypeFloat) {
					plug.setValue(p.paramName, p.paramValue.valFloat);
				}
				else if (p.paramType == PluginAttr::AttrTypeColor) {
					plug.setValue(p.paramName, to_vray_color(p.paramValue.valColor));
				}
				else if (p.paramType == PluginAttr::AttrTypeVector) {
					plug.setValue(p.paramName, to_vray_vector(p.paramValue.valVector));
				}
				else if (p.paramType == PluginAttr::AttrTypeAColor) {
					plug.setValue(p.paramName, to_vray_acolor(p.paramValue.valAColor));
				}
				else if (p.paramType == PluginAttr::AttrTypePlugin) {
					std::string pluginName = p.paramValue.valPlugin.name;
					if (NOT(p.paramValue.valPluginOutput.empty())) {
						pluginName.append("::");
						pluginName.append(p.paramValue.valPluginOutput);
					}

					plug.setValueAsString(p.paramName, pluginName);
				}
				else if (p.paramType == PluginAttr::AttrTypeTransform) {
					plug.setValue(p.paramName, to_vray_transform(p.paramValue.valTransform));
				}
				else if (p.paramType == PluginAttr::AttrTypeString) {
					plug.setValue(p.paramName, p.paramValue.valString);
#if CGR_DEBUG_APPSDK_VALUES
					PRINT_INFO("AttrTypeString:  %s [%s] = %s",
					           p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
				}
#if 0
				else if (p.paramType == PluginAttr::AttrTypeListPlugin) {
					plug.setValue(p.paramName, VRay::Value(p.paramValue.valListValue));
				}
#endif
#if 0
				else if (p.paramType == PluginAttr::AttrTypeListValue) {
					plug.setValue(p.paramName, VRay::Value(p.paramValue.valListValue));
#if CGR_DEBUG_APPSDK_VALUES
					PRINT_INFO("AttrTypeListValue:  %s [%s] = %s",
					           p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
				}
#endif
				else if (p.paramType == PluginAttr::AttrTypeListInt) {
					plug.setValue(p.paramName,
					              (void*)*p.paramValue.valListInt,
					              p.paramValue.valListInt.count * sizeof(int));
				}
				else if (p.paramType == PluginAttr::AttrTypeListFloat) {
					plug.setValue(p.paramName,
					              (void*)*p.paramValue.valListFloat,
					              p.paramValue.valListFloat.count * sizeof(float));
				}
				else if (p.paramType == PluginAttr::AttrTypeListVector) {
					plug.setValue(p.paramName,
					              (void*)*p.paramValue.valListVector,
					              p.paramValue.valListVector.count * sizeof(AttrVector));
				}
				else if (p.paramType == PluginAttr::AttrTypeListColor) {
					plug.setValue(p.paramName,
					              (void*)*p.paramValue.valListColor,
					              p.paramValue.valListColor.count * sizeof(AttrColor));
				}
			}
		}
	}

	return plugin;
}


VRay::Plugin AppSdkExporter::new_plugin(const PluginDesc &pluginDesc)
{
	VRay::Plugin plug = m_vray->newPlugin(pluginDesc.pluginName, pluginDesc.pluginID);

	if (NOT(pluginDesc.pluginName.empty())) {
		m_used_map[pluginDesc.pluginName.c_str()] = PluginUsed(plug);
	}

	return plug;
}


VRayForBlender::PluginExporter* VRayForBlender::ExporterCreate(VRayForBlender::ExpoterType type)
{
	PluginExporter *exporter = nullptr;

	switch (type) {
		case ExpoterTypeFile:
			break;
		case ExpoterTypeCloud:
			break;
		case ExpoterTypeZMQ:
			break;
		case ExpoterTypeAppSDK:
			exporter = new AppSdkExporter();
			break;
		default:
			break;
	}

	if (exporter) {
		exporter->init();
	}

	return exporter;
}


void VRayForBlender::ExporterDelete(VRayForBlender::PluginExporter *exporter)
{
	FreePtr(exporter);
}
