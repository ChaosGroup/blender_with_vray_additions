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


std::string VRayNodeExporter::exportVRayNodeTransform(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	float tm[4][4];
	float itm[4][4];

	BL::NodeSocket objectSock = VRayNodeExporter::getSocketByName(node, "Object");
	if(objectSock && objectSock.is_linked()) {
		BL::Node selectObjectNode = VRayNodeExporter::getConnectedNode(objectSock, context);
		if(selectObjectNode && (selectObjectNode.bl_idname() == "VRayNodeSelectObject")) {
			BL::Object b_ob = VRayNodeExporter::exportVRayNodeSelectObject(ntree, selectObjectNode, objectSock, context);
			if(b_ob) {
				Object *ob = (Object*)b_ob.ptr.data;

				copy_m4_m4(tm, ob->obmat);
			}
		}
	}
	else {
		float rot[3]  = {0.0f, 0.0f, 0.0f};
		float sca[3]  = {0.0f, 0.0f, 0.0f};
		float offs[3] = {0.0f, 0.0f, 0.0f};

		float mSca[4][4];
		float mRot[4][4];

		unit_m4(mSca);
		unit_m4(mRot);

		RNA_float_get_array(&node.ptr, "rotate", rot);
		RNA_float_get_array(&node.ptr, "scale",  sca);
		RNA_float_get_array(&node.ptr, "offset", offs);

		size_to_mat4(mSca, sca);

		rotate_m4(mRot, 'X', rot[0]);
		rotate_m4(mRot, 'Y', rot[1]);
		rotate_m4(mRot, 'Z', rot[2]);

		mul_m4_m4m4(tm, mRot, mSca);

		copy_v3_v3(tm[3], offs);
	}

	invert_m4_m4(itm, tm);

	char tmHex[CGR_TRANSFORM_HEX_SIZE];
	GetTransformHex(itm, tmHex);

	return BOOST_FORMAT_TM(tmHex);
}


std::string VRayNodeExporter::exportVRayNodeMatrix(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	float rot[3] = {0.0f, 0.0f, 0.0f};
	float sca[3] = {0.0f, 0.0f, 0.0f};

	RNA_float_get_array(&node.ptr, "rotate", rot);
	RNA_float_get_array(&node.ptr, "scale",  sca);

	float mSca[4][4];
	float mRot[4][4];
	float tm[4][4];

	unit_m4(mSca);
	unit_m4(mRot);

	size_to_mat4(mSca, sca);

	rotate_m4(mRot, 'X', rot[0]);
	rotate_m4(mRot, 'Y', rot[1]);
	rotate_m4(mRot, 'Z', rot[2]);

	mul_m4_m4m4(tm, mRot, mSca);

	return BOOST_FORMAT_MATRIX(tm);
}


std::string VRayNodeExporter::exportVRayNodeVector(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	float vector[3] = {0.0f, 0.0f, 0.0f};

	RNA_float_get_array(&node.ptr, "vector", vector);

	return BOOST_FORMAT_VECTOR(vector);
}
