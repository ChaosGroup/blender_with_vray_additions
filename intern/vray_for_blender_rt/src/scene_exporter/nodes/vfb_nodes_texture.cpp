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
#include "vfb_utils_blender.h"
#include "vfb_utils_nodes.h"
#include "vfb_utils_math.h"
#include "vfb_utils_mesh.h"
#include "vfb_utils_string.h"
#include "vfb_params_json.h"

using namespace VRayForBlender;

int DataExporter::fillBitmapAttributes(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &, NodeContext &, PluginDesc &pluginDesc)
{
	BL::Texture texture(Blender::GetDataFromProperty<BL::Texture>(&node.ptr, "texture"));
	if (texture) {
		BL::ImageTexture imageTexture(texture);
		if (imageTexture) {
			BL::Image image(imageTexture.image());
			if (image) {
				std::string absFilepath = Blender::GetFilepath(image.filepath(), (ID*)ntree.ptr.data);

				if(image.source() == BL::Image::source_SEQUENCE) {
					BL::ImageUser imageUser = imageTexture.image_user();

					int seqFrame = 0;

					int seqOffset = imageUser.frame_offset();
					int seqLength = imageUser.frame_duration();
					int seqStart  = imageUser.frame_start();
					int seqEnd    = seqLength - seqStart + 1;
					int currentFrame = static_cast<int>(m_settings.settings_animation.frame_current);

					if (imageUser.use_cyclic()) {
						seqFrame = ((currentFrame - seqStart) % seqLength) + 1;
					}
					else {
						if (currentFrame < seqStart){
							seqFrame = seqStart;
						} else if (currentFrame > seqEnd) {
							seqFrame = seqEnd;
						} else {
							seqFrame = seqStart + currentFrame - 1;
						}
					}
					if(seqOffset < 0) {
						if((seqFrame - abs(seqOffset)) < 0) {
							seqFrame += seqLength;
						}
					}

					pluginDesc.add("frame_sequence", true);
					pluginDesc.add("frame_offset", seqOffset);
					pluginDesc.add("frame_number", seqFrame);
				}

				pluginDesc.add(PluginAttr("file", absFilepath, INVALID_FRAME));
			}

			PointerRNA vrayScene = RNA_pointer_get(&m_scene.ptr, "vray");
			PointerRNA settingsColorMapping = RNA_pointer_get(&vrayScene, "SettingsColorMapping");

			PointerRNA bitmapBuffer = RNA_pointer_get(&node.ptr, "BitmapBuffer");
			bool use_input_gamma = RNA_boolean_get(&bitmapBuffer, "use_input_gamma") &&
			                       RNA_boolean_get(&settingsColorMapping, "use_input_gamma");
			if(use_input_gamma) {
				pluginDesc.add("gamma", RNA_float_get(&settingsColorMapping, "input_gamma"));
				pluginDesc.add("color_space", 1);

				getLog().info("Node tree: %s => Node name: %s => \"Use Input Gamma\" is used. "
				              "\"Color Space\" is forced to \"Gamma Corrected\"",
				              ntree.name().c_str(), node.name().c_str());
			}
		}
	}

	return 0;
}


