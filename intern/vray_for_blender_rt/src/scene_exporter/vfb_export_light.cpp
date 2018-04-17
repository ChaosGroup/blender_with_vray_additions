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

#include "vfb_params_json.h"

#include "vfb_node_exporter.h"
#include "vfb_utils_nodes.h"
#include "vfb_utils_blender.h"

using namespace VRayForBlender;

AttrValue DataExporter::exportLight(BL::Object ob, bool check_updated, const ObjectOverridesAttrs & override)
{
	AttrValue plugin;

	bool is_updated      = check_updated ? ob.is_updated()      : true;
	bool is_data_updated = check_updated ? ob.is_updated_data() : true;

	if (!is_updated && ob.parent()) {
		BL::Object parent(ob.parent());
		is_updated = parent.is_updated();
	}
	if (!is_data_updated && ob.parent()) {
		BL::Object parent(ob.parent());
		is_data_updated = parent.is_updated_data();
	}

	BL::Lamp lamp(ob.data());

	if (!lamp) {
		return plugin;
	}

	BL::NodeTree ntree = Nodes::GetNodeTree(lamp);
	if (ntree) {
		is_data_updated |= ntree.is_updated();
		DataExporter::tag_ntree(ntree, false);
	}

	const std::string &lightPluginName = getLightName(ob);
	// nothing changed - just return name
	if (!(is_updated || is_data_updated || m_layer_changed)) {
		return plugin;
	}

	PointerRNA vrayLamp = RNA_pointer_get(&lamp.ptr, "vray");

	int includeExclude   = RNA_enum_get(&vrayLamp, "include_exclude");
	int includeIllumShad = RNA_enum_get(&vrayLamp, "illumination_shadow");

	// none = 0
	// exclude == 1
	// include == 2

	if (includeExclude != 0) {
		SettingsLightLinker::LightLink link;
		link.isExcludeList = includeExclude == 1;
		switch (includeIllumShad) {
			case 0:  link.excludeType = SettingsLightLinker::LightLink::Illumination; break;
			case 1:  link.excludeType = SettingsLightLinker::LightLink::Shadows; break;
			default: link.excludeType = SettingsLightLinker::LightLink::Both; break;
		}

		const auto obName = RNA_std_string_get(&vrayLamp, "exclude_objects");
		const auto grName = RNA_std_string_get(&vrayLamp, "exclude_groups");

		link.objectList = getObjectList(obName, grName);
		m_light_linker.addLightLink(lightPluginName, std::move(link));
	}

	// Find plugin ID
	//
	std::string pluginID;

	if (lamp.type() == BL::Lamp::type_AREA) {
		pluginID = "LightRectangle";
	}
	else if (lamp.type() == BL::Lamp::type_HEMI) {
		pluginID = "LightDome";
	}
	else if (lamp.type() == BL::Lamp::type_SPOT) {
		const int spotType = RNA_enum_get(&vrayLamp, "spot_type");
		switch(spotType) {
			case 0: pluginID = "LightSpotMax"; break;
			case 1: pluginID = "LightIESMax";  break;
		}
	}
	else if (lamp.type() == BL::Lamp::type_POINT) {
		const int omniType = RNA_enum_get(&vrayLamp, "omni_type");
		switch(omniType) {
			case 0: pluginID = "LightOmniMax";    break;
			case 1: pluginID = "LightAmbientMax"; break;
			case 2: pluginID = "LightSphere";     break;
		}
	}
	else if (lamp.type() == BL::Lamp::type_SUN) {
		const int directType = RNA_enum_get(&vrayLamp, "direct_type");
		switch(directType) {
			case 0: pluginID = "LightDirectMax"; break;
			case 1: pluginID = "SunLight";       break;
		}
	}
	else {
		PRINT_ERROR("Lamp: %s Type: %i => Lamp type is not supported!",
				    ob.name().c_str(), lamp.type());
		return plugin;
	}


	PointerRNA lampPropGroup = RNA_pointer_get(&vrayLamp, pluginID.c_str());

	const std::string & lampName = override.namePrefix + lightPluginName;

	PluginDesc pluginDesc(lampName, pluginID);

	BL::Node     lightNode(PointerRNA_NULL);
	if (ntree) {
		const static std::string lightNodeType("VRayNode");
		const std::string & vrayNodeType = lightNodeType + pluginID;

		lightNode = Nodes::GetNodeByType(ntree, vrayNodeType);
	}

	if (ntree && !lightNode) {
		PRINT_ERROR("Lamp \"%s\" node tree output node is not found!",
					ob.name().c_str());
	}

	const ParamDesc::PluginParamDesc &pluginParamDesc = GetPluginDescription(pluginID);
	for (const auto &descIt : pluginParamDesc.attributes) {
		const std::string         &attrName = descIt.second.name;
		const ParamDesc::AttrType &attrType = descIt.second.type;

		if (attrType > ParamDesc::AttrTypeOutputStart && attrType < ParamDesc::AttrTypeOutputEnd) {
			continue;
		}

		// For lights we are interested in mappable types only in linked sockets
		// ignore otherwize
		if (!ParamDesc::TypeHasSocket(attrType)) {
			setAttrFromPropGroup(&lampPropGroup, (ID*)lamp.ptr.data, descIt.second, pluginDesc);
		}
		else if (lightNode){
			BL::NodeSocket sock = Nodes::GetSocketByAttr(lightNode, attrName);
			if (sock && sock.is_linked()) {
				NodeContext context;
				context.object_context.object = ob;
				AttrValue socketValue = exportSocket(ntree, sock, context);
				if (socketValue) {
					pluginDesc.add(attrName, socketValue);
				}
			}
		}
	}
	if (override) {
		pluginDesc.add("transform", override.tm);
		pluginDesc.add("enabled", override.visible);
	} else {
		pluginDesc.add("transform", AttrTransformFromBlTransform(ob.matrix_world()));
	}


	if (pluginID == "LightRectangle") {
		BL::AreaLamp areaLamp(lamp);

		const float sizeX = areaLamp.size() / 2.0f;
		const float sizeY = areaLamp.shape() == BL::AreaLamp::shape_SQUARE
					        ? sizeX
					        : areaLamp.size_y() / 2.0f;

		pluginDesc.add("u_size", sizeX);
		pluginDesc.add("v_size", sizeY);

		// Q: Ignoring UI option "use_rect_tex" at all?
		PluginAttr *rect_tex = pluginDesc.get("rect_tex");
		bool use_rect_tex = (rect_tex && rect_tex->attrValue.type == ValueTypePlugin);
		pluginDesc.add("use_rect_tex", use_rect_tex);
		// if lamp is hidden by override also make it invisible to
		if (override) {
			pluginDesc.add("invisible", !override.visible);
		}
	}
	else if (pluginID == "LightDome") {
		// Q: Ignoring UI option "use_dome_tex" at all?
		PluginAttr *dome_tex = pluginDesc.get("dome_tex");
		bool use_dome_tex = (dome_tex && dome_tex->attrValue.type == ValueTypePlugin);
		pluginDesc.add("use_dome_tex", use_dome_tex);
	}
	else if (ELEM(pluginID, "LightOmniMax", "LightSpotMax")) {
		if (pluginID == "LightSpotMax") {
			BL::SpotLamp spotLamp(lamp);
			pluginDesc.add("fallsize", spotLamp.spot_size());
		}

		const auto shadowRadius = pluginDesc.get("shadowRadius");
		if (shadowRadius && shadowRadius->attrValue.as<AttrSimpleType<float>>() != 0.f) {
			const auto sr1 = pluginDesc.get("shadowRadius1");
			const auto sr2 = pluginDesc.get("shadowRadius2");

			if (sr1 && sr1->attrValue.as<AttrSimpleType<float>>() == 0.f) {
				pluginDesc.add("shadowRadius1", shadowRadius->attrValue.as<AttrSimpleType<float>>());
			}

			if (sr2 && sr2->attrValue.as<AttrSimpleType<float>>() == 0.f) {
				pluginDesc.add("shadowRadius2", shadowRadius->attrValue.as<AttrSimpleType<float>>());
			}
		}
	}
	else if (ELEM(pluginID, "LightRectangle", "LightSphere", "LightDome")) {
		pluginDesc.add("objectID", ob.pass_index());
	}

	BL::Object checkOb = ob;
	if (override && override.dupliEmitter.ptr.data) {
		checkOb = override.dupliEmitter;
	}

	auto lightSelectIter = m_light_select_channel.lightSelectMap.find(checkOb);
	if (lightSelectIter != m_light_select_channel.lightSelectMap.end()) {
		// current lamp that we are exporting is product of the selected object
		//  - it is either the object itself
		//  - the selected object is duplicator and current ob is child of it
		// find in which channels this plugin should be present
		for (const auto & chanTypeMap : lightSelectIter->second) {
			const LightSelect::ChannelType type = chanTypeMap.first;
			std::string typeName;
			switch (type) {
			case LightSelect::ChannelType::Raw: typeName = "channels_raw"; break;
			case LightSelect::ChannelType::Diffuse: typeName = "channels_diffuse"; break;
			case LightSelect::ChannelType::Specular: typeName = "channels_specular"; break;
			case LightSelect::ChannelType::Full: typeName = "channels_full"; break;
			}
			AttrListPlugin plList;
			for (const auto & chanName : chanTypeMap.second) {
				plList.append(chanName);
			}

			if (!plList.empty()) {
				pluginDesc.add(typeName, plList);
			}
		}
	}

	pluginDesc.add("scene_name", AttrListString(cryptomatteAllNames(ob)));

	plugin = m_exporter->export_plugin(pluginDesc);

	return plugin;
}

