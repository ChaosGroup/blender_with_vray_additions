#ifndef VRAY_FOR_BLENDER_SPECIALISED_EXPORTER_H
#define VRAY_FOR_BLENDER_SPECIALISED_EXPORTER_H

#include "vfb_scene_exporter.h"


namespace VRayForBlender {

class InteractiveExporter
        : public SceneExporter
{
public:
	InteractiveExporter(BL::Context context, BL::RenderEngine engine, BL::BlendData data, BL::Scene scene)
	    : SceneExporter(context, engine, data, scene, BL::SpaceView3D(context.space_data()), context.region_data(), context.region())
	{}

	virtual bool  do_export() override;
	virtual void  sync_dupli(BL::Object ob, const int &check_updated = false) override;
	virtual void  sync_object(BL::Object ob, const int &check_updated = false, const ObjectOverridesAttrs & = ObjectOverridesAttrs()) override;
	virtual void  create_exporter() override;
};


class ProductionExporter
        : public SceneExporter
{
public:
	ProductionExporter(BL::Context context, BL::RenderEngine engine, BL::BlendData data, BL::Scene scene)
	    : SceneExporter(context, engine, data, scene)
	{}

	virtual bool  do_export() override;
	virtual void  sync_dupli(BL::Object ob, const int &check_updated=false) override;
	virtual void  sync_object(BL::Object ob, const int &check_updated=false, const ObjectOverridesAttrs & =ObjectOverridesAttrs()) override;
	virtual void  create_exporter() override;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_SPECIALISED_EXPORTER_H
