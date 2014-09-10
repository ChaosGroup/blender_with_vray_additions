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

#define MAX_NUM_POINTS 64

struct MyPoint {
	float x;
	float y;
};


void VRayNodeExporter::getNodeVectorCurveData(BL::Node node, StrVector &points, StrVector &types)
{
	BL::ShaderNodeVectorCurve curveNode(node);

	BL::CurveMapping curveMapping = curveNode.mapping();
	curveMapping.initialize();

	BL::CurveMap curve = curveMapping.curves[0];

	const int numPoints = curve.points.length();

	if (NOT(numPoints))
		return;

	MyPoint point[MAX_NUM_POINTS];

	BL::CurveMap::points_iterator pointIt;
	int p = 0;
	for (curve.points.begin(pointIt); pointIt != curve.points.end(); ++pointIt, ++p) {
		BL::CurveMapPoint mapPoint = *pointIt;

		BL::Array<float, 2> pointLoc = mapPoint.location();

		point[p].x = REMAP_CURVE_POINT(pointLoc[0]);
		point[p].y = REMAP_CURVE_POINT(pointLoc[1]);

		// The type of the control points:
		//   0 - free, 1 - bezier, 2 - bezier smooth
		//
		const int pointVRayType = (mapPoint.handle_type() == BL::CurveMapPoint::handle_type_AUTO) ? 1 : 0;

		types.push_back(BOOST_FORMAT_INT(pointVRayType));
	}

	float  deltaX[MAX_NUM_POINTS + 1];
	float  ySecon[MAX_NUM_POINTS];
	float  yPrim[MAX_NUM_POINTS];
	float  d[MAX_NUM_POINTS];
	float  w[MAX_NUM_POINTS];
	int    i;

	for(i = 1; i < numPoints; i++)
		deltaX[i] = point[i].x - point[i-1].x;
	deltaX[0] = deltaX[1];
	deltaX[numPoints] = deltaX[numPoints-1];
	for(i = 1; i < numPoints-1; i++) {
		d[i] = 2 * (point[i + 1].x - point[i - 1].x);
		w[i] = 6 * ((point[i + 1].y - point[i].y) / deltaX[i+1] - (point[i].y - point[i - 1].y) / deltaX[i]);
	}
	for(i = 1; i < numPoints-2; i++) {
		w[i + 1] -= w[i] * deltaX[i+1] / d[i];
		d[i + 1] -= deltaX[i+1] * deltaX[i+1] / d[i];
	}
	ySecon[0] = 0;
	ySecon[numPoints-1] = 0;
	for(i = numPoints - 2; i >= 1; i--)
		ySecon[i] = (w[i] - deltaX[i+1] * ySecon[i + 1]) / d[i];
	for(i = 0; i < numPoints-1; i++)
		yPrim[i] = (point[i+1].y - point[i].y) / deltaX[i+1] - (deltaX[i+1] / 6.0f) * (2 * ySecon[i] + ySecon[i+1]);
	yPrim[i] = (point[i].y - point[i-1].y) / deltaX[i] + (deltaX[i] / 6.0f) * ySecon[i-1];

	for(p = 0; p < numPoints; ++p) {
		points.push_back(BOOST_FORMAT_FLOAT(point[p].x));
		points.push_back(BOOST_FORMAT_FLOAT(point[p].y));
		points.push_back(BOOST_FORMAT_FLOAT((-deltaX[p] / 3)));
		points.push_back(BOOST_FORMAT_FLOAT((-deltaX[p] * yPrim[p] / 3)));
		points.push_back(BOOST_FORMAT_FLOAT((deltaX[p+1] / 3)));
		points.push_back(BOOST_FORMAT_FLOAT((deltaX[p+1] * yPrim[p] / 3)));
	}
}


std::string VRayNodeExporter::exportBlenderNodeNormal(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext *context)
{
	BL::NodeSocket socket = VRayNodeExporter::getOutputSocketByName(node, "Normal");

	float vector[3];
	RNA_float_get_array(&socket.ptr, "default_value", vector);

	return BOOST_FORMAT_VECTOR(vector);
}