AttrValue DataExporter::exportVRayNodeBitmapBuffer(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	AttrValue plugin;

	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(pluginName, "BitmapBuffer");

	if (fillBitmapAttributes(ntree, node, fromSocket, context, pluginDesc)) {
		getLog().error("Node tree: %s => Node name: %s => Something wrong with BitmapBuffer!",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

		plugin = m_exporter->export_plugin(pluginDesc);
	}

	return plugin;
}


void DataExporter::fillRampAttributes(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &, NodeContext &context,
                                      PluginDesc &pluginDesc,
                                      const std::string &texAttrName, const std::string &colAttrName, const std::string &posAttrName, const std::string &typesAttrName)
{
	const char * subPluginNameFmt("%s@%s");
	const char * subTexNameFmt("%sPos%i");

	BL::Texture tex(Blender::GetDataFromProperty<BL::Texture>(&node.ptr, texAttrName));
	if (tex) {
		char pluginName[String::MAX_PLG_LEN] = {0, };
		snprintf(pluginName, sizeof(pluginName), subPluginNameFmt, DataExporter::GenPluginName(node, ntree, context).c_str(), texAttrName.c_str());

		BL::ColorRamp ramp = tex.color_ramp();

		AttrListPlugin  colors;
		AttrListValue   positions;
		AttrListInt     types;

		int interp;
		switch(ramp.interpolation()) {
			case BL::ColorRamp::interpolation_CONSTANT: interp = 0; break;
			case BL::ColorRamp::interpolation_LINEAR:   interp = 1; break;
			case BL::ColorRamp::interpolation_EASE:     interp = 2; break;
			case BL::ColorRamp::interpolation_CARDINAL: interp = 3; break;
			case BL::ColorRamp::interpolation_B_SPLINE: interp = 4; break;
			default:                                    interp = 1;
		}

		int elNum = 0;
		BL::ColorRamp::elements_iterator elIt;
		for (ramp.elements.begin(elIt); elIt != ramp.elements.end(); ++elIt, ++elNum) {
			BL::ColorRampElement el(*elIt);
			char colPluginName[String::MAX_PLG_LEN] = {0, };
			snprintf(colPluginName, sizeof(colPluginName), subTexNameFmt, pluginName, elNum);
			const float pos = el.position();

			PluginDesc colDesc(colPluginName, "TexAColor");
			colDesc.add("texture", AttrAColorFromBlColor(el.color()));

			colors.append(m_exporter->export_plugin(colDesc));
			positions.append(pos);
			types.append(interp);
		}

		pluginDesc.add(colAttrName, colors);
		pluginDesc.add(posAttrName, positions);
		if (NOT(typesAttrName.empty())) {
			pluginDesc.add(typesAttrName, types);
		}
	}
}


AttrValue DataExporter::exportVRayNodeMetaImageTexture(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	AttrValue plugin;

	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(pluginName, "TexBitmap");

	const std::string &bitmapPluginName = "Bitmap@" + pluginName;
	PluginDesc bitmapDesc(bitmapPluginName, "BitmapBuffer");

	if (fillBitmapAttributes(ntree, node, fromSocket, context, bitmapDesc)) {
		getLog().error("Node tree: %s => Node name: %s => Something wrong with BitmapBuffer!",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		setAttrsFromNode(ntree, node, fromSocket, context, bitmapDesc, "BitmapBuffer", ParamDesc::PluginTexture);

		AttrValue bitmapPlugin = m_exporter->export_plugin(bitmapDesc);

		const int   mappingType = RNA_enum_ext_get(&node.ptr, "mapping_type");
		std::string mappingPluginID;
		switch (mappingType) {
			case 0: mappingPluginID = "UVWGenMayaPlace2dTexture"; break;
			case 1: mappingPluginID = "UVWGenProjection"; break;
			case 2: mappingPluginID = "UVWGenObject"; break;
			case 3: mappingPluginID = "UVWGenEnvironment"; break;
			default:
				break;
		}

		const std::string &mappingPluginName = "Mapping@" + pluginName;

		AttrValue   mappingPlugin;
		PluginDesc  mappingDesc(mappingPluginName, mappingPluginID);

		// This means manually specified mapping node
		if (mappingPluginID.empty()) {
			BL::NodeSocket mappingSock = Nodes::GetInputSocketByName(node, "Mapping");
			if (mappingSock && mappingSock.is_linked()) {
				mappingPlugin = DataExporter::exportLinkedSocket(ntree, mappingSock, context);
			}
			else {
				// Fallback to some default
				mappingDesc.pluginID   = "UVWGenChannel";
				mappingDesc.pluginName = "DefChannelMapping@" + pluginName;
				mappingDesc.add("uvw_channel", 0);

				mappingPlugin = m_exporter->export_plugin(mappingDesc);
			}
		}
		else {
			if (mappingPluginID == "UVWGenMayaPlace2dTexture") {
				BL::NodeSocket rotateFrameTexSock = Nodes::GetSocketByAttr(node, "rotate_frame_tex");
				if (rotateFrameTexSock && NOT(rotateFrameTexSock.is_linked())) {
					const float rotate_frame_tex = DEG_TO_RAD(RNA_float_get(&rotateFrameTexSock.ptr, "value"));

					mappingDesc.add("rotate_frame_tex", rotate_frame_tex);
				}
			}
			else if (mappingPluginID == "UVWGenEnvironment") {
				PointerRNA UVWGenEnvironment = RNA_pointer_get(&node.ptr, "UVWGenEnvironment");
				const int mapping_type = RNA_enum_get(&UVWGenEnvironment, "mapping_type");

				if (context.isWorldNtree) {
					// for world ntrees allow exporting uvw_matrix so env textures can be rotated
					auto inputSocket = Nodes::GetSocketByAttr(node, "uvw_matrix");
					if (inputSocket && inputSocket.is_linked()) {
						mappingDesc.add("uvw_matrix", exportLinkedSocket(ntree, inputSocket, context));
					} else {
						mappingDesc.add("uvw_matrix", AttrTransform::identity().m);
					}
				} else if (context.object_context.object && context.object_context.object.type() == BL::Object::type_LAMP) {
					// adds rotations for lamp textures (dome)
					float obMat[4][4];
					invert_m4_m4(obMat, reinterpret_cast<float (*)[4]>(context.object_context.object.matrix_world().data));
					AttrMatrix matrix = AttrTransformFromBlTransform(obMat).m;

					mappingDesc.add("uvw_matrix", matrix);
				} else if (!context.object_context.object) {
					mappingDesc.add("uvw_matrix", AttrTransform::identity().m);
				}

				mappingDesc.add("mapping_type", EnvironmentMappingType[mapping_type]);
			}

			mappingDesc.pluginID   = mappingPluginID;
			mappingDesc.pluginName = "Mapping@" + pluginName + "|" + mappingPluginID;

			setAttrsFromNode(ntree, node, fromSocket, context, mappingDesc, mappingPluginID, ParamDesc::PluginUvwgen);

			PluginAttr *uv_set_name = mappingDesc.get("uv_set_name");
			if (uv_set_name) {
				uv_set_name->attrValue.as<AttrSimpleType<std::string>>() = uv_set_name->attrValue.as<AttrSimpleType<std::string>>().value;
			}
			else {
				mappingDesc.add("uv_set_name", "UVMap");
			}

			mappingPlugin = m_exporter->export_plugin(mappingDesc);
		}

		pluginDesc.add("bitmap", bitmapPlugin.as<AttrPlugin>());
		pluginDesc.add("uvwgen", mappingPlugin.as<AttrPlugin>());

		setAttrsFromNode(ntree, node, fromSocket, context, pluginDesc, "TexBitmap", ParamDesc::PluginTexture);

		plugin = m_exporter->export_plugin(pluginDesc);
	}

	return plugin;
}


AttrValue DataExporter::exportVRayNodeTexMulti(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);

	AttrListPlugin textures;
	AttrListInt    textures_ids;

	for(int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		char texSocketName[32] = {0, };
		snprintf(texSocketName, sizeof(texSocketName), "Texture %i", i);

		BL::NodeSocket texSock = Nodes::GetInputSocketByName(node, texSocketName);
		if (texSock && texSock.is_linked()) {
			AttrValue texture = exportLinkedSocket(ntree, texSock, context);

			textures.append(texture.as<AttrPlugin>());
			textures_ids.append(RNA_int_get(&texSock.ptr, "value"));
		}
	}

	PluginDesc pluginDesc(pluginName, "TexMulti");
	if (!textures.empty()) {
		pluginDesc.add("textures_list", textures);
		pluginDesc.add("ids_list", textures_ids);
	}
	pluginDesc.add("interpolate", RNA_boolean_get(&node.ptr, "interpolate"));

	const int modeIdx = RNA_enum_get(&node.ptr, "mode");
	const int modeValueMap[] = {0, 1, 2, 3, 4, 6, 30};
	const int mode = modeValueMap[Math::clamp<int>(modeIdx, 0, sizeof(modeValueMap) / sizeof(modeValueMap[0]) - 1)];
	pluginDesc.add("mode", mode);

	BL::NodeSocket textureDefaultSock = Nodes::GetInputSocketByName(node, "Default");
	if (textureDefaultSock) {
		pluginDesc.add("default_texture", exportSocket(ntree, textureDefaultSock, context));
	}

	BL::NodeSocket idGenTex = Nodes::GetSocketByAttr(node, "id_gen_tex");
	if (idGenTex && idGenTex.is_linked() && mode == 30) {
		pluginDesc.add("id_gen_tex", exportLinkedSocket(ntree, idGenTex, context));
	}

	return m_exporter->export_plugin(pluginDesc);
}


AttrValue DataExporter::exportVRayNodeTexLayered(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context) {
	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);

	AttrListPlugin  textures;
	AttrListInt     blend_modes;

	for (int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		char texSockName[32] = {0, };
		snprintf(texSockName, sizeof(texSockName), "Texture %i", i);

		BL::NodeSocket texSock = Nodes::GetInputSocketByName(node, texSockName);
		if (texSock && texSock.is_linked()) {
			AttrValue texture = exportLinkedSocket(ntree, texSock, context);
			if (texture) {
				// XXX: For some reason TexLayered doesn't like ::out_smth
				texture.as<AttrPlugin>().output.clear();

				const int   blend_mode = RNA_enum_get(&texSock.ptr, "value");
				const float blend_amount = RNA_float_get(&texSock.ptr, "blend");

				// If blend amount is less then 1.0f we'll modify alpha
				if (blend_amount < 1.0f) {
					char blendName[String::MAX_PLG_LEN] = {0, };
					snprintf(blendName, sizeof(blendName), "Tex%sBlend%i", pluginName.c_str(), i);

					PluginDesc blendDesc(blendName, "TexAColorOp");
					blendDesc.add("color_a", texture);
					blendDesc.add("mult_a", 1.0f);
					blendDesc.add("mode", 0); // Mode: "result_a"
					blendDesc.add("result_alpha", blend_amount);

					texture = m_exporter->export_plugin(blendDesc);
				}

				textures.append(texture.as<AttrPlugin>());
				blend_modes.append(blend_mode);
			}
		}
	}

	std::reverse(textures.getData()->begin(), textures.getData()->end());
	std::reverse(blend_modes.getData()->begin(), blend_modes.getData()->end());

	PluginDesc pluginDesc(pluginName, "TexLayered");
	pluginDesc.add("textures", textures);
	pluginDesc.add("blend_modes", blend_modes);


	const ParamDesc::PluginParamDesc &pluginParamDesc = GetPluginDescription("TexLayered");
	HashSet<std::string> mappableValues = {"alpha", "alpha_mult", "alpha_offset", "nouvw_color", "color_mult", "color_offset"};

	for (const auto &descIt : pluginParamDesc.attributes) {
		const auto & attrName = descIt.second.name;

		if (mappableValues.find(attrName) == mappableValues.end()) {
			continue;
		}

		BL::NodeSocket attrSock = getSocketByAttr(node, attrName);
		if(attrSock && attrSock.is_linked()) {
			pluginDesc.add(attrName, exportLinkedSocket(ntree, attrSock, context));
		}
	}

	return m_exporter->export_plugin(pluginDesc);
}


