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

#include <boost/format.hpp>

#include "vfb_params_json.h"
#include "vfb_node_exporter.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_string.h"
#include "vfb_utils_nodes.h"

extern "C" {
#include "BKE_node.h" // For ntreeUpdateTree()
}


static boost::format FormatFloat("%.6g");
static boost::format FormatString("\"%s\"");
static boost::format FormatTmHex("TransformHex(\"%s\")");
static boost::format FormatInt("%i");
static boost::format FormatUInt("%u");
static boost::format FormatColor("Color(%.6g,%.6g,%.6g)");
static boost::format FormatAColor("AColor(%.6g,%.6g,%.6g,%.6g)");
static boost::format FormatVector("Vector(%.6g,%.6g,%.6g)");

#define BOOST_FORMAT_STRING(s)  boost::str(FormatFloat  % s)
#define BOOST_FORMAT_FLOAT(f)   boost::str(FormatString % f)
#define BOOST_FORMAT_TM(tm)     boost::str(FormatTmHex  % tm)
#define BOOST_FORMAT_INT(i)     boost::str(FormatInt    % i)
#define BOOST_FORMAT_UINT(i)    boost::str(FormatUInt   % i)
#define BOOST_FORMAT_BOOL(i)    boost::str(FormatInt    % i)
#define BOOST_FORMAT_COLOR(c)   boost::str(FormatColor  % c[0] % c[1] % c[2]       );
#define BOOST_FORMAT_COLOR1(c)  boost::str(FormatColor  % c    % c    % c          );
#define BOOST_FORMAT_ACOLOR(c)  boost::str(FormatAColor % c[0] % c[1] % c[2] % c[3]);
#define BOOST_FORMAT_ACOLOR3(c) boost::str(FormatAColor % c[0] % c[1] % c[2] % 1.0f);
#define BOOST_FORMAT_VECTOR(v)  boost::str(FormatVector % v[0] % v[1] % v[2]       )


using namespace VRayForBlender;
using namespace VRayForBlender::Nodes;


std::string DataExporter::GenPluginName(BL::Node node, BL::NodeTree ntree, NodeContext *context)
{
	std::string pluginName = Blender::GetIDName(ntree, "NT") + "N" + node.name();

	if (context) {
		BL::NodeTree  parent = context->getNodeTree();
		BL::NodeGroup group  = context->getGroupNode();
		if (parent) {
			pluginName += Blender::GetIDName(parent, "NP");
		}
		if (group) {
			pluginName += Blender::GetIDName(parent, "GR");
		}
	}

	return String::StripString(pluginName);
}


ParamDesc::PluginType DataExporter::GetNodePluginType(BL::Node node)
{
	ParamDesc::PluginType pluginType;

	if (RNA_struct_find_property(&node.ptr, "vray_type")) {
		pluginType = ParamDesc::GetPluginTypeFromString(RNA_std_string_get(&node.ptr, "vray_type"));
	}

	return pluginType;
}


std::string DataExporter::GetNodePluginID(BL::Node node)
{
	std::string pluginID;

	if (RNA_struct_find_property(&node.ptr, "vray_plugin")) {
		pluginID = RNA_std_string_get(&node.ptr, "vray_plugin");
	}

	return pluginID;
}


void DataExporter::init(PluginExporter *exporter)
{
	m_exporter = exporter;
}


void DataExporter::init(PluginExporter *exporter, ExporterSettings settings)
{
	m_exporter = exporter;
	m_settings = settings;
}


void DataExporter::init_data(BL::BlendData data, BL::Scene scene, BL::RenderEngine engine, BL::Context context)
{
	m_data = data;
	m_scene = scene;
	m_engine = engine;
	m_context = context;

	// NOTE: On scene save node links are not properly updated for some
	// reason; simply manually update everything...
	//
	BL::BlendData::node_groups_iterator nIt;
	for (m_data.node_groups.begin(nIt); nIt != m_data.node_groups.end(); ++nIt) {
		ntreeUpdateTree((struct Main*)data.ptr.data, (struct bNodeTree*)nIt->ptr.data);
	}
}


