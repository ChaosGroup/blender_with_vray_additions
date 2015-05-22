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

#include "vfb_plugin_exporter_zmq.h"

using namespace VRayForBlender;

void ZmqRenderImage::update(const VRayMessage & msg) {
	const float * imgData = reinterpret_cast<const float *>(msg.getValue<VRayBaseTypes::AttrImage>()->data.get());
	
	this->w = msg.getValue<VRayBaseTypes::AttrImage>()->width;
	this->h = msg.getValue<VRayBaseTypes::AttrImage>()->height;
	
	const int imgWidth = w;
	const int imgHeight = h;

	const int rowSize = 4 * imgWidth;
	const int imageSize = rowSize * imgHeight;

	float *buf = new float[rowSize];

	float * myImage = new float[imageSize];
	memcpy(myImage, imgData, imageSize * sizeof(float));

	const int halfHeight = imgHeight / 2;
	int bottomRow = 0;

	for (int row = 0; row < halfHeight; ++row) {
		bottomRow = imgHeight - row - 1;

		const int topRowStart = row       * rowSize;
		const int bottomRowStart = bottomRow * rowSize;

		float *topRowPtr = myImage + topRowStart;
		float *bottomRowPtr = myImage + bottomRowStart;

		std::memcpy(buf, topRowPtr, rowSize * sizeof(float));
		std::memcpy(topRowPtr, bottomRowPtr, rowSize * sizeof(float));
		std::memcpy(bottomRowPtr, buf, rowSize * sizeof(float));
	}

	delete[] buf;
	delete[] pixels;
	this->pixels = myImage;
}


ZmqExporter::ZmqExporter(): m_Client(new ZmqClient())
{
}


ZmqExporter::~ZmqExporter()
{
	delete m_Client;
}


RenderImage ZmqExporter::get_image() {
	RenderImage img;

	if (this->m_CurrentImage.pixels) {
		img.w = this->m_CurrentImage.w;
		img.h = this->m_CurrentImage.h;
		img.pixels = new float[this->m_CurrentImage.w * this->m_CurrentImage.h * 4];
		memcpy(img.pixels, this->m_CurrentImage.pixels, this->m_CurrentImage.w * this->m_CurrentImage.h * 4 * sizeof(float));
	}
	
	return img;
}

void ZmqExporter::init()
{
	try {
		m_Client->setCallback([this](VRayMessage & message, ZmqWrapper * client) {
			if (message.getType() == VRayMessage::Type::SingleValue && message.getValueType() == VRayBaseTypes::ValueType::ValueTypeImage) {
				this->m_CurrentImage.update(message);
				if (callback_on_image_ready) {
					callback_on_image_ready.cb();
				}
			}
		});

		m_Client->connect("tcp://127.0.0.1:5555");
	} catch (zmq::error_t &e) {
		std::cerr << e.what() << std::endl;
	}
	
}


void ZmqExporter::free()
{
	m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::Free));
}


void ZmqExporter::sync()
{

}

void ZmqExporter::set_render_size(const int &w, const int &h) {
	m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::Resize, w, h));
}

void ZmqExporter::start()
{
	m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::Start));
}


void ZmqExporter::stop()
{
	m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::Stop));
}


AttrPlugin ZmqExporter::export_plugin(const PluginDesc &pluginDesc)
{
	if (pluginDesc.pluginID.empty()) {
		PRINT_WARN("[%s] PluginDesc.pluginID is not set!",
			pluginDesc.pluginName.c_str());
		return AttrPlugin();
	}
	const std::string & name = pluginDesc.pluginName;

	m_Client->send(VRayMessage::createMessage(name, pluginDesc.pluginID));
	
	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;
		PRINT_INFO_EX("Updating: \"%s\" => %s.%s",
			name.c_str(), pluginDesc.pluginID.c_str(), attr.attrName.c_str());

		switch (attr.attrValue.type) {
		case ValueTypeUnknown:
			break;
		case ValueTypeInt:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, VRayBaseTypes::AttrSimpleType<int>(attr.attrValue.valInt)));
			break;
		case ValueTypeFloat:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, VRayBaseTypes::AttrSimpleType<float>(attr.attrValue.valFloat)));
			break;
		case ValueTypeString:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, VRayBaseTypes::AttrSimpleType<std::string>(attr.attrValue.valString)));
			break;
		case ValueTypeColor:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valColor));
			break;
		case ValueTypeVector:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valVector));
			break;
		case ValueTypeAColor:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valAColor));
			break;
		case ValueTypePlugin:
			{
				std::string pluginData = attr.attrValue.valPlugin.plugin;;
				if (NOT(attr.attrValue.valPlugin.output.empty())) {
					pluginData.append("::");
					pluginData.append(attr.attrValue.valPlugin.output);
				}
				m_Client->send(VRayMessage::createMessage(name, attr.attrName, pluginData, true));
			}
			break;
		case ValueTypeTransform:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valTransform));
			break;
		case ValueTypeListInt:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListInt));
			break;
		case ValueTypeListFloat:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListFloat));
			break;
		case ValueTypeListVector:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListVector));
			break;
		case ValueTypeListColor:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListColor));
			break;
		case ValueTypeListPlugin:
			PRINT_INFO_EX("--- > UNIMPLEMENTED ValueTypeListPlugin");
			//VRay::ValueList pluginList;
			//for (int i = 0; i < p.attrValue.valListPlugin.getCount(); ++i) {
			//	pluginList.push_back(VRay::Value((*p.attrValue.valListPlugin)[i].plugin));
			//}
			//plug.setValue(p.attrName, VRay::Value(pluginList));
			//m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListPlugin));
			break;
		case ValueTypeListString:
			PRINT_INFO_EX("--- > UNIMPLEMENTED ValueTypeListString");
			//VRay::ValueList string_list;
			//for (int i = 0; i < p.attrValue.valListString.getCount(); ++i) {
			//	string_list.push_back(VRay::Value((*p.attrValue.valListString)[i]));
			//}
			//plug.setValue(p.attrName, VRay::Value(string_list));
			//m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListString));
			break;
		case ValueTypeMapChannels:
			PRINT_INFO_EX("--- > UNIMPLEMENTED ValueTypeMapChannels");
			//m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valMapChannels));
			break;
		case ValueTypeInstancer:
			PRINT_INFO_EX("--- > UNIMPLEMENTED ValueTypeInstancer");
			//m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valInstancer));
			break;
		default:
			PRINT_INFO_EX("--- > UNIMPLEMENTED DEFAULT");
			assert(false);
			break;
		}
	}
	m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::ExportScene, "D:/dev/exported.vrscene"));

	AttrPlugin plugin;
	plugin.plugin = name;
	
	return plugin;
}
