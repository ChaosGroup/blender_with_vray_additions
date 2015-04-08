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


int DataExporter::exportBitmapBuffer(VRayNodeExportParam, PluginDesc &pluginDesc)
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


AttrValue DataExporter::exportVRayNodeBitmapBuffer(VRayNodeExportParam)
{
	AttrValue plugin;

	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(pluginName, "BitmapBuffer");

	if (exportBitmapBuffer(ntree, node, fromSocket, context, pluginDesc)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with BitmapBuffer!",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

		plugin = m_exporter->export_plugin(pluginDesc);
	}

	return plugin;
}


void DataExporter::exportRampAttribute(VRayNodeExportParam, PluginDesc &attrs,
                                       const std::string &texAttrName, const std::string &colAttrName, const std::string &posAttrName, const std::string &typesAttrName)
{
#if 0
	BL::Texture b_tex = NodeExporter::getTextureFromProperty(&node.ptr, texAttrName);
	if(b_tex) {
		std::string pluginName = NodeExporter::getPluginName(node, ntree, context) + "@" + texAttrName;

		BL::ColorRamp ramp = b_tex.color_ramp();

		StrVector   colors;
		StrVector   positions;
		StrVector   types;
		std::string interpType;

		int interp;
		switch(ramp.interpolation()) {
			case BL::ColorRamp::interpolation_CONSTANT: interp = 0; break;
			case BL::ColorRamp::interpolation_LINEAR:   interp = 1; break;
			case BL::ColorRamp::interpolation_EASE:     interp = 2; break;
			case BL::ColorRamp::interpolation_CARDINAL: interp = 3; break;
			case BL::ColorRamp::interpolation_B_SPLINE: interp = 4; break;
			default:                                    interp = 1;
		}
		interpType = BOOST_FORMAT_INT(interp);

		BL::ColorRamp::elements_iterator elIt;
		int                              elNum = 0;

		// XXX: Check why in Python I use reversed order
		//
		for(ramp.elements.begin(elIt); elIt != ramp.elements.end(); ++elIt) {
			BL::ColorRampElement el = *elIt;

			std::string colPluginName = boost::str(boost::format("%sPos%i") % pluginName % elNum++);

			std::string color    = BOOST_FORMAT_ACOLOR(el.color());
			std::string position = BOOST_FORMAT_FLOAT(el.position());

			PluginDesc colAttrs;
			colAttrs["texture"] = color;

			VRayNodePluginExporter::exportPlugin("TEXTURE", "TexAColor", colPluginName, colAttrs);

			colors.push_back(colPluginName);
			positions.push_back(position);
			types.push_back(interpType);
		}

		attrs[colAttrName] = BOOST_FORMAT_LIST(colors);
		attrs[posAttrName] = BOOST_FORMAT_LIST_FLOAT(positions);
		if (NOT(typesAttrName.empty())) {
			attrs[typesAttrName] = BOOST_FORMAT_LIST_INT(types);
		}
	}
#endif
}


AttrValue DataExporter::exportVRayNodeTexEdges(VRayNodeExportParam)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "TexEdges");

	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	pluginDesc.add("world_width", pluginDesc.get("pixel_width"));

	return m_exporter->export_plugin(pluginDesc);
}


AttrValue DataExporter::exportVRayNodeTexSoftbox(VRayNodeExportParam)
{
#if 0
	PointerRNA texSoftbox = RNA_pointer_get(&node.ptr, "TexSoftbox");
	PluginDesc manualAttrs;

	if (RNA_boolean_get(&texSoftbox, "grad_vert_on")) {
		NodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
		                                  manualAttrs, "ramp_grad_vert", "grad_vert_col", "grad_vert_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "grad_horiz_on")) {
		NodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
		                                  manualAttrs, "ramp_grad_horiz", "grad_horiz_col", "grad_horiz_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "grad_rad_on")) {
		NodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
		                                  manualAttrs, "ramp_grad_rad", "grad_rad_col", "grad_rad_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "frame_on")) {
		NodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
		                                  manualAttrs, "ramp_frame", "frame_col", "frame_pos");
	}

	return NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, manualAttrs);
#endif
	return AttrValue();
}


AttrValue DataExporter::exportVRayNodeTexRemap(VRayNodeExportParam)
{
#if 0
	BL::Texture b_tex = NodeExporter::getTextureFromProperty(&node.ptr, "texture");
	if(b_tex) {
		PluginDesc manualAttrs;
		NodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
		                                  manualAttrs, "texture", "color_colors", "color_positions", "color_types");

		return NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, manualAttrs);
	}

	PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with TexRemap!",
	            ntree.name().c_str(), node.name().c_str());

	return "NULL";
#endif
	return AttrValue();
}


