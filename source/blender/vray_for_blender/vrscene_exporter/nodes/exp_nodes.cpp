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


ExpoterSettings *VRayNodeExporter::m_set = NULL;

VRayNodeCache    VRayNodePluginExporter::m_nodeCache;
StrSet           VRayNodePluginExporter::m_namesCache;


void VRayNodeExporter::getAttributesList(const std::string &pluginID, StrSet &attrSet, bool mappable)
{
	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);
	if(NOT(pluginDesc)) {
		PRINT_ERROR("Plugin '%s' description is not found!",
					pluginID.c_str());
		return;
	}

	BOOST_FOREACH(PluginJson::value_type &v, pluginDesc->get_child("Parameters")) {
		std::string attrName = v.second.get_child("attr").data();
		std::string attrType = v.second.get_child("type").data();

		if(SKIP_TYPE(attrType))
			continue;

		if(v.second.count("skip"))
			if(v.second.get<bool>("skip"))
				continue;

		if(mappable && MAPPABLE_TYPE(attrType)) {
			attrSet.insert(attrName);
		}
		else {
			attrSet.insert(attrName);
		}
	}
}


std::string VRayNodeExporter::getValueFromPropGroup(PointerRNA *propGroup, ID *holder, const std::string &attrName)
{
	PropertyRNA *prop = RNA_struct_find_property(propGroup, attrName.c_str());
	if(NOT(prop))
		return "NULL";

	PropertyType propType = RNA_property_type(prop);

	if(propType == PROP_STRING) {
		char value[FILE_MAX] = "";
		RNA_string_get(propGroup, attrName.c_str(), value);

		if(strlen(value) == 0)
			return "NULL";

		PropertySubType propSubType = RNA_property_subtype(prop);
		if(propSubType == PROP_FILEPATH || propSubType == PROP_DIRPATH) {
			BLI_path_abs(value, ID_BLEND_PATH(G.main, holder));
		}

		return BOOST_FORMAT_STRING(value);
	}
	else if(propType == PROP_BOOLEAN) {
		return BOOST_FORMAT_BOOL(RNA_boolean_get(propGroup, attrName.c_str()));
	}
	else if(propType == PROP_INT) {
		return BOOST_FORMAT_INT(RNA_int_get(propGroup, attrName.c_str()));
	}
	else if(propType == PROP_ENUM) {
		return BOOST_FORMAT_INT(RNA_enum_get(propGroup, attrName.c_str()));
	}
	else if(propType == PROP_FLOAT) {
		if(NOT(RNA_property_array_check(prop))) {
			return BOOST_FORMAT_FLOAT(RNA_float_get(propGroup, attrName.c_str()));
		}
		else {
			PropertySubType propSubType = RNA_property_subtype(prop);
			if(propSubType == PROP_COLOR) {
				if(RNA_property_array_length(propGroup, prop) == 4) {
					float acolor[4];
					RNA_float_get_array(propGroup, attrName.c_str(), acolor);
					return BOOST_FORMAT_ACOLOR(acolor);
				}
				else {
					float color[3];
					RNA_float_get_array(propGroup, attrName.c_str(), color);
					return BOOST_FORMAT_COLOR(color);
				}
			}
			else {
				float vector[3];
				RNA_float_get_array(propGroup, attrName.c_str(), vector);
				return BOOST_FORMAT_VECTOR(vector);
			}
		}
	}
	else {
		PRINT_ERROR("Property '%s': Unsupported property type '%i'.", RNA_property_identifier(prop), propType);
	}

	return "NULL";
}


