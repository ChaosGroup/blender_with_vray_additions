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

namespace VRayForBlender {

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

	static void updateImageRegion(float * dest, int destW, int destH, int x, int y, const float * source, int sourceW, int sourceH, int channels);

	void   updateRegion(const float *data, int x, int y, int w, int h);
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

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_RENDER_IMAGE_H
