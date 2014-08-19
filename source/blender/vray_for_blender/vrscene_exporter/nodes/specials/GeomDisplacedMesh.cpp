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


std::string VRayNodeExporter::exportVRayNodeGeomDisplacedMesh(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	if(NOT(context->obCtx.ob)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	BL::NodeSocket meshSock = getSocketByName(node, "Mesh");
	if(NOT(meshSock.is_linked())) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Mesh socket is not linked!",
					ntree.name().c_str(), node.name().c_str());

		return "NULL";
	}

	const std::string meshName = VRayNodeExporter::exportLinkedSocket(ntree, meshSock, context);

	if(NOT(ExporterSettings::gSet.m_useDisplaceSubdiv))
		return meshName;

	AttributeValueMap manualAttrs;
	manualAttrs["mesh"] = meshName;

	PointerRNA geomDisplacedMesh = RNA_pointer_get(&node.ptr, "GeomDisplacedMesh");

	int displace_type = RNA_enum_get(&geomDisplacedMesh, "type");

	if(displace_type == 1) {
		manualAttrs["displace_2d"]         = "0";
		manualAttrs["vector_displacement"] = "0";
	}
	else if(displace_type == 0) {
		manualAttrs["displace_2d"]         = "1";
		manualAttrs["vector_displacement"] = "0";
	}
	else if(displace_type == 2) {
		manualAttrs["displace_2d"]         = "0";
		manualAttrs["vector_displacement"] = "1";
	}

	if(displace_type == 2) {
		BL::NodeSocket displacement_tex_color = VRayNodeExporter::getSocketByAttr(node, "displacement_tex_color");
		if(displacement_tex_color.is_linked()) {
			manualAttrs["displacement_tex_color"] = VRayNodeExporter::exportLinkedSocket(ntree, displacement_tex_color, context);

		}
		else {
			PRINT_ERROR("Node tree: %s => Node name: %s => 3D displacement is selected, but no color texture presents!",
						ntree.name().c_str(), node.name().c_str());
		}
	}
	else {
		BL::NodeSocket displacement_tex_float = VRayNodeExporter::getSocketByAttr(node, "displacement_tex_float");
		if(displacement_tex_float.is_linked()) {
			manualAttrs["displacement_tex_float"] = VRayNodeExporter::exportLinkedSocket(ntree, displacement_tex_float, context);
		}
		else {
			PRINT_ERROR("Node tree: %s => Node name: %s => Normal/2D displacement is selected, but no float texture presents!",
						ntree.name().c_str(), node.name().c_str());
		}
	}

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, manualAttrs);
}
