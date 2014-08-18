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
#include "vrscene_api.h"


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


std::string VRayNodeExporter::exportVRayNodeTexVoxelData(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
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
						PointerRNA texVoxelData = RNA_pointer_get(&node.ptr, "TexVoxelData");

						std::string pluginName    = VRayNodeExporter::getPluginName(node, ntree, context);
						int         interpolation = RNA_enum_get(&texVoxelData, "interpolation");

						ExportTexVoxelData(ExporterSettings::gSet.m_fileGeom,
										   ExporterSettings::gSet.m_sce,
										   (Object*)domainOb.ptr.data,
										   (SmokeModifierData*)smokeMod.ptr.data,
										   pluginName.c_str(),
										   interpolation);
						return pluginName;
					}
				}
			}
		}
	}

	return "NULL";
}


static std::string ExportSmokeDomain(BL::NodeTree ntree, BL::Node node, BL::Object domainOb, VRayNodeContext *context)
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
					lightsList = VRayNodeExporter::exportVRayNode(ntree, lightsNode, lightsSock);

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

		AttributeValueMap pluginAttrs;
		pluginAttrs["geometry"]  = geomPluginName;
		pluginAttrs["transform"] = BOOST_FORMAT_TM(transform);

		VRayNodePluginExporter::exportPlugin("EFFECT", "EnvFogMeshGizmo", pluginName, pluginAttrs);
	}

	// Exclude object from Node creation
	ExporterSettings::gSet.m_exporter->addSkipObject(domainOb.ptr.data);

	return pluginName;
}


std::string VRayNodeExporter::exportVRayNodeEnvFogMeshGizmo(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	BL::NodeSocket objectSock = VRayNodeExporter::getSocketByName(node, "Object");
	if(objectSock && objectSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(objectSock, context);
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


std::string VRayNodeExporter::exportVRayNodeEnvironmentFog(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	if (NOT(ExporterSettings::gSet.m_exportSmoke))
		return "NULL";

	AttributeValueMap  manualAttrs;

	BL::NodeSocket gizmosSock = VRayNodeExporter::getSocketByAttr(node, "gizmos");
	if (gizmosSock.is_linked()) {
		std::string gizmos = "";

		BL::Node conNode = VRayNodeExporter::getConnectedNode(gizmosSock);
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
			return "NULL";
		}
		else {
			manualAttrs["gizmos"] = gizmos;
		}
	}

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, manualAttrs);
}
