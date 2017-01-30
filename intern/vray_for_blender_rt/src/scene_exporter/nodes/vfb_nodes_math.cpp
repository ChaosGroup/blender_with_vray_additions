/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "vfb_node_exporter.h"
#include "vfb_utils_nodes.h"

#include "BLI_math.h"


using namespace VRayForBlender;


AttrValue DataExporter::exportVRayNodeTransform(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket&, NodeContext &context)
{
	float tm[4][4];

	BL::NodeSocket objectSock = Nodes::GetInputSocketByName(node, "Object");
	if (objectSock && objectSock.is_linked()) {
		BL::Node selectObjectNode = getConnectedNode(ntree, objectSock, context);
		if (selectObjectNode && (selectObjectNode.bl_idname() == "VRayNodeSelectObject")) {
			BL::Object b_ob = exportVRayNodeSelectObject(ntree, selectObjectNode, objectSock, context);
			if (b_ob) {
				memcpy(tm, b_ob.matrix_world().data, sizeof(float[4][4]));
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

	// NOTE: old exporter always inverts transform - do the same until we decide to fix
	const bool uiOptionInvert = RNA_boolean_get(&node.ptr, "invert");
	if (!uiOptionInvert) { // if ui opt is on we will invert twice as per NOTE above, so invert only when it is off
		float itm[4][4];
		invert_m4_m4(itm, tm);
		copy_m4_m4(tm, itm);
	}

	return AttrTransform(tm);
}


AttrValue DataExporter::exportVRayNodeMatrix(BL::NodeTree&, BL::Node &node, BL::NodeSocket&, NodeContext&)
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

	if (RNA_boolean_get(&node.ptr, "invert")) {
		float itm[4][4];
		invert_m4_m4(itm, tm);
		copy_m4_m4(tm, itm);
	}

	return AttrMatrix(tm);
}


AttrValue DataExporter::exportVRayNodeVector(BL::NodeTree&, BL::Node &node, BL::NodeSocket&, NodeContext&)
{
	float vector[3] = {0.0f, 0.0f, 0.0f};
	RNA_float_get_array(&node.ptr, "vector", vector);

	return AttrVector(vector);
}
