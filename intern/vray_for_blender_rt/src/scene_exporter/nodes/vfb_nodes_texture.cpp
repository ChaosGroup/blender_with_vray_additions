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
#include "vfb_params_json.h"


int DataExporter::fillBitmapAttributes(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &, NodeContext &, PluginDesc &pluginDesc)
{
	BL::Texture texture(Blender::GetDataFromProperty<BL::Texture>(&node.ptr, "texture"));
	if (texture) {
		BL::ImageTexture imageTexture(texture);
		if (imageTexture) {
			BL::Image image(imageTexture.image());
			if (image) {
				std::string absFilepath = Blender::GetFilepath(image.filepath(), (ID*)ntree.ptr.data);
				//absFilepath = BlenderUtils::CopyDRAsset(absFilepath);
#if 0
				if(image.source() == BL::Image::source_SEQUENCE) {
					BL::ImageUser imageUser = imageTexture.image_user();

					int seqFrame = 0;

					int seqOffset = imageUser.frame_offset();
					int seqLength = imageUser.frame_duration();
					int seqStart  = imageUser.frame_start();
					int seqEnd    = seqLength - seqStart + 1;

					if(imageUser.use_cyclic()) {
						seqFrame = ((ExporterSettings::gSet.m_frameCurrent - seqStart) % seqLength) + 1;
					}
					else {
						if(ExporterSettings::gSet.m_frameCurrent < seqStart){
							seqFrame = seqStart;
						}
						else if(ExporterSettings::gSet.m_frameCurrent > seqEnd) {
							seqFrame = seqEnd;
						}
						else {
							seqFrame = seqStart + ExporterSettings::gSet.m_frameCurrent - 1;
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
#endif
				pluginDesc.add("file", absFilepath);
			}
#if 0
			PointerRNA vrayScene = RNA_pointer_get(&ExporterSettings::gSet.b_scene.ptr, "vray");
			PointerRNA settingsColorMapping = RNA_pointer_get(&vrayScene, "SettingsColorMapping");

			PointerRNA bitmapBuffer = RNA_pointer_get(&node.ptr, "BitmapBuffer");
			bool use_input_gamma = RNA_boolean_get(&bitmapBuffer, "use_input_gamma") &&
			                       RNA_boolean_get(&settingsColorMapping, "use_input_gamma");
			if(use_input_gamma) {
				pluginDesc["gamma"] = BOOST_FORMAT_FLOAT(RNA_float_get(&settingsColorMapping, "input_gamma"));
				pluginDesc["color_space"] = "1";

				PRINT_INFO_EX("Node tree: %s => Node name: %s => \"Use Input Gamma\" is used. "
				              "\"Color Space\" is forced to \"Gamma Corrected\"",
				              ntree.name().c_str(), node.name().c_str());
			}
#endif
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
		PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with BitmapBuffer!",
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
	boost::format subPluginNameFmt("%s@%s");
	boost::format subTexNameFmt("%sPos%i");

	BL::Texture tex(Blender::GetDataFromProperty<BL::Texture>(&node.ptr, texAttrName));
	if (tex) {
		const std::string &pluginName = boost::str(subPluginNameFmt
		                                           % DataExporter::GenPluginName(node, ntree, context)
		                                           % texAttrName);

		BL::ColorRamp ramp = tex.color_ramp();

		AttrListPlugin  colors;
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

		int elNum = 0;
		BL::ColorRamp::elements_iterator elIt;
		for (ramp.elements.begin(elIt); elIt != ramp.elements.end(); ++elIt, ++elNum) {
			BL::ColorRampElement el(*elIt);

			const std::string &colPluginName = boost::str(subTexNameFmt
			                                              % pluginName
			                                              % elNum);

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


AttrValue DataExporter::exportVRayNodeTexEdges(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "TexEdges");

	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	pluginDesc.add("world_width", pluginDesc.get("pixel_width"));

	return m_exporter->export_plugin(pluginDesc);
}


AttrValue DataExporter::exportVRayNodeMetaImageTexture(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	AttrValue plugin;

	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(pluginName, "TexBitmap");

	const std::string &bitmapPluginName = "Bitmap@" + pluginName;
	PluginDesc bitmapDesc(bitmapPluginName, "BitmapBuffer");

	if (fillBitmapAttributes(ntree, node, fromSocket, context, bitmapDesc)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with BitmapBuffer!",
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

				// adds rotations for lamp textures (dome)
				if (context.object_context.object && context.object_context.object.type() == BL::Object::type_LAMP) {
					float obMat[4][4];
					invert_m4_m4(obMat, reinterpret_cast<float (*)[4]>(context.object_context.object.matrix_world().data));
					AttrMatrix matrix = AttrTransformFromBlTransform(obMat).m;

					mappingDesc.add("uvw_matrix", matrix);
				}

				mappingDesc.add("mapping_type", EnvironmentMappingType[mapping_type]);
			}

			mappingDesc.pluginID   = mappingPluginID;
			mappingDesc.pluginName = "Mapping@" + pluginName;

			setAttrsFromNode(ntree, node, fromSocket, context, mappingDesc, mappingPluginID, ParamDesc::PluginUvwgen);

			PluginAttr *uv_set_name = mappingDesc.get("uv_set_name");
			if (uv_set_name) {
				uv_set_name->attrValue.valString = VRayForBlender::Mesh::UvChanNamePrefix + uv_set_name->attrValue.valString;
			}
			else {
				mappingDesc.add("uv_set_name", "UvUVMap");
			}

			mappingPlugin = m_exporter->export_plugin(mappingDesc);
		}

		pluginDesc.add("bitmap", bitmapPlugin.valPlugin);
		pluginDesc.add("uvwgen", mappingPlugin.valPlugin);

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
		boost::format sockTexFmt("Texture %i");

		const std::string &texSockName = boost::str(sockTexFmt % i);

		BL::NodeSocket texSock = Nodes::GetInputSocketByName(node, texSockName);
		if (texSock && texSock.is_linked()) {
			AttrValue texture = exportLinkedSocket(ntree, texSock, context);

			textures.append(texture.valPlugin);
			textures_ids.append(RNA_int_get(&texSock.ptr, "value"));
		}
	}

	PluginDesc pluginDesc(pluginName, "TexMulti");
	pluginDesc.add("textures_list", textures);
	pluginDesc.add("ids_list", textures_ids);
	pluginDesc.add("mode", RNA_enum_get(&node.ptr, "mode"));

	BL::NodeSocket textureDefaultSock = Nodes::GetInputSocketByName(node, "Default");
	if (textureDefaultSock && textureDefaultSock.is_linked()) {
		pluginDesc.add("default_texture", exportLinkedSocket(ntree, textureDefaultSock, context));
	}

	return exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
}


AttrValue DataExporter::exportVRayNodeTexLayered(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context) {
	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);

	AttrListPlugin  textures;
	AttrListInt     blend_modes;

	for (int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		boost::format  sockTexFmt("Texture %i");
		const std::string    &texSockName = boost::str(sockTexFmt % i);

		BL::NodeSocket texSock = Nodes::GetInputSocketByName(node, texSockName);
		if (texSock && texSock.is_linked()) {
			AttrValue texture = exportLinkedSocket(ntree, texSock, context);
			if (texture) {
				// XXX: For some reason TexLayered doesn't like ::out_smth
				texture.valPlugin.output.clear();

				const int   blend_mode = RNA_enum_get(&texSock.ptr, "value");
				const float blend_amount = RNA_float_get(&texSock.ptr, "blend");

				// If blend amount is less then 1.0f we'll modify alpha
				if (blend_amount < 1.0f) {
					boost::format  texBlendNameFmt("Tex%sBlend%i");
					const std::string    &blendName = boost::str(texBlendNameFmt % pluginName % i);

					PluginDesc blendDesc(blendName, "TexAColorOp");
					blendDesc.add("color_a", texture);
					blendDesc.add("mult_a", 1.0f);
					blendDesc.add("mode", 0); // Mode: "result_a"
					blendDesc.add("result_alpha", blend_amount);

					texture = m_exporter->export_plugin(blendDesc);
				}

				textures.append(texture.valPlugin);
				blend_modes.append(blend_mode);
			}
		}
	}

	std::reverse(textures.ptr()->begin(), textures.ptr()->end());
	std::reverse(blend_modes.ptr()->begin(), blend_modes.ptr()->end());

	PluginDesc pluginDesc(pluginName, "TexLayered");
	pluginDesc.add("textures", textures);
	pluginDesc.add("blend_modes", blend_modes);


	const ParamDesc::PluginDesc &pluginParamDesc = GetPluginDescription("TexLayered");
	std::unordered_set<std::string> mappableValues = {"alpha", "alpha_mult", "alpha_offset", "nouvw_color", "color_mult", "color_offset"};

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
				PRINT_ERROR("Sun node could be selected only with \"Select Object\" node.");
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
		texBezierCurveAttrs.add("input_float", output);
		texBezierCurveAttrs.add("points", points);
		texBezierCurveAttrs.add("types", types);

		auto plg = m_exporter->export_plugin(texBezierCurveAttrs);
		plg.output = "blend_output";

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
		channel_name->attrValue.valString = VRayForBlender::Mesh::ColChanNamePrefix + channel_name->attrValue.valString;
	}

	return m_exporter->export_plugin(pluginDesc);
}
