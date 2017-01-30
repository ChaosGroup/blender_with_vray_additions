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

#include "vfb_params_json.h"

#ifdef _MSC_VER
#include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <map>

#define SKIP_TYPE(attrType) (\
	attrType == "LIST"     || \
	attrType == "MAPCHANNEL_LIST" || \
	attrType == "COLOR_LIST" || \
	attrType == "VECTOR_LIST" || \
	attrType == "TRANSFORM_LIST" || \
	attrType == "INT_LIST" || \
	attrType == "FLOAT_LIST")

#define OUTPUT_TYPE(attrType) (\
	attrType == "OUTPUT_PLUGIN"         || \
	attrType == "OUTPUT_COLOR"          || \
	attrType == "OUTPUT_TEXTURE"        || \
	attrType == "OUTPUT_INT_TEXTURE"    || \
	attrType == "OUTPUT_FLOAT_TEXTURE"  || \
	attrType == "OUTPUT_VECTOR_TEXTURE" || \
	attrType == "OUTPUT_MATRIX_TEXTURE" || \
	attrType == "OUTPUT_TRANSFORM_TEXTURE")

#define MAPPABLE_TYPE(attrType) (\
	attrType == "BRDF"              || \
	attrType == "MATERIAL"          || \
	attrType == "GEOMETRY"          || \
	attrType == "PLUGIN"            || \
	attrType == "VECTOR"            || \
	attrType == "UVWGEN"            || \
	attrType == "MATRIX"            || \
	attrType == "TRANSFORM"         || \
	attrType == "TEXTURE"           || \
	attrType == "FLOAT_TEXTURE"     || \
	attrType == "INT_TEXTURE"       || \
	attrType == "VECTOR_TEXTURE"    || \
	attrType == "MATRIX_TEXTURE"    || \
	attrType == "TRANSFORM_TEXTURE")

#define NOT_ANIMATABLE_TYPE(attrType) (\
	attrType == "BRDF"     || \
	attrType == "MATERIAL" || \
	attrType == "GEOMETRY" || \
	attrType == "UVWGEN"   || \
	attrType == "PLUGIN")


using namespace VRayForBlender;
using namespace VRayForBlender::ParamDesc;


typedef std::map<std::string, PluginDesc> MapPluginDesc;


static MapPluginDesc PluginDescriptions;


