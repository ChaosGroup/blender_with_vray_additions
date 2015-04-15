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
#include "vfb_utils_blender.h"


using namespace VRayForBlender;


BL::Object DataExporter::exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, NodeContext *context)
{
	return Blender::GetObjectByName(m_data, RNA_std_string_get(&node.ptr, "objectName"));
}


BL::Group DataExporter::exportVRayNodeSelectGroup(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, NodeContext *context)
{
	BL::Group group(PointerRNA_NULL);

	const std::string &groupName = RNA_std_string_get(&node.ptr, "groupName");
	if (!groupName.empty()) {
		BL::BlendData::groups_iterator grIt;
		for(m_data.groups.begin(grIt); grIt != m_data.groups.end(); ++grIt) {
			BL::Group b_gr(*grIt);
			if (b_gr.name() == groupName) {
				group = b_gr;
				break;
			}
		}
	}

	return group;
}


AttrValue DataExporter::getObjectNameList(BL::Group group)
{
	AttrListPlugin pluginList;

	if (!group) {
		PRINT_ERROR("");
	}
	else {
		BL::Group::objects_iterator obIt;
		for (group.objects.begin(obIt); obIt != group.objects.end(); ++obIt) {
			BL::Object b_ob(*obIt);

			// pluginList.push_back(Blender::GetIDName(b_ob));
		}
	}

	return pluginList;
}


void DataExporter::getNodeSelectObjects(BL::Node node, ObList &obList)
{
	if(node.bl_idname() == "VRayNodeSelectObject") {
		BL::Object ob = DataExporter::exportVRayNodeSelectObject(PointerRNA_NULL, node, PointerRNA_NULL, NULL);
		if(ob) {
			obList.push_back(ob);
		}
	}
	else if(node.bl_idname() == "VRayNodeSelectGroup") {
		BL::Group group = DataExporter::exportVRayNodeSelectGroup(PointerRNA_NULL, node, PointerRNA_NULL, NULL);
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