AttrValue DataExporter::exportVRayNodeTexSky(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);

	BL::Object sun(PointerRNA_NULL);

	BL::NodeSocket sunSock(Nodes::GetInputSocketByName(node, "Sun"));
	if (sunSock && sunSock.is_linked()) {
		BL::Node conNode(Nodes::GetConnectedNode(sunSock));
		if (conNode) {
			if (NOT(conNode.bl_idname() == "VRayNodeSelectObject")) {
				getLog().error("Sun node could be selected only with \"Select Object\" node.");
			}
			else {
				BL::Object sunOb = exportVRayNodeSelectObject(ntree, conNode, sunSock, context);
				if (sunOb && sunOb.type() == BL::Object::type_LAMP) {
					sun = sunOb;
				}
			}
		}
	}
	else {
		BL::Scene::objects_iterator obIt;
		// TODO: is it possible not to iterate all objects
		for (m_scene.objects.begin(obIt); obIt != m_scene.objects.end(); ++obIt) {
			BL::Object ob(*obIt);
			if (ob && ob.type() == BL::Object::type_LAMP) {
				BL::ID laID(ob.data());
				if (laID) {
					BL::Lamp la(laID);
					if (la.type() == BL::Lamp::type_SUN) {
						PointerRNA vrayLight = RNA_pointer_get(&la.ptr, "vray");
						const int direct_type = RNA_enum_get(&vrayLight, "direct_type");
						if (direct_type == 1) {
							sun = ob;
							break;
						}
					}
				}
			}
		}
	}

	PluginDesc pluginDesc(pluginName, "TexSky");
	if (sun) {
		pluginDesc.add("sun", AttrPlugin(getLightName(sun)));
	}

	return exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}


