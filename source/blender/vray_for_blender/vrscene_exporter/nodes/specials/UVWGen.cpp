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


static const char *sMappingType[] = {
	"angular",
	"cubic",
	"spherical",
	"mirror_ball",
	"screen",
	"max_spherical",
	"spherical_vray",
	"max_cylindrical",
	"max_shrink_wrap",
	NULL
};


std::string VRayNodeExporter::exportVRayNodeUVWGenEnvironment(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	PointerRNA UVWGenEnvironment = RNA_pointer_get(&node.ptr, "UVWGenEnvironment");

	const int mapping_type = RNA_enum_get(&UVWGenEnvironment, "mapping_type");

	AttributeValueMap  manualAttrs;
	manualAttrs["mapping_type"] = BOOST_FORMAT_STRING(sMappingType[mapping_type]);

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, manualAttrs);
}
