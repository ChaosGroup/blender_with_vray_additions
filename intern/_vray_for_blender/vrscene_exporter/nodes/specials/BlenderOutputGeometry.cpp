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

#include "GeomStaticMesh.h"


std::string VRayNodeExporter::exportVRayNodeBlenderOutputGeometry(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	std::string pluginName = "NULL";

	if (NOT(context.obCtx.ob && context.obCtx.ob->data)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		PointerRNA obRNA;
		RNA_id_pointer_create((ID*)context.obCtx.ob, &obRNA);
		BL::Object ob(obRNA);

		PointerRNA sceRNA;
		RNA_id_pointer_create((ID*)context.obCtx.sce, &sceRNA);
		BL::Scene sce(sceRNA);

		BL::ID dataID = ob.data();
		if (dataID) {
			const bool could_instance = CouldInstance(sce, ob);

			pluginName = GetIDName(could_instance
			                       ? (ID*)context.obCtx.ob->data
			                       : (ID*)context.obCtx.ob) + "@Geom";

			if(ExporterSettings::gSet.m_exportMeshes) {
				if(ExporterSettings::gSet.m_isAnimation) {
					if(ExporterSettings::gSet.DoUpdateCheck() && NOT(IsObjectDataUpdated(context.obCtx.ob))) {
						return pluginName;
					}
				}

				BL::ID dataKey = could_instance
				                 ? ob.data()
				                 : ob;

				if (Node::sMeshCache.count(dataKey)) {
					pluginName = Node::sMeshCache[dataKey];
				}
				else {
					VRayScene::GeomStaticMesh *geomStaticMesh = new VRayScene::GeomStaticMesh(context.obCtx.sce, context.obCtx.main, context.obCtx.ob);
					geomStaticMesh->init();
					geomStaticMesh->initName(pluginName);
					geomStaticMesh->initAttributes(&node.ptr);

					if (could_instance || context.obCtx.nodeAttrs.dynamic_geometry) {
						geomStaticMesh->setDynamicGeometry(true);
					}

					int toDelete = geomStaticMesh->write(ExporterSettings::gSet.m_fileGeom, ExporterSettings::gSet.m_frameCurrent);
					if(toDelete) {
						delete geomStaticMesh;
					}

					Node::sMeshCache[dataKey] = pluginName;
				}
			}
		}
	}

	return pluginName;
}