void DataExporter::init_defaults()
{
	// Exporter default material
	//
	PluginDesc defaultBrdfDesc("DefaultBRDF", "BRDFDiffuse");
	defaultBrdfDesc.add("color_tex", AttrColor(0.5f, 0.5f, 0.5f));

	PluginDesc defaultMtlDesc("DefaultMtl", "MtlSingleBRDF");
	defaultMtlDesc.add("brdf", m_exporter->export_plugin(defaultBrdfDesc));

	m_defaults.default_material = m_exporter->export_plugin(defaultMtlDesc);

	// Export override material
	//
	if (!m_settings.override_material.empty()) {

	}
}


#if 0
void NodeExporter::getAttributesList(const std::string &pluginID, StrSet &attrSet, bool mappable)
{
	const ParamDesc::PluginDesc &pluginDesc = GetPluginDescription(pluginID);

	for (const auto &attrDesc : pluginDesc.attributes) {
		const std::string         &attrName = attrDesc.name;
		const ParamDesc::AttrType &attrType = attrDesc.type;
#if 0
		if (SKIP_TYPE(attrType))
			continue;

		if (v.second.count("skip"))
			if (v.second.get<bool>("skip"))
				continue;

		if (mappable) {
			if (MAPPABLE_TYPE(attrType))
				attrSet.insert(attrName);
		}
		else {
			if (NOT(MAPPABLE_TYPE(attrType)))
				attrSet.insert(attrName);
		}
#endif
	}
}
#endif


AttrValue DataExporter::exportDefaultSocket(BL::NodeTree ntree, BL::NodeSocket socket)
{
	AttrValue attrValue;

	std::string socketVRayType = socket.rna_type().identifier();
	if (socketVRayType == "VRaySocketColor" ||
	    socketVRayType == "VRaySocketEnvironment") {
		float color[3];
		RNA_float_get_array(&socket.ptr, "value", color);
		attrValue = AttrColor(color);
	}
	else if (socketVRayType == "VRaySocketEnvironmentOverride") {
		if (RNA_boolean_get(&socket.ptr, "use")) {
			float color[3];
			RNA_float_get_array(&socket.ptr, "value", color);
			attrValue = AttrColor(color);
		}
	}
	else if (socketVRayType == "VRaySocketFloatColor" ||
	         socketVRayType == "VRaySocketFloat") {
		attrValue = RNA_float_get(&socket.ptr, "value");
	}
	else if (socketVRayType == "VRaySocketInt") {
		attrValue = RNA_int_get(&socket.ptr, "value");
	}
	else if (socketVRayType == "VRaySocketVector") {
		float vector[3];
		RNA_float_get_array(&socket.ptr, "value", vector);
		attrValue = AttrVector(vector);
	}
	else if (socketVRayType == "VRaySocketBRDF") {
		PRINT_ERROR("Node tree: %s => Node name: %s => Mandatory socket of type '%s' is not linked!",
		            ntree.name().c_str(), socket.node().name().c_str(), socketVRayType.c_str());
	}
	// These sockets do not have default value, they must be linked or skipped otherwise.
	//
	else if (socketVRayType == "VRaySocketTransform") {}
	else if (socketVRayType == "VRaySocketFloatNoValue") {}
	else if (socketVRayType == "VRaySocketColorNoValue") {}
	else if (socketVRayType == "VRaySocketCoords") {}
	else if (socketVRayType == "VRaySocketObject") {}
	else if (socketVRayType == "VRaySocketEffect") {}
	else if (socketVRayType == "VRaySocketMtl") {}
	else {
		PRINT_ERROR("Node tree: %s => Node name: %s => Unsupported socket type: %s",
		            ntree.name().c_str(), socket.node().name().c_str(), socketVRayType.c_str());
	}

	return attrValue;
}


AttrValue DataExporter::exportSocket(BL::NodeTree ntree, BL::NodeSocket socket, NodeContext *context)
{
	if (socket.is_linked())
		return DataExporter::exportLinkedSocket(ntree, socket, context);

	return DataExporter::exportDefaultSocket(ntree, socket);
}


AttrValue DataExporter::exportSocket(BL::NodeTree ntree, BL::Node node, const std::string &socketName, NodeContext *context)
{
	BL::NodeSocket socket = Nodes::GetInputSocketByName(node, socketName);
	return DataExporter::exportSocket(ntree, socket, context);
}


AttrValue DataExporter::exportVRayNodeAuto(VRayNodeExportParam, PluginDesc &pluginDesc)
{
	// Export attributes automatically from node
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	return m_exporter->export_plugin(pluginDesc);
}


