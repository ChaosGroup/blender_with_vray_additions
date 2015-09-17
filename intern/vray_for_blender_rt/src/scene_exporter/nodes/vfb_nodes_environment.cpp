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

#include "vfb_node_exporter.h"
#include "vfb_utils_nodes.h"


void DataExporter::exportVRayEnvironment(NodeContext &context)
{
	BL::Scene worldScene(PointerRNA_NULL);

	if (m_engine && m_engine.is_preview()) {
		if (!m_context) {
			PRINT_WARN("Invalid context!");
			return;
		}
		worldScene = m_context.scene();
	}
	else {
		worldScene = m_scene;
	}

	BL::World world = worldScene.world();
	if (!world) {
		PRINT_WARN("Scene doesn't contain a \"World\" datablock!");
	}
	else {
		PointerRNA vrayWorld = RNA_pointer_get(&world.ptr, "vray");

		const float globalLightLevel = RNA_float_get(&vrayWorld, "global_light_level");

		PluginDesc pluginDesc("settingsEnvironment", "SettingsEnvironment");
		pluginDesc.add("global_light_level", AttrColor(globalLightLevel, globalLightLevel, globalLightLevel));

		BL::NodeTree worldTree = Nodes::GetNodeTree(world);
		if (worldTree) {
			BL::Node worldOutput = Nodes::GetNodeByType(worldTree, "VRayNodeWorldOutput");
			if (!worldOutput) {
				PRINT_ERROR("Environment: \"World Output\" node is not found!");
			}
			else {
				AttrListPlugin environment_volume;

				// Effects must be exported before environment because of 'environment_volume' attribute
				BL::NodeSocket effectsSock = Nodes::GetInputSocketByName(worldOutput, "Effects");
				if (effectsSock && effectsSock.is_linked()) {
					BL::Node effectsNode = getConnectedNode(effectsSock, context);
					if (effectsNode) {
						if (NOT(effectsNode.bl_idname() == "VRayNodeEffectsHolder")) {
							PRINT_ERROR("Environment: \'Effects\' socket must be connected to \"Effects Container\" node!");
						}
						else {
							BL::Node::inputs_iterator inIt;
							for (effectsNode.inputs.begin(inIt); inIt != effectsNode.inputs.end(); ++inIt) {
								BL::NodeSocket inSock(*inIt);
								if (RNA_boolean_get(&inSock.ptr, "use")) {
									AttrValue effect = exportSocket(worldTree, inSock, context);
									if (effect) {
										environment_volume.append(effect.valPlugin);
									}
								}
							}
						}
					}
				}

				BL::NodeSocket envSock = Nodes::GetInputSocketByName(worldOutput, "Environment");
				if (envSock && envSock.is_linked()) {
					BL::Node envNode = getConnectedNode(envSock, context);
					if (envNode) {
						if (NOT(envNode.bl_idname() == "VRayNodeEnvironment")) {
							PRINT_ERROR("Environment: \'Environment\' socket must be connected to \"Environment\" node!");
						}
						else {
							pluginDesc.add("bg_color",      AttrColor(0.0f, 0.0f, 0.0f));
							pluginDesc.add("gi_color",      AttrColor(0.0f, 0.0f, 0.0f));
							pluginDesc.add("reflect_color", AttrColor(0.0f, 0.0f, 0.0f));
							pluginDesc.add("refract_color", AttrColor(0.0f, 0.0f, 0.0f));

							if (!environment_volume.empty()) {
								pluginDesc.add("environment_volume", environment_volume);
							}

							// Background
							BL::NodeSocket bgSock = Nodes::GetSocketByAttr(envNode, "bg_tex");
							AttrValue bg_tex = exportSocket(worldTree, bgSock, context);
							AttrValue bg_tex_mult(RNA_float_get(&bgSock.ptr, "multiplier"));

							pluginDesc.add("bg_tex",      bg_tex);
							pluginDesc.add("bg_tex_mult", bg_tex_mult);
#if 0
							// Overrides
							StrSet envOverrides;
							envOverrides.insert("gi_tex");
							envOverrides.insert("reflect_tex");
							envOverrides.insert("refract_tex");

							StrSet::const_iterator envOverIt;
							for (envOverIt = envOverrides.begin(); envOverIt != envOverrides.end(); ++envOverIt) {
								const std::string &overAttr     = *envOverIt;
								const std::string  overMultAttr =  overAttr + "_mult";

								BL::NodeSocket overSock = NodeExporter::getSocketByAttr(envNode, overAttr);
								bool           overUse  = RNA_boolean_get(&overSock.ptr, "use");
								float          overMult = RNA_float_get(&overSock.ptr, "multiplier");

								if (NOT(overUse)) {
									pluginDesc[overAttr]     = bg_tex;
									pluginDesc[overMultAttr] = bg_tex_mult;
								}
								else {
									pluginDesc[overAttr]     = NodeExporter::exportSocket(worldTree, overSock, context);
									pluginDesc[overMultAttr] = BOOST_FORMAT_FLOAT(overMult);
								}
							}
#endif
						}
					}
				}
			}
		}

		m_exporter->export_plugin(pluginDesc);
	}
}
