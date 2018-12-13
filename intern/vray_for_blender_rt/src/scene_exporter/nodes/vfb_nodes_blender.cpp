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
#include "vfb_utils_blender.h"


using namespace VRayForBlender;

#define MAX_NUM_POINTS 64


struct MyPoint {
	float x;
	float y;
};


void DataExporter::fillNodeVectorCurveData(BL::NodeTree ntree, BL::Node node, AttrListFloat &points, AttrListInt &types)
{
	BL::ShaderNodeVectorCurve curveNode(node);

	BL::CurveMapping curveMapping = curveNode.mapping();
	{
		SCOPED_TRACE_EX("Waiting for WRITE_LOCK_BLENDER for CurveMapping for (%s)", node.name().c_str());
		WRITE_LOCK_BLENDER_RAII
		curveMapping.update();
	}

	BL::CurveMap curve = curveMapping.curves[0];
	if (!curve) {
		getLog().error("Node tree: %s => Node name: %s => Incorrect curve!",
		            ntree.name().c_str(), node.name().c_str());
	}
	else {
		const int numPoints = curve.points.length();
		if (numPoints) {
			// NOTE: point (x,y) + 2 * handle (x,y) = 6 float
			points.resize(numPoints * 6);
			types.resize(numPoints);

			MyPoint point[MAX_NUM_POINTS];

			BL::CurveMap::points_iterator pointIt;
			int p = 0;
			for (curve.points.begin(pointIt); pointIt != curve.points.end(); ++pointIt, ++p) {
				BL::CurveMapPoint mapPoint(*pointIt);

				BlVector2 pointLoc = mapPoint.location();

				point[p].x = pointLoc[0];
				point[p].y = pointLoc[1];

				// The type of the control points:
				//   0 - free, 1 - bezier, 2 - bezier smooth
				//
				const int pointVRayType = (mapPoint.handle_type() == BL::CurveMapPoint::handle_type_AUTO) ? 1 : 0;

				(*types)[p] = pointVRayType;
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

			i = 0;
			for (p = 0; p < numPoints; ++p) {
				(*points)[i++] =  point[p].x;
				(*points)[i++] =  point[p].y;
				(*points)[i++] = -deltaX[p] / 3;
				(*points)[i++] = -deltaX[p] * yPrim[p] / 3;
				(*points)[i++] =  deltaX[p+1] / 3;
				(*points)[i++] =  deltaX[p+1] * yPrim[p] / 3;
			}
		}
	}
}


AttrValue DataExporter::exportBlenderNodeNormal(BL::NodeTree&, BL::Node &node, BL::NodeSocket&, NodeContext&)
{
	BL::NodeSocket socket = Nodes::GetOutputSocketByName(node, "Normal");

	float vector[3];
	RNA_float_get_array(&socket.ptr, "default_value", vector);

	return AttrVector(vector);
}
