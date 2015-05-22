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
#include "vfb_utils_blender.h"


#if 0

static BL::SmokeModifier GetSmokeModifier(BL::Object ob)
{
	BL::Object::modifiers_iterator modIt;
	for(ob.modifiers.begin(modIt); modIt != ob.modifiers.end(); ++modIt) {
		BL::Modifier mod = *modIt;
		if(mod.type() == BL::Modifier::type_SMOKE) {
			return BL::SmokeModifier(mod);
		}
	}
	return PointerRNA_NULL;
}


std::string VRayNodeExporter::exportVRayNodeTexMayaFluid(VRayNodeExportParam)
{
	BL::NodeSocket domainSock = VRayNodeExporter::getSocketByName(node, "Domain");
	if(domainSock && domainSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(domainSock, context);
		if(domainNode) {
			if(NOT(domainNode.bl_idname() == "VRayNodeSelectObject")) {
				PRINT_ERROR("Domain object must be selected with \"Select Object\" node!");
			}
			else {
				BL::Object domainOb = VRayNodeExporter::exportVRayNodeSelectObject(ntree, domainNode, domainSock, context);
				if(domainOb && VRayNodeExporter::isObjectVisible(domainOb)) {
					BL::SmokeModifier smokeMod = GetSmokeModifier(domainOb);

					// This is a smoke simulation and we need to export smoke data
					if(smokeMod) {
						PointerRNA texMayaFluid = RNA_pointer_get(&node.ptr, "TexMayaFluid");

						std::string pluginName    = VRayNodeExporter::getPluginName(node, ntree, context);
						int         interpolation = RNA_enum_get(&texMayaFluid, "interpolation_type");

						if (ExporterSettings::gSet.m_exportSmoke) {
							ExportVoxelDataAsFluid(ExporterSettings::gSet.m_fileGeom,
												   ExporterSettings::gSet.m_sce,
												   (Object*)domainOb.ptr.data,
												   (SmokeModifierData*)smokeMod.ptr.data,
												   pluginName.c_str(),
												   interpolation);
						}

						// Exclude object from Node creation
						ExporterSettings::gSet.m_exporter->addSkipObject(domainOb.ptr.data);

						return pluginName;
					}
				}
			}
		}
	}

	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeTexVoxelData(VRayNodeExportParam)
{
	BL::NodeSocket domainSock = VRayNodeExporter::getSocketByName(node, "Domain");
	if(domainSock && domainSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(domainSock, context);
		if(domainNode) {
			if(NOT(domainNode.bl_idname() == "VRayNodeSelectObject")) {
				PRINT_ERROR("Domain object must be selected with \"Select Object\" node!");
			}
			else {
				BL::Object domainOb = VRayNodeExporter::exportVRayNodeSelectObject(ntree, domainNode, domainSock, context);
				if(domainOb && VRayNodeExporter::isObjectVisible(domainOb)) {
					BL::SmokeModifier smokeMod = GetSmokeModifier(domainOb);

					// This is a smoke simulation and we need to export smoke data
					if (NOT(smokeMod)) {
						PRINT_ERROR("Invalid Smoke modifier!");
					}
					else {
						PointerRNA texVoxelData = RNA_pointer_get(&node.ptr, "TexVoxelData");

						std::string pluginName    = VRayNodeExporter::getPluginName(node, ntree, context);
						int         interpolation = RNA_enum_get(&texVoxelData, "interpolation");

						if (ExporterSettings::gSet.m_exportSmoke) {
							ExportTexVoxelData(ExporterSettings::gSet.m_fileGeom,
											   ExporterSettings::gSet.m_sce,
											   (Object*)domainOb.ptr.data,
											   (SmokeModifierData*)smokeMod.ptr.data,
											   pluginName.c_str(),
											   interpolation);
						}

						return pluginName;
					}
				}
			}
		}
	}

	return "NULL";
}


static std::string ExportSmokeDomain(BL::NodeTree ntree, BL::Node node, BL::Object domainOb, NodeContext &context)
{
	BL::SmokeModifier smokeMod = GetSmokeModifier(domainOb);

	std::string pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

	// This is a smoke simulation
	if(smokeMod) {
		std::string lightsList;
		BL::NodeSocket lightsSock = VRayNodeExporter::getSocketByName(node, "Lights");
		if(lightsSock && lightsSock.is_linked()) {
			BL::Node lightsNode = VRayNodeExporter::getConnectedNode(lightsSock, context);
			if(lightsNode) {
				if(lightsNode.bl_idname() == "VRayNodeSelectObject" || lightsNode.bl_idname() == "VRayNodeSelectGroup") {
					lightsList = VRayNodeExporter::exportVRayNode(ntree, lightsNode, lightsSock, context);

					// NOTE: Attribute expects list even only one object is selected
					if(lightsNode.bl_idname() == "VRayNodeSelectObject") {
						lightsList = "List(" + lightsList + ")";
					}
				}
			}
		}

		ExportSmokeDomain(ExporterSettings::gSet.m_fileGeom,
						  ExporterSettings::gSet.m_sce,
						  (Object*)domainOb.ptr.data,
						  (SmokeModifierData*)smokeMod.ptr.data,
						  pluginName.c_str(),
						  lightsList.c_str());
	}
	// This is just a container - export as mesh
	else {
		std::string geomPluginName = pluginName + "@Domain";

		ExportGeomStaticMesh(ExporterSettings::gSet.m_fileGeom,
							 ExporterSettings::gSet.m_sce,
							 (Object*)domainOb.ptr.data,
							 ExporterSettings::gSet.m_main,
							 geomPluginName.c_str(),
							 NULL);

		char transform[CGR_TRANSFORM_HEX_SIZE];
		GetTransformHex(((Object*)domainOb.ptr.data)->obmat, transform);

		PluginDesc pluginAttrs;
		pluginAttrs["geometry"]  = geomPluginName;
		pluginAttrs["transform"] = BOOST_FORMAT_TM(transform);

		VRayNodePluginExporter::exportPlugin("EFFECT", "EnvFogMeshGizmo", pluginName, pluginAttrs);
	}

	// Exclude object from Node creation
	ExporterSettings::gSet.m_exporter->addSkipObject(domainOb.ptr.data);

	return pluginName;
}
#endif


AttrValue DataExporter::exportVRayNodeEnvFogMeshGizmo(VRayNodeExportParam)
{
	AttrListPlugin domains;

	BL::NodeSocket objectSock = Nodes::GetInputSocketByName(node, "Object");
	if (objectSock && objectSock.is_linked()) {
		BL::Node domainNode = getConnectedNode(objectSock, context);
		if (domainNode) {
			ObList domainObList;
			getSelectorObjectList(domainNode, domainObList);

			for(ObList::const_iterator obIt = domainObList.begin(); obIt != domainObList.end(); ++obIt) {
				BL::Object domainOb(*obIt);
				if (isObjectVisible(domainOb)) {
					// domains.append(ExportSmokeDomain(ntree, node, domainOb, context));
				}
			}
		}
	}

	return domains;
}


AttrValue DataExporter::exportVRayNodeEnvironmentFog(VRayNodeExportParam)
{
	AttrValue plugin;

	if (m_settings.export_fluids) {
		PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
		                      "EnvironmentFog");

		bool valid_gizmos = true;

		BL::NodeSocket gizmosSock = Nodes::GetSocketByAttr(node, "gizmos");
		if (gizmosSock && gizmosSock.is_linked()) {
			AttrValue gizmos;

			BL::Node conNode = getConnectedNode(gizmosSock, context);
			if (conNode) {
				if (conNode.bl_idname() != "VRayNodeEnvFogMeshGizmo") {
					PRINT_ERROR("\"Gizmos\" socket expects \"Fog Gizmo\" node!");
				}
				else {
					gizmos = exportSocket(ntree, gizmosSock, context);
					if (!gizmos || (gizmos.type == ValueTypeListPlugin && gizmos.valListPlugin.empty())) {
						// If socket is linked it means user have attached the gizmo node,
						// but if gizmos list is empty it means gizmo object is invisible.
						// We don't need to export the whole effect at all because it will cover the whole
						// scene without gizmo.
						valid_gizmos = false;
					}
					else {
						pluginDesc.add("gizmos", gizmos);
					}
				}
			}
		}

		if (valid_gizmos) {
			plugin = exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
		}
	}

	return plugin;
}


