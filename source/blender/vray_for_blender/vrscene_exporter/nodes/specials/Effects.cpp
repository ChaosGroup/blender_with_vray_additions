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
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(domainSock);
		if(domainNode && (domainNode.bl_idname() == "VRayNodeSelectObject")) {
			BL::Object domainOb = VRayNodeExporter::exportVRayNodeSelectObject(ntree, domainNode, domainSock, context);
			if(domainOb) {
				BL::SmokeModifier smokeMod = GetSmokeModifier(domainOb);

				// This is a smoke simulation and we need to export smoke data
				if(smokeMod) {
					PointerRNA texVoxelData = RNA_pointer_get(&node.ptr, "TexVoxelData");

					std::string pluginName    = VRayNodeExporter::getPluginName(node, ntree, context);
					int         interpolation = RNA_enum_get(&texVoxelData, "interpolation");

					ExportTexVoxelData(ExpoterSettings::gSet.m_fileGeom,
									   ExpoterSettings::gSet.m_sce,
									   (Object*)domainOb.ptr.data,
									   (SmokeModifierData*)smokeMod.ptr.data,
									   pluginName.c_str(),
									   interpolation);
					return pluginName;
				}
			}
		}
	}

	return "NULL";
}


std::string VRayNodeExporter::exportVRayNodeEnvFogMeshGizmo(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	BL::NodeSocket domainSock = VRayNodeExporter::getSocketByName(node, "Object");
	if(domainSock && domainSock.is_linked()) {
		BL::Node domainNode = VRayNodeExporter::getConnectedNode(domainSock);

		// NOTE: Rewrite to support object / group select

		if(domainNode && (domainNode.bl_idname() == "VRayNodeSelectObject")) {
			BL::Object domainOb = VRayNodeExporter::exportVRayNodeSelectObject(ntree, domainNode, domainSock, context);
			if(domainOb) {
				BL::SmokeModifier smokeMod = GetSmokeModifier(domainOb);

				std::string pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

				// This is a smoke simulation
				if(smokeMod) {
					std::string lightsList;
					BL::NodeSocket lightsSock = VRayNodeExporter::getSocketByName(node, "Lights");
					if(lightsSock && lightsSock.is_linked()) {
						BL::Node lightsNode = VRayNodeExporter::getConnectedNode(lightsSock);
						if(lightsNode && (lightsNode.bl_idname() == "VRayNodeSelectObject" || lightsNode.bl_idname() == "VRayNodeSelectGroup")) {
							lightsList = exportVRayNode(ntree, lightsNode, lightsSock);
						}
					}

					ExportSmokeDomain(ExpoterSettings::gSet.m_fileGeom,
									  ExpoterSettings::gSet.m_sce,
									  (Object*)domainOb.ptr.data,
									  (SmokeModifierData*)smokeMod.ptr.data,
									  pluginName.c_str(),
									  lightsList.c_str());
				}
				// This is just a container - export as mesh
				else {
					std::string geomPluginName = pluginName + "@Domain";

					ExportGeomStaticMesh(ExpoterSettings::gSet.m_fileGeom,
										 ExpoterSettings::gSet.m_sce,
										 (Object*)domainOb.ptr.data,
										 ExpoterSettings::gSet.m_main,
										 pluginName.c_str(),
										 NULL);

					char transform[CGR_TRANSFORM_HEX_SIZE];
					GetTransformHex(((Object*)domainOb.ptr.data)->obmat, transform);

					AttributeValueMap pluginAttrs;
					pluginAttrs["geometry"]  = geomPluginName;
					pluginAttrs["transform"] = BOOST_FORMAT_TM(transform);
					
					VRayNodePluginExporter::exportPlugin("NODE", "EnvFogMeshGizmo", pluginName, pluginAttrs);
				}

				// Exclude object from Node creation
				ExpoterSettings::gSet.m_exporter->addSkipObject(domainOb.ptr.data);
			}
		}
	}

	return "NULL";
}