void VRayForBlender::InitPluginDescriptions(const std::string &dirPath)
{
	boost::filesystem::recursive_directory_iterator pIt(dirPath);
	boost::filesystem::recursive_directory_iterator end;

	for (; pIt != end; ++pIt) {
		const boost::filesystem::path &path = *pIt;
		if (path.extension() == ".json") {
			// NOTE: Filename is plugin ID
#ifdef _WIN32
			const std::string &fileName = path.stem().string();
#else
			const std::string &fileName = path.stem().c_str();
#endif
			std::ifstream fileStream(path.c_str());

			boost::property_tree::ptree pTree;
			boost::property_tree::json_parser::read_json(fileStream, pTree);

			PluginDesc &pluginDesc = PluginDescriptions[fileName];
			pluginDesc.pluginID   = fileName;
			pluginDesc.pluginType = ParamDesc::GetPluginTypeFromString(pTree.get_child("Type").data());

			for (auto &v : pTree.get_child("Parameters")) {
				const std::string &attrName = v.second.get_child("attr").data();
				const std::string &attrType = v.second.get_child("type").data();

				// NOTE: "skip" means fake attribute and / or that attribute must be handled
				// manually
				if (v.second.count("skip")) {
					if (v.second.get<bool>("skip")) {
						continue;
					}
				}

				AttrDesc &attrDesc = pluginDesc.attributes[attrName];
				attrDesc.name = attrName;
				attrDesc.options = AttrOption_None;
				if (v.second.count("options")) {
					if (v.second.get_child("options").data().find("EXPORT_AS_ACOLOR")) {
						attrDesc.options |= AttrOption_ExportAsColor;
					}
				}
				attrDesc.type = AttrTypeInvalid;

				if (attrType == "BOOL") {
					attrDesc.type = AttrTypeBool;
				}
				else if (attrType == "INT") {
					attrDesc.type = AttrTypeInt;
				}
				else if (attrType == "FLOAT") {
					attrDesc.type = AttrTypeFloat;
				}
				else if (attrType == "ENUM") {
					attrDesc.type = AttrTypeEnum;
				}
				else if (attrType == "COLOR") {
					attrDesc.type = AttrTypeColor;
				}
				else if (attrType == "ACOLOR") {
					attrDesc.type = AttrTypeAColor;
				}
				else if (attrType == "MATRIX") {
					attrDesc.type = AttrTypeMatrix;
				}
				else if (attrType == "TRANSFORM") {
					attrDesc.type = AttrTypeTransform;
				}
				else if (attrType == "TEXTURE") {
					attrDesc.type = AttrTypePluginTexture;
				}
				else if (attrType == "FLOAT_TEXTURE") {
					attrDesc.type = AttrTypePluginTextureFloat;
				}
				else if (attrType == "INT_TEXTURE") {
					attrDesc.type = AttrTypePluginTextureInt;
				}
				else if (attrType == "STRING") {
					attrDesc.type = AttrTypeString;
				}
				else if (attrType == "PLUGIN") {
					attrDesc.type = AttrTypePlugin;
				}
				else if (attrType == "BRDF") {
					attrDesc.type = AttrTypePluginBRDF;
				}
				else if (attrType == "UVWGEN") {
					attrDesc.type = AttrTypePluginUvwgen;
				}
				else if (attrType == "MATERIAL") {
					attrDesc.type = AttrTypePluginMaterial;
				}
				else if (attrType == "OUTPUT_PLUGIN") {
					attrDesc.type = AttrTypeOutputPlugin;
				}
				else if (attrType == "OUTPUT_COLOR") {
					attrDesc.type = AttrTypeOutputColor;
				}
				else if (attrType == "OUTPUT_TEXTURE") {
					attrDesc.type = AttrTypeOutputTexture;
				}
				else if (attrType == "OUTPUT_FLOAT_TEXTURE") {
					attrDesc.type = AttrTypeOutputTextureFloat;
				}
				else if (attrType == "OUTPUT_INT_TEXTURE") {
					attrDesc.type = AttrTypeOutputTextureInt;
				}
				else if (attrType == "OUTPUT_VECTOR_TEXTURE") {
					attrDesc.type = AttrTypeOutputTextureVector;
				}
				else if (attrType == "OUTPUT_MATRIX_TEXTURE") {
					attrDesc.type = AttrTypeOutputTextureMatrix;
				}
				else if (attrType == "OUTPUT_TRANSFORM_TEXTURE") {
					attrDesc.type = AttrTypeOutputTextureTransform;
				}
				else if (attrType == "LIST") {
					attrDesc.type = AttrTypeList;
				}
				else if (attrType == "WIDGET_RAMP") {
					attrDesc.type = AttrTypeWidgetRamp;

					const auto &rampDesc = v.second.get_child("attrs");
					if (rampDesc.count("colors")) {
						attrDesc.descRamp.colors = rampDesc.get_child("colors").data();
					}
					if (rampDesc.count("positions")) {
						attrDesc.descRamp.positions = rampDesc.get_child("positions").data();
					}
					if (rampDesc.count("interpolations")) {
						attrDesc.descRamp.interpolations = rampDesc.get_child("interpolations").data();
					}
				}
				else if (attrType == "WIDGET_CURVE") {
					attrDesc.type = AttrTypeWidgetCurve;

					const auto &curveDesc = v.second.get_child("attrs");
					if (curveDesc.count("values")) {
						attrDesc.descCurve.values = curveDesc.get_child("values").data();
					}
					if (curveDesc.count("positions")) {
						attrDesc.descCurve.positions = curveDesc.get_child("positions").data();
					}
					if (curveDesc.count("interpolations")) {
						attrDesc.descCurve.interpolations = curveDesc.get_child("interpolations").data();
					}
				}
			}
		}
	}
}


const VRayForBlender::ParamDesc::PluginDesc& VRayForBlender::GetPluginDescription(const std::string &pluginID)
{
	return PluginDescriptions[pluginID];
}
