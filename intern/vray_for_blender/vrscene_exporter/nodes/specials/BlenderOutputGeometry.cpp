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


std::string VRayNodeExporter::exportVRayNodeBlenderOutputGeometry(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	if(NOT(context->obCtx.ob)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	std::string pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

	if(ExporterSettings::gSet.m_exportMeshes) {
		if(ExporterSettings::gSet.m_isAnimation) {
			if(ExporterSettings::gSet.DoUpdateCheck() && NOT(IsObjectDataUpdated(context->obCtx.ob))) {
				return pluginName;
			}
		}

		VRayScene::GeomStaticMesh *geomStaticMesh = new VRayScene::GeomStaticMesh(context->obCtx.sce, context->obCtx.main, context->obCtx.ob);
		geomStaticMesh->init();
		geomStaticMesh->initName(pluginName);
		geomStaticMesh->initAttributes(&node.ptr);

		int toDelete = geomStaticMesh->write(ExporterSettings::gSet.m_fileGeom, ExporterSettings::gSet.m_frameCurrent);
		if(toDelete)
			delete geomStaticMesh;
	}

	return pluginName;
}
