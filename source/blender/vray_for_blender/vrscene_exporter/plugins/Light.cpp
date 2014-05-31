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

#include "exp_nodes.h"

#include "Light.h"
#include "CGR_rna.h"
#include "CGR_string.h"
#include "CGR_vrscene.h"

#include "DNA_lamp_types.h"

#include "BLI_math_matrix.h"


#define STR_CMP(a, b) strcmp(a, b) == 0


const char *ParamsLightOmni[] = {
	"enabled",
	"shadows",
	"shadowColor",
	"shadowBias",
	"causticSubdivs",
	"causticMult",
	"cutoffThreshold",
	"affectDiffuse",
	"affectSpecular",
	"bumped_below_surface_check",
	"nsamples",
	"diffuse_contribution",
	"specular_contribution",
	"intensity",
	"shadowRadius",
	"areaSpeculars",
	"shadowSubdivs",
	"decay",
	NULL
};


const char *ParamsLightAmbient[] = {
	"enabled",
	"shadowBias",
	"decay",
	"ambientShade",
	NULL
};


const char *ParamsLightSphere[] = {
	"enabled",
	"shadows",
	"shadowColor",
	"shadowBias",
	"causticSubdivs",
	"causticMult",
	"cutoffThreshold",
	"affectDiffuse",
	"affectSpecular",
	"bumped_below_surface_check",
	"nsamples",
	"diffuse_contribution",
	"specular_contribution",
	"intensity",
	"subdivs",
	"storeWithIrradianceMap",
	"invisible",
	"affectReflections",
	"noDecay",
	"radius",
	"sphere_segments",
	NULL,
};


const char *ParamsLightRectangle[] = {
	"enabled",
	"shadows",
	"shadowColor",
	"shadowBias",
	"causticSubdivs",
	"causticMult",
	"cutoffThreshold",
	"affectDiffuse",
	"affectSpecular",
	"bumped_below_surface_check",
	"nsamples",
	"diffuse_contribution",
	"specular_contribution",
	"intensity",
	"subdivs",
	"storeWithIrradianceMap",
	"invisible",
	"affectReflections",
	"doubleSided",
	"noDecay",
	NULL
};


const char *ParamsLightDirectMax[] = {
	"enabled",
	"shadows",
	"shadowColor",
	"shadowBias",
	"causticSubdivs",
	"causticMult",
	"cutoffThreshold",
	"affectDiffuse",
	"affectSpecular",
	"bumped_below_surface_check",
	"nsamples",
	"diffuse_contribution",
	"specular_contribution",
	"intensity",
	"shadowRadius",
	"areaSpeculars",
	"shadowSubdivs",
	"fallsize",
	NULL
};


const char *ParamsSunLight[] = {
	"turbidity",
	"ozone",
	"water_vapour",
	"intensity_multiplier",
	"size_multiplier",
	"invisible",
	"horiz_illum",
	"shadows",
	"shadowBias",
	"shadow_subdivs",
	"shadow_color",
	"causticSubdivs",
	"causticMult",
	"enabled",
	NULL
};


const char *ParamsLightIESMax[] = {
	"enabled",
	"intensity",
	"shadows",
	"shadowColor",
	"shadowBias",
	"causticSubdivs",
	"causticMult",
	"cutoffThreshold",
	"affectDiffuse",
	"affectSpecular",
	"bumped_below_surface_check",
	"nsamples",
	"diffuse_contribution",
	"specular_contribution",
	"shadowSubdivs",
	"ies_file",
	"soft_shadows",
	NULL
};


const char *ParamsLightDome[] = {
	"enabled",
	"shadows",
	"shadowColor",
	"shadowBias",
	"causticSubdivs",
	"causticMult",
	"cutoffThreshold",
	"affectDiffuse",
	"affectSpecular",
	"bumped_below_surface_check",
	"nsamples",
	"diffuse_contribution",
	"specular_contribution",
	"intensity",
	"subdivs",
	"invisible",
	"affectReflections",
	"dome_targetRadius",
	"dome_emitRadius",
	"dome_spherical",
	"dome_rayDistance",
	"dome_rayDistanceMode",
	NULL
};