BL::NodeSocket DataExporter::getSocketByAttr(BL::Node node, const std::string &attrName)
{
	BL::Node::inputs_iterator sockIt;
	for(node.inputs.begin(sockIt); sockIt != node.inputs.end(); ++sockIt) {
		std::string sockAttrName;
		if(RNA_struct_find_property(&sockIt->ptr, "vray_attr")) {
			sockAttrName = RNA_std_string_get(&sockIt->ptr, "vray_attr");
		}

		if(sockAttrName.empty())
			continue;

		if(attrName == sockAttrName)
			return *sockIt;
	}

	return BL::NodeSocket(PointerRNA_NULL);
}


AttrValue DataExporter::exportVRayNodeTexFalloff(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	const std::string &pluginName = GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(pluginName, "TexFalloff");

	BL::NodeSocket blendInputSock = getSocketByAttr(node, "blend_input");
	BL::Node       blendInputNode = this->getConnectedNode(ntree, blendInputSock,context);
	if (blendInputNode && blendInputNode.is_a(&RNA_ShaderNodeVectorCurve)) {
		const std::string &subFalloffTexName = "SubFalloff@" + GenPluginName(node, ntree, context);
		PluginDesc subFalloffTexAttrs(subFalloffTexName, "TexFalloff");
		subFalloffTexAttrs.add("use_blend_input", 0);
		subFalloffTexAttrs.add("blend_input", AttrPlugin("NULL"));

		auto output = exportVRayNodeAuto(ntree, node, fromSocket, context, subFalloffTexAttrs);

		AttrListFloat points;
		AttrListInt types;
		fillNodeVectorCurveData(ntree, blendInputNode, points, types);

		const std::string &texBezierCurveName = GenPluginName(blendInputNode, ntree, context);
		PluginDesc texBezierCurveAttrs(texBezierCurveName, "TexBezierCurve");
		output.as<AttrPlugin>().output = "blend_output";
		texBezierCurveAttrs.add("input_float", output);
		texBezierCurveAttrs.add("points", points);
		texBezierCurveAttrs.add("types", types);

		auto plg = m_exporter->export_plugin(texBezierCurveAttrs);

		pluginDesc.add("use_blend_input", true);
		pluginDesc.add("blend_input", plg);
	}

	return exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}


