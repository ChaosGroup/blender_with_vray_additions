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

#include "Node.h"


std::string VRayNodeExporter::exportVRayNodeBlenderOutputMaterial(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context)
{
	if(NOT(context)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node context! Probably used in not suitable node tree type.",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	AttributeValueMap mtlMulti;
	std::string mtlName = Node::GetNodeMaterial(context->ob, context->mtlOverride, mtlMulti);

	// NOTE: Function could return only one material in 'mtlName'
	if(mtlMulti.find("mtls_list") == mtlMulti.end())
		return mtlName;

	std::string pluginName = StripString("NT" + ntree.name() + "N" + node.name());

#if 0
	// NOTE: INT_TEXTURE is not yet supported
	BL::NodeSocket mtlid_gen = getSocketByName(node, "ID Generator");
	if(mtlid_gen.is_linked()) {
		mtlMulti["mtlid_gen"] = exportLinkedSocket(ntree, mtlid_gen);
	}
#endif

	bool           has_mtlid_gen   = false;
	BL::NodeSocket mtlid_gen_float = getSocketByName(node, "ID Generator");

	// XXX: is_linked() crashing Blender is socket doesn't exist. Try to fix this.
	//
	if(mtlid_gen_float.is_linked()) {
		mtlMulti["mtlid_gen_float"] = exportLinkedSocket(ntree, mtlid_gen_float);
		has_mtlid_gen = true;
	}

	mtlMulti["wrap_id"] = boost::str(boost::format("%i") % RNA_int_get(&node.ptr, "wrap_id"));

	sstream plugin;

	plugin << "\n" << "MtlMulti" << " " << pluginName << " {";

	AttributeValueMap::const_iterator attrIt;
	for(attrIt = mtlMulti.begin(); attrIt != mtlMulti.end(); ++attrIt) {
		const std::string attrName  = attrIt->first;
		const std::string attrValue = attrIt->second;

		if(attrName == "ids_list" && has_mtlid_gen)
			continue;

		plugin << "\n\t" << attrName << "=" << VRayExportable::m_interpStart << attrValue << VRayExportable::m_interpEnd << ";";
	}

	plugin << "\n}\n";

	PYTHON_PRINT(VRayNodeExporter::m_exportSettings->m_fileObject, plugin.str().c_str());

	return pluginName;
}