const char *ParamsLightSpot[] = {
	"enabled",
	"shadows",
	"shadowColor",
	"shadowBias",
	"causticSubdivs",
	"causticMult",
	"cutoffThreshold",
	"affectDiffuse",
	"affectSpecular",
	"bumped_below_surface_check",
	"nsamples",
	"diffuse_contribution",
	"specular_contribution",
	"intensity",
	"shadowRadius",
	"areaSpeculars",
	"shadowSubdivs",
	"decay",
	NULL
};


VRayScene::Light::Light(Scene *scene, Main *main, Object *ob, DupliObject *dOb):
	VRayExportable(scene, main, ob)
{
	m_dupliObject = dOb;
	m_object      = m_dupliObject ? m_dupliObject->ob : m_ob;

	initTransform();
	initType();
	initName();
	initHash();
}


void VRayScene::Light::initName(const std::string &name)
{
	if(NOT(name.empty()))
		m_name = name;
	else {
		m_name.clear();
		if(m_dupliObject)
			m_name = GetIDName((ID*)m_ob);
		m_name.append(GetIDName((ID*)m_object));
		if(m_dupliObject)
			m_name.append(BOOST_FORMAT_INT(m_dupliObject->persistent_id[0]));
	}
}


void VRayScene::Light::initTransform()
{
	float tm[4][4];

	if(m_dupliObject)
		copy_m4_m4(tm, m_dupliObject->mat);
	else
		copy_m4_m4(tm, m_object->obmat);

	// Reset light scale
	normalize_m4(tm);

	GetTransformHex(tm, m_transform);
}


void VRayScene::Light::initHash()
{
	writePlugin();

	m_hash = HashCode(m_plugin.str().c_str());
}


void VRayScene::Light::writeData(PyObject *output, VRayExportable *prevState, bool keyFrame)
{
	PYTHON_PRINT(output, m_plugin.str().c_str());
}


void VRayScene::Light::initType()
{
	Lamp *la = (Lamp*)m_object->data;

	RnaAccess::RnaValue lampRna((ID*)la, "vray");

	switch(la->type) {
		case LA_LOCAL:
			if(lampRna.getEnum("omni_type") == 0)
				if(lampRna.getFloat("radius") > 0.0f) {
					m_vrayPluginID = "LightSphere";
					m_paramDesc = ParamsLightSphere;
				}
				else {
					m_vrayPluginID = "LightOmni";
					m_paramDesc = ParamsLightOmni;
				}
			else {
				m_vrayPluginID = "LightAmbient";
				m_paramDesc = ParamsLightAmbient;
			}
			break;
		case LA_SUN:
			if(lampRna.getEnum("direct_type") == 0) {
				m_vrayPluginID = "LightDirectMax";
				m_paramDesc = ParamsLightDirectMax;
			}
			else {
				m_vrayPluginID = "SunLight";
				m_paramDesc = ParamsSunLight;
			}
			break;
		case LA_SPOT:
			if(lampRna.getEnum("spot_type") == 0) {
				m_vrayPluginID = "LightSpot";
				m_paramDesc = ParamsLightSpot;
			}
			else {
				m_vrayPluginID = "LightIESMax";
				m_paramDesc = ParamsLightIESMax;
			}
			break;
		case LA_HEMI:
			m_vrayPluginID = "LightDome";
			m_paramDesc = ParamsLightDome;
			break;
		case LA_AREA:
			m_vrayPluginID = "LightRectangle";
			m_paramDesc = ParamsLightRectangle;
			break;
		default:
			m_vrayPluginID = "LightOmni";
			m_paramDesc = ParamsLightOmni;
			break;
	}
}


void VRayScene::Light::writeKelvinColor(const std::string &name, const int &temp)
{
	writeHeader("TexTemperature", name.c_str());
	writeAttribute("color_mode", 1);
	writeAttribute("temperature", temp);
	writeFooter();
}


