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


BL::Object VRayNodeExporter::getObjectByName(const std::string &name)
{
	if(NOT(name.empty())) {
		BL::BlendData data = ExporterSettings::gSet.b_data;

		BL::BlendData::objects_iterator obIt;
		for(data.objects.begin(obIt); obIt != data.objects.end(); ++obIt) {
			BL::Object ob = *obIt;
			if(ob.name() == name)
				return ob;
		}
	}

	return BL::Object(PointerRNA_NULL);
}


BL::Object VRayNodeExporter::exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	return VRayNodeExporter::getObjectByName(RNA_std_string_get(&node.ptr, "objectName"));
}


BL::Group VRayNodeExporter::exportVRayNodeSelectGroup(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	char buf[MAX_ID_NAME] = "";
	RNA_string_get(&node.ptr, "groupName", buf);

	std::string groupName = buf;

	if(NOT(groupName.empty())) {
		BL::BlendData b_data = ExporterSettings::gSet.b_data;
		
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
	VRayNodeContext ctx;
	if(node.bl_idname() == "VRayNodeSelectObject") {
		BL::Object ob = VRayNodeExporter::exportVRayNodeSelectObject(PointerRNA_NULL, node, PointerRNA_NULL, ctx);
		if(ob) {
			obList.push_back(ob);
		}
	}
	else if(node.bl_idname() == "VRayNodeSelectGroup") {
		BL::Group group = VRayNodeExporter::exportVRayNodeSelectGroup(PointerRNA_NULL, node, PointerRNA_NULL, ctx);
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

static void getDuplicatorNames(BL::Object & ob, StrSet & obNames)
{
	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
	const int overrideObjectID = RNA_int_get(&vrayObject, "dupliGroupIDOverride");
	const int useInstancer     = RNA_boolean_get(&vrayObject, "use_instancer");


	ob.dupli_list_create(ExporterSettings::gSet.b_scene, 2);
	BL::Object::dupli_list_iterator b_dup;
	for (ob.dupli_list.begin(b_dup); b_dup != ob.dupli_list.end(); ++b_dup) {
		BL::DupliObject dup(*b_dup);
		BL::Object      dop(dup.object());

		MHash persistendID;
		MurmurHash3_x86_32((const void*)dup.persistent_id().data, 8 * sizeof(int), 42, &persistendID);

		const std::string &dupliNamePrefix = StripString("D" + BOOST_FORMAT_UINT(persistendID) + "@" + ob.name());
		obNames.insert(dupliNamePrefix + GetIDName(ob));
	}
	ob.dupli_list_clear();
}

void VRayNodeExporter::getNodeSelectLightsNames(BL::Node node, StrSet &obNames)
{
	VRayNodeContext ctx;
	if(node.bl_idname() == "VRayNodeSelectObject") {
		BL::Object ob = VRayNodeExporter::exportVRayNodeSelectObject(PointerRNA_NULL, node, PointerRNA_NULL, ctx);
		if(ob) {
			if (ob.is_duplicator()) {
				getDuplicatorNames(ob, obNames);
			}
			obNames.insert(GetIDName(ob));
		}
	}
	else if(node.bl_idname() == "VRayNodeSelectGroup") {
		BL::Group group = VRayNodeExporter::exportVRayNodeSelectGroup(PointerRNA_NULL, node, PointerRNA_NULL, ctx);
		if(group) {
			BL::Group::objects_iterator obIt;
			for(group.objects.begin(obIt); obIt != group.objects.end(); ++obIt) {
				BL::Object ob = *obIt;
				if (ob) {
					if (ob.is_duplicator()) {
						getDuplicatorNames(ob, obNames);
					} else {
						obNames.insert(GetIDName(ob));
					}
				}
			}
		}
	}
}