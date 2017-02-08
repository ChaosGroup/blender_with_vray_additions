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
#include "vfb_export_settings.h"
#include "vfb_params_json.h"
#include "jpeglib.h"

#include "BLI_utildefines.h"

#include <setjmp.h>
#include <limits>

using namespace VRayForBlender;

std::mutex ZmqServer::clientMtx;
ClientPtr ZmqServer::serverCheck;

bool ZmqServer::isRunning() {
	std::lock_guard<std::mutex> lock(clientMtx);
	return serverCheck && serverCheck->good() && serverCheck->connected();
}

bool ZmqServer::start(const char * addr) {
	std::lock_guard<std::mutex> lock(clientMtx);

	if (!serverCheck) {
		PRINT_INFO_EX("Starting hearbeat client for %s", addr);
		serverCheck.reset(new ZmqClient(true));
		serverCheck->connect(addr);
		if (serverCheck->connected()) {
			return true;
		}
	} else {
		PRINT_ERROR("Heartbeat client already running...");
		// return true as we are running good.
		if (serverCheck->good() && serverCheck->connected()) {
			return true;
		}
	}

	return false;
}

bool ZmqServer::stop() {
	std::lock_guard<std::mutex> lock(clientMtx);

	if (serverCheck) {
		PRINT_INFO_EX("Stopping hearbeat client... ");
		if (serverCheck->good() && serverCheck->connected()) {
			serverCheck->stopServer();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		serverCheck->syncStop();
		serverCheck.reset();
		PRINT_INFO_EX("... done.");
		return true;
	}

	PRINT_ERROR("No zmq heartbeat client running...");
	return false;
}


struct JpegErrorManager {
	jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

static void jpegErrorExit(j_common_ptr cinfo) {
	JpegErrorManager * myerr = (JpegErrorManager*)cinfo->err;
	char jpegErrMsg[JMSG_LENGTH_MAX + 1];
	(*cinfo->err->format_message) (cinfo, jpegErrMsg);
	PRINT_WARN("Error in jpeg decompress [%s]!", jpegErrMsg);
	longjmp(myerr->setjmp_buffer, 1);
}


static void init_source(j_decompress_ptr) {}

static boolean fill_input_buffer (j_decompress_ptr cinfo) {
	unsigned char *buf = (unsigned char *) cinfo->src->next_input_byte - 2;

	buf[0] = (JOCTET) 0xFF;
	buf[1] = (JOCTET) JPEG_EOI;

	cinfo->src->next_input_byte = buf;
	cinfo->src->bytes_in_buffer = 2;

	return TRUE;
}

static void skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
    struct jpeg_source_mgr* src = (struct jpeg_source_mgr*) cinfo->src;

    if (num_bytes > 0) {
        src->next_input_byte += (size_t) num_bytes;
        src->bytes_in_buffer -= (size_t) num_bytes;
    }
}

static void term_source(j_decompress_ptr) {}

static void jpeg_mem_src_own(j_decompress_ptr cinfo, const unsigned char * buffer, int nbytes) {
    struct jpeg_source_mgr* src;

    if (cinfo->src == NULL) {   /* first time for this JPEG object? */
        cinfo->src = (struct jpeg_source_mgr *)
            (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
            sizeof(struct jpeg_source_mgr));
    }

    src = (struct jpeg_source_mgr*) cinfo->src;
    src->init_source = init_source;
    src->fill_input_buffer = fill_input_buffer;
    src->skip_input_data = skip_input_data;
    src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
    src->term_source = term_source;
    src->bytes_in_buffer = nbytes;
    src->next_input_byte = (JOCTET*)buffer;
}

static float * jpegToPixelData(unsigned char * data, int size, int &channels) {
	jpeg_decompress_struct jpegInfo;
	JpegErrorManager jpegError;

	jpegInfo.err = jpeg_std_error(&jpegError.pub);

	jpegError.pub.error_exit = jpegErrorExit;

	if (setjmp(jpegError.setjmp_buffer)) {
		PRINT_WARN("Longjmp after jpeg error!");
		jpeg_destroy_decompress(&jpegInfo);
		return nullptr;
	}

	jpeg_create_decompress(&jpegInfo);
	jpeg_mem_src_own(&jpegInfo, data, size);

	if (jpeg_read_header(&jpegInfo, TRUE) != JPEG_HEADER_OK) {
		return nullptr;
	}

	jpegInfo.out_color_space = JCS_EXT_RGBX;

	if (!jpeg_start_decompress(&jpegInfo)) {
		return nullptr;
	}

	channels = jpegInfo.output_components;
	int rowStride = jpegInfo.output_width * jpegInfo.output_components;
	float * imageData = new float[jpegInfo.output_height * rowStride];
	JSAMPARRAY buffer = (*jpegInfo.mem->alloc_sarray)((j_common_ptr)&jpegInfo, JPOOL_IMAGE, rowStride, 1);

	int c = 0;
	while (jpegInfo.output_scanline < jpegInfo.output_height) {
		jpeg_read_scanlines(&jpegInfo, buffer, 1);

		float * dest = imageData + c * rowStride;
		unsigned char * source = buffer[0];

		for (int r = 0; r < jpegInfo.image_width * jpegInfo.output_components; ++r) {
			if ((r + 1) % 4 == 0) {
				dest[r] = 1.f;
			} else {
				dest[r] = source[r] / 255.f;
			}
		}

		++c;
	}

	jpeg_finish_decompress(&jpegInfo);
	jpeg_destroy_decompress(&jpegInfo);

	return imageData;
}

void ZmqExporter::ZmqRenderImage::update(const VRayBaseTypes::AttrImage &img, ZmqExporter * exp, bool fixImage) {
	// convertions here should match the blender's render pass channel requirements

	if (img.imageType == VRayBaseTypes::AttrImage::ImageType::RGBA_REAL && img.isBucket()) {
		// merge in the bucket

		if (!pixels) {
			std::unique_lock<std::mutex> lock(exp->m_ImgMutex);
			if (!pixels) {
				w = exp->m_RenderWidth;
				h = exp->m_RenderHeight;
				channels = 4;

				pixels = new float[w * h * channels];
				memset(pixels, 0, w * h * channels * sizeof(float));

				resetUpdated();
			}
		}

		fixImage = false;
		const float * sourceImage = reinterpret_cast<const float *>(img.data.get());
		updateRegion(sourceImage, img.x, img.y, img.width, img.height);

	} else if (img.imageType == VRayBaseTypes::AttrImage::ImageType::JPG) {
		int channels = 0;
		float * imgData = jpegToPixelData(reinterpret_cast<unsigned char*>(img.data.get()), img.size, channels);

		{
			std::lock_guard<std::mutex> lock(exp->m_ImgMutex);

			this->channels = channels;
			this->w = img.width;
			this->h = img.height;
			delete[] pixels;
			this->pixels = imgData;
		}
	} else if (img.imageType == VRayBaseTypes::AttrImage::ImageType::RGBA_REAL ||
		       img.imageType == VRayBaseTypes::AttrImage::ImageType::RGB_REAL ||
		       img.imageType == VRayBaseTypes::AttrImage::ImageType::BW_REAL) {

		const float * imgData = reinterpret_cast<const float *>(img.data.get());
		float * myImage = nullptr;
		int channels = 0;

		switch (img.imageType) {
		case VRayBaseTypes::AttrImage::ImageType::RGBA_REAL:
			channels = 4;
			myImage = new float[img.width * img.height * channels];
			memcpy(myImage, imgData, img.width * img.height * channels * sizeof(float));

			break;
		case VRayBaseTypes::AttrImage::ImageType::RGB_REAL:
			channels = 3;
			myImage = new float[img.width * img.height * channels];

			for (int c = 0; c < img.width * img.height; ++c) {
				const float * source = imgData + (c * 4);
				float * dest = myImage + (c * channels);

				dest[0] = source[0];
				dest[1] = source[1];
				dest[2] = source[2];
			}

			break;
		case VRayBaseTypes::AttrImage::ImageType::BW_REAL:
			channels = 1;
			myImage = new float[img.width * img.height * channels];

			for (int c = 0; c < img.width * img.height; ++c) {
				const float * source = imgData + (c * 4);
				float * dest = myImage + (c * channels);

				dest[0] = source[0];
			}

			break;
		default:
			PRINT_WARN("MISSING IMAGE FORMAT CONVERTION FOR %d", img.imageType);
		}

		{
			std::lock_guard<std::mutex> lock(exp->m_ImgMutex);
			this->channels = channels;
			this->w = img.width;
			this->h = img.height;
			delete[] pixels;
			this->pixels = myImage;
		}
	}

	if (fixImage) {
		flip();
		resetAlpha();
		clamp(1.0f, 1.0f);
	}
}


ZmqExporter::ZmqExporter()
	: m_Client(nullptr)
	, m_LastExportedFrame(-1000.f)
	, m_IsAborted(false)
	, m_Started(false)
	, m_RenderQuality(100)
	, m_RenderWidth(0)
	, m_RenderHeight(0)
{
	checkZmqClient();
}


ZmqExporter::~ZmqExporter()
{
	free();

	{
		std::lock_guard<std::mutex> lock(m_ZmqClientMutex);
		m_Client->setCallback([](const VRayMessage &, ZmqClient *) {});
		m_Client.reset();
	}

	// we could be destroyed while someone is inside get_render_channel and is accessing m_LayerImges
	// but we can't protect it from inside this class
}

RenderImage ZmqExporter::get_render_channel(RenderChannelType channelType) {
	RenderImage img;

	auto imgIter = m_LayerImages.find(channelType);
	if (imgIter != m_LayerImages.end()) {
		std::unique_lock<std::mutex> lock(m_ImgMutex);
		imgIter = m_LayerImages.find(channelType);

		if (imgIter != m_LayerImages.end()) {
			RenderImage &storedImage = imgIter->second;
			if (storedImage.pixels) {
				img = std::move(RenderImage::deepCopy(storedImage));
			}
		}
	}
	return img;
}

RenderImage ZmqExporter::get_image() {
	return get_render_channel(RenderChannelType::RenderChannelTypeNone);
}

void ZmqExporter::zmqCallback(const VRayMessage & message, ZmqClient *) {
	const auto msgType = message.getType();
	if (msgType == VRayMessage::Type::SingleValue && message.getValueType() == VRayBaseTypes::ValueType::ValueTypeString) {
		if (this->callback_on_message_update) {
			auto msg = message.getValue<VRayBaseTypes::AttrSimpleType<std::string>>()->m_Value;
			auto newLine = msg.find_first_of("\n\r");
			if (newLine != std::string::npos) {
				msg.resize(newLine);
			}

			this->callback_on_message_update("", msg.c_str());
		}
	} else if (msgType == VRayMessage::Type::Image) {
		auto * set = message.getValue<VRayBaseTypes::AttrImageSet>();
		bool ready = set->sourceType == VRayBaseTypes::ImageSourceType::ImageReady;
		bool rtImageUpdate = false;
		for (const auto &img : set->images) {
			m_LayerImages[img.first].update(img.second, this, !is_viewport);
			// for result buckets use on bucket ready, otherwise rt image updated callback
			if (img.first == RenderChannelType::RenderChannelTypeNone && img.second.isBucket() && this->callback_on_bucket_ready) {
				this->callback_on_bucket_ready(img.second);
			} else {
				rtImageUpdate = true;
			}
		}

		if (rtImageUpdate && this->callback_on_rt_image_updated) {
			callback_on_rt_image_updated.cb();
		}

		if (ready && this->callback_on_image_ready) {
			this->callback_on_image_ready.cb();
		}

	} else if (msgType == VRayMessage::Type::ChangeRenderer) {
		if (message.getRendererAction() == VRayMessage::RendererAction::SetRendererState) {
			m_IsAborted = false;
			switch (message.getRendererState()) {
			case VRayMessage::RendererState::Abort:
				m_IsAborted = true;
				break;
			case VRayMessage::RendererState::Progress:
				render_progress = message.getValue<VRayBaseTypes::AttrSimpleType<float>>()->m_Value;
				break;
			case VRayMessage::RendererState::ProgressMessage:
				progress_message = message.getValue<VRayBaseTypes::AttrSimpleType<std::string>>()->m_Value;
				break;
			case VRayMessage::RendererState::Continue:
				this->last_rendered_frame = message.getValue<VRayBaseTypes::AttrSimpleType<float>>()->m_Value;
				break;
			default:
				BLI_assert(!"Receieved unexpected RendererState message from renderer.");
			}
		}
	}
}

void ZmqExporter::init()
{
	try {
		PRINT_INFO_EX("Initing ZmqExporter");
		using std::placeholders::_1;
		using std::placeholders::_2;

		m_Client->setCallback(std::bind(&ZmqExporter::zmqCallback, this, _1, _2));

		if (!m_Client->connected()) {
			char portStr[32];
			snprintf(portStr, 32, ":%d", this->m_ServerPort);
			m_Client->connect(("tcp://" + this->m_ServerAddress + portStr).c_str());
		}

		if (m_Client->connected()) {
			auto mode = this->animation_settings.use && !this->is_viewport ? VRayMessage::RendererType::Animation : VRayMessage::RendererType::RT;
			if (mode == VRayMessage::RendererType::Animation || !this->is_viewport) {
				PRINT_INFO_EX("Setting RenderMode::RenderModeProduction");
				m_RenderMode = RenderMode::RenderModeProduction;
			}
			m_Client->send(VRayMessage::msgRendererType(mode));
			m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::Init));
			m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetRenderMode, static_cast<int>(m_RenderMode)));
			m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetQuality, m_RenderQuality));

			m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::GetImage, static_cast<int>(RenderChannelType::RenderChannelTypeNone)));
			if (!is_viewport && !this->animation_settings.use) {
				m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::GetImage, static_cast<int>(RenderChannelType::RenderChannelTypeVfbRealcolor)));
			}
		}
	} catch (zmq::error_t &e) {
		PRINT_ERROR("Failed to initialize ZMQ client\n%s", e.what());
	}
}

