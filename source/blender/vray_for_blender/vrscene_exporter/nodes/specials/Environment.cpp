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


void VRayNodeExporter::exportVRayEnvironment(VRayNodeContext *context)
{
	BL::World world = ExpoterSettings::gSet.b_scene.world();

	PointerRNA vrayWorld = RNA_pointer_get(&world.ptr, "vray");

	float globalLightLevel = RNA_float_get(&vrayWorld, "global_light_level");

	BL::NodeTree worldTree = VRayNodeExporter::getNodeTree(ExpoterSettings::gSet.b_data, (ID*)world.ptr.data);
	if (NOT(worldTree)) {
		AttributeValueMap pluginAttrs;
		pluginAttrs["global_light_level"] = BOOST_FORMAT_COLOR1(globalLightLevel);

		VRayNodePluginExporter::exportPlugin("ENVIRONMENT", "SettingsEnvironment", "SettingsEnvironment", pluginAttrs);
	}
	else {
		BL::Node worldOutput = VRayNodeExporter::getNodeByType(worldTree, "VRayNodeWorldOutput");
		if (NOT(worldOutput)) {
			PRINT_ERROR("Environment: \"World Output\" node is not found!");
			return;
		}

		StrSet environment_volume;

		// Effects must be exported before environment because of 'environment_volume' attribute
		BL::NodeSocket effectsSock = VRayNodeExporter::getSocketByName(worldOutput, "Effects");
		if (effectsSock && effectsSock.is_linked()) {
			BL::Node effectsNode = VRayNodeExporter::getConnectedNode(effectsSock, context);
			if (effectsNode) {
				if (NOT(effectsNode.bl_idname() == "VRayNodeEffectsHolder")) {
					PRINT_ERROR("Environment: \'Effects\' socket must be connected to \"Effects Container\" node!");
				}
				else {
					BL::Node::inputs_iterator inIt;
					for (effectsNode.inputs.begin(inIt); inIt != effectsNode.inputs.end(); ++inIt) {
						BL::NodeSocket inSock = *inIt;
						if (RNA_boolean_get(&inSock.ptr, "use")) {
							const std::string effectName = VRayNodeExporter::exportSocket(worldTree, inSock, context);
							if (effectName != "NULL") {
								environment_volume.insert(effectName);
							}
						}
					}
				}
			}
		}

		BL::NodeSocket envSock = VRayNodeExporter::getSocketByName(worldOutput, "Environment");
		if (envSock && envSock.is_linked()) {
			BL::Node envNode = VRayNodeExporter::getConnectedNode(envSock, context);
			if (envNode) {
				if (NOT(envNode.bl_idname() == "VRayNodeEnvironment")) {
					PRINT_ERROR("Environment: \'Environment\' socket must be connected to \"Environment\" node!");
				}
				else {
					AttributeValueMap pluginAttrs;
					pluginAttrs["bg_color"]      = "Color(0.0,0.0,0.0)";
					pluginAttrs["gi_color"]      = "Color(0.0,0.0,0.0)";
					pluginAttrs["reflect_color"] = "Color(0.0,0.0,0.0)";
					pluginAttrs["refract_color"] = "Color(0.0,0.0,0.0)";

					pluginAttrs["global_light_level"] = BOOST_FORMAT_COLOR1(globalLightLevel);
					pluginAttrs["environment_volume"] = BOOST_FORMAT_LIST(environment_volume);

					// Background
					BL::NodeSocket bgSock = VRayNodeExporter::getSocketByAttr(envNode, "bg_tex");
					const std::string bg_tex = VRayNodeExporter::exportSocket(worldTree, bgSock, context);
					const std::string bg_tex_mult = BOOST_FORMAT_FLOAT(RNA_float_get(&bgSock.ptr, "multiplier"));

					pluginAttrs["bg_tex"]      = bg_tex;
					pluginAttrs["bg_tex_mult"] = bg_tex_mult;

					// Overrides
					StrSet envOverrides;
					envOverrides.insert("gi_tex");
					envOverrides.insert("reflect_tex");
					envOverrides.insert("refract_tex");

					StrSet::const_iterator envOverIt;
					for (envOverIt = envOverrides.begin(); envOverIt != envOverrides.end(); ++envOverIt) {
						const std::string &overAttr     = *envOverIt;
						const std::string  overMultAttr =  overAttr + "_mult";

						BL::NodeSocket overSock = VRayNodeExporter::getSocketByAttr(envNode, overAttr);
						bool           overUse  = RNA_boolean_get(&overSock.ptr, "use");
						float          overMult = RNA_float_get(&overSock.ptr, "multiplier");

						if (NOT(overUse)) {
							pluginAttrs[overAttr]     = bg_tex;
							pluginAttrs[overMultAttr] = bg_tex_mult;
						}
						else {
							pluginAttrs[overAttr]     = VRayNodeExporter::exportSocket(worldTree, overSock, context);
							pluginAttrs[overMultAttr] = BOOST_FORMAT_FLOAT(overMult);
						}
					}

					VRayNodePluginExporter::exportPlugin("ENVIRONMENT", "SettingsEnvironment", "SettingsEnvironment", pluginAttrs);
				}
			}
		}
	}
}
