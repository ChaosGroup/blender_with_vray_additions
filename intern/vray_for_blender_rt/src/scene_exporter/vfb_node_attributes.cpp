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

#include <boost/format.hpp>

#include "vfb_params_json.h"
#include "vfb_node_exporter.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_string.h"
#include "vfb_utils_nodes.h"

#include "utils/cgr_paths.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BKE_main.h"
#include "BKE_global.h"

void DataExporter::setAttrFromPropGroup(PointerRNA *propGroup, ID *holder, const ParamDesc::AttrDesc &attrDesc, PluginDesc &pluginDesc)
{
	// XXX: Check if we could get rid of ID and use (ID*)propGroup->data
	// Test with library linking
	//
	const std::string & attrName = attrDesc.name;
	PropertyRNA *prop = RNA_struct_find_property(propGroup, attrName.c_str());
	if (NOT(prop)) {
		PRINT_ERROR("Property '%s' not found!",
		            attrName.c_str());
	}
	else {
		PropertyType propType = RNA_property_type(prop);

		if (propType == PROP_STRING) {
			std::string absFilepath = RNA_std_string_get(propGroup, attrName);
			if (NOT(absFilepath.empty())) {
				PropertySubType propSubType = RNA_property_subtype(prop);
				if (propSubType == PROP_FILEPATH || propSubType == PROP_DIRPATH) {
					char fixedPath[PATH_MAX];
					strncpy(fixedPath, absFilepath.c_str(), PATH_MAX);
					BLI_path_abs(fixedPath, m_data.filepath().c_str());
					absFilepath = fixedPath;

					if (propSubType == PROP_FILEPATH) {
						absFilepath = BlenderUtils::GetFullFilepath(absFilepath, holder);
						absFilepath = BlenderUtils::CopyDRAsset(absFilepath);
					}
				}

				pluginDesc.add(attrName, absFilepath);
			}
		}
		else if (propType == PROP_BOOLEAN) {
			pluginDesc.add(attrName, RNA_boolean_get(propGroup, attrName.c_str()));
		}
		else if (propType == PROP_INT) {
			pluginDesc.add(attrName, RNA_int_get(propGroup, attrName.c_str()));
		}
		else if (propType == PROP_ENUM) {
			pluginDesc.add(attrName, RNA_enum_ext_get(propGroup, attrName.c_str()));
		}
		else if (propType == PROP_FLOAT) {
			if (NOT(RNA_property_array_check(prop))) {
				const float value = RNA_float_get(propGroup, attrName.c_str());
				if (attrDesc.options & ParamDesc::AttrOption_ExportAsColor) {
					pluginDesc.add(attrName, AttrAColor(value));
				} else {
					pluginDesc.add(attrName, value);
				}
			}
			else {
				PropertySubType propSubType = RNA_property_subtype(prop);
				if (propSubType == PROP_COLOR) {
					if (RNA_property_array_length(propGroup, prop) == 4) {
						float acolor[4];
						RNA_float_get_array(propGroup, attrName.c_str(), acolor);

						pluginDesc.add(attrName, AttrAColor(AttrColor(acolor), acolor[3]));
					}
					else {
						float color[3];
						RNA_float_get_array(propGroup, attrName.c_str(), color);

						pluginDesc.add(attrName, AttrColor(color));
					}
				}
				else {
					float vector[3];
					RNA_float_get_array(propGroup, attrName.c_str(), vector);

					pluginDesc.add(attrName, AttrVector(vector));
				}
			}
		}
		else {
			PRINT_ERROR("Property '%s': Unsupported property type '%i'.",
			            RNA_property_identifier(prop), propType);
		}
	}
}


