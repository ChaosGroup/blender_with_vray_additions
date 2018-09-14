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
#include "exp_api.h"


static const int sX = 0;
static const int sY = 5;
static const int sZ = 10;


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
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(ntree, domainSock, context);
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


std::string VRayNodeExporter::exportVRayNodeTexVoxelData(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	BL::NodeSocket domainSock = VRayNodeExporter::getSocketByName(node, "Domain");
	if(domainSock && domainSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(ntree, domainSock, context);
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


static std::string ExportSmokeDomain(BL::NodeTree ntree, BL::Node node, BL::Object domainOb, VRayNodeContext &context)
{
	BL::SmokeModifier smokeMod = GetSmokeModifier(domainOb);

	std::string pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

	std::string attr_lights;
	BL::NodeSocket lightsSock = VRayNodeExporter::getSocketByName(node, "Lights");
	if (lightsSock && lightsSock.is_linked()) {
		BL::Node lightsSelector = VRayNodeExporter::getConnectedNode(ntree, lightsSock, context);
		if (lightsSelector) {
			StrSet lights;
			VRayNodeExporter::getNodeSelectLightsNames(lightsSelector, lights);

			if (lights.size()) {
				attr_lights = BOOST_FORMAT_LIST(lights);
			}
		}
	}

	// This is a smoke simulation
	if(smokeMod) {
		ExportSmokeDomain(ExporterSettings::gSet.m_fileGeom,
						  ExporterSettings::gSet.m_sce,
						  (Object*)domainOb.ptr.data,
						  (SmokeModifierData*)smokeMod.ptr.data,
						  pluginName.c_str(),
						  attr_lights.c_str());
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

		PointerRNA EnvFogMeshGizmo = RNA_pointer_get(&node.ptr, "EnvFogMeshGizmo");

		AttributeValueMap pluginAttrs;
		pluginAttrs["geometry"]  = geomPluginName;
		pluginAttrs["transform"] = BOOST_FORMAT_TM(transform);
		pluginAttrs["fade_out_radius"] = BOOST_FORMAT_FLOAT(RNA_float_get(&EnvFogMeshGizmo, "fade_out_radius"));

		if (!attr_lights.empty()) {
			pluginAttrs["lights"] = attr_lights;
		}

		VRayNodePluginExporter::exportPlugin("EFFECT", "EnvFogMeshGizmo", pluginName, pluginAttrs);
	}

	// Exclude object from Node creation
	ExporterSettings::gSet.m_exporter->addSkipObject(domainOb.ptr.data);

	return pluginName;
}


std::string VRayNodeExporter::exportVRayNodeEnvFogMeshGizmo(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	BL::NodeSocket objectSock = VRayNodeExporter::getSocketByName(node, "Object");
	if(objectSock && objectSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(ntree, objectSock, context);
		if(domainNode) {
			StrSet domains;

			ObList domainObList;
			VRayNodeExporter::getNodeSelectObjects(domainNode, domainObList);

			if(domainObList.size()) {
				ObList::const_iterator obIt;
				for(obIt = domainObList.begin(); obIt != domainObList.end(); ++obIt) {
					BL::Object domainOb = *obIt;
					if(VRayNodeExporter::isObjectVisible(domainOb)) {
						domains.insert(ExportSmokeDomain(ntree, node, domainOb, context));
					}
				}
			}

			return BOOST_FORMAT_LIST(domains);
		}
	}

	return "List()";
}


std::string VRayNodeExporter::exportVRayNodeEnvironmentFog(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	if (NOT(ExporterSettings::gSet.m_exportSmoke))
		return "NULL";

	AttributeValueMap  manualAttrs;

	BL::NodeSocket gizmosSock = VRayNodeExporter::getSocketByAttr(node, "gizmos");
	if (gizmosSock.is_linked()) {
		std::string gizmos = "";

		BL::Node conNode = VRayNodeExporter::getConnectedNode(ntree, gizmosSock, context);
		if (NOT(conNode.bl_idname() == "VRayNodeEnvFogMeshGizmo")) {
			PRINT_ERROR("\"Gizmos\" socket expects \"Fog Gizmo\" node");
		}
		else {
			gizmos = VRayNodeExporter::exportSocket(ntree, gizmosSock, context);
		}

		// If socket is linked it means user have attached the gizmo node,
		// but if gizmos list is empty it means gizmo object is invisible.
		// We don't need to export the whole effect at all because it will cover the whole
		// scene without gizmo.
		if (gizmos.empty() || gizmos == "NULL" || gizmos == "List()") {
			PRINT_ERROR("No smoke gizmos found!");
			return "NULL";
		}
		else {
			manualAttrs["gizmos"] = gizmos;
		}
	}

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, manualAttrs);
}


std::string VRayNodeExporter::exportVRayNodePhxShaderSimVol(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	AttributeValueMap pluginAttrs;
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


std::string VRayNodeExporter::exportVRayNodePhxShaderSim(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	const std::string &pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

	const std::string fluidCacheName = pluginName + "@Cache";

	BL::Object     domainOb(PointerRNA_NULL);
	BL::NodeSocket domainSock = VRayNodeExporter::getSocketByAttr(node, "cache");
	if(domainSock && domainSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(ntree, domainSock, context);
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

					AttributeValueMap fluidCacheAttrs;
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

	AttributeValueMap pluginAttrs;
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
    AttributeValueMap alphaTexAttrs;
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


std::string VRayNodeExporter::exportVRayNodeSphereFade(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	StrSet gizmos;

	BL::Node::inputs_iterator inIt;
	for (node.inputs.begin(inIt); inIt != node.inputs.end(); ++inIt) {
		BL::NodeSocket inSock = *inIt;
		if (inSock && inSock.is_linked()) {
			BL::Node connNode = VRayNodeExporter::getConnectedNode(ntree, inSock, context);
			if (connNode && connNode.bl_idname() == "VRayNodeSphereFadeGizmo") {
				gizmos.insert(VRayNodeExporter::exportLinkedSocket(ntree, inSock, context));
			}
		}
	}

	AttributeValueMap  manualAttrs;
	manualAttrs["gizmos"] = BOOST_FORMAT_LIST(gizmos);

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, manualAttrs);
}


std::string VRayNodeExporter::exportVRayNodeSphereFadeGizmo(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	PointerRNA sphereFadeGizmo = RNA_pointer_get(&node.ptr, "SphereFadeGizmo");

	BL::Object ob = VRayNodeExporter::getObjectByName(RNA_std_string_get(&sphereFadeGizmo, "object"));
	if (ob && ob.type() == BL::Object::type_EMPTY) {
		BLTransform tm = ob.matrix_world();

		float radius = ob.empty_draw_size() * (tm.data[sX] + tm.data[sY] + tm.data[sZ]) / 3.0f;

		// Reset rotation / scale
		::memset(tm.data, 0, 9 * sizeof(float));

		// Set scale to 1.0
		tm.data[sX] = 1.0f;
		tm.data[sY] = 1.0f;
		tm.data[sZ] = 1.0f;

		AttributeValueMap attrs;
		attrs["radius"]    = BOOST_FORMAT_FLOAT(radius);
		attrs["transform"] = BOOST_FORMAT_TM(GetTransformHex(tm));

		return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, attrs);
	}

	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeVolumeVRayToon(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	// TODO: exclude shaped lights

	AttributeValueMap pluginAttrs;
	VRayNodeExporter::getVRayNodeAttributes(pluginAttrs, ntree, node, fromSocket, context);

	// NOTE: When size is in pixels 'lineWidth_tex' value is ignored
	if (IsStdStringDigit(pluginAttrs["lineWidth_tex"])) {
		// TODO: Check, may be this parameter is always needed
		pluginAttrs["lineWidth"] = pluginAttrs["lineWidth_tex"];
	}

	BL::NodeSocket excludeSock = VRayNodeExporter::getSocketByAttr(node, "excludeList");
	if (excludeSock && excludeSock.is_linked()) {
		BL::Node obSelector = VRayNodeExporter::getConnectedNode(ntree, excludeSock, context);
		if (obSelector) {
			ObList excludeObjects;
			VRayNodeExporter::getNodeSelectObjects(obSelector, excludeObjects);

			if (excludeObjects.size()) {
				StrSet excludeList;

				for (ObList::const_iterator obIt = excludeObjects.begin(); obIt != excludeObjects.end(); ++obIt) {
					BL::Object ob(*obIt);
					excludeList.insert(GetIDName(ob));
				}

				pluginAttrs["excludeList"] = BOOST_FORMAT_LIST(excludeList);
			}
		}
	}

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, pluginAttrs);
}
