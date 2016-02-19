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


std::string VRayNodeExporter::exportVRayNodeLightMesh(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context)
{
	std::string plugin = "NULL";

	if (NOT(context.obCtx.ob)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		BL::NodeSocket geomSock = VRayNodeExporter::getSocketByName(node, "Geometry");
		if (NOT(geomSock.is_linked())) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Geometry socket is not linked!",
			            ntree.name().c_str(), node.name().c_str());
		}
		else {
			PointerRNA obPtr;
			RNA_id_pointer_create((ID*)context.obCtx.ob, &obPtr);
			BL::Object ob(obPtr);

			std::string pluginName = "MeshLight@" + GetIDName(ob);

			std::string transform = GetTransformHex(ob.matrix_world());
			int         objectID  = ob.pass_index();

			NodeAttrs &attrs = context.obCtx.nodeAttrs;
			if (attrs.override) {
				pluginName = attrs.namePrefix + pluginName;
				objectID   = attrs.objectID;
				transform  = GetTransformHex(attrs.tm);
			}

			AttributeValueMap manualAttrs;
			manualAttrs["geometry"]  = VRayNodeExporter::exportLinkedSocket(ntree, geomSock, context);
			manualAttrs["transform"] = BOOST_FORMAT_TM(transform);
			manualAttrs["objectID"]  = BOOST_FORMAT_INT(objectID);
			VRayNodeExporter::getVRayNodeAttributes(manualAttrs, ntree, node, fromSocket, context, manualAttrs);

			manualAttrs["use_tex"] = BOOST_FORMAT_BOOL(manualAttrs.count("tex"));

			plugin = VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context,
			                                                    /* customAttrs */ manualAttrs,
			                                                    /* customName  */ pluginName);
		}
	}

	return plugin;
}