void ZmqExporter::checkZmqClient()
{
	std::lock_guard<std::mutex> lock(m_ZmqClientMutex);

	if (!m_Client) {
		m_Client = ClientPtr(new ZmqClient());
	} else {
		if (!m_Client->connected()) {
			m_IsAborted = true;
			// we can't connect dont retry
			return;
		}

		if (!m_Client->good()) {
			m_IsAborted = true;
			BLI_assert(!"ZMQ client disconnected from server!");
		}
	}
}

void ZmqExporter::set_settings(const ExporterSettings & settings)
{
	PluginExporter::set_settings(settings);

	if (this->is_viewport) {
		this->m_RenderMode = settings.getViewportRenderMode();
	} else {
		this->m_RenderMode = settings.getRenderMode();
	}
	this->m_RenderQuality = settings.viewportQuality;
	this->m_ServerPort = settings.zmq_server_port;
	this->m_ServerAddress = settings.zmq_server_address;
	this->animation_settings = settings.settings_animation;
	if (this->animation_settings.use) {
		this->last_rendered_frame = this->animation_settings.frame_start - 1;
	}
	// set to inverted so first time we dont hit cache
	m_vfbVisible = !settings.showViewport;
}


void ZmqExporter::free()
{
	checkZmqClient();
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::Free));
}