AttrValue DataExporter::exportVRayNodeTexGradRamp(VRayNodeExportParam)
{
#if 0
	BL::Texture b_tex = NodeExporter::getTextureFromProperty(&node.ptr, "texture");
	if(b_tex) {
		PluginDesc manualAttrs;
		NodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
		                                  manualAttrs, "texture", "colors", "positions");

		return NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, manualAttrs);
	}

	PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with TexGradRamp!",
	            ntree.name().c_str(), node.name().c_str());

	return "NULL";
#endif
	return AttrValue();
}



AttrValue DataExporter::exportVRayNodeMetaImageTexture(VRayNodeExportParam)
{
	AttrValue plugin;

	const std::string &pluginName = DataExporter::GenPluginName(node, ntree, context);
	PluginDesc pluginDesc(pluginName, "TexBitmap");

	const std::string &bitmapPluginName = "Bitmap@" + pluginName;
	PluginDesc bitmapDesc(bitmapPluginName, "BitmapBuffer");

	if (exportBitmapBuffer(ntree, node, fromSocket, context, bitmapDesc)) {
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

				mappingDesc.add("mapping_type", EnvironmentMappingType[mapping_type]);
			}

			mappingDesc.pluginID   = mappingPluginID;
			mappingDesc.pluginName = "Mapping@" + pluginName;

			setAttrsFromNode(ntree, node, fromSocket, context, mappingDesc, mappingPluginID, ParamDesc::PluginUvwgen);

			mappingPlugin = m_exporter->export_plugin(mappingDesc);
		}

		pluginDesc.add("bitmap", bitmapPlugin.valPlugin);
		pluginDesc.add("uvwgen", mappingPlugin.valPlugin);

		setAttrsFromNode(ntree, node, fromSocket, context, pluginDesc, "TexBitmap", ParamDesc::PluginTexture);

		plugin = m_exporter->export_plugin(pluginDesc);
	}

	return plugin;
}


AttrValue DataExporter::exportVRayNodeTexMulti(VRayNodeExportParam)
{
#if 0
	std::string pluginName = NodeExporter::getPluginName(node, ntree, context);

	StrVector textures;
	StrVector textures_ids;

	BL::NodeSocket textureDefaultSock = NodeExporter::getSocketByName(node, "Default");

	for(int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		std::string texSockName = boost::str(boost::format("Texture %i") % i);

		BL::NodeSocket texSock = NodeExporter::getSocketByName(node, texSockName);
		if(NOT(texSock))
			continue;

		if(NOT(texSock.is_linked()))
			continue;

		std::string texture   = NodeExporter::exportLinkedSocket(ntree, texSock, context);
		std::string textureID = BOOST_FORMAT_INT(RNA_int_get(&texSock.ptr, "value"));

		textures.push_back(texture);
		textures_ids.push_back(textureID);
	}

	PluginDesc pluginAttrs;
	pluginAttrs["textures_list"]   = BOOST_FORMAT_LIST(textures);
	pluginAttrs["ids_list"]        = BOOST_FORMAT_LIST_INT(textures_ids);
	pluginAttrs["mode"]            = BOOST_FORMAT_INT(RNA_enum_get(&node.ptr, "mode"));
	pluginAttrs["default_texture"] = NodeExporter::exportLinkedSocket(ntree, textureDefaultSock, context);

	VRayNodePluginExporter::exportPlugin("TEXTURE", "TexMulti", pluginName, pluginAttrs);

	return pluginName;
#endif
	return AttrValue();
}


AttrValue DataExporter::exportVRayNodeTexLayered(VRayNodeExportParam)
{
#if 0
	std::string pluginName = NodeExporter::getPluginName(node, ntree, context);

	StrVector textures;
	StrVector blend_modes;

	for(int i = 1; i <= CGR_MAX_LAYERED_TEXTURES; ++i) {
		std::string texSockName = boost::str(boost::format("Texture %i") % i);

		BL::NodeSocket texSock = NodeExporter::getSocketByName(node, texSockName);
		if(NOT(texSock))
			continue;

		if(NOT(texSock.is_linked()))
			continue;

		std::string texture = NodeExporter::exportLinkedSocket(ntree, texSock, context);

		// NOTE: For some reason TexLayered doesn't like ::out_smth
		size_t semiPos = texture.find("::");
		if(semiPos != std::string::npos)
			texture.erase(texture.begin()+semiPos, texture.end());

		std::string blend_mode = boost::str(boost::format("%i") % RNA_enum_get(&texSock.ptr, "value"));

		const float blend_amount = RNA_float_get(&texSock.ptr, "blend");
		if (blend_amount != 1.0f) {
			const std::string &blendName =  boost::str(boost::format("Tex%sBlend%i") % pluginName % i);

			PluginDesc blendAttrs;
			blendAttrs["color_a"] = texture;
			blendAttrs["mult_a"]  = "1.0";
			blendAttrs["mode"]    = "0"; // Mode: "result_a"

			blendAttrs["result_alpha"] = BOOST_FORMAT_FLOAT(blend_amount);

			VRayNodePluginExporter::exportPlugin("TEXTURE", "TexAColorOp", blendName, blendAttrs);

			texture = blendName;
		}

		textures.push_back(texture);
		blend_modes.push_back(blend_mode);
	}

	std::reverse(textures.begin(), textures.end());
	std::reverse(blend_modes.begin(), blend_modes.end());

	PluginDesc pluginAttrs;
	pluginAttrs["textures"]    = BOOST_FORMAT_LIST(textures);
	pluginAttrs["blend_modes"] = BOOST_FORMAT_LIST_INT(blend_modes);

	StrVector mappableValues;
	mappableValues.push_back("alpha");
	mappableValues.push_back("alpha_mult");
	mappableValues.push_back("alpha_offset");
	mappableValues.push_back("nouvw_color");
	mappableValues.push_back("color_mult");
	mappableValues.push_back("color_offset");

	for(StrVector::const_iterator mvIt = mappableValues.begin(); mvIt != mappableValues.end(); ++mvIt) {
		const std::string attrName = *mvIt;

		BL::NodeSocket attrSock = NodeExporter::getSocketByAttr(node, attrName);
		if(attrSock) {
			std::string socketValue = NodeExporter::exportSocket(ntree, attrSock);
			if(socketValue != "NULL")
				pluginAttrs[attrName] = socketValue;
		}
	}

	VRayNodePluginExporter::exportPlugin("TEXTURE", "TexLayered", pluginName, pluginAttrs);

	return pluginName;
#endif
	return AttrValue();
}


