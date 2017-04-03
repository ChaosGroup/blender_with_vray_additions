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
			if (param->type == TypeDesc::STRING) {
				// only string is mappable currently
				auto sockVal = exportSocket(ntree, sock, context);
				BLI_assert(sockVal.type == ValueTypePlugin);
				// empty plugins should be empty string and not blank
				if (sockVal.as<AttrPlugin>().plugin == "") {
					sockVal = AttrSimpleType<std::string>("");
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
	// this node is pure blender's node so it has no "vray" property

	BL::NodeSocket toSocket(Nodes::GetConnectedSocket(fromSocket));
	if (!toSocket) {
		// this should not happen since we get here if toSocket is valid
		PRINT_ERROR("Exporting disconnected node!");
		return AttrValue();
	}

	const std::string outputClosure = toSocket.name();
	std::string pluginId;
	if (node.bl_idname() == "VRayNodeMtlOSL") {
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
	plgDesc.add("output_closure", outputClosure);
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, plgDesc);

	return m_exporter->export_plugin(plgDesc);
}

