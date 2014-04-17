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

#ifndef CGR_EXPORT_NODES_H
#define CGR_EXPORT_NODES_H

#include "exp_scene.h"

#define SKIP_TYPE(attrType) \
	attrType == "LIST"              || \
	attrType == "INT_LIST"          || \
	attrType == "MATRIX"            || \
	attrType == "TRANSFORM"         || \
	attrType == "TRANSFORM_TEXTURE"

#define OUTPUT_TYPE(attrType) \
	attrType == "OUTPUT_PLUGIN"            || \
	attrType == "OUTPUT_COLOR"             || \
	attrType == "OUTPUT_FLOAT_TEXTURE"     || \
	attrType == "OUTPUT_VECTOR_TEXTURE"    || \
	attrType == "OUTPUT_TRANSFORM_TEXTURE" || \
	attrType == "OUTPUT_TEXTURE"

#define MAPPABLE_TYPE(attrType) \
	attrType == "BRDF"     || \
	attrType == "MATERIAL" || \
	attrType == "PLUGIN"   || \
	attrType == "TEXTURE"  || \
	attrType == "UVWGEN"


namespace VRayScene {

class VRayNodeExporter {
public:
	static std::string exportVRayNodeBlenderOutputMaterial(BL::NodeTree ntree, BL::Node node);
	static std::string exportVRayNodeBlenderOutputGeometry(BL::NodeTree ntree, BL::Node node);
	static std::string exportVRayNodeBRDFLayered(BL::NodeTree ntree, BL::Node node);
	static std::string exportVRayNodeTexLayered(BL::NodeTree ntree, BL::Node node);
	static std::string exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node);
	static std::string exportVRayNodeSelectGroup(BL::NodeTree ntree, BL::Node node);
}; // VRayNodesExporter

} // namespace VRayScene

#endif // CGR_EXPORT_NODES_H
