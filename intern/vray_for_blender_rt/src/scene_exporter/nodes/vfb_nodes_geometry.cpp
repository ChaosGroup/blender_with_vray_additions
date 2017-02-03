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
#include "vfb_utils_mesh.h"


AttrValue DataExporter::exportVRayNodeGeomDisplacedMesh(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	AttrValue attrValue;

	if (!context.object_context.object) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		BL::NodeSocket meshSock = Nodes::GetInputSocketByName(node, "Mesh");
		if (!(meshSock && meshSock.is_linked())) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Mesh socket is not linked!",
			            ntree.name().c_str(), node.name().c_str());
		}
		else {
			AttrValue mesh;
			if (m_settings.export_meshes) {
				mesh = exportLinkedSocket(ntree, meshSock, context);
			} else {
				mesh = getConnectedNodePluginName(ntree, meshSock, context);
			}

			if (!mesh) {
				PRINT_ERROR("Node tree: %s => Node name: %s => Error exporting connected mesh!",
				            ntree.name().c_str(), node.name().c_str());
			}
			else {
				if (!m_settings.use_displace_subdiv) {
					attrValue = mesh;
				}
				else {
					PointerRNA geomDisplacedMesh = RNA_pointer_get(&node.ptr, "GeomDisplacedMesh");
					const int &displace_type = RNA_enum_get(&geomDisplacedMesh, "type");

					PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
					                      "GeomDisplacedMesh");
					pluginDesc.add("mesh", mesh);
					pluginDesc.add("displace_2d",         (displace_type == 0));
					pluginDesc.add("vector_displacement", (displace_type == 2));

					if (displace_type == 2) {
						BL::NodeSocket texSock = Nodes::GetSocketByAttr(node, "displacement_tex_color");
						if (!(texSock && texSock.is_linked())) {
							PRINT_ERROR("Node tree: %s => Node name: %s => 3D displacement is selected, but no color texture presents!",
							            ntree.name().c_str(), node.name().c_str());

						}
						else {
							pluginDesc.add("displacement_tex_color", exportLinkedSocket(ntree, texSock, context));
						}
					}
					else {
						BL::NodeSocket texSock = Nodes::GetSocketByAttr(node, "displacement_tex_float");
						if (!(texSock && texSock.is_linked())) {
							PRINT_ERROR("Node tree: %s => Node name: %s => Normal/2D displacement is selected, but no float texture presents!",
							            ntree.name().c_str(), node.name().c_str());
						}
						else {
							pluginDesc.add("displacement_tex_float", exportLinkedSocket(ntree, texSock, context));
						}
					}

					setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

					attrValue = m_exporter->export_plugin(pluginDesc);
				}
			}
		}
	}

	return attrValue;
}


AttrValue DataExporter::exportVRayNodeGeomStaticSmoothedMesh(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	AttrValue attrValue;

	if (!context.object_context.object) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		BL::NodeSocket meshSock = Nodes::GetInputSocketByName(node, "Mesh");
		if (!(meshSock && meshSock.is_linked())) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Mesh socket is not linked!",
			            ntree.name().c_str(), node.name().c_str());
		}
		else {
			if (m_settings.use_displace_subdiv) {
				context.object_context.merge_uv = true;
			}
			AttrValue mesh;
			if (m_settings.export_meshes) {
				mesh = exportLinkedSocket(ntree, meshSock, context);
			} else {
				mesh = getConnectedNodePluginName(ntree, meshSock, context);
			}

			if (!mesh) {
				PRINT_ERROR("Node tree: %s => Node name: %s => Error exporting connected mesh!",
				            ntree.name().c_str(), node.name().c_str());
			}
			else {
				if (!m_settings.use_displace_subdiv) {
					attrValue = mesh;
				}
				else {
					PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
					                      "GeomStaticSmoothedMesh");
					pluginDesc.add("mesh", mesh);

					setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

					attrValue = m_exporter->export_plugin(pluginDesc);
				}
			}
		}
	}

	return attrValue;
}


AttrValue DataExporter::exportVRayNodeBlenderOutputGeometry(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	AttrValue  attrValue;
	BL::Object ob(context.object_context.object);

	if (!(ob && ob.data())) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		PluginDesc geomDesc(getMeshName(ob), "GeomStaticMesh");
		attrValue = AttrPlugin(geomDesc.pluginName);

		if (m_settings.export_meshes) {
			VRayForBlender::Mesh::ExportOptions options;
			options.merge_channel_vertices = context.object_context.merge_uv;
			options.mode = m_evalMode;

			int err = VRayForBlender::Mesh::FillMeshData(m_data, m_scene, ob, options, geomDesc);
			if (!err) {
				// Set additional attributes from the node
				setAttrsFromNodeAuto(ntree, node, fromSocket, context, geomDesc);

				attrValue = m_exporter->export_plugin(geomDesc);
			}
		}
	}

	return attrValue;
}
