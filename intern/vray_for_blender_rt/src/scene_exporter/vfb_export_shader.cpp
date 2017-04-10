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

#include "BLI_path_util.h"
#include "BLI_string.h"

#include "utils/cgr_string.h"

#include <fstream>


AttrValue convertToOSLArgument(const AttrValue & val) {
	if (val.type == ValueTypeColor) {
		AttrListValue vlist;
		vlist.append(AttrValue(val.as<AttrColor>().r));
		vlist.append(AttrValue(val.as<AttrColor>().g));
		vlist.append(AttrValue(val.as<AttrColor>().b));
		return vlist;
	} else if (val.type == ValueTypeVector) {
		AttrListValue vlist;
		vlist.append(AttrValue(val.as<AttrVector>().x));
		vlist.append(AttrValue(val.as<AttrVector>().y));
		vlist.append(AttrValue(val.as<AttrVector>().z));
		return vlist;
	} else {
		return val;
	}
}

AttrListValue DataExporter::buildScriptArgumentList(BL::NodeTree &ntree, BL::Node &node, NodeContext &context, OSL::OSLQuery & query) {
	OIIO_NAMESPACE_USING
	AttrListValue list;

	for (int c = 0; c < query.nparams(); c++) {
		const OSL::OSLQuery::Parameter *param = query.getparam(c);
		// usuported types
		if (param->varlenarray || param->isstruct || param->type.arraylen > 1 || param->isoutput) {
			continue;
		}
		const std::string sockName = param->name.string();
		BL::NodeSocket sock = node.inputs[sockName];
		// we don't create sockets for unsupported params
		if (sock) {
			
			list.append(AttrSimpleType<std::string>(sockName));
			// NOTE: OSL string param could be either plugin or string, if the socket is linked
			// the value type will be ValueTypePlugin and contain the connected plugin instance name
			// or it can be string in which case the default value is used
			if (param->type == TypeDesc::STRING) {
				// only string is mappable currently
				auto sockVal = exportSocket(ntree, sock, context);
				if (sockVal.type == ValueTypePlugin) {
					if (sockVal.as<AttrPlugin>().plugin == "") {
						// empty plugins should be empty string and not blank
						sockVal = AttrSimpleType<std::string>("");
					}
					sockVal.as<AttrPlugin>().output = ""; // no output name needed for osl
				}
				list.append(sockVal);
			} else {
				list.append(convertToOSLArgument(exportDefaultSocket(ntree, sock)));
			}
		}
	}

	return list;
}

AttrValue DataExporter::exportVRayNodeShaderScript(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context) {
	BL::NodeSocket toSocket(Nodes::GetConnectedSocket(fromSocket));
	if (!toSocket) {
		// this should not happen since we get here if toSocket is valid
		PRINT_ERROR("Exporting disconnected node!");
		return AttrValue();
	}

	const std::string outputClosure = toSocket.name();
	std::string pluginId;
	bool isMtl = node.bl_idname() == "VRayNodeMtlOSL";
	if (isMtl) {
		pluginId = "MtlOSL";
	} else {
		pluginId = "TexOSL";
	}

	const std::string pluginName = pluginId + "|" + GenPluginName(node, ntree, context);
	PluginDesc plgDesc(pluginName, pluginId);

	OSL::OSLQuery query;
	// if this is inline script - save it to file
	// TODO: what about DR and zmq?
	const auto & scriptData = RNA_std_string_get(&node.ptr, "bytecode");
	if (!query.open_bytecode(scriptData)) {
		PRINT_ERROR("Failed to load script for node \"%s\"", node.name().c_str());
		plgDesc.add("input_parameters", AttrListValue());
	} else {
		plgDesc.add("input_parameters", buildScriptArgumentList(ntree, node, context, query));
	}

	const auto &scriptPath = RNA_std_string_get(&node.ptr, "export_filepath");
	plgDesc.add("shader_file", scriptPath);
	if (isMtl) {
		plgDesc.add("output_closure", outputClosure);
	} else {
		plgDesc.add("output_color", outputClosure);
	}
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, plgDesc);

	return m_exporter->export_plugin(plgDesc);
}