void DataExporter::exportLightLinker()
{
	auto lock = raiiLock();

	PluginDesc lightLinker("settingsLightLinker", "SettingsLightLinker");
	AttrListValue ignoreLights, ignoreShadowLights;

	for (const auto & linkIter : m_light_linker.lightsMap) {
		const auto & link = linkIter.second;
		AttrListPlugin excludePlugins;
		excludePlugins.append(linkIter.first); // light name

		if (link.isExcludeList) {
			// get all plugins for each excluded object
			for (const auto ob : link.objectList) {
				auto obPlugins = m_id_track.getAllObjectPlugins(ob);
				std::copy(obPlugins.begin(), obPlugins.end(), std::back_inserter(*excludePlugins.getData()));
			}
		} else {
			// all objects except ones selected
			for (const auto ob : m_id_track.data) {
				// link.objectList is in include mode so we need all *other* objects
				if (link.objectList.find(ob.second.object) == link.objectList.end()) {
					// copy all plugins to the output list
					std::transform(ob.second.plugins.begin(), ob.second.plugins.end(),
						std::back_inserter(*excludePlugins.getData()),
						std::bind(&std::map<std::string, IdTrack::PluginInfo>::value_type::first, std::placeholders::_1));
				}
			}

		}

		if (excludePlugins.getCount() > 1) { // more than just the light plugin
			if (link.excludeType & SettingsLightLinker::LightLink::Illumination) {
				ignoreLights.append(excludePlugins);
			}

			if (link.excludeType & SettingsLightLinker::LightLink::Shadows) {
				ignoreShadowLights.append(excludePlugins);
			}
		}
	}

	lightLinker.add("ignored_lights", ignoreLights);
	lightLinker.add("ignored_shadow_lights", ignoreShadowLights);

	m_exporter->export_plugin(lightLinker);
}