void DataExporter::setAttrsFromPropGroupAuto(PluginDesc &pluginDesc, PointerRNA *propGroup, const std::string &pluginID)
{
	const ParamDesc::PluginDesc &pluginParamDesc = GetPluginDescription(pluginID);

	for (const auto &descIt : pluginParamDesc.attributes) {
		const std::string         &attrName = descIt.second.name;
		const ParamDesc::AttrType &attrType = descIt.second.type;

		if (attrType > ParamDesc::AttrTypeOutputStart && attrType < ParamDesc::AttrTypeOutputEnd) {
			continue;
		}
		else if (attrType >= ParamDesc::AttrTypeList && attrType < ParamDesc::AttrTypeListEnd) {
			continue;
		}
		else if (attrType >= ParamDesc::AttrTypeWidgetStart && attrType < ParamDesc::AttrTypeWidgetEnd) {
			continue;
		}
		// Skip manually specified attributes
		else if (!pluginDesc.get(attrName)) {
			// Set non-mapped attributes only
			if (!ParamDesc::TypeHasSocket(attrType)) {
				setAttrFromPropGroup(propGroup, (ID*)propGroup->data, descIt.second, pluginDesc);
			}
		}
	}
}


void DataExporter::setAttrsFromNode(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context, PluginDesc &pluginDesc, const std::string &pluginID, const ParamDesc::PluginType &pluginType)
{
	const ParamDesc::PluginDesc &pluginParamDesc = GetPluginDescription(pluginID);
	PointerRNA                   propGroup       = RNA_pointer_get(&node.ptr, pluginID.c_str());

	// Set non-mapped attributes
	setAttrsFromPropGroupAuto(pluginDesc, &propGroup, pluginID);

	// Set mapped attributes
	for (const auto &descIt : pluginParamDesc.attributes) {
		const ParamDesc::AttrDesc &attrDesc = descIt.second;
		const std::string         &attrName = attrDesc.name;
		const ParamDesc::AttrType &attrType = attrDesc.type;

		if (attrType > ParamDesc::AttrTypeOutputStart && attrType < ParamDesc::AttrTypeOutputEnd) {
			continue;
		}
		else if (attrType >= ParamDesc::AttrTypeList && attrType < ParamDesc::AttrTypeListEnd) {
			continue;
		}
		// Skip manually specified attributes
		else if (!pluginDesc.get(attrName)) {
			// PRINT_INFO_EX("  Processing attribute: \"%s\"", attrName.c_str());

			if (ParamDesc::TypeHasSocket(attrType)) {
				BL::NodeSocket sock = Nodes::GetSocketByAttr(node, attrName);
				if (sock) {
					AttrValue socketValue = exportSocket(ntree, sock, context);
					if (sock.is_linked()) {
						if (socketValue.type == ValueTypePlugin) {
							if (RNA_struct_find_property(&sock.ptr, "multiplier")) {
								const float mult = RNA_float_get(&sock.ptr, "multiplier") / 100.0f;
								if (mult != 1.0f) {
									boost::format multFmt("N%sS%sA%sMult");

									// XXX: Name here could be an issue with group nodes
									std::string multPluginName = boost::str(multFmt
									                                        % DataExporter::GenPluginName(node, ntree, context)
									                                        % sock.node().name()
									                                        % sock.name());

									const bool is_float_socket = (sock.rna_type().identifier().find("Float") != std::string::npos);
									if (is_float_socket) {
										PluginDesc multTex(multPluginName, "TexFloatOp");
										multTex.add("float_a", socketValue);
										multTex.add("float_b", mult);
										multTex.add("mode", 2); // "product"

										socketValue = m_exporter->export_plugin(multTex);
									}
									else {
										PluginDesc multTex(multPluginName, "TexAColorOp");
										multTex.add("color_a", socketValue);
										multTex.add("mult_a", mult);
										multTex.add("mode", 0); // "result_a"

										socketValue = m_exporter->export_plugin(multTex);
									}
								}
							}
						}
					}
					else {
						if ((pluginType == ParamDesc::PluginTexture) &&
						    (attrType == ParamDesc::AttrTypePluginUvwgen)) {
							const std::string uvwgenName = "UVW@" + DataExporter::GenPluginName(node, ntree, context);
							std::string       uvwgenType = "UVWGenObject";

							PluginDesc uvwgenDesc(uvwgenName, uvwgenType);

							if ((pluginID == "TexBitmap") ||
							    (DataExporter::GetConnectedNodePluginType(fromSocket) == ParamDesc::PluginLight))
							{
								uvwgenDesc.pluginID = "UVWGenChannel";
								uvwgenDesc.add("uvw_channel", int(0));
							}
							else if (m_settings.default_mapping == ExporterSettings::DefaultMappingCube) {
								uvwgenDesc.pluginID = "UVWGenProjection";
								uvwgenDesc.add("type", 5);
								uvwgenDesc.add("object_space", 1);
							}
							else if (m_settings.default_mapping == ExporterSettings::DefaultMappingObject) {
								uvwgenDesc.pluginID = "UVWGenObject";
							}
							else if (m_settings.default_mapping == ExporterSettings::DefaultMappingChannel) {
								uvwgenDesc.pluginID = "UVWGenChannel";
								uvwgenDesc.add("uvw_channel", 0);
							}

							socketValue = m_exporter->export_plugin(uvwgenDesc);
						}
#if 0
						else if (attrType == "TRANSFORM" ||
						         attrType == "TRANSFORM_TEXTURE") {
							pluginAttrs[attrName] = "TransformHex(\"" CGR_IDENTITY_TM  "\")";
						}
						else if (attrType == "MATRIX" ||
						         attrType == "MATRIX_TEXTURE") {
							pluginAttrs[attrName] = "Matrix(Vector(1,0,0),Vector(0,1,0),Vector(0,0,1))";
						}
#endif
					}

					pluginDesc.add(attrName, socketValue);
				}
			}
			else if (attrType == ParamDesc::AttrTypeWidgetRamp) {
				// To preserve compatibility with already existing projects
				const std::string texAttrName = ((pluginID == "TexGradRamp") || (pluginID == "TexRemap"))
				                                ? "texture"
				                                : attrName;

				fillRampAttributes(ntree, node, fromSocket, context, pluginDesc,
				                   texAttrName,
				                   attrDesc.descRamp.colors, attrDesc.descRamp.positions, attrDesc.descRamp.interpolations);
			}
			else if (attrType == ParamDesc::AttrTypeWidgetCurve) {
			}
		}

#if 0
		if (pluginType == "RENDERCHANNEL" && NOT(manualAttrs.count("name"))) {
			// Value will already contain quotes
			boost::replace_all(pluginAttrs["name"], "\"", "");

			std::string chanName = pluginAttrs["name"];
			if (NOT(chanName.length())) {
				PRINT_WARN("Node tree: \"%s\" => Node: \"%s\" => Render channel name is not set! Generating default..",
				           ntree.name().c_str(), node.name().c_str());

				if (pluginID == "RenderChannelColor") {
					PointerRNA renderChannelColor = RNA_pointer_get(&node.ptr, "RenderChannelColor");
					chanName = RNA_enum_name_get(&renderChannelColor, "alias");
				}
				else if (pluginID == "RenderChannelLightSelect") {
					chanName = "Light Select";
				}
				else {
					chanName = NodeExporter::GenPluginName(node, ntree, context);
				}
			}

			// Export in quotes
			pluginAttrs["name"] = BOOST_FORMAT_STRING(GetUniqueChannelName(chanName));
		}
#endif
	}
}


void DataExporter::setAttrsFromNodeAuto(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context, PluginDesc &pluginDesc)
{
	const ParamDesc::PluginType &pluginType = DataExporter::GetNodePluginType(node);
	const std::string           &pluginID   = DataExporter::GetNodePluginID(node);

	if (pluginID.empty()) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node plugin ID!",
		            ntree.name().c_str(), node.name().c_str());
	}
	else if (NOT(RNA_struct_find_property(&node.ptr, pluginID.c_str()))) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Property group \"%s\" not found!",
		            ntree.name().c_str(), node.name().c_str(), pluginID.c_str());
	}
	else {
		setAttrsFromNode(ntree, node, fromSocket, context, pluginDesc, pluginID, pluginType);
	}
}