AttrValue DataExporter::exportVRayNode(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, NodeContext *context)
{
	AttrValue attrValue;

	const std::string &nodeClass = node.bl_idname();
#if 0
	PRINT_INFO_EX("Exporting \"%s\" from \"%s\"...",
	              node.name().c_str(), ntree.name().c_str());
#endif
	if (nodeClass == "VRayNodeBlenderOutputMaterial") {
		attrValue = DataExporter::exportVRayNodeBlenderOutputMaterial(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeBlenderOutputGeometry") {
		attrValue = DataExporter::exportVRayNodeBlenderOutputGeometry(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeBRDFLayered") {
		attrValue = DataExporter::exportVRayNodeBRDFLayered(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeBRDFVRayMtl") {
		attrValue = DataExporter::exportVRayNodeBRDFVRayMtl(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexLayered") {
		attrValue = DataExporter::exportVRayNodeTexLayered(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexMulti") {
		attrValue = DataExporter::exportVRayNodeTexMulti(ntree, node, fromSocket, context);
	}
#if 0
	else if (nodeClass == "VRayNodeSelectObject") {
		BL::Object b_ob = NodeExporter::exportVRayNodeSelectObject(ntree, node, fromSocket, context);
		if (NOT(b_ob))
			return "NULL";
		return GetIDName(b_ob);
	}
	else if (nodeClass == "VRayNodeSelectGroup") {
		BL::Group b_gr = NodeExporter::exportVRayNodeSelectGroup(ntree, node, fromSocket, context);
		return NodeExporter::getObjectNameList(b_gr);
	}
#endif
	else if (nodeClass == "VRayNodeLightMesh") {
		attrValue = DataExporter::exportVRayNodeLightMesh(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeGeomDisplacedMesh") {
		attrValue = DataExporter::exportVRayNodeGeomDisplacedMesh(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeGeomStaticSmoothedMesh") {
		attrValue = DataExporter::exportVRayNodeGeomStaticSmoothedMesh(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeBitmapBuffer") {
		attrValue = DataExporter::exportVRayNodeBitmapBuffer(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexGradRamp") {
		attrValue = DataExporter::exportVRayNodeTexGradRamp(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexRemap") {
		attrValue = DataExporter::exportVRayNodeTexRemap(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexSoftbox") {
		attrValue = DataExporter::exportVRayNodeTexSoftbox(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexSky") {
		attrValue = DataExporter::exportVRayNodeTexSky(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexFalloff") {
		attrValue = DataExporter::exportVRayNodeTexFalloff(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexEdges") {
		attrValue = DataExporter::exportVRayNodeTexEdges(ntree, node, fromSocket, context);
	}
#if 0
	else if (nodeClass == "VRayNodeTexVoxelData") {
		plugin = NodeExporter::exportVRayNodeTexVoxelData(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexMayaFluid") {
		plugin = NodeExporter::exportVRayNodeTexMayaFluid(ntree, node, fromSocket, context);
	}
#endif
	else if (nodeClass == "VRayNodeMtlMulti") {
		attrValue = DataExporter::exportVRayNodeMtlMulti(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeOutputMaterial") {
		BL::NodeSocket materialInSock = Nodes::GetInputSocketByName(node, "Material");
		if (!materialInSock.is_linked()) {
			PRINT_ERROR("");
		}
		else {
			attrValue = DataExporter::exportLinkedSocket(ntree, materialInSock, context);
		}
	}
	else if (nodeClass == "VRayNodeOutputTexture") {
		BL::NodeSocket textureInSock = Nodes::GetInputSocketByName(node, "Texture");
		if (!textureInSock.is_linked()) {
			PRINT_ERROR("");
		}
		else {
			attrValue = DataExporter::exportLinkedSocket(ntree, textureInSock, context);
		}
	}
#if 0
	else if (nodeClass == "VRayNodeTransform") {
		plugin = NodeExporter::exportVRayNodeTransform(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeMatrix") {
		plugin = NodeExporter::exportVRayNodeMatrix(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeVector") {
		plugin = NodeExporter::exportVRayNodeVector(ntree, node, fromSocket, context);
	}
#endif
#if 0
	else if (nodeClass == "VRayNodeEnvFogMeshGizmo") {
		plugin = NodeExporter::exportVRayNodeEnvFogMeshGizmo(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeEnvironmentFog") {
		plugin = NodeExporter::exportVRayNodeEnvironmentFog(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodePhxShaderSimVol") {
		plugin = NodeExporter::exportVRayNodePhxShaderSimVol(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodePhxShaderSim") {
		plugin = NodeExporter::exportVRayNodePhxShaderSim(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeSphereFadeGizmo") {
		plugin = NodeExporter::exportVRayNodeSphereFadeGizmo(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeSphereFade") {
		plugin = NodeExporter::exportVRayNodeSphereFade(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeVolumeVRayToon") {
		plugin = NodeExporter::exportVRayNodeVolumeVRayToon(ntree, node, fromSocket, context);
	}
#endif
	else if (nodeClass == "VRayNodeUVWGenEnvironment") {
		attrValue = DataExporter::exportVRayNodeUVWGenEnvironment(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeUVWGenMayaPlace2dTexture") {
		attrValue = DataExporter::exportVRayNodeUVWGenMayaPlace2dTexture(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeUVWGenChannel") {
		attrValue = DataExporter::exportVRayNodeUVWGenChannel(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeRenderChannelLightSelect") {
		attrValue = DataExporter::exportVRayNodeRenderChannelLightSelect(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeRenderChannelColor") {
		attrValue = DataExporter::exportVRayNodeRenderChannelColor(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeMetaImageTexture") {
		attrValue = DataExporter::exportVRayNodeMetaImageTexture(ntree, node, fromSocket, context);
	}
	else if (node.is_a(&RNA_ShaderNodeNormal)) {
		attrValue = DataExporter::exportBlenderNodeNormal(ntree, node, fromSocket, context);
	}
	else {
		PluginDesc pluginDesc(DataExporter::GenPluginName(node, ntree, context),
		                      DataExporter::GetNodePluginID(node));
		attrValue = exportVRayNodeAuto(ntree, node, fromSocket, context, pluginDesc);
	}

	return attrValue;
}


void DataExporter::getUserAttributes(PointerRNA *ptr, StrVector &user_attributes)
{
	RNA_BEGIN(ptr, itemptr, "user_attributes") {
		bool useAttr = RNA_boolean_get(&itemptr, "use");
		if (useAttr) {
			const std::string &attrName = RNA_std_string_get(&itemptr, "name");;
			std::string attrValue       = "0";

			UserAttributeType attrType = (UserAttributeType)RNA_enum_get(&itemptr, "value_type");
			switch (attrType) {
				case UserAttributeInt:
					attrValue = BOOST_FORMAT_INT(RNA_int_get(&itemptr, "value_int"));
					break;
				case UserAttributeFloat:
					attrValue = BOOST_FORMAT_FLOAT(RNA_float_get(&itemptr, "value_float"));
					break;
				case UserAttributeColor:
					float color[3];
					RNA_float_get_array(&itemptr, "value_color", color);
					attrValue = BOOST_FORMAT_COLOR(color);
					break;
				case UserAttributeString:
					attrValue = RNA_std_string_get(&itemptr, "value_string");
					break;
				default:
					break;
			}

			std::string userAttr = attrName + "=" + attrValue;
			user_attributes.push_back(userAttr);
		}
	}
	RNA_END;
}


std::string DataExporter::GetConnectedNodePluginID(BL::NodeSocket fromSocket)
{
	std::string pluginID;

	BL::Node fromNode = fromSocket.node();
	if (fromNode) {
		pluginID = DataExporter::GetNodePluginID(fromNode);
	}

	return pluginID;
}


std::string DataExporter::getNodeName(BL::Object ob)
{
	static boost::format obNameFormat("Node@%s");
	return boost::str(obNameFormat % ob.name());
}


std::string DataExporter::getMeshName(BL::Object ob)
{
	static boost::format meshNameFormat("Geom@%s");
	return boost::str(meshNameFormat % ob.name());
}


std::string DataExporter::getHairName(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSettings pset)
{
	static boost::format hairNameFormat("Hair@%s|%s|%s");
	return boost::str(hairNameFormat % ob.name() % psys.name() % pset.name());
}


ParamDesc::PluginType DataExporter::GetConnectedNodePluginType(BL::NodeSocket fromSocket)
{
	ParamDesc::PluginType pluginType = ParamDesc::PluginUnknown;

	BL::Node fromNode = fromSocket.node();
	if (fromNode) {
		pluginType = DataExporter::GetNodePluginType(fromNode);
	}

	return pluginType;
}