#if 0
std::string VRayNodeExporter::exportVRayNodePhxShaderSimVol(VRayNodeExportParam)
{
	PluginDesc pluginAttrs;
	VRayNodeExporter::getVRayNodeAttributes(pluginAttrs, ntree, node, fromSocket, context);

	pluginAttrs["phoenix_sim"] = "List(" + pluginAttrs["phoenix_sim"] + ")";

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, pluginAttrs);
}


static void MatchMayaFluid(std::string &name)
{
	boost::replace_all(name, "::out_density", "@Density");
	boost::replace_all(name, "::out_flame",   "@Flame");
	boost::replace_all(name, "::out_fuel",    "@Fuel");
}


std::string VRayNodeExporter::exportVRayNodePhxShaderSim(VRayNodeExportParam)
{
	const std::string &pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

	const std::string fluidCacheName = pluginName + "@Cache";

	BL::Object     domainOb(PointerRNA_NULL);
	BL::NodeSocket domainSock = VRayNodeExporter::getSocketByAttr(node, "cache");
	if(domainSock && domainSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(domainSock, context);
		if(domainNode) {
			if(NOT(domainNode.bl_idname() == "VRayNodeSelectObject")) {
				PRINT_ERROR("Domain object must be selected with \"Select Object\" node!");
			}
			else {
				domainOb = VRayNodeExporter::exportVRayNodeSelectObject(ntree, domainNode, domainSock, context);
				if(NOT(domainOb && VRayNodeExporter::isObjectVisible(domainOb))) {
					return "NULL";
				}
				else {
					BL::SmokeModifier smokeMod = GetSmokeModifier(domainOb);

					BL::SmokeDomainSettings smokeDomainSettings = smokeMod.domain_settings();
					const BL::Array<int, 3> &domainResolution = smokeDomainSettings.domain_resolution();

					PluginDesc fluidCacheAttrs;
					fluidCacheAttrs["grid_size_x"] = BOOST_FORMAT_INT(domainResolution.data[0]);
					fluidCacheAttrs["grid_size_y"] = BOOST_FORMAT_INT(domainResolution.data[1]);
					fluidCacheAttrs["grid_size_z"] = BOOST_FORMAT_INT(domainResolution.data[2]);
					VRayNodePluginExporter::exportPlugin("EFFECT", "PhxShaderCache", fluidCacheName, fluidCacheAttrs);

					// Exclude object from Node creation
					ExporterSettings::gSet.m_exporter->addSkipObject(domainOb.ptr.data);
				}
			}
		}
	}

	PointerRNA phxShaderSim = RNA_pointer_get(&node.ptr, "PhxShaderSim");

	PluginDesc pluginAttrs;
	VRayNodeExporter::getVRayNodeAttributes(pluginAttrs, ntree, node, fromSocket, context);

	BLTransform node_transform = domainOb.matrix_world();

	// To match Phoenix container
	node_transform.data[sX] *= 2.0f;
	node_transform.data[sY] *= 2.0f;
	node_transform.data[sZ] *= 2.0f;

	pluginAttrs["node_transform"] = BOOST_FORMAT_TM(GetTransformHex(node_transform));
	pluginAttrs["cache"] = fluidCacheName;

    std::string dtex = pluginAttrs["dtex"];
    MatchMayaFluid(dtex);

    std::string etex = pluginAttrs["etex"];
    MatchMayaFluid(etex);

    std::string ttex = pluginAttrs["ttex"];
    MatchMayaFluid(ttex);

    const std::string alphaTexName = pluginName + "@Alpha";
    PluginDesc alphaTexAttrs;
    alphaTexAttrs["ttex"] = ttex;
    alphaTexAttrs["transparency"] = "AColor(0.5,0.5,0.5,1.0)";
    VRayNodePluginExporter::exportPlugin("EFFECT", "PhxShaderTexAlpha", alphaTexName, alphaTexAttrs);

    // XXX: Test data! Finish this!
    pluginAttrs["darg"] = "4";
    pluginAttrs["dtex"] = dtex;
    pluginAttrs["earg"] = "4";
    pluginAttrs["etex"] = etex;
    pluginAttrs["targ"] = "4";
    pluginAttrs["ttex"] = alphaTexName;
    pluginAttrs["camera_visibility"] = "1";
    pluginAttrs["cell_aspect"] = "ListFloat(1.0,1.0,1.0)";
    pluginAttrs["enabled"] = "1";
    pluginAttrs["jitter"] =  "1";
    pluginAttrs["rendstep"] = BOOST_FORMAT_INT(RNA_int_get(&phxShaderSim, "rendstep"));
    pluginAttrs["shadow_opacity"] = "0.5";
    pluginAttrs["shadows_visibility"] =  "1";
    pluginAttrs["transpmode"] = "1";
    pluginAttrs["no_alpha_e"] = "0";
    pluginAttrs["lightcache"] = "1";
    pluginAttrs["noscatter"] =  "1";
    pluginAttrs["bounces"] = "1";

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, pluginAttrs);
}
#endif