void ZmqExporter::sync()
{
	checkZmqClient();
	// we send current time if there are any changes, but if exporting animation and frame has no changes
	// frame won't be sent and we must manually update
	if (animation_settings.use && !is_viewport) {
		BLI_assert(m_LastExportedFrame <= this->current_scene_frame && "Exporting out of order frames!");
		if (m_LastExportedFrame != this->current_scene_frame) {
			m_LastExportedFrame = this->current_scene_frame;
			m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetCurrentTime, this->current_scene_frame));
		}
	}
}

void ZmqExporter::show_frame_buffer()
{
	if (m_vfbVisible) {
		return;
	}
	checkZmqClient();
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetVfbShow, static_cast<int>(true)));
	m_vfbVisible = true;
}

void ZmqExporter::hide_frame_buffer()
{
	if (!m_vfbVisible) {
		return;
	}
	checkZmqClient();
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetVfbShow, static_cast<int>(false)));
	m_vfbVisible = false;
}

void ZmqExporter::set_viewport_quality(int quality)
{
	if (quality != m_RenderQuality) {
		m_RenderQuality = quality;
		m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetQuality, m_RenderQuality));
	}
}

void ZmqExporter::set_render_size(const int &w, const int &h)
{
	{
		std::unique_lock<std::mutex> lock(m_ImgMutex);
		m_RenderWidth = w;
		m_RenderHeight = h;
	}

	checkZmqClient();
	m_Client->send(VRayMessage::msgRendererResize(w, h));
}

