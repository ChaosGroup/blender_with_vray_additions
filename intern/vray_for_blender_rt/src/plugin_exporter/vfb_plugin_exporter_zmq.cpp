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

#ifdef USE_BLENDER_VRAY_ZMQ

#include "vfb_plugin_exporter_zmq.h"
#include "vfb_export_settings.h"
#include "jpeglib.h"
#include <setjmp.h>


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

void ZmqExporter::ZmqRenderImage::update(const VRayMessage & msg, ZmqExporter * exp) {
	auto img = msg.getValue<VRayBaseTypes::AttrImage>();

	if (img->imageType == VRayBaseTypes::AttrImage::ImageType::JPG) {
		float * imgData = jpegToPixelData(reinterpret_cast<unsigned char*>(img->data.get()), img->size);

		std::unique_lock<std::mutex> lock(exp->m_ImgMutex);

		this->w = img->width;
		this->h = img->height;
		delete[] pixels;
		this->pixels = imgData;
	} else if (img->imageType == VRayBaseTypes::AttrImage::ImageType::RGBA_REAL) {
		const float * imgData = reinterpret_cast<const float *>(img->data.get());
		float * myImage = new float[img->width * img->height * 4];

		std::unique_lock<std::mutex> lock(exp->m_ImgMutex);

		memcpy(myImage, imgData, img->width * img->height * 4 * sizeof(float));

		this->w = img->width;
		this->h = img->height;
		delete[] pixels;
		this->pixels = myImage;
	}
}


ZmqExporter::ZmqExporter():
	m_Client(new ZmqClient())
{
}


ZmqExporter::~ZmqExporter()
{
	stop();
	free();
	m_Client->setFlushOnExit(true);
	delete m_Client;
}


RenderImage ZmqExporter::get_image() {
	RenderImage img;

	if (this->m_CurrentImage.pixels) {
		std::unique_lock<std::mutex> lock(m_ImgMutex);

		img.w = this->m_CurrentImage.w;
		img.h = this->m_CurrentImage.h;
		img.pixels = new float[this->m_CurrentImage.w * this->m_CurrentImage.h * 4];
		memcpy(img.pixels, this->m_CurrentImage.pixels, this->m_CurrentImage.w * this->m_CurrentImage.h * 4 * sizeof(float));
	}

	return img;
}

void ZmqExporter::zmqCallback(VRayMessage & message, ZmqWrapper * client) {
	const auto msgType = message.getType();
	if (msgType == VRayMessage::Type::SingleValue) {
		switch (message.getValueType()) {
		case VRayBaseTypes::ValueType::ValueTypeImage:
			this->m_CurrentImage.update(message, this);
			if (this->callback_on_rt_image_updated) {
				callback_on_rt_image_updated.cb();
			}
			break;
		case VRayBaseTypes::ValueType::ValueTypeString:
			if (this->on_message_update) {
				auto msg = message.getValue<VRayBaseTypes::AttrSimpleType<std::string>>()->m_Value;
				auto newLine = msg.find_first_of("\n\r");
				if (newLine != std::string::npos) {
					msg.resize(newLine);
				}

				this->on_message_update("", msg.c_str());
			}
			break;
		}
	} else if (msgType == VRayMessage::Type::ChangeRenderer) {
		if (message.getRendererAction() == VRayMessage::RendererAction::FrameRendered) {
			this->last_rendered_frame = message.getValue<VRayBaseTypes::AttrSimpleType<int>>()->m_Value;
		}
	}
}

void ZmqExporter::init()
{
	try {
		using std::placeholders::_1;
		using std::placeholders::_2;

		m_Client->setCallback(std::bind(&ZmqExporter::zmqCallback, this, _1, _2));

		char portStr[32];
		snprintf(portStr, 32, ":%d", this->m_ServerPort);
		m_Client->connect(("tcp://" + this->m_ServerAddress + portStr).c_str());

		auto mode = this->animation_settings.use ? VRayMessage::RendererType::Animation : VRayMessage::RendererType::RT;
		m_Client->send(VRayMessage::createMessage(mode));
		m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::Init));
	} catch (zmq::error_t &e) {
		PRINT_ERROR("Failed to initialize ZMQ client\n%s", e.what());
	}
}

void ZmqExporter::checkZmqClient()
{
	if (!m_Client->connected()) {
		// we can't connect dont retry
		return;
	}

	if (!m_Client->good()) {
		delete m_Client;
		m_Client = new ZmqClient();
		this->init();
	}
}

void ZmqExporter::set_settings(const ExporterSettings & settings)
{
	this->m_ServerPort = settings.zmq_server_port;
	this->m_ServerAddress = settings.zmq_server_address;
	this->animation_settings = settings.settings_animation;
	if (this->animation_settings.use) {
		this->last_rendered_frame = this->animation_settings.frame_start - 1;
	}
}


void ZmqExporter::free()
{
	checkZmqClient();
	m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::Free));
}

void ZmqExporter::sync()
{
}

void ZmqExporter::set_render_size(const int &w, const int &h)
{
	checkZmqClient();
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

void ZmqExporter::export_vrscene(const std::string &filepath)
{
	checkZmqClient();
	m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::ExportScene, filepath));
}

AttrPlugin ZmqExporter::export_plugin_impl(const PluginDesc & pluginDesc)
{
	checkZmqClient();

	if (pluginDesc.pluginID.empty()) {
		PRINT_WARN("[%s] PluginDesc.pluginID is not set!",
			pluginDesc.pluginName.c_str());
		return AttrPlugin();
	}

	const std::string & name = pluginDesc.pluginName;
	AttrPlugin plugin(name);

	if (pluginDesc.pluginAttrs.empty()) {
		return plugin;
	}

	m_Client->send(VRayMessage::createMessage(name, pluginDesc.pluginID));

	double lastTime = 0;

	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;

		if (lastTime != attr.time) {
			m_Client->send(VRayMessage::createMessage(VRayMessage::RendererAction::SetCurrentTime, attr.time));
			lastTime = attr.time;
		}

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
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valPlugin));
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
			m_Client->send(VRayMessage::createMessage(name, attr.attrName, attr.attrValue.valInstancer));
			break;
		default:
			PRINT_INFO_EX("--- > UNIMPLEMENTED DEFAULT");
			assert(false);
			break;
		}
	}

	return plugin;
}

#endif // USE_BLENDER_VRAY_ZMQ
