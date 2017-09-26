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

#ifndef VRAY_FOR_BLENDER_UTILS_MATH_H
#define VRAY_FOR_BLENDER_UTILS_MATH_H

#include "vfb_rna.h"

#include <cmath>
#include <algorithm>
#include <string>


#define DEG_TO_RAD(d) (d * M_PI / 180.0f)


namespace VRayForBlender {
namespace Math {

BlTransform InvertTm(const BlTransform &tm);
float       GetDistanceTmTm(BlTransform a, BlTransform b);

template <typename T>
T clamp(T value, T min, T max) {
	if (value > max) {
		return max;
	} else if (value < min) {
		return min;
	} else {
		return value;
	}
}

template <typename T>
T Abs(T value) {
	return value < 0 ? -value : value;
}

template <typename T>
bool isZero(T value, T eps) {
	return abs(value) < Max( Abs( value ) * eps, eps );
}

inline bool floatEqual(float a, float b) {
	const float diff = fabs(a - b);
	const float eps = 1e-6f;
	return diff < std::max<float>(eps * diff, eps);
}

} // namespace Math
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_MATH_H
