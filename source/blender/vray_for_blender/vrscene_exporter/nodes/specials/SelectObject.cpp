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


BL::Object VRayNodeExporter::exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	char buf[MAX_ID_NAME] = "";
	RNA_string_get(&node.ptr, "objectName", buf);

	std::string objectName = buf;

	if(NOT(objectName.empty())) {
		BL::BlendData b_data = m_set->b_data;
		
		BL::BlendData::objects_iterator obIt;
		for(b_data.objects.begin(obIt); obIt != b_data.objects.end(); ++obIt) {
			BL::Object b_ob = *obIt;
			if(b_ob.name() == buf)
				return b_ob;
		}
	}

	return BL::Object(PointerRNA_NULL);
}
