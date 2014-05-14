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

#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/join.hpp>

#include "exp_scene.h"

#include "CGR_string.h"
#include "CGR_rna.h"

#include <BLI_string.h>


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
	attrType == "BRDF"          || \
	attrType == "MATERIAL"      || \
	attrType == "GEOMETRY"      || \
	attrType == "PLUGIN"        || \
	attrType == "TEXTURE"       || \
	attrType == "FLOAT_TEXTURE" || \
	attrType == "INT_TEXTURE"   || \
	attrType == "VECTOR"        || \
	attrType == "UVWGEN"


namespace VRayScene {

struct VRayObjectContext {
	Scene       *sce;
	Main        *main;
	Object      *ob;

	std::string  mtlOverride;

};


class VRayNodeExporter {
public:
	static BL::NodeTree     getNodeTree(BL::BlendData b_data, ID *id);

	static BL::Node         getNodeByType(BL::NodeTree nodeTree, const std::string &nodeType);

	static BL::NodeSocket   getSocketByName(BL::Node node, const std::string &socketName);
	static BL::NodeSocket   getSocketByAttr(BL::Node node, const std::string &attrName);

	static BL::Node         getConnectedNode(BL::NodeTree nodeTree, BL::NodeSocket socket);
	static BL::Node         getConnectedNode(BL::NodeTree nodeTree, BL::Node node, const std::string &socketName);

	static BL::NodeSocket   getConnectedSocket(BL::NodeTree nodeTree, BL::NodeSocket socket);

	static std::string      exportVRayNodeBlenderOutputMaterial(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context);
	static std::string      exportVRayNodeBlenderOutputGeometry(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context);

	static std::string      exportVRayNodeLightMesh(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context);
	static std::string      exportVRayNodeGeomDisplacedMesh(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context);

	static std::string      exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node);
	static std::string      exportVRayNodeSelectGroup(BL::NodeTree ntree, BL::Node node);
	static std::string      exportVRayNodeSelectNodeTree(BL::NodeTree ntree, BL::Node node);

	static std::string      exportVRayNodeBRDFLayered(BL::NodeTree ntree, BL::Node node);
	static std::string      exportVRayNodeTexLayered(BL::NodeTree ntree, BL::Node node);
	static std::string      exportVRayNodeBitmapBuffer(BL::NodeTree ntree, BL::Node node);
	static std::string      exportVRayNodeTexGradRamp(BL::NodeTree ntree, BL::Node node);
	static std::string      exportVRayNodeTexRemap(BL::NodeTree ntree, BL::Node node);

	static std::string      exportVRayNodeGeneric(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context=NULL, const AttributeValueMap &manualAttrs=AttributeValueMap());
	// TODO: static std::string      exportVRayNodeGeneric(BL::NodeTree ntree, BL::NodeSocket socket, VRayObjectContext *context=NULL, const AttributeValueMap &manualAttrs=AttributeValueMap());

	static std::string      exportVRayNodeAttributes(BL::NodeTree ntree, BL::Node node, const AttributeValueMap &manualAttrs=AttributeValueMap());

	static std::string      exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayObjectContext *context=NULL);

	static ExpoterSettings *m_exportSettings;

private:
	static BL::Texture      getTextureFromIDRef(PointerRNA *ptr, const std::string &propName);

	static AttributeCache   m_attrCache;

}; // VRayNodesExporter

} // namespace VRayScene

#endif // CGR_EXPORT_NODES_H
