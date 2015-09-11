#ifndef VRAY_FOR_BLENDER_SPECIALISED_EXPORTER_H
#define VRAY_FOR_BLENDER_SPECIALISED_EXPORTER_H

#include "vfb_scene_exporter.h"

namespace VRayForBlender {

class InteractiveExporter: public SceneExporter {
public:
	InteractiveExporter(BL::Context         context,
	                    BL::RenderEngine    engine,
	                    BL::BlendData       data,
	                    BL::Scene           scene,
	                    BL::SpaceView3D     view3d,
	                    BL::RegionView3D    region3d,
	                    BL::Region          region):
	    SceneExporter(context, engine, data, scene, view3d, region3d, region)
	{}

	virtual bool         do_export();
	virtual void         sync_dupli(BL::Object ob, const int &check_updated = false);
	virtual void         create_exporter();
};

class ProductionExporter: public SceneExporter {
public:
	ProductionExporter(BL::Context         context,
	                   BL::RenderEngine    engine,
	                   BL::BlendData       data,
	                   BL::Scene           scene,
	                   BL::SpaceView3D     view3d,
	                   BL::RegionView3D    region3d,
	                   BL::Region          region):
	    SceneExporter(context, engine, data, scene, view3d, region3d, region)
	{}

	virtual bool         do_export();
	virtual void         sync_dupli(BL::Object ob, const int &check_updated = false);
	virtual void         create_exporter();
};

} // VRayForBlender

#endif // VRAY_FOR_BLENDER_SPECIALISED_EXPORTER_H
