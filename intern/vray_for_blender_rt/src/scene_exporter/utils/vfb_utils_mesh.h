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

#ifndef VRAY_FOR_BLENDER_UTILS_MESH_H
#define VRAY_FOR_BLENDER_UTILS_MESH_H

#include "vfb_rna.h"
#include "vfb_plugin_attrs.h"

#include <boost/format.hpp>


namespace VRayForBlender {
namespace Mesh {

extern const char * UvChanNameFmt;
extern const char * ColChanNameFmt;

struct ExportOptions {
	ExportOptions()
	    : mode(EvalModeRender)
	    , merge_channel_vertices(false)
	    , force_dynamic_geometry(false)
	{}

	EvalMode mode;
	int      merge_channel_vertices;
	int      force_dynamic_geometry;
};

int FillMeshData(BL::BlendData data, BL::Scene scene, BL::Object ob, ExportOptions options, PluginDesc &pluginDesc);

} // namespace Mesh
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_MESH_H
