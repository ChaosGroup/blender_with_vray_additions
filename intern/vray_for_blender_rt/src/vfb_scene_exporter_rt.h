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

#ifndef VRAY_FOR_BLENDER_EXPORTER_RT_H
#define VRAY_FOR_BLENDER_EXPORTER_RT_H

#include "vfb_scene_exporter.h"


namespace VRayForBlender {

class InteractiveExporter
        : public SceneExporter
{
public:
	InteractiveExporter(BL::Context context, BL::RenderEngine engine, BL::BlendData data, BL::Scene scene)
	    : SceneExporter(context, engine, data, scene, BL::SpaceView3D(context.space_data()), context.region_data(), context.region())
	{}

	virtual void  draw() override;
	virtual void  sync_dupli(BL::Object ob, const int &check_updated = false) override;
	virtual void  create_exporter() override;

	virtual bool  export_scene(const bool check_updated = false) override;

	virtual void  setup_callbacks() override;

public:
	void          cb_on_image_ready() { tag_redraw(); }

};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_EXPORTER_RT_H