AttrValue DataExporter::exportVRayNodeTexMeshVertexColorChannel(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "TexMeshVertexColorChannel");

	// Export attributes automatically from node
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	PluginAttr *channel_name = pluginDesc.get("channel_name");
	if (channel_name) {
		channel_name->attrValue.as<AttrSimpleType<std::string>>() = channel_name->attrValue.as<AttrSimpleType<std::string>>().value;
	}

	return m_exporter->export_plugin(pluginDesc);
}

AttrValue DataExporter::exportVRayNodeTexRemap(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PointerRNA texRemapPtr = RNA_pointer_get(&node.ptr, "TexRemap");
	enum RemapType {
		Value, Color, HSV
	};

	PluginDesc pluginDesc(GenPluginName(node, ntree, context), GetNodePluginID(node));

	// if this is used to output color we need split the remaps for color channels
	if (fromSocket.bl_idname() == "VRaySocketColor") {
		const RemapType type = static_cast<RemapType>(RNA_enum_ext_get(&texRemapPtr, "type"));

		// this hack is here because GPU does not support remapping of float to color
		// TODO: remove this when GPU adds support
		if (type == Value) {
			pluginDesc.add("color_colors", 0);
			// split the float input to color and pass the color as input
			PluginDesc floatToColorSplit(GenPluginName(node, ntree, context), "TexFloatToColor", "DummyFloatToColor@");
			auto inputColorSock = getSocketByAttr(node, "input_value");
			auto floatPlgInput = exportSocket(ntree, inputColorSock, context);

			floatToColorSplit.add("input", floatPlgInput);

			pluginDesc.add("input_color", m_exporter->export_plugin(floatToColorSplit));

			// set the mode to color
			pluginDesc.add("type", static_cast<int>(Color));
		}

		BL::Texture tex(Blender::GetDataFromProperty<BL::Texture>(&node.ptr, "texture"));
		if (!tex) {
			getLog().error("Failed to export TexRemap ramp for %s", node.name().c_str());
			return AttrPlugin("NULL");
		}
		char pluginName[String::MAX_PLG_LEN] = {0, };
		snprintf(pluginName, sizeof(pluginName), "%s@%s", GenPluginName(node, ntree, context).c_str(), "color_ramp");

		BL::ColorRamp ramp = tex.color_ramp();

		AttrListPlugin  floatVals[3];
		AttrListFloat   positions;
		AttrListInt     types;

		int interp;
		switch(ramp.interpolation()) {
			case BL::ColorRamp::interpolation_CONSTANT: interp = 0; break;
			case BL::ColorRamp::interpolation_LINEAR:   interp = 1; break;
			case BL::ColorRamp::interpolation_EASE:     interp = 2; break;
			case BL::ColorRamp::interpolation_CARDINAL: interp = 3; break;
			case BL::ColorRamp::interpolation_B_SPLINE: interp = 4; break;
			default:                                    interp = 1;
		}

		const int chCount = 3;
		static std::string channelNames[chCount] = {"red", "green", "blue"};

		int elNum = 0;
		for (auto & el : Blender::collection(ramp.elements)) {
			const float pos = el.position();
			const auto color = el.color();

			for (int c = 0; c < chCount; c++) {
				char colPluginName[String::MAX_PLG_LEN] = {0, };
				snprintf(colPluginName, sizeof(colPluginName), "%s_Channel|%sPos%i", pluginName, channelNames[c].c_str(), elNum);
				PluginDesc floatValTex(colPluginName, "TexFloat");
				floatValTex.add("input", color[c]);
				floatVals[c].append(m_exporter->export_plugin(floatValTex));
			}
			positions.append(pos);
			types.append(interp);
			elNum++;
		}

		for (int c = 0; c < chCount; c++) {
			pluginDesc.add(channelNames[c] + "_values", floatVals[c]);
			pluginDesc.add(channelNames[c] + "_positions", positions);
			pluginDesc.add(channelNames[c] + "_types", types);
		}
	}

	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	return m_exporter->export_plugin(pluginDesc);
}