AttrValue DataExporter::exportVRayNodeSphereFadeGizmo(VRayNodeExportParam)
{
	AttrValue plugin;

	PointerRNA sphereFadeGizmo = RNA_pointer_get(&node.ptr, "SphereFadeGizmo");

	BL::Object ob = Blender::GetObjectByName(m_data, RNA_std_string_get(&sphereFadeGizmo, "object"));
	if (ob && ob.type() == BL::Object::type_EMPTY) {
		BlTransform tm     = ob.matrix_world();
		const float radius = ob.empty_draw_size() * (tm.data[0] + tm.data[5] + tm.data[10]) / 3.0f;

		// Reset rotation / scale
		memset(tm.data, 0, 9 * sizeof(float));

		// Set scale to 1.0
		tm.data[0]  = 1.0f;
		tm.data[5]  = 1.0f;
		tm.data[10] = 1.0f;

		PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
		                      "SphereFadeGizmo");
		pluginDesc.add("radius", radius);
		pluginDesc.add("transform", AttrTransformFromBlTransform(tm));

		plugin = m_exporter->export_plugin(pluginDesc);
	}

	return plugin;
}


AttrValue DataExporter::exportVRayNodeSphereFade(VRayNodeExportParam)
{
	AttrListPlugin gizmos;

	BL::Node::inputs_iterator inIt;
	for (node.inputs.begin(inIt); inIt != node.inputs.end(); ++inIt) {
		BL::NodeSocket inSock = *inIt;
		if (inSock && inSock.is_linked()) {
			BL::Node connNode = getConnectedNode(inSock, context);
			if (connNode && connNode.bl_idname() == "VRayNodeSphereFadeGizmo") {
				AttrValue sphereFadeGizmo = exportLinkedSocket(ntree, inSock, context);
				if (sphereFadeGizmo && sphereFadeGizmo.type == ValueTypePlugin) {
					gizmos.append(sphereFadeGizmo.valPlugin);
				}
			}
		}
	}

	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "SphereFade");
	pluginDesc.add("gizmos", gizmos);

	return m_exporter->export_plugin(pluginDesc);
}


