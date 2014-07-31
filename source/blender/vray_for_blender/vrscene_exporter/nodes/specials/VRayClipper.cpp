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


void VRayNodeExporter::exportVRayClipper(BL::BlendData bl_data, BL::Object bl_ob)
{
	PointerRNA vrayObject  = RNA_pointer_get(&bl_ob.ptr, "vray");
	PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");

	const std::string &pluginName = "VRayClipper@" + GetIDName(bl_ob);

	char transform[CGR_TRANSFORM_HEX_SIZE];
	GetTransformHex(((Object*)bl_ob.ptr.data)->obmat, transform);

	const std::string &material = VRayNodeExporter::exportMtlMulti(bl_data, bl_ob);

	AttributeValueMap attrs;
	attrs["enabled"] = "1";
	attrs["affect_light"]     = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "affect_light"));
	attrs["only_camera_rays"] = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "only_camera_rays"));
	attrs["clip_lights"]      = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "clip_lights"));
	attrs["use_obj_mtl"]      = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "use_obj_mtl"));
	attrs["set_material_id"]  = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "set_material_id"));
	attrs["material_id"]      = BOOST_FORMAT_INT(RNA_int_get(&vrayClipper, "material_id"));
	attrs["object_id"]        = BOOST_FORMAT_INT(bl_ob.pass_index());
	attrs["transform"]        = BOOST_FORMAT_TM(transform);

	char buf[MAX_ID_NAME];
	RNA_string_get(&vrayClipper, "exclusion_nodes", buf);
	const std::string excludeGroupName = buf;
	if (NOT(excludeGroupName.empty())) {
		StrSet exclusion_nodes;
		BL::BlendData::groups_iterator grIt;
		for (bl_data.groups.begin(grIt); grIt != bl_data.groups.end(); ++grIt) {
			BL::Group gr = *grIt;
			if (gr.name() == excludeGroupName) {
				BL::Group::objects_iterator grObIt;
				for (gr.objects.begin(grObIt); grObIt != gr.objects.end(); ++grObIt) {
					BL::Object ob = *grObIt;
					exclusion_nodes.insert(GetIDName(ob));
				}
				break;
			}
		}

		attrs["exclusion_mode"] = BOOST_FORMAT_INT(RNA_enum_get(&vrayClipper, "exclusion_mode"));
		attrs["exclusion_nodes"] = BOOST_FORMAT_LIST_JOIN(exclusion_nodes);
	}

	if (NOT(material.empty()) && material != "NULL")
		attrs["material"] = material;

	VRayNodePluginExporter::exportPlugin("NODE", "VRayClipper", pluginName, attrs);
}
