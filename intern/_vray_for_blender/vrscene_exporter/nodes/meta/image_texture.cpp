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
#include "BLI_math.h"


std::string VRayNodeExporter::exportVRayNodeMetaImageTexture(VRayNodeExportParam)
{
	const std::string &pluginName = VRayNodeExporter::getPluginName(node, ntree, context);
	AttributeValueMap  pluginAttrs;

	const std::string &bitmapPlugin = "Bitmap@" + pluginName;
	AttributeValueMap  bitmapAttrs;
	exportBitmapBuffer(ntree, node, fromSocket, context, bitmapAttrs);

	if (NOT(bitmapAttrs.size())) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with MetaImageTexture's BitmapBuffer!",
					ntree.name().c_str(), node.name().c_str());
	}
	else {
		exportVRayNodeAttributes(ntree, node, fromSocket, context,
								 bitmapAttrs,
								 bitmapPlugin,
								 "BitmapBuffer",
								 "TEXTURE");

		const int   mappingType = RNA_enum_ext_get(&node.ptr, "mapping_type");
		std::string mappingPluginID;
		switch (mappingType) {
			case  0:  mappingPluginID = "UVWGenMayaPlace2dTexture"; break;
			case  1:  mappingPluginID = "UVWGenProjection"; break;
			case  2:  mappingPluginID = "UVWGenObject"; break;
			case  3:  mappingPluginID = "UVWGenEnvironment"; break;
			default:
				break;
		}

		BL::NodeSocket     mappingSock(PointerRNA_NULL);
		std::string        mappingPlugin;
		AttributeValueMap  mappingAttrs;

		// This means manually specified mapping node
		if (mappingPluginID.empty()) {
			mappingSock = VRayNodeExporter::getSocketByName(node, "Mapping");
			if (mappingSock && mappingSock.is_linked()) {
				mappingPlugin = VRayNodeExporter::exportLinkedSocket(ntree, mappingSock, context);
			}
			else {
				// Fallback to some default
				mappingPlugin = "DefChannelMapping@" + pluginName;
				mappingAttrs["uvw_channel"] = "0";
				VRayNodePluginExporter::exportPlugin("TEXTURE", "UVWGenChannel", mappingPlugin, mappingAttrs);
			}
		}

		if (mappingPluginID == "UVWGenMayaPlace2dTexture") {
			BL::NodeSocket rotateFrameTexSock = VRayNodeExporter::getSocketByAttr(node, "rotate_frame_tex");
			if (rotateFrameTexSock && NOT(rotateFrameTexSock.is_linked())) {
				const float rotate_frame_tex = DEG_TO_RAD(RNA_float_get(&rotateFrameTexSock.ptr, "value"));

				mappingAttrs["rotate_frame_tex"] = BOOST_FORMAT_FLOAT(rotate_frame_tex);
			}
		}
		else if (mappingPluginID == "UVWGenEnvironment") {
			PointerRNA UVWGenEnvironment = RNA_pointer_get(&node.ptr, "UVWGenEnvironment");
			const int mapping_type = RNA_enum_get(&UVWGenEnvironment, "mapping_type");

			mappingAttrs["mapping_type"] = BOOST_FORMAT_STRING(EnvironmentMappingType[mapping_type]);

			Object *ob = context.obCtx.ob;
			if (ob && ob->type == OB_LAMP) {
				float ltm[4][4];
				invert_m4_m4(ltm, ob->obmat);

				static char matbuf[1024];
				sprintf(matbuf, "Matrix(Vector(%g,%g,%g),Vector(%g,%g,%g),Vector(%g,%g,%g))",
				        ltm[0][0], ltm[0][1], ltm[0][2],
				        ltm[1][0], ltm[1][1], ltm[1][2],
				        ltm[2][0], ltm[2][1], ltm[2][2]);

				mappingAttrs["uvw_matrix"] = matbuf;
			}
		}

		if (mappingPlugin.empty()) {
			mappingPlugin = "Mapping@" + pluginName;
			exportVRayNodeAttributes(ntree, node, fromSocket, context,
									 mappingAttrs,
									 mappingPlugin,
									 mappingPluginID,
									 "UVWGEN");
		}

		pluginAttrs["bitmap"] = bitmapPlugin;
		pluginAttrs["uvwgen"] = mappingPlugin;

		exportVRayNodeAttributes(ntree, node, fromSocket, context,
								 pluginAttrs,
								 pluginName,
								 "TexBitmap",
								 "TEXTURE");

		return pluginName;
	}

	return "NULL";
}