AttrValue DataExporter::exportVRayNodeVolumeVRayToon(VRayNodeExportParam)
{
	PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
	                      "VolumeVRayToon");
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	const PluginAttr *lineWidth_tex = pluginDesc.get("lineWidth_tex");
	if (lineWidth_tex && (lineWidth_tex->attrValue.type == ValueTypeFloat)) {
		// NOTE: When size is in pixels 'lineWidth_tex' value is ignored
		pluginDesc.add("lineWidth", lineWidth_tex->attrValue);
	}

	BL::NodeSocket excludeSock = Nodes::GetSocketByAttr(node, "excludeList");
	if (excludeSock && excludeSock.is_linked()) {
		BL::Node obSelector = getConnectedNode(excludeSock, context);
		if (obSelector) {
			ObList excludeObjects;
			getSelectorObjectList(obSelector, excludeObjects);

			if (excludeObjects.size()) {
				AttrListPlugin excludeList;

				for (ObList::const_iterator obIt = excludeObjects.begin(); obIt != excludeObjects.end(); ++obIt) {
					BL::Object ob(*obIt);
					excludeList.append(Blender::GetIDName(ob, "OB"));
				}

				pluginDesc.add("excludeList", excludeList);
			}
		}
	}

	return m_exporter->export_plugin(pluginDesc);
}
