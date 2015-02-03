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
			default:  mappingPluginID = "UVWGenObject"; break;
		}

		const std::string &mappingPlugin = "Mapping@" + pluginName;
		AttributeValueMap  mappingAttrs;
		exportVRayNodeAttributes(ntree, node, fromSocket, context,
								 mappingAttrs,
								 mappingPlugin,
								 mappingPluginID,
								 "UVWGEN");

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