BL::NodeSocket VRayNodeExporter::getSocketByName(BL::Node node, const std::string &socketName)
{
	BL::Node::inputs_iterator input;
	for(node.inputs.begin(input); input != node.inputs.end(); ++input)
		if(input->name() == socketName)
			return *input;

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::NodeSocket VRayNodeExporter::getSocketByAttr(BL::Node node, const std::string &attrName)
{
	char rnaStringBuf[CGR_MAX_PLUGIN_NAME];

	BL::Node::inputs_iterator sockIt;
	for(node.inputs.begin(sockIt); sockIt != node.inputs.end(); ++sockIt) {
		std::string sockAttrName;
		if(RNA_struct_find_property(&sockIt->ptr, "vray_attr")) {
			RNA_string_get(&sockIt->ptr, "vray_attr", rnaStringBuf);
			sockAttrName = rnaStringBuf;
		}

		if(sockAttrName.empty())
			continue;

		if(attrName == sockAttrName)
			return *sockIt;
	}

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::Node VRayNodeExporter::getNodeByType(BL::NodeTree nodeTree, const std::string &nodeType)
{
	BL::NodeTree::nodes_iterator node;
	for(nodeTree.nodes.begin(node); node != nodeTree.nodes.end(); ++node)
		if(node->rna_type().identifier() == nodeType)
			return *node;

	return BL::Node(PointerRNA_NULL);
}


BL::Node VRayNodeExporter::getConnectedNode(BL::NodeTree nodeTree, BL::NodeSocket socket)
{
	// XXX: For some reason Python's Node has 'links' attribute,
	// but here we have to iterate thought tree's 'links'
	//
	BL::NodeTree::links_iterator link;
	for(nodeTree.links.begin(link); link != nodeTree.links.end(); ++link)
		if(socket.ptr.data == link->to_socket().ptr.data)
			return link->from_node();

	return BL::Node(PointerRNA_NULL);
}


BL::NodeSocket VRayNodeExporter::getConnectedSocket(BL::NodeTree nodeTree, BL::NodeSocket socket)
{
	BL::NodeTree::links_iterator link;
	for(nodeTree.links.begin(link); link != nodeTree.links.end(); ++link)
		if(socket.ptr.data == link->to_socket().ptr.data)
			return link->from_socket();

	return BL::NodeSocket(PointerRNA_NULL);
}


BL::Node VRayNodeExporter::getConnectedNode(BL::NodeTree nodeTree, BL::Node node, const std::string &socketName)
{
	BL::NodeSocket socket = getSocketByName(node, socketName);
	if(NOT(socket.is_linked()))
		return BL::Node(PointerRNA_NULL);

	return getConnectedNode(nodeTree, socket);
}



BL::NodeTree VRayNodeExporter::getNodeTree(BL::BlendData b_data, ID *id)
{
	RnaAccess::RnaValue VRayObject(id, "vray");

#if CGR_NTREE_DRIVER
	if(VRayObject.hasProperty("ntree__enum__")) {
		int ntreePtr = VRayObject.getEnum("ntree__enum__");
		if(ntreePtr != -1) {
			bNodeTree *ntree = (bNodeTree*)(intptr_t)ntreePtr;
			PointerRNA ntreeRNA;
			RNA_id_pointer_create((ID*)(&ntree->id), &ntreeRNA);
			return BL::NodeTree(ntreePtr);
		}
	}
#else
	if(VRayObject.hasProperty("ntree__name__")) {
		std::string ntreeName = VRayObject.getString("ntree__name__");
		if(NOT(ntreeName.empty())) {
			PRINT_INFO("ID: '%s' -> node tree is '%s'", id->name, ntreeName.c_str());

			BL::BlendData::node_groups_iterator nodeGroupIt;
			for(b_data.node_groups.begin(nodeGroupIt); nodeGroupIt != b_data.node_groups.end(); ++nodeGroupIt) {
				BL::NodeTree nodeTree = *nodeGroupIt;
				if(nodeTree.name() == ntreeName) {
					return nodeTree;
				}
			}
		}
	}
#endif

	return BL::NodeTree(PointerRNA_NULL);
}


BL::Texture VRayNodeExporter::getTextureFromIDRef(PointerRNA *ptr, const std::string &propName)
{
#if CGR_NTREE_DRIVER
#else
	if(RNA_struct_find_property(ptr, propName.c_str())) {
		char textureName[MAX_ID_NAME];
		RNA_string_get(ptr, propName.c_str(), textureName);

		BL::BlendData b_data = VRayNodeExporter::m_set->b_data;

		BL::BlendData::textures_iterator texIt;
		for(b_data.textures.begin(texIt); texIt != b_data.textures.end(); ++texIt) {
			BL::Texture b_tex = *texIt;
			if(b_tex.name() == textureName)
				return b_tex;
		}
	}
#endif

	return BL::Texture(PointerRNA_NULL);
}


std::string VRayNodeExporter::exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayObjectContext *context)
{
	BL::Node       conNode = VRayNodeExporter::getConnectedNode(ntree, socket);
	BL::NodeSocket conSock = VRayNodeExporter::getConnectedSocket(ntree, socket);

	std::string connectedPlugin = VRayNodeExporter::exportVRayNode(ntree, conNode, context);

	std::string conSockAttrName;
	if(RNA_struct_find_property(&conSock.ptr, "vray_attr")) {
		char rnaStringBuf[CGR_MAX_PLUGIN_NAME];
		RNA_string_get(&conSock.ptr, "vray_attr", rnaStringBuf);
		conSockAttrName = rnaStringBuf;
	}

	if(NOT(conSockAttrName.empty())) {
		if(NOT(conSockAttrName == "uvwgen" || conSockAttrName == "bitmap")) {
			connectedPlugin += "::" + conSockAttrName;
		}
	}

	return connectedPlugin;
}


std::string VRayNodeExporter::exportDefaultSocket(BL::NodeTree ntree, BL::NodeSocket socket)
{
	std::string socketVRayType = socket.rna_type().identifier();

	if(socketVRayType == "VRaySocketColor") {
		float color[3];
		RNA_float_get_array(&socket.ptr, "value", color);
		return BOOST_FORMAT_ACOLOR3(color);
	}
	else if(socketVRayType == "VRaySocketFloatColor") {
		return BOOST_FORMAT_FLOAT(RNA_float_get(&socket.ptr, "value"));
	}
	else if(socketVRayType == "VRaySocketInt") {
		return BOOST_FORMAT_INT(RNA_int_get(&socket.ptr, "value"));
	}
	else if(socketVRayType == "VRaySocketFloat") {
		return BOOST_FORMAT_FLOAT(RNA_float_get(&socket.ptr, "value"));
	}
	else if(socketVRayType == "VRaySocketVector") {
		float vector[3];
		RNA_float_get_array(&socket.ptr, "value", vector);
		return BOOST_FORMAT_VECTOR(vector);
	}
	else if(socketVRayType == "VRaySocketBRDF") {
		PRINT_ERROR("Node tree: %s => Node name: %s => Mandatory socket of type '%s' is not linked!",
					ntree.name().c_str(), socket.node().name().c_str(), socketVRayType.c_str());
	}
	// These sockets do not have default value, they must be linked or skipped otherwise.
	//
	else if(socketVRayType == "VRaySocketFloatNoValue") {}
	else if(socketVRayType == "VRaySocketColorNoValue") {}
	else if(socketVRayType == "VRaySocketCoords") {}
	else if(socketVRayType == "VRaySocketObject") {}
	else {
		PRINT_ERROR("Node tree: %s => Node name: %s => Unsupported socket type: %s",
					ntree.name().c_str(), socket.node().name().c_str(), socketVRayType.c_str());
	}

	return "NULL";
}


std::string VRayNodeExporter::exportSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayObjectContext *context)
{
	if(socket.is_linked())
		return VRayNodeExporter::exportLinkedSocket(ntree, socket, context);

	return VRayNodeExporter::exportDefaultSocket(ntree, socket);
}


