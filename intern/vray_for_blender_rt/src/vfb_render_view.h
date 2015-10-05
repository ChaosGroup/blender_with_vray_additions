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

#ifndef VRAY_FOR_BLENDER_RENDER_VIEW_H
#define VRAY_FOR_BLENDER_RENDER_VIEW_H

#include "vfb_rna.h"
#include "vfb_util_defines.h"

#include <string>


namespace VRayForBlender {

struct RenderSizeParams {
	RenderSizeParams()
	    : w(0)
	    , h(0)
	    , offs_x(0)
	    , offs_y(0)
	{}

	int  w;
	int  h;
	int  offs_x;
	int  offs_y;
};


struct RenderViewParams {
	RenderViewParams()
	    : fov(0.785398f)
	    , ortho(false)
	    , ortho_width(1.0f)
	    , use_clip_start(false)
	    , clip_start(0.0f)
	    , use_clip_end(false)
	    , clip_end(1.0f)
	{}

	bool operator == (const RenderViewParams &other) const {
		return (MemberEq(fov) &&
		        MemberEq(ortho) &&
		        MemberEq(ortho_width) &&
		        MemberEq(use_clip_start) &&
		        MemberEq(clip_start) &&
		        MemberEq(use_clip_end) &&
		        MemberEq(clip_end) &&
		        (memcmp(tm.data, other.tm.data, sizeof(BlTransform)) == 0));
	}

	bool operator != (const RenderViewParams &other) const {
		return !(*this == other);
	}

	float        fov;
	BlTransform  tm;

	int          ortho;
	float        ortho_width;

	int          use_clip_start;
	float        clip_start;
	int          use_clip_end;
	float        clip_end;
};


struct ViewParams {
	static const std::string renderViewPluginName;
	static const std::string physicalCameraPluginName;
	static const std::string defaultCameraPluginName;
	static const std::string settingsCameraDofPluginName;

	ViewParams()
	    : usePhysicalCamera(false)
	    , cameraObject(PointerRNA_NULL)
	{}

	int changedParams(const ViewParams &other) const {
		return MemberNotEq(renderView);
	}

	int changedSize(const ViewParams &other) const {
		return (MemberNotEq(renderSize.w) ||
		        MemberNotEq(renderSize.h));
	}

	int changedViewPosition(const ViewParams &other) const {
		return (MemberNotEq(renderSize.offs_x) ||
		        MemberNotEq(renderSize.offs_y));
	}

	int needReset(ViewParams &other) const {
		return (MemberNotEq(usePhysicalCamera) ||
		        MemberNotEq(renderView.ortho) ||
		        MemberNotEq(cameraObject));
	}

	RenderSizeParams  renderSize;
	RenderViewParams  renderView;

	int               usePhysicalCamera;
	BL::Object        cameraObject;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_RENDER_VIEW_H
