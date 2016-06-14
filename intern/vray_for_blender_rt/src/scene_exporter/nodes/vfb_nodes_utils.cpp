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


BL::Object DataExporter::exportVRayNodeSelectObject(BL::NodeTree&, BL::Node &node, BL::NodeSocket&, NodeContext&)
{
	return Blender::GetObjectByName(m_data, RNA_std_string_get(&node.ptr, "objectName"));
}


BL::Group DataExporter::exportVRayNodeSelectGroup(BL::NodeTree&, BL::Node &node, BL::NodeSocket&, NodeContext&)
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
		PRINT_ERROR("Not a valid Group node!");
	}
	else {
		BL::Group::objects_iterator obIt;
		for (group.objects.begin(obIt); obIt != group.objects.end(); ++obIt) {
			BL::Object b_ob(*obIt);

			pluginList.append(Blender::GetIDName(b_ob));
		}
	}

	return pluginList;
}

void DataExporter::getSelectorObjectNames(BL::Node node, AttrListPlugin & plugins)
{
	static BL::NodeTree   ntree(PointerRNA_NULL);
	static BL::NodeSocket fromSocket(PointerRNA_NULL);

	if (node.bl_idname() == "VRayNodeSelectObject") {
		NodeContext ctx;
		BL::Object ob = exportVRayNodeSelectObject(ntree, node, fromSocket, ctx);
		if (ob) {
			plugins.append(Blender::GetIDName(ob));
		}
	}
	else if (node.bl_idname() == "VRayNodeSelectGroup") {
		NodeContext ctx;
		BL::Group group = exportVRayNodeSelectGroup(ntree, node, fromSocket, ctx);
		if (group) {
			BL::Group::objects_iterator obIt;
			for (group.objects.begin(obIt); obIt != group.objects.end(); ++obIt) {
				BL::Object ob(*obIt);
				if (ob) {
					if (ob.is_duplicator()) {
						ob.dupli_list_create(m_scene, EvalMode::EvalModeRender);

						BL::Object::dupli_list_iterator dupIt;
						for (ob.dupli_list.begin(dupIt); dupIt != ob.dupli_list.end(); ++dupIt) {

							BL::DupliObject dupliOb(*dupIt);
							BL::Object      dupOb(dupliOb.object());

							const bool is_hidden = m_exporter->get_is_viewport() ? dupliOb.hide() : dupOb.hide_render();
							if (Blender::IsLight(dupOb)) {
								MHash persistendID;
								MurmurHash3_x86_32((const void*)dupIt->persistent_id().data, 8 * sizeof(int), 42, &persistendID);

								char namePrefix[255] = {0, };
								namePrefix[0] = 'D';
								snprintf(namePrefix + 1, 250, "%u", persistendID);
								strcat(namePrefix, "@");
								strcat(namePrefix, ob.name().c_str());

								plugins.append(std::string(namePrefix));
							}
						}

						ob.dupli_list_clear();
					} else {
						plugins.append(Blender::GetIDName(ob));
					}
				}
			}
		}
	}
}

void DataExporter::getSelectorObjectList(BL::Node node, ObList &obList)
{
	static BL::NodeTree   ntree(PointerRNA_NULL);
	static BL::NodeSocket fromSocket(PointerRNA_NULL);

	if (node.bl_idname() == "VRayNodeSelectObject") {
		NodeContext ctx;
		BL::Object ob = exportVRayNodeSelectObject(ntree, node, fromSocket, ctx);
		if (ob) {
			obList.push_back(ob);
		}
	}
	else if (node.bl_idname() == "VRayNodeSelectGroup") {
		NodeContext ctx;
		BL::Group group = exportVRayNodeSelectGroup(ntree, node, fromSocket, ctx);
		if (group) {
			BL::Group::objects_iterator obIt;
			for (group.objects.begin(obIt); obIt != group.objects.end(); ++obIt) {
				BL::Object ob(*obIt);
				if (ob) {
					obList.push_back(ob);
				}
			}
		}
	}
}