std::string VRayNodeExporter::exportSocket(BL::NodeTree ntree, BL::Node node, const std::string &socketName, VRayObjectContext *context)
{
	BL::NodeSocket socket = VRayNodeExporter::getSocketByName(node, socketName);
	return VRayNodeExporter::exportSocket(ntree, socket, context);
}


std::string VRayNodeExporter::exportVRayNodeAttributes(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context, const AttributeValueMap &manualAttrs)
{
	std::string pluginType;
	std::string pluginID;

	char rnaStringBuf[CGR_MAX_PLUGIN_NAME];

	if(RNA_struct_find_property(&node.ptr, "vray_type")) {
		RNA_string_get(&node.ptr, "vray_type", rnaStringBuf);
		pluginType = rnaStringBuf;
	}

	if(RNA_struct_find_property(&node.ptr, "vray_plugin")) {
		RNA_string_get(&node.ptr, "vray_plugin", rnaStringBuf);
		pluginID = rnaStringBuf;
	}

	if(pluginID.empty()) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Incorrect node plugin ID!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	if(NOT(RNA_struct_find_property(&node.ptr, pluginID.c_str()))) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Property group not found!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	PointerRNA propGroup = RNA_pointer_get(&node.ptr, pluginID.c_str());

	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);
	if(NOT(pluginDesc)) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Node is not supported!",
					ntree.name().c_str(), node.name().c_str());
		return "NULL";
	}

	PRINT_INFO("  Node represents: type = \"%s\" plugin = \"%s\"", pluginType.c_str(), pluginID.c_str());

	std::string        pluginName = StripString("NT" + ntree.name() + "N" + node.name());
	AttributeValueMap  pluginAttrs;

	// VRayNodeContext nodeContext(ntree, node);
	// VRayNodeExporter::getAttrsFromPropGroupAndNode(pluginAttrs, pluginID, propGroup, manualAttrs, nodeContext);

	BOOST_FOREACH(PluginJson::value_type &v, pluginDesc->get_child("Parameters")) {
		std::string attrName = v.second.get_child("attr").data();
		std::string attrType = v.second.get_child("type").data();

		// Skip attributes only if they are not manully specified
		if(SKIP_TYPE(attrType) && NOT(manualAttrs.count(attrName)))
			continue;

		// PRINT_INFO("  Processing attribute: \"%s\"", attrName.c_str());

		AttributeValueMap::const_iterator manualAttrIt = manualAttrs.find(attrName);
		if(manualAttrIt != manualAttrs.end()) {
			pluginAttrs[attrName] = manualAttrIt->second;
		}
		else {
			if(v.second.count("skip"))
				if(v.second.get<bool>("skip"))
					continue;

			if(MAPPABLE_TYPE(attrType)) {
				BL::NodeSocket sock = VRayNodeExporter::getSocketByAttr(node, attrName);
				if(sock) {
					std::string socketValue = VRayNodeExporter::exportSocket(ntree, sock, context);
					if(socketValue != "NULL")
						pluginAttrs[attrName] = socketValue;
				}
			}
			else {
				std::string propValue = VRayNodeExporter::getValueFromPropGroup(&propGroup, (ID*)ntree.ptr.data, attrName.c_str());
				if(propValue != "NULL")
					pluginAttrs[attrName] = propValue;
			}
		}
	}

	VRayNodePluginExporter::exportPlugin(pluginType, pluginID, pluginName, pluginAttrs);

	return pluginName;
}


