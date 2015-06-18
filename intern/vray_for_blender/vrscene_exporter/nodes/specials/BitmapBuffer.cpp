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
#include "cgr_paths.h"


void VRayNodeExporter::exportBitmapBuffer(VRayNodeExportParam, AttributeValueMap &attrs)
{
	BL::Texture b_tex = VRayNodeExporter::getTextureFromIDRef(&node.ptr, "texture");
	if(b_tex) {
		BL::ImageTexture imageTexture(b_tex.ptr);
		if(imageTexture) {
			std::string absFilepath;

			BL::Image image = imageTexture.image();
			if(image) {
				absFilepath = BlenderUtils::GetFullFilepath(image.filepath(), (ID*)ntree.ptr.data);
				absFilepath = BlenderUtils::CopyDRAsset(absFilepath);

				if(image.source() == BL::Image::source_SEQUENCE) {
					BL::ImageUser imageUser = imageTexture.image_user();

					int seqFrame = 0;

					int seqOffset = imageUser.frame_offset();
					int seqLength = imageUser.frame_duration();
					int seqStart  = imageUser.frame_start();
					int seqEnd    = seqLength - seqStart + 1;

					if(imageUser.use_cyclic()) {
						seqFrame = ((int(ExporterSettings::gSet.m_frameCurrent) - seqStart) % seqLength) + 1;
					}
					else {
						if(ExporterSettings::gSet.m_frameCurrent < seqStart){
							seqFrame = seqStart;
						}
						else if(ExporterSettings::gSet.m_frameCurrent > seqEnd) {
							seqFrame = seqEnd;
						}
						else {
							seqFrame = seqStart + ExporterSettings::gSet.m_frameCurrent - 1;
						}
					}
					if(seqOffset < 0) {
						if((seqFrame - abs(seqOffset)) < 0) {
							seqFrame += seqLength;
						}
					}

					attrs["frame_sequence"] = "1";
					attrs["frame_offset"] = BOOST_FORMAT_INT(seqOffset);
					attrs["frame_number"] = BOOST_FORMAT_INT(seqFrame);
				}
			}

			attrs["file"] = BOOST_FORMAT_STRING(absFilepath.c_str());

			PointerRNA vrayScene = RNA_pointer_get(&ExporterSettings::gSet.b_scene.ptr, "vray");
			PointerRNA settingsColorMapping = RNA_pointer_get(&vrayScene, "SettingsColorMapping");

			PointerRNA bitmapBuffer = RNA_pointer_get(&node.ptr, "BitmapBuffer");
			bool use_input_gamma = RNA_boolean_get(&bitmapBuffer, "use_input_gamma") &&
								   RNA_boolean_get(&settingsColorMapping, "use_input_gamma");
			if(use_input_gamma) {
				attrs["gamma"] = BOOST_FORMAT_FLOAT(RNA_float_get(&settingsColorMapping, "input_gamma"));
				attrs["color_space"] = "1";

				PRINT_INFO_EX("Node tree: %s => Node name: %s => \"Use Input Gamma\" is used. "
							  "\"Color Space\" is forced to \"Gamma Corrected\"",
							  ntree.name().c_str(), node.name().c_str());
			}
		}
	}
}


std::string VRayNodeExporter::exportVRayNodeBitmapBuffer(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	AttributeValueMap bitmapAttrs;
	exportBitmapBuffer(ntree, node, fromSocket, context, bitmapAttrs);

	if (bitmapAttrs.size()) {
		return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, bitmapAttrs);
	}
	else {
		PRINT_ERROR("Node tree: %s => Node name: %s => Something wrong with BitmapBuffer!",
					ntree.name().c_str(), node.name().c_str());
	}

	return "NULL";
}
