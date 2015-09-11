#include "vfb_specialised_exporter.h"

bool InteractiveExporter::do_export()
{
	sync(false);
	m_exporter->start();
	return true;
}

void InteractiveExporter::create_exporter()
{
#ifdef USE_BLENDER_VRAY_ZMQ
	m_settings.exporter_type = ExpoterType::ExpoterTypeZMQ;
#else
	m_settings.exporter_type = ExpoterType::ExporterTypeInvalid;
#endif
	SceneExporter::create_exporter();
	if (m_exporter) {
		m_exporter->set_is_viewport(true);
		m_exporter->set_settings(m_settings);
	}
}

void InteractiveExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(m_scene, EvalMode::EvalModePreview);
	SceneExporter::sync_dupli(ob, check_updated);
	ob.dupli_list_clear();
}


bool ProductionExporter::do_export()
{
	if (m_settings.settings_animation.use) {
		return export_animation();
	} else {
		sync(false);
		m_exporter->start();
		return true;
	}
}

void ProductionExporter::create_exporter()
{
	SceneExporter::create_exporter();
	if (m_exporter) {
		m_exporter->set_is_viewport(false);
		m_exporter->set_settings(m_settings);
	}
}

void ProductionExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(m_scene, EvalMode::EvalModeRender);
	SceneExporter::sync_dupli(ob, check_updated);
	ob.dupli_list_clear();
}