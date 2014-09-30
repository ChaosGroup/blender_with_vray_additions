/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * * ***** END GPL LICENSE BLOCK *****
 */

#include "exp_settings.h"

#include "Node.h"
#include "cgr_rna.h"

#include "DNA_scene_types.h"
#include "RNA_access.h"


ExporterSettings ExporterSettings::gSet;


void ExporterSettings::init(BL::Context context, BL::Scene scene, BL::BlendData data, BL::RenderEngine engine)
{
	b_context = context;

	b_scene  = scene;
	b_data   = data;
	b_engine = engine;
}


void ExporterSettings::init()
{
	PointerRNA vrayScene = RNA_pointer_get(&b_scene.ptr, "vray");
	PointerRNA vrayExporter = RNA_pointer_get(&vrayScene, "Exporter");

	m_exportHair        = RNA_boolean_get(&vrayExporter, "use_hair");
	m_exportSmoke       = RNA_boolean_get(&vrayExporter, "use_smoke");
	m_useDisplaceSubdiv = RNA_boolean_get(&vrayExporter, "use_displace");
	m_useAltInstances   = RNA_boolean_get(&vrayExporter, "use_alt_d_instances");

	// Check what layers to use
	//
	const int useLayers = RNA_enum_get(&vrayExporter, "activeLayers");
	if(useLayers == 0) {
		// Current active layers
		m_activeLayers = m_sce->lay;
	}
	else if(useLayers == 1) {
		// All layers
		m_activeLayers = ~(1<<21);
	}
	else {
		// Custom layers
		// Load custom render layers
		int layer_values[20];
		RNA_boolean_get_array(&vrayExporter, "customRenderLayers", layer_values);

		m_activeLayers = 0;
		for(int a = 0; a < 20; ++a) {
			if(layer_values[a]) {
				gSet.m_activeLayers |= (1 << a);
			}
		}
	}

	// Find if we need hide from view here
	const int animationMode = RNA_enum_get(&vrayExporter, "animation_mode");
	if(animationMode == 4) {
		// "Camera Loop"
		BL::BlendData::cameras_iterator caIt;
		for(b_data.cameras.begin(caIt); caIt != b_data.cameras.end(); ++caIt) {
			BL::Camera ca = *caIt;

			PointerRNA vrayCamera = RNA_pointer_get(&ca.ptr, "vray");

			if(RNA_boolean_get(&vrayCamera, "use_camera_loop")) {
				if(RNA_boolean_get(&vrayCamera, "hide_from_view")) {
					m_useHideFromView = true;
					break;
				}
			}
		}
	}
	else {
		BL::Object caOb = b_scene.camera();
		// NOTE: Could happen if scene has no camera and we initing exporter for
		// proxy export
		if (caOb) {
			BL::Camera ca(caOb.data());

			PointerRNA vrayCamera = RNA_pointer_get(&ca.ptr, "vray");

			m_useHideFromView = RNA_boolean_get(&vrayCamera, "hide_from_view");
		}
	}

	m_mtlOverride.clear();
	PointerRNA settingsOptions = RNA_pointer_get(&vrayScene, "SettingsOptions");
	if(RNA_boolean_get(&settingsOptions, "mtl_override_on")) {
		const std::string &overrideName = RNA_std_string_get(&settingsOptions, "mtl_override");
		if(NOT(overrideName.empty())) {
			BL::BlendData::materials_iterator maIt;
			for(b_data.materials.begin(maIt); maIt != b_data.materials.end(); ++maIt) {
				BL::Material ma = *maIt;
				if(ma.name() == overrideName) {
					m_mtlOverride = Node::GetMaterialName((Material*)ma.ptr.data);
					// m_mtlOverride = bl_ma;
					break;
				}
			}
		}
	}
}


void ExporterSettings::reset()
{
	m_sce  = NULL;
	m_main = NULL;

	m_fileObject = NULL;
	m_fileGeom   = NULL;
	m_fileLights = NULL;
	m_fileMat    = NULL;
	m_fileTex    = NULL;

	m_activeLayers  = 0;

	m_useHideFromView   = false;
	m_useDisplaceSubdiv = true;

	m_exportNodes  = true;
	m_exportMeshes = true;
	m_exportHair   = true;
	m_exportSmoke  = true;

	m_isAnimation  = false;
	m_frameCurrent = 0;
	m_frameStart   = 1;
	m_frameStep    = 1;

	m_mtlOverride = "";
	m_drSharePath = "";
}


bool ExporterSettings::DoUpdateCheck()
{
	return m_isAnimation && (m_frameCurrent > m_frameStart);
}


bool ExporterSettings::IsFirstFrame()
{
	if(NOT(m_isAnimation))
		return true;
	if(m_frameCurrent > m_frameStart)
		return true;
	return false;
}