void ZmqExporter::set_camera_plugin(const std::string &pluginName)
{
	if (m_activeCamera == "") {
		m_activeCamera = pluginName;
	} else {
		if (m_activeCamera == pluginName) {
			return;
		}
	}
	checkZmqClient();
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetCurrentCamera, pluginName));
}

void ZmqExporter::set_commit_state(VRayBaseTypes::CommitAction ca)
{
	PluginExporter::set_commit_state(ca);
	checkZmqClient();
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetCommitAction, static_cast<int>(ca)));
}

void ZmqExporter::start()
{
	checkZmqClient();
	m_Started = true;
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::Start));
}


void ZmqExporter::stop()
{
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::Stop));
}

void ZmqExporter::export_vrscene(const std::string &filepath)
{
	checkZmqClient();
	m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::ExportScene, filepath));
}

int ZmqExporter::remove_plugin_impl(const std::string &name)
{
	checkZmqClient();
	m_Client->send(VRayMessage::msgPluginAction(name, VRayMessage::PluginAction::Remove));
	return PluginExporter::remove_plugin_impl(name);
}

void ZmqExporter::replace_plugin(const std::string & oldPlugin, const std::string & newPlugin)
{
	checkZmqClient();
	m_Client->send(VRayMessage::msgPluginReplace(oldPlugin, newPlugin));
}