std::string VRayNodeExporter::exportVRayNode(BL::NodeTree ntree, BL::Node node, VRayObjectContext *context, const AttributeValueMap &manualAttrs)
{
	std::string nodeClass = node.bl_idname();

	PRINT_INFO("Exporting: ntree = \"%s\" node = \"%s\"", ntree.name().c_str(), node.name().c_str());
	PRINT_INFO("  Class: node = \"%s\"", nodeClass.c_str());

	if(nodeClass == "VRayNodeBlenderOutputMaterial") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputMaterial(ntree, node, context);
	}
	else if(nodeClass == "VRayNodeBlenderOutputGeometry") {
		return VRayNodeExporter::exportVRayNodeBlenderOutputGeometry(ntree, node, context);
	}
	else if(nodeClass == "VRayNodeBRDFLayered") {
		return VRayNodeExporter::exportVRayNodeBRDFLayered(ntree, node);
	}
	else if(nodeClass == "VRayNodeTexLayered") {
		return VRayNodeExporter::exportVRayNodeTexLayered(ntree, node);
	}
	else if(nodeClass == "VRayNodeSelectObject") {
		return VRayNodeExporter::exportVRayNodeSelectObject(ntree, node);
	}
	else if(nodeClass == "VRayNodeSelectGroup") {
		return VRayNodeExporter::exportVRayNodeSelectGroup(ntree, node);
	}
	else if(nodeClass == "VRayNodeSelectNodeTree") {
		return VRayNodeExporter::exportVRayNodeSelectNodeTree(ntree, node);
	}
	else if(nodeClass == "VRayNodeLightMesh") {
		return VRayNodeExporter::exportVRayNodeLightMesh(ntree, node, context);
	}
	else if(nodeClass == "VRayNodeGeomDisplacedMesh") {
		return VRayNodeExporter::exportVRayNodeGeomDisplacedMesh(ntree, node, context);
	}
	else if(nodeClass == "VRayNodeBitmapBuffer") {
		return VRayNodeExporter::exportVRayNodeBitmapBuffer(ntree, node);
	}
	else if(nodeClass == "VRayNodeTexGradRamp") {
		return VRayNodeExporter::exportVRayNodeTexGradRamp(ntree, node);
	}
	else if(nodeClass == "VRayNodeTexRemap") {
		return VRayNodeExporter::exportVRayNodeTexRemap(ntree, node);
	}
	else if(nodeClass == "VRayNodeOutputMaterial") {
		BL::NodeSocket materialInSock = VRayNodeExporter::getSocketByName(node, "Material");
		if(materialInSock.is_linked())
			return VRayNodeExporter::exportLinkedSocket(ntree, materialInSock);
		else
			return "NULL";
	}
	else if(nodeClass == "VRayNodeOutputTexture") {
		BL::NodeSocket textureInSock = VRayNodeExporter::getSocketByName(node, "Texture");
		if(textureInSock.is_linked())
			return VRayNodeExporter::exportLinkedSocket(ntree, textureInSock);
		else
			return "NULL";
	}

	return exportVRayNodeAttributes(ntree, node, context, manualAttrs);
}


