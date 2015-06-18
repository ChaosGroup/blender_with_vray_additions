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

#include "vfb_plugin_attrs.h"
#include "vfb_utils_math.h"

#include "BLI_math.h"


using namespace VRayForBlender;


BlTransform VRayForBlender::Math::InvertTm(const BlTransform &tm)
{
	BlTransform itm;

	float m[4][4];
	memcpy(&m, &tm.data, sizeof(float[4][4]));

	invert_m4(m);
	memcpy(&itm.data, m, sizeof(float[4][4]));

	return itm;
}


float VRayForBlender::Math::GetDistanceTmTm(BlTransform a, BlTransform b)
{
	AttrVector a_offs(&a.data[12]);
	AttrVector b_offs(&b.data[12]);
	AttrVector d = a_offs - b_offs;
	return d.len();
}
