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


std::string VRayNodeExporter::exportVRayNodeTexSky(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	AttributeValueMap  attrs;

	BL::NodeSocket sunSock = VRayNodeExporter::getSocketByName(node, "Sun");
	if (sunSock && sunSock.is_linked()) {
		BL::Node conNode = VRayNodeExporter::getConnectedNode(sunSock);
		if (conNode) {
			if (NOT(conNode.bl_idname() == "VRayNodeSelectObject")) {
				PRINT_ERROR("Sun node could be selected only with \"Select Object\" node.");
			}
			else {
				BL::Object sunOb = VRayNodeExporter::exportVRayNodeSelectObject(ntree, conNode, sunSock, context);
				if (sunOb && sunOb.type() == BL::Object::type_LAMP) {
					attrs["sun"] = GetIDName(sunOb, "LA");
				}
			}
		}
	}
	else {
		BL::Scene::objects_iterator obIt;
		for (ExporterSettings::gSet.b_scene.objects.begin(obIt); obIt != ExporterSettings::gSet.b_scene.objects.end(); ++obIt) {
			BL::Object ob = *obIt;
			if (ob.type() == BL::Object::type_LAMP) {
				BL::ID laID = ob.data();
				if (laID) {
					BL::Lamp la(laID);
					if (la.type() == BL::Lamp::type_SUN) {
						PointerRNA vrayLight = RNA_pointer_get(&la.ptr, "vray");
						const int direct_type = RNA_enum_get(&vrayLight, "direct_type");
						if (direct_type == 1) {
							attrs["sun"] = GetIDName(ob, "LA");
							break;
						}
					}
				}
			}
		}
	}

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, attrs);
}


std::string VRayNodeExporter::exportVRayNodeTexFalloff(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	AttributeValueMap falloffTexAttrs;

	BL::NodeSocket blendInputSock = VRayNodeExporter::getSocketByAttr(node, "blend_input");
	BL::Node       blendInputNode = VRayNodeExporter::getConnectedNode(blendInputSock, context);
	if (blendInputNode && blendInputNode.is_a(&RNA_ShaderNodeVectorCurve)) {
		const std::string &subFalloffTexName = "SubFalloff@" + VRayNodeExporter::getPluginName(node, ntree, context);
		AttributeValueMap  subFalloffTexAttrs(falloffTexAttrs);
		subFalloffTexAttrs["use_blend_input"] = "0";
		subFalloffTexAttrs["blend_input"]     = "NULL";

		VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, subFalloffTexAttrs, subFalloffTexName);

		StrVector points;
		StrVector types;
		getNodeVectorCurveData(blendInputNode, points, types);

		const std::string &texBezierCurveName = VRayNodeExporter::getPluginName(blendInputNode, ntree, context);
		AttributeValueMap texBezierCurveAttrs;
		texBezierCurveAttrs["input_float"] = subFalloffTexName + "::blend_output";
		texBezierCurveAttrs["points"] = BOOST_FORMAT_LIST_FLOAT(points);
		texBezierCurveAttrs["types"]  = BOOST_FORMAT_LIST_INT(types);

		VRayNodePluginExporter::exportPlugin("TEXTURE", "TexBezierCurve", texBezierCurveName, texBezierCurveAttrs);

		falloffTexAttrs["use_blend_input"] = "1";
		falloffTexAttrs["blend_input"]     = texBezierCurveName;
	}

	return VRayNodeExporter::exportVRayNodeAttributes(ntree, node, fromSocket, context, falloffTexAttrs);
}
