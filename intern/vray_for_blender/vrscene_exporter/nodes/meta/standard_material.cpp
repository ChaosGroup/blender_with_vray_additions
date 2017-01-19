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


std::string VRayNodeExporter::exportVRayNodeMetaStandardMaterial(VRayNodeExportParam)
{
	const std::string &baseName = VRayNodeExporter::getPluginName(node, ntree, context);

	// BRDFVRayMtl
	//
	const std::string &brdfVRayMtlName = "BRDFVRayMtl@" + baseName;
	AttributeValueMap brdfVRayMtl;
	VRayNodeExporter::getVRayNodeAttributes(brdfVRayMtl, ntree, node, fromSocket, context, AttributeValueMap(), "BRDFVRayMtl");

	PointerRNA brdfVRayMtlPtr = RNA_pointer_get(&node.ptr, "BRDFVRayMtl");
	if (RNA_boolean_get(&brdfVRayMtlPtr, "hilight_glossiness_lock")) {
		brdfVRayMtl["hilight_glossiness"] = brdfVRayMtl["reflect_glossiness"];
	}

	exportVRayNodeAttributes(ntree, node, fromSocket, context,
	                         brdfVRayMtl,
	                         brdfVRayMtlName,
	                         "BRDFVRayMtl",
	                         "BRDF");

	// Material BRDF
	std::string materialBrdf = brdfVRayMtlName;

	// BRDFBump
	//
	BL::NodeSocket sockBump   = VRayNodeExporter::getSocketByAttr(node, "bump_tex_float");
	BL::NodeSocket sockNormal = VRayNodeExporter::getSocketByAttr(node, "bump_tex_color");
	const bool useBump = (sockBump && sockBump.is_linked()) || (sockNormal && sockNormal.is_linked());
	if (useBump) {
		const std::string &brdfBumpName = "BRDFBump@" + baseName;

		AttributeValueMap bumpPluginAttrs;
		bumpPluginAttrs["base_brdf"] = brdfVRayMtlName;

		VRayNodeExporter::getVRayNodeAttributes(bumpPluginAttrs, ntree, node, fromSocket, context, AttributeValueMap(), "BRDFBump", "BRDF");

		if (sockBump && sockBump.is_linked()) {
			bumpPluginAttrs.erase("bump_tex_color");
		}
		else {
			bumpPluginAttrs.erase("bump_tex_float");
		}

		VRayNodePluginExporter::exportPlugin("BRDF", "BRDFBump", brdfBumpName, bumpPluginAttrs);

		materialBrdf = brdfBumpName;
	}

	const std::string &fromSocketType = fromSocket.rna_type().identifier();

	// If this node is connected as BRDF then skip material plugins export
	if (fromSocketType == "VRaySocketBRDF") {
		return materialBrdf;
	}

	// MtlSingleBRDF
	//
	const std::string &mtlSingleBrdfName = "MtlSingleBRDF@" + baseName;
	AttributeValueMap mtlSingleBrdf;
	mtlSingleBrdf["brdf"] = materialBrdf;

	exportVRayNodeAttributes(ntree, node, fromSocket, context,
	                         mtlSingleBrdf,
	                         mtlSingleBrdfName,
	                         "MtlSingleBRDF",
	                         "MATERIAL");

	// MtlMaterialID
	//
	AttributeValueMap mtlMaterialId;
	mtlMaterialId["base_mtl"] = mtlSingleBrdfName;

	exportVRayNodeAttributes(ntree, node, fromSocket, context,
	                         mtlMaterialId,
	                         baseName,
	                         "MtlMaterialID",
	                         "MATERIAL");

	return baseName;
}