// NOTE: Completely follows the function from "render.py" except textures.
// Textures and full params export will be supported with 'vb30'.
//
void VRayScene::Light::writePlugin()
{
	Lamp                *la = (Lamp*)m_object->data;
	RnaAccess::RnaValue  laRna((ID*)la, "vray");
	PointerRNA          *la_ptr = laRna.getPtr();

	std::string kelvinColor;

	if(NOT(m_vrayPluginID == "SunLight")) {
		if(laRna.getEnum("color_type")) {
			kelvinColor = "Kelvin"+m_name;
			writeKelvinColor(kelvinColor, laRna.getInt("temperature"));
		}
	}

	writeHeader(m_vrayPluginID.c_str(), m_name.c_str());

	m_plugin << "\n\t" << "transform" << "=" << m_interpStart;
	m_plugin << "TransformHex(\"" << m_transform << "\")";
	m_plugin << m_interpEnd << ";";

	if(m_vrayPluginID == "SunLight") {
		writeAttribute("sky_model", laRna.getEnum("sky_model"));
	}
	else {
		if(NOT(kelvinColor.empty()))
			writeAttribute("color_tex", kelvinColor.c_str());
		else {
			MyColor color(la->r, la->g, la->b);
			writeAttribute("color", color);
		}

		if(NOT(m_vrayPluginID == "LightIESMax" || m_vrayPluginID == "LightAmbient")) {
			writeAttribute("units", laRna.getEnum("units"));
		}

		if(m_vrayPluginID == "LightIESMax") {
			writeAttribute("ies_light_shape", laRna.getBool("ies_light_shape") ? 1 : -1);

			int   lockWidth = laRna.getBool("ies_light_shape_lock");
			float iesWidth  = laRna.getFloat("ies_light_width");

			writeAttribute("ies_light_width", iesWidth);
			writeAttribute("ies_light_diameter", laRna.getFloat("ies_light_diameter"));
			writeAttribute("ies_light_length",   lockWidth ? iesWidth : laRna.getFloat("ies_light_length"));
			writeAttribute("ies_light_height",   lockWidth ? iesWidth : laRna.getFloat("ies_light_height"));
		}
	}

	if(m_vrayPluginID == "LightSpot") {
		float middleRegion = la->dist - la->spotblend;

		writeAttribute(la_ptr, "decay");

		writeAttribute("coneAngle", la->spotsize);
		writeAttribute("penumbraAngle", -la->spotsize * la->spotblend);

		writeAttribute("useDecayRegions", 1);
		writeAttribute("startDistance1", 0);
		writeAttribute("endDistance1",   middleRegion);
		writeAttribute("startDistance2", middleRegion);
		writeAttribute("endDistance2",   middleRegion);
		writeAttribute("startDistance3", middleRegion);
		writeAttribute("endDistance3", la->dist);
	}

	if(m_vrayPluginID == "LightRectangle") {
		float sizeX = la->area_size / 2.0f;
		float sizeY = la->area_shape == LA_AREA_SQUARE ? sizeX : la->area_sizey / 2.0f;

		writeAttribute("u_size", sizeX);
		writeAttribute("v_size", sizeY);
		writeAttribute("lightPortal", laRna.getEnum("lightPortal"));
	}

	while(*m_paramDesc) {
		if(STR_CMP(*m_paramDesc, "shadow_subdivs")) {
			writeAttribute(la_ptr, "shadow_subdivs", "subdivs");
		}
		else if(STR_CMP(*m_paramDesc, "shadowSubdivs")) {
			writeAttribute(la_ptr, "shadowSubdivs", "subdivs");
		}
		else if(STR_CMP(*m_paramDesc, "shadowRadius") && m_vrayPluginID == "LightDirectMax") {
			float shadowRadius = laRna.getInt("shadowRadius");

			writeAttribute("shadowShape",   laRna.getEnum("shadowShape"));
			writeAttribute("shadowRadius",  shadowRadius);
			writeAttribute("shadowRadius1", shadowRadius);
			writeAttribute("shadowRadius2", shadowRadius);
		}
		else if(STR_CMP(*m_paramDesc, "intensity") && m_vrayPluginID == "LightIESMax") {
			writeAttribute(la_ptr, "power", "intensity");
		}
		else
			writeAttribute(la_ptr, *m_paramDesc);

		m_paramDesc++;
	}

	writeFooter();
}