AttrPlugin ZmqExporter::export_plugin_impl(const PluginDesc & pluginDesc)
{
	checkZmqClient();

	if (pluginDesc.pluginID.empty()) {
		PRINT_WARN("[%s] PluginDesc.pluginID is not set!",
			pluginDesc.pluginName.c_str());
		return AttrPlugin();
	}

	const bool checkAnimation = animation_settings.use && !is_viewport;
	const std::string & name = pluginDesc.pluginName;
	AttrPlugin plugin(name);

	const auto pluginType = GetPluginDescription(pluginDesc.pluginID).pluginType;
	if (pluginType == ParamDesc::PluginType::PluginChannel) {
		static const std::pair<std::string, RenderChannelType> channelMap[] = {
			{"RenderChannelBumpNormals", RenderChannelType::RenderChannelTypeVfbBumpnormal},
			{"RenderChannelColor", RenderChannelType::RenderChannelTypeVfbColor},
			{"RenderChannelDenoiser", RenderChannelType::RenderChannelTypeVfbDenoised},
			{"RenderChannelDRBucket", RenderChannelType::RenderChannelTypeDrbucket},
			{"RenderChannelNodeID", RenderChannelType::RenderChannelTypeVfbNodeid},
			{"RenderChannelNormals", RenderChannelType::RenderChannelTypeVfbNormal},
			{"RenderChannelRenderID", RenderChannelType::RenderChannelTypeVfbRenderID},
			{"RenderChannelVelocity", RenderChannelType::RenderChannelTypeVfbVelocity},
			{"RenderChannelZDepth", RenderChannelType::RenderChannelTypeVfbZdepth},
			// {"RenderChannelColorModo", }
			// {"RenderChannelCoverage", }
			// {"RenderChannelExtraTex", }
			// {"RenderChannelGlossiness", }
			// {"RenderChannelLightSelect", }
			// {"RenderChannelMultiMatte", }
			// {"RenderChannelObjectSelect", }
		};

		for (int c = 0; c < sizeof(channelMap) / sizeof(channelMap[0]); ++c) {
			if (pluginDesc.pluginID == channelMap[c].first) {
				m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::GetImage, static_cast<int>(channelMap[c].second)));
			}
		}
	}

	m_Client->send(VRayMessage::msgPluginCreate(name, pluginDesc.pluginID));

	if (checkAnimation) {
		BLI_assert(m_LastExportedFrame <= this->current_scene_frame && "Exporting out of order frames!");
		if (m_LastExportedFrame != this->current_scene_frame) {
			m_LastExportedFrame = this->current_scene_frame;
			m_Client->send(VRayMessage::msgRendererAction(VRayMessage::RendererAction::SetCurrentTime, this->current_scene_frame));
		}
	}

	for (auto & attributePairs : pluginDesc.pluginAttrs) {
		const PluginAttr & attr = attributePairs.second;

		switch (attr.attrValue.type) {
		case ValueTypeUnknown:
			break;
		case ValueTypeInt:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, VRayBaseTypes::AttrSimpleType<int>(attr.attrValue.valInt)));
			break;
		case ValueTypeFloat:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, VRayBaseTypes::AttrSimpleType<float>(attr.attrValue.valFloat)));
			break;
		case ValueTypeString:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, VRayBaseTypes::AttrSimpleType<std::string>(attr.attrValue.valString)));
			break;
		case ValueTypeColor:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valColor));
			break;
		case ValueTypeVector:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valVector));
			break;
		case ValueTypeAColor:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valAColor));
			break;
		case ValueTypePlugin:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valPlugin));
			break;
		case ValueTypeTransform:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valTransform));
			break;
		case ValueTypeMatrix:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valMatrix));
			break;
		case ValueTypeListInt:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valListInt));
			break;
		case ValueTypeListFloat:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valListFloat));
			break;
		case ValueTypeListVector:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valListVector));
			break;
		case ValueTypeListColor:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valListColor));
			break;
		case ValueTypeListPlugin:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valListPlugin));
			break;
		case ValueTypeListString:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valListString));
			break;
		case ValueTypeMapChannels:
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valMapChannels));
			break;
		case ValueTypeInstancer:
			if (checkAnimation && attr.attrValue.valInstancer.frameNumber != this->current_scene_frame) {
				PRINT_WARN("Exporting instancer in frame %d, while it has %d frame", static_cast<int>(current_scene_frame), attr.attrValue.valInstancer.frameNumber);
				const_cast<PluginAttr&>(attr).attrValue.valInstancer.frameNumber = current_scene_frame;
			}
			m_Client->send(VRayMessage::msgPluginSetProperty(name, attr.attrName, attr.attrValue.valInstancer));
			break;
		default:
			BLI_assert("Unsupported attribute type");
			break;
		}
	}

	return plugin;
}
