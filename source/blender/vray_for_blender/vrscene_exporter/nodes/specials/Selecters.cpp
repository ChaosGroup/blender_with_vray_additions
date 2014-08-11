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


BL::Object VRayNodeExporter::exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	char buf[MAX_ID_NAME] = "";
	RNA_string_get(&node.ptr, "objectName", buf);

	std::string objectName = buf;

	if(NOT(objectName.empty())) {
		BL::BlendData b_data = ExpoterSettings::gSet.b_data;

		BL::BlendData::objects_iterator obIt;
		for(b_data.objects.begin(obIt); obIt != b_data.objects.end(); ++obIt) {
			BL::Object b_ob = *obIt;
			if(b_ob.name() == buf)
				return b_ob;
		}
	}

	return BL::Object(PointerRNA_NULL);
}


BL::Group VRayNodeExporter::exportVRayNodeSelectGroup(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	char buf[MAX_ID_NAME] = "";
	RNA_string_get(&node.ptr, "groupName", buf);

	std::string groupName = buf;

	if(NOT(groupName.empty())) {
		BL::BlendData b_data = ExpoterSettings::gSet.b_data;
		
		BL::BlendData::groups_iterator grIt;
		for(b_data.groups.begin(grIt); grIt != b_data.groups.end(); ++grIt) {
			BL::Group b_gr = *grIt;
			if(b_gr.name() == buf)
				return b_gr;
		}
	}

	return BL::Group(PointerRNA_NULL);
}


std::string VRayNodeExporter::getObjectNameList(BL::Group group)
{
	if(NOT(group))
		return "List()";

	StrVector obNames;

	BL::Group::objects_iterator obIt;
	for(group.objects.begin(obIt); obIt != group.objects.end(); ++obIt) {
		BL::Object b_ob = *obIt;

		obNames.push_back(GetIDName(b_ob));
	}

	return BOOST_FORMAT_LIST(obNames);
}


void VRayNodeExporter::getNodeSelectObjects(BL::Node node, ObList &obList)
{
	if(node.bl_idname() == "VRayNodeSelectObject") {
		BL::Object ob = VRayNodeExporter::exportVRayNodeSelectObject(PointerRNA_NULL, node, PointerRNA_NULL, NULL);
		if(ob) {
			obList.push_back(ob);
		}
	}
	else if(node.bl_idname() == "VRayNodeSelectGroup") {
		BL::Group group = VRayNodeExporter::exportVRayNodeSelectGroup(PointerRNA_NULL, node, PointerRNA_NULL, NULL);
		if(group) {
			BL::Group::objects_iterator obIt;
			for(group.objects.begin(obIt); obIt != group.objects.end(); ++obIt) {
				BL::Object ob = *obIt;
				if(ob) {
					obList.push_back(ob);
				}
			}
		}
	}
}