AttrValue DataExporter::exportVRayNodeTexSoftbox(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PointerRNA texSoftbox = RNA_pointer_get(&node.ptr, "TexSoftbox");
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context), "TexSoftbox");

	if (RNA_boolean_get(&texSoftbox, "grad_vert_on")) {
		pluginDesc.add("grad_vert_on", true);
		DataExporter::fillRampAttributes(ntree, node, fromSocket, context,
											  pluginDesc, "ramp_grad_vert", "grad_vert_col", "grad_vert_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "grad_horiz_on")) {
		pluginDesc.add("grad_horiz_on", true);
		DataExporter::fillRampAttributes(ntree, node, fromSocket, context,
											  pluginDesc, "ramp_grad_horiz", "grad_horiz_col", "grad_horiz_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "grad_rad_on")) {
		pluginDesc.add("grad_rad_on", true);
		DataExporter::fillRampAttributes(ntree, node, fromSocket, context,
											  pluginDesc, "ramp_grad_rad", "grad_rad_col", "grad_rad_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "frame_on")) {
		pluginDesc.add("frame_on", true);
		DataExporter::fillRampAttributes(ntree, node, fromSocket, context,
											  pluginDesc, "ramp_frame", "frame_col", "frame_pos");
	}

	return exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}


AttrValue DataExporter::exportVRayNodeTexDistance(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
		DataExporter::GetNodePluginID(node));

	BL::NodeSocket objectsSocket = Nodes::GetSocketByAttr(node, "objects");
	if (objectsSocket.is_linked()) {
		BL::Node selectorNode = getConnectedNode(ntree, objectsSocket, context);
		// group selec is handled properly by DataExporter::exportVRayNode
		if (selectorNode && selectorNode.bl_idname() == "VRayNodeSelectObject") {
			BL::Object selectedOb = exportVRayNodeSelectObject(ntree, selectorNode, objectsSocket, context);
			if (selectedOb) {
				AttrListPlugin objectList = { getNodeName(selectedOb) };
				pluginDesc.add("objects", objectList);
			}
		}
	}

	return exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}