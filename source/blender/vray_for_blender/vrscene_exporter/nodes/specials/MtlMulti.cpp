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


std::string VRayNodeExporter::exportVRayNodeMtlMulti(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	std::string pluginName = VRayNodeExporter::getPluginName(node, ntree, context);

	StrVector mtls_list;
	StrVector ids_list;

	BL::NodeSocket mtlid_gen_sock  = VRayNodeExporter::getSocketByAttr(node, "mtlid_gen");
	BL::NodeSocket mtlid_gen_float_sock = VRayNodeExporter::getSocketByAttr(node, "mtlid_gen_float");

	for(int i = 1; i <= CGR_MAX_LAYERED_BRDFS; ++i) {
		std::string mtlSockName = boost::str(boost::format("Material %i") % i);

		BL::NodeSocket mtlSock = VRayNodeExporter::getSocketByName(node, mtlSockName);
		if(NOT(mtlSock))
			continue;

		if(NOT(mtlSock.is_linked()))
			continue;

		std::string material   = VRayNodeExporter::exportLinkedSocket(ntree, mtlSock, context);
		std::string materialID = BOOST_FORMAT_INT(RNA_int_get(&mtlSock.ptr, "value"));

		mtls_list.push_back(material);
		ids_list.push_back(materialID);
	}

	AttributeValueMap pluginAttrs;
	pluginAttrs["mtls_list"] = BOOST_FORMAT_LIST(mtls_list);
	pluginAttrs["ids_list"]  = BOOST_FORMAT_LIST_INT(ids_list);
	if(mtlid_gen_sock.is_linked()) {
		pluginAttrs["mtlid_gen"] = VRayNodeExporter::exportLinkedSocket(ntree, mtlid_gen_sock, context);
	}
	else if(mtlid_gen_float_sock.is_linked()) {
		pluginAttrs["mtlid_gen_float"] = VRayNodeExporter::exportLinkedSocket(ntree, mtlid_gen_float_sock, context);
	}
	pluginAttrs["wrap_id"] = BOOST_FORMAT_INT(RNA_boolean_get(&node.ptr, "wrap_id"));

	VRayNodePluginExporter::exportPlugin("MATERIAL", "MtlMulti", pluginName, pluginAttrs);

	return pluginName;
}


std::string VRayNodeExporter::exportMtlMulti(BL::BlendData bl_data, BL::Object bl_ob)
{
	std::string pluginName = "MtlMulti@" + GetIDName(bl_ob);

	StrVector mtls_list;
	StrVector ids_list;

	BL::Object::material_slots_iterator maSlotIt;
	int i = 0;
	for (bl_ob.material_slots.begin(maSlotIt); maSlotIt != bl_ob.material_slots.end(); ++maSlotIt, ++i) {
		BL::MaterialSlot maSlot = *maSlotIt;
		if (NOT(maSlot))
			continue;

		BL::Material ma = maSlot.material();
		if (NOT(ma))
			continue;

		mtls_list.push_back(Node::GetMaterialName((Material*)ma.ptr.data, m_set->m_mtlOverride));
		ids_list.push_back(BOOST_FORMAT_INT(i));
	}

	if (mtls_list.size() == 0)
		return "NULL";
	else if (mtls_list.size() == 1)
		return mtls_list[0];

	AttributeValueMap pluginAttrs;
	pluginAttrs["mtls_list"] = BOOST_FORMAT_LIST(mtls_list);
	pluginAttrs["ids_list"]  = BOOST_FORMAT_LIST_INT(ids_list);
	pluginAttrs["wrap_id"] = "1";

	VRayNodePluginExporter::exportPlugin("Node", "MtlMulti", pluginName, pluginAttrs);

	return pluginName;
}