static const std::string GetAttrType(PluginJson *pluginDesc, const std::string &attributeName)
{
	if(pluginDesc) {
		BOOST_FOREACH(PluginJson::value_type &v, pluginDesc->get_child("Parameters")) {
			const std::string attrName = v.second.get_child("attr").data();
			const std::string attrType = v.second.get_child("type").data();
			if(attrName == attributeName)
				return attrType;
		}
	}

	return "NULL";
}


int VRayNodePluginExporter::exportPlugin(const std::string &pluginType, const std::string &pluginID, const std::string &pluginName, const AttributeValueMap &pluginAttrs)
{
	// Check names cache not to export duplicated data for this frame
	//
	if(m_namesCache.find(pluginName) != m_namesCache.end())
		return 1;

	m_namesCache.insert(pluginName);

	bool pluginIsInCache = VRayExportable::m_set->m_isAnimation ? m_nodeCache.pluginInCache(pluginName) : false;

	std::stringstream outAttributes;
	std::stringstream outPlugin;

	PluginJson *pluginDesc = VRayExportable::m_pluginDesc.getTree(pluginID);

	AttributeValueMap::const_iterator attrIt;
	for(attrIt = pluginAttrs.begin(); attrIt != pluginAttrs.end(); ++attrIt) {
		const std::string attrName  = attrIt->first;
		const std::string attrValue = attrIt->second;
		const std::string attrType  = GetAttrType(pluginDesc, attrName);

		if(NOT(VRayExportable::m_set->m_isAnimation)) {
			outAttributes << "\n\t" << attrName << "=" << attrValue << ";";
		}
		else {
			MHash attrHash = HashCode(attrValue.c_str());

			const int currentFrame = VRayExportable::m_set->m_frameCurrent;
			const int prevFrame    = currentFrame - VRayExportable::m_set->m_frameStep;

			const int attrNonAnim = NOT_ANIMATABLE_TYPE(attrType);

			// Check if plugin is in cache
			// If plugin is in cache check the stored attribute value
			// and decide whether to export the keyframe
			//
			if(NOT(pluginIsInCache)) {
				if(attrNonAnim) {
					outAttributes << "\n\t" << attrName << "=" << attrValue << ";";
				}
				else {
					outAttributes << "\n\t" << attrName << "=interpolate((" << currentFrame << "," << attrValue << "));";
				}
			}
			else {
				if(NOT(attrNonAnim)) {
					MHash cachedHash = m_nodeCache.getCachedHash(pluginName, attrName);

					if(cachedHash == attrHash)
						continue;

					const int         cachedFrame = m_nodeCache.getCachedFrame(pluginName, attrName);
					const std::string cachedValue = m_nodeCache.getCachedValue(pluginName, attrName);

					outAttributes << "\n\t" << attrName << "=interpolate(";
					if(cachedFrame < prevFrame) {
						// Cached value is more then 'frameStep' before the current frame
						// need to insert keyframe
						outAttributes << "(" << prevFrame    << "," << cachedValue << "),";
						outAttributes << "(" << currentFrame << "," << attrValue   << ")";
					}
					else {
						// It's simply the next frame - export as usual
						outAttributes << "(" << currentFrame << "," << attrValue << ")";
					}
					outAttributes << ");";
				}
			}

			// Store/update value in cache
			m_nodeCache.addToCache(pluginName, attrName, currentFrame, attrValue, attrHash);
		}
	}

	if(outAttributes.str().empty()) {
		// This plugin doesn't have any attributes
		if(pluginID == "GeomPlane") {
			// NOTE: Try to make it exported only once
			//
			outPlugin << "\n" << pluginID << " " << pluginName << " {}\n";
			PYTHON_PRINT(VRayNodeExporter::m_set->m_fileObject, outPlugin.str().c_str());
		}
	}
	else {
		outPlugin << "\n" << pluginID << " " << pluginName << " {";
		outPlugin << outAttributes.str();
		outPlugin << "\n}\n";

		PyObject *output = VRayNodeExporter::m_set->m_fileObject;

		if(pluginType == "TEXTURE" || pluginType == "UVWGEN") {
			output = VRayNodeExporter::m_set->m_fileTex;
		}
		else if(pluginType == "MATERIAL" || pluginType == "BRDF") {
			output = VRayNodeExporter::m_set->m_fileMat;
		}
		else if(pluginType == "LIGHT") {
			output = VRayNodeExporter::m_set->m_fileLights;
		}
		else if(pluginType == "GEOMETRY") {
			if(pluginID == "GeomDisplacedMesh"      ||
			   pluginID == "GeomStaticSmoothedMesh" ||
			   pluginID == "GeomPlane")
			{
				// Store dynamic geometry plugins in 'Node' file
				output = VRayNodeExporter::m_set->m_fileObject;
			}
			else {
				output = VRayNodeExporter::m_set->m_fileGeom;
			}
		}

		PYTHON_PRINT(output, outPlugin.str().c_str());
	}

	return 0;
}


void VRayNodePluginExporter::clearNamesCache()
{
	VRayNodePluginExporter::m_namesCache.clear();
}


void VRayNodePluginExporter::clearNodesCache()
{
	VRayNodePluginExporter::m_nodeCache.clearCache();
}
