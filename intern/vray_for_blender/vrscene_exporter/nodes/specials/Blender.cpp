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

// Remap values from [-1.0 .. 1.0] to [0.0 .. 1.0]
#define REMAP_CURVE_POINT(x) (0.5f * ((x) + 1.0f))


void VRayNodeExporter::getNodeVectorCurveData(BL::Node node, StrVector &points, StrVector &types)
{
	BL::ShaderNodeVectorCurve curveNode(node);

	BL::CurveMapping curveMapping = curveNode.mapping();
	curveMapping.initialize();

	BL::CurveMap curve = curveMapping.curves[0];
	if (NOT(curve.points.length()))
		return;

	BL::CurveMap::points_iterator pointIt;
	for (curve.points.begin(pointIt); pointIt != curve.points.end(); ++pointIt) {
		BL::CurveMapPoint point = *pointIt;

		BL::Array<float, 2>                  pointLoc  = point.location();
		BL::CurveMapPoint::handle_type_enum  pointType = point.handle_type();

		int pointVRayType = 0;
		if (pointType == BL::CurveMapPoint::handle_type_AUTO)
			pointVRayType = 1;

		const float tanDelta = 0.001;

		const float curvePointLocX = pointLoc[0];
		const float curvePointLocY = pointLoc[1];

		const float curveTan1LocX = curvePointLocX - tanDelta;
		const float curveTan1LocY = curve.evaluate(curveTan1LocX);

		const float curveTan2LocX = curvePointLocX + tanDelta;
		const float curveTan2LocY = curve.evaluate(curveTan2LocX);

		const float pointLocX = REMAP_CURVE_POINT(curvePointLocX);
		const float pointLocY = REMAP_CURVE_POINT(curvePointLocY);

		// Left handle
		const float tan1LocX = REMAP_CURVE_POINT(curveTan1LocX);
		const float tan1LocY = REMAP_CURVE_POINT(curveTan1LocY);

		// Right handle
		const float tan2LocX = REMAP_CURVE_POINT(curveTan2LocX);
		const float tan2LocY = REMAP_CURVE_POINT(curveTan2LocY);

		// The control points;
		// 6 floats for each point: 2D coords of the point itself,
		// and 2D coords of the left and right tangent.
		points.push_back(BOOST_FORMAT_FLOAT(pointLocX));
		points.push_back(BOOST_FORMAT_FLOAT(pointLocY));
		points.push_back(BOOST_FORMAT_FLOAT(tan1LocX));
		points.push_back(BOOST_FORMAT_FLOAT(tan1LocY));
		points.push_back(BOOST_FORMAT_FLOAT(tan2LocX));
		points.push_back(BOOST_FORMAT_FLOAT(tan2LocY));

		types.push_back(BOOST_FORMAT_INT(pointVRayType));
	}
}


std::string VRayNodeExporter::exportBlenderNodeNormal(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	BL::NodeSocket socket = VRayNodeExporter::getOutputSocketByName(node, "Normal");

	float vector[3];
	RNA_float_get_array(&socket.ptr, "default_value", vector);

	return BOOST_FORMAT_VECTOR(vector);
}
