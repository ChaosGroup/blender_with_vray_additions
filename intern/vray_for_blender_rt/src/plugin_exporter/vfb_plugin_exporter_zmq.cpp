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
#include "jpeglib.h"

static std::mutex imgMutex;

using namespace VRayForBlender;

struct JpegErrorManager {
	jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

static void jpegErrorExit(j_common_ptr cinfo) {
	JpegErrorManager * myerr = (JpegErrorManager*)cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(myerr->setjmp_buffer, 1);
}

static float * jpegToPixelData(unsigned char * data, int size) {
	jpeg_decompress_struct jpegInfo;
	JpegErrorManager jpegError;

	jpegInfo.err = jpeg_std_error(&jpegError.pub);

	jpegError.pub.error_exit = jpegErrorExit;

	if (setjmp(jpegError.setjmp_buffer)) {
		jpeg_destroy_decompress(&jpegInfo);
		return nullptr;
	}

	jpeg_create_decompress(&jpegInfo);
	jpeg_mem_src(&jpegInfo, data, size);

	if (jpeg_read_header(&jpegInfo, TRUE) != JPEG_HEADER_OK) {
		return nullptr;
	}

	jpegInfo.out_color_space = JCS_EXT_RGBA;
	
	if (!jpeg_start_decompress(&jpegInfo)) {
		return nullptr;
	}

	int rowStride = jpegInfo.output_width * jpegInfo.output_components;
	float * imageData = new float[jpegInfo.output_height * rowStride];
	JSAMPARRAY buffer = (*jpegInfo.mem->alloc_sarray)((j_common_ptr)&jpegInfo, JPOOL_IMAGE, rowStride, 1);

	int c = 0;
	while (jpegInfo.output_scanline < jpegInfo.output_height) {
		jpeg_read_scanlines(&jpegInfo, buffer, 1);

		float * dest = imageData + c * rowStride;
		unsigned char * source = buffer[0];

		for (int r = 0; r < jpegInfo.image_width * jpegInfo.output_components; ++r) {
			dest[r] = source[r] / 255.f;
		}

		++c;
	}

	jpeg_finish_decompress(&jpegInfo);
	jpeg_destroy_decompress(&jpegInfo);

	return imageData;
}

void ZmqRenderImage::update(const VRayMessage & msg) {
	auto img = msg.getValue<VRayBaseTypes::AttrImage>();
	const float * imgData = nullptr;
	bool freeData = false;

	if (img->imageType == VRayBaseTypes::AttrImage::ImageType::JPG) {
		imgData = jpegToPixelData(reinterpret_cast<unsigned char*>(img->data.get()), img->size);
		freeData = true;
	} else if (img->imageType == VRayBaseTypes::AttrImage::ImageType::RGBA_REAL) {
		imgData = reinterpret_cast<const float *>(img->data.get());
	}

	if (!imgData) {
		return;
	}
	
	const int imgWidth = msg.getValue<VRayBaseTypes::AttrImage>()->width;
	const int imgHeight = msg.getValue<VRayBaseTypes::AttrImage>()->height;

	const int rowSize = 4 * imgWidth;
	const int imageSize = rowSize * imgHeight;

	float *buf = new float[rowSize];

	float * myImage = new float[imageSize];
	memcpy(myImage, imgData, imageSize * sizeof(float));

	const int halfHeight = imgHeight / 2;
	int bottomRow = 0;

	for (int row = 0; row < halfHeight; ++row) {
		bottomRow = imgHeight - row - 1;

		const int topRowStart =    row       * rowSize;
		const int bottomRowStart = bottomRow * rowSize;

		float *topRowPtr = myImage + topRowStart;
		float *bottomRowPtr = myImage + bottomRowStart;

		std::memcpy(buf, topRowPtr, rowSize * sizeof(float));
		std::memcpy(topRowPtr, bottomRowPtr, rowSize * sizeof(float));
		std::memcpy(bottomRowPtr, buf, rowSize * sizeof(float));
	}

	delete[] buf;
	if (freeData) {
		delete[] imgData;
	}

	{
		std::unique_lock<std::mutex> lock(imgMutex);

		this->w = imgWidth;
		this->h = imgHeight;
		delete[] pixels;
		this->pixels = myImage;
	}
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
		std::unique_lock<std::mutex> lock(imgMutex);

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
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListPlugin));
			break;
		case ValueTypeListString:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valListString));
			break;
		case ValueTypeMapChannels:
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valMapChannels));
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

	AttrPlugin plugin;
	plugin.plugin = name;
	
	return plugin;
}