AttrValue DataExporter::exportVRayNodeTexSky(VRayNodeExportParam)
{
#if 0
	PluginDesc  attrs;

	BL::NodeSocket sunSock = NodeExporter::getSocketByName(node, "Sun");
	if (sunSock && sunSock.is_linked()) {
		BL::Node conNode = NodeExporter::getConnectedNode(sunSock);
		if (conNode) {
			if (NOT(conNode.bl_idname() == "VRayNodeSelectObject")) {
				PRINT_ERROR("Sun node could be selected only with \"Select Object\" node.");
			}
			else {
				BL::Object sunOb = NodeExporter::exportVRayNodeSelectObject(ntree, conNode, sunSock, context);
				if (sunOb && sunOb.type() == BL::Object::type_LAMP) {
					attrs["sun"] = GetIDName(sunOb, "LA");
				}
			}
		}
	}
	else {
		BL::Scene::objects_iterator obIt;
		for (ExporterSettings::gSet.b_scene.objects.begin(obIt); obIt != ExporterSettings::gSet.b_scene.objects.end(); ++obIt) {
			BL::Object ob = *obIt;
			if (ob.type() == BL::Object::type_LAMP) {
				BL::ID laID = ob.data();
				if (laID) {
					BL::Lamp la(laID);
					if (la.type() == BL::Lamp::type_SUN) {
						PointerRNA vrayLight = RNA_pointer_get(&la.ptr, "vray");
						const int direct_type = RNA_enum_get(&vrayLight, "direct_type");
						if (direct_type == 1) {
							attrs["sun"] = GetIDName(ob, "LA");
							break;
						}
					}
				}
			}
		}
	}

	return NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, attrs);
#endif
	return AttrValue();
}


AttrValue DataExporter::exportVRayNodeTexFalloff(VRayNodeExportParam)
{
#if 0
	PluginDesc falloffTexAttrs;

	BL::NodeSocket blendInputSock = NodeExporter::getSocketByAttr(node, "blend_input");
	BL::Node       blendInputNode = NodeExporter::getConnectedNode(blendInputSock, context);
	if (blendInputNode && blendInputNode.is_a(&RNA_ShaderNodeVectorCurve)) {
		const std::string &subFalloffTexName = "SubFalloff@" + NodeExporter::getPluginName(node, ntree, context);
		PluginDesc  subFalloffTexAttrs(falloffTexAttrs);
		subFalloffTexAttrs["use_blend_input"] = "0";
		subFalloffTexAttrs["blend_input"]     = "NULL";

		NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, subFalloffTexAttrs, subFalloffTexName);

		StrVector points;
		StrVector types;
		getNodeVectorCurveData(ntree, blendInputNode, points, types);

		const std::string &texBezierCurveName = NodeExporter::getPluginName(blendInputNode, ntree, context);
		PluginDesc texBezierCurveAttrs;
		texBezierCurveAttrs["input_float"] = subFalloffTexName + "::blend_output";
		texBezierCurveAttrs["points"] = BOOST_FORMAT_LIST_FLOAT(points);
		texBezierCurveAttrs["types"]  = BOOST_FORMAT_LIST_INT(types);

		VRayNodePluginExporter::exportPlugin("TEXTURE", "TexBezierCurve", texBezierCurveName, texBezierCurveAttrs);

		falloffTexAttrs["use_blend_input"] = "1";
		falloffTexAttrs["blend_input"]     = texBezierCurveName;
	}

	return NodeExporter::exportVRayNodeAuto(ntree, node, fromSocket, context, falloffTexAttrs);
#endif
	return AttrValue();
}

