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

#ifndef VRAY_FOR_BLENDER_RENDER_IMAGE_H
#define VRAY_FOR_BLENDER_RENDER_IMAGE_H

#include "cgr_config.h"
#include "vfb_util_defines.h"

#include <utility>
#include <vector>
#include <cassert>
#include <algorithm>

namespace VRayForBlender {

struct ImageSize {
	int w, h;
	int channels;
};


struct ImageRegion {
	ImageRegion(ImageSize sz)
		: x(0), y(0), w(sz.w), h(sz.h) {}

	ImageRegion(int x, int y, int w, int h)
		: x(x), y(y), w(w), h(h) {}

	int x, y;
	int w, h;

	enum Options {
		NONE        = 0,
		RESET_ALPHA = 1 << 0, ///< reset alpha to 1.0
		CLAMP       = 1 << 1, ///< clamp values to [0, 1]

		FROM_RENDERER = RESET_ALPHA | CLAMP, ///< convinient default for images from vray
	};
};


/// Copy a region from one image to a region in another image
/// @dest and @source must not overlap, @sourceRegion and @destRegion must have same size
/// @param dest - memory for the destination
/// @param destSize - size of the destination memory
/// @param destRegion - the region in the destination image
/// @param source - pointer to source memory
/// @param sourceSize - size of the source image
/// @param sourceRegion - the region in the source image
/// @param options - specifies additional actions to be performed on the destination
void updateImageRegion(
	void * __restrict dest, ImageSize destSize, ImageRegion destRegion,
	const void * __restrict source, ImageSize sourceSize, ImageRegion sourceRegion,
	ImageRegion::Options options = ImageRegion::Options::FROM_RENDERER
);


struct RenderImage {
	RenderImage()
	    : pixels(nullptr)
	    , w(0)
	    , h(0)
	    , channels(0)
	    , updated(0)
	{}

	RenderImage(const RenderImage &) = delete;
	RenderImage & operator=(const RenderImage &) = delete;

	static RenderImage deepCopy(const RenderImage &source);

	RenderImage(RenderImage && other);
	RenderImage & operator=(RenderImage && other);

	virtual ~RenderImage();

	operator bool () const {
		return !!(pixels);
	}

	void   updateRegion(const float *source, ImageRegion destRegion);
	void   flip();
	void   clamp(float max=1.0f, float val=1.0f);
	void   resetAlpha();
	// gets the center width X height image out of the original, if target is bigger - does nothings
	void   cropTo(int width, int height);

	void   resetUpdated() { updated = 0.f; }

public:

	float *pixels; ///< data of the image
	int    w; ///< width in pixels
	int    h; ///< height in pixels
	int    channels; ///< channels count (usually 1, 3 or 4)
	float  updated;///< will hold % of updated area
};

float * jpegToPixelData(unsigned char * data, int size, int &channels);

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_RENDER_IMAGE_H
