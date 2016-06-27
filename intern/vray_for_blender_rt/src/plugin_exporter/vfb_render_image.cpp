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

#include "vfb_render_image.h"

#include <cstring>
#include <algorithm>

using namespace VRayForBlender;

namespace {
	void resetAlpha(float * data, int w, int h, int channels) {
		if (channels == 4) {
			for (int c = 3; c < w * h * channels; c+=4) {
				data[c] = 1.0f;
			}
		}
	}

	void clamp(float * data, int w, int h, int channels, float max, float val) {
		const int pixelCount = w * h;
		for (int p = 0; p < pixelCount; ++p) {
			float *pixel = data + (p * channels);
			switch (channels) {
			case 4:
			case 3: pixel[2] = pixel[2] > max ? val : pixel[2];
			case 2: pixel[1] = pixel[1] > max ? val : pixel[1];
			case 1: pixel[0] = pixel[0] > max ? val : pixel[0];
			}
		}
	}
}


RenderImage::RenderImage(RenderImage && other):
	pixels(nullptr),
	w(0),
	h(0),
	channels(0),
	updated(0.f)
{
	*this = std::move(other);
}

RenderImage & RenderImage::operator=(RenderImage && other)
{
	if (this != &other) {
		std::swap(updated, other.updated);
		std::swap(pixels, other.pixels);
		std::swap(w, other.w);
		std::swap(h, other.h);
		std::swap(channels, other.channels);
	}
	return *this;
}

RenderImage RenderImage::deepCopy(const RenderImage &source)
{
	RenderImage dest;

	dest.updated = source.updated;
	dest.w = source.w;
	dest.h = source.h;
	dest.channels = source.channels;
	dest.pixels = new float[source.w * source.h * source.channels];
	memcpy(dest.pixels, source.pixels, source.w * source.h * source.channels * sizeof(float));

	return dest;
}

RenderImage::~RenderImage()
{
	delete pixels;
	pixels = nullptr;
}

void RenderImage::updateRegion(const float *data, int x, int y, int w, int h)
{
	updated += (float)(w * h) / std::max((float)(this->w * this->h), 1.f);
	y = this->h - y;

	for (int c = 0; c < h; ++c) {
		const float * source = data + c * w * channels;
		float * dest = this->pixels + (y - c - 1) * this->w * channels + x * channels;
		memcpy(dest, source, sizeof(float) * w * channels);

		::resetAlpha(dest, w, 1, channels);
		::clamp(dest, w, 1, channels, 1.0f, 1.0f);
	}
}

void RenderImage::flip()
{
	if (pixels && w && h) {
		const int half_h = h / 2;

		const int row_items = w * channels;
		const int row_bytes = row_items * sizeof(float);

		float *buf = new float[row_items];

		for (int i = 0; i < half_h; ++i) {
			float *to_row   = pixels + (i       * row_items);
			float *from_row = pixels + ((h-i-1) * row_items);

			memcpy(buf,      to_row,   row_bytes);
			memcpy(to_row,   from_row, row_bytes);
			memcpy(from_row, buf,      row_bytes);
		}

		FreePtr(buf);
	}
}


void RenderImage::resetAlpha()
{
	if (pixels && w && h) {
		::resetAlpha(pixels, w, h, channels);
	}
}


void RenderImage::clamp(float max, float val)
{
	if (pixels && w && h) {
		::clamp(pixels, w, h, channels, max, val);
	}
}


void RenderImage::cropTo(int width, int height)
{
	int t_width = width < this->w ? width : this->w;
	int t_height = height < this->h ? height : this->h;

	if (t_width == this->w && t_height == this->h) {
		PRINT_WARN("Failed to crop image to [%dx%d] from [%dx%d]", width, height, w, h);
		return;
	}

	float * newImg = new float[t_width * t_height * channels];

	const int left_offset = (w - t_width) / 2;
	const int top_offset = (h - t_height) / 2;

	for (int r = 0; r < t_height; ++r) {
		const float * src = pixels + ((r + top_offset) * w * channels) + left_offset * channels;
		float * dst = newImg + (r * t_width * channels);
		memcpy(dst, src, t_width * channels * sizeof(float));
	}

	delete[] pixels;
	pixels = newImg;
}