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


using namespace VRayForBlender;


void RenderImage::free()
{
	FreePtrArr(pixels);
}


void RenderImage::flip()
{
	if (pixels && w && h) {
		const int _half_h = h / 2;
		const int half_h = h % 2 ? _half_h : _half_h - 1;

		const int row_items = w * 4;
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
		const int pixelCount = w * h;
		for (int p = 0; p < pixelCount; ++p) {
			float *pixel = pixels + (p * 4);
			pixel[3] = 1.0f;
		}
	}
}


void RenderImage::clamp(float max, float val)
{
	if (pixels && w && h) {
		const int pixelCount = w * h;
		for (int p = 0; p < pixelCount; ++p) {
			float *pixel = pixels + (p * 4);
			for (int c = 0; c < 3; ++c) {
				if (pixel[c] > max) {
					pixel[c] = val;
				}
			}
		}
	}
}
