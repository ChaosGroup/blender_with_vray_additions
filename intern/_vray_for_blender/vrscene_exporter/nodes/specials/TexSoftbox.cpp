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


std::string VRayNodeExporter::exportVRayNodeTexSoftbox(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	PointerRNA texSoftbox = RNA_pointer_get(&node.ptr, "TexSoftbox");
	AttributeValueMap manualAttrs;

	if (RNA_boolean_get(&texSoftbox, "grad_vert_on")) {
		VRayNodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
											  manualAttrs, "ramp_grad_vert", "grad_vert_col", "grad_vert_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "grad_horiz_on")) {
		VRayNodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
											  manualAttrs, "ramp_grad_horiz", "grad_horiz_col", "grad_horiz_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "grad_rad_on")) {
		VRayNodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
											  manualAttrs, "ramp_grad_rad", "grad_rad_col", "grad_rad_pos");
	}
	if (RNA_boolean_get(&texSoftbox, "frame_on")) {
		VRayNodeExporter::exportRampAttribute(ntree, node, fromSocket, context,
											  manualAttrs, "ramp_frame", "frame_col", "frame_pos");
	}

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, manualAttrs);
}
