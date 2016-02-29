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


std::string DataExporter::GenPluginName(BL::Node node, BL::NodeTree ntree, NodeContext &context)
{
	std::string pluginName = Blender::GetIDName(ntree, "NT") + "N" + node.name();

	BL::NodeTree parent(context.getNodeTree());
	if (parent) {
		pluginName += Blender::GetIDName(parent, "NP");
	}

	BL::NodeGroup group(context.getGroupNode());
	if (group) {
		pluginName += Blender::GetIDName(parent, "GR");
	}

	return pluginName;
}


ParamDesc::PluginType DataExporter::GetNodePluginType(BL::Node node)
{
	ParamDesc::PluginType pluginType = ParamDesc::PluginUnknown;

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
	m_evalMode = EvalModePreview;
}


void DataExporter::init(PluginExporter *exporter, ExporterSettings settings)
{
	m_exporter = exporter;
	m_settings = settings;
	m_evalMode = exporter->get_is_viewport() ? EvalModePreview : EvalModeRender;
}


void DataExporter::sync()
{
	for (auto dIt = m_id_track.data.begin(); dIt != m_id_track.data.end(); ++dIt) {
		auto ob = dIt->first;
		auto &dep = dIt->second;

		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
		const bool dupli_use_instancer = RNA_boolean_get(&vrayObject, "use_instancer");

		for (auto plIter = dep.plugins.cbegin(), end = dep.plugins.cend(); plIter != end; /*nop*/) {
			bool should_remove = false;
			const char * type = nullptr;

			if (!dep.used) {
				// object not used at all - remove all plugins
				should_remove = true;
			} else if (!plIter->second.used) {
				// object used, but not this plugin - check if object still has it
				switch (plIter->second.type) {
				case IdTrack::CLIPPER:
					should_remove = !RNA_boolean_get(&vrayClipper, "enabled");
					type = "DUPLI_CLIPPER";
					break;
				case IdTrack::DUPLI_NODE:
					// we had dupli *without* instancer, now we dont have dupli, or its via instancer
					should_remove = !ob.is_duplicator() || dupli_use_instancer;
					type = "DUPLI_NODE";
					break;
				case IdTrack::DUPLI_INSTACER:
					// we had dupli *with* instancer, now we have node based dupli or not at all
					should_remove = !ob.is_duplicator() || !dupli_use_instancer;
					type = "DUPLI_INSTACER";
					break;
				}
			}

			if (should_remove) {
				PRINT_INFO_EX("Removing plugin: %s, with type: %s", plIter->first.c_str(), type);
				m_exporter->remove_plugin(plIter->first);
				plIter = dep.plugins.erase(plIter);
			} else {
				++plIter;
			}
		}
	}
}


void DataExporter::init_data(BL::BlendData data, BL::Scene scene, BL::RenderEngine engine, BL::Context context)
{
	m_data = data;
	m_scene = scene;
	m_engine = engine;
	m_context = context;
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
		// TODO
	}
}


AttrValue DataExporter::exportDefaultSocket(BL::NodeTree &ntree, BL::NodeSocket &socket)
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


AttrValue DataExporter::exportSocket(BL::NodeTree &ntree, BL::NodeSocket &socket, NodeContext &context)
{
	bool doExport = false;

	if (socket.is_linked()) {
		auto connectedNode = getConnectedNode(ntree, socket, context);
		doExport = connectedNode && !connectedNode.mute();
	}

	return doExport ?
		exportLinkedSocket(ntree, socket, context) :
		exportDefaultSocket(ntree, socket);
}


AttrValue DataExporter::exportSocket(BL::NodeTree &ntree, BL::Node &node, const std::string &socketName, NodeContext &context)
{
	BL::NodeSocket socket = Nodes::GetInputSocketByName(node, socketName);
	return exportSocket(ntree, socket, context);
}


AttrValue DataExporter::exportVRayNodeAuto(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context, PluginDesc &pluginDesc)
{
	// Export attributes automatically from node
	setAttrsFromNodeAuto(ntree, node, fromSocket, context, pluginDesc);

	return m_exporter->export_plugin(pluginDesc);
}


AttrValue DataExporter::exportVRayNode(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context)
{
	if (node.mute()) {
		return AttrValue("NULL");
	}

	AttrValue attrValue;

	const std::string &nodeClass = node.bl_idname();


#if 0
	PRINT_INFO_EX("Exporting \"%s\" from \"%s\"",
	              node.name().c_str(), ntree.name().c_str());
	if (context.object_context.object) {
		PRINT_INFO_EX("  For object \"%s\"",
		              context.object_context.object.name().c_str());
	}
#endif

	// Outputs
	//
	if (nodeClass == "VRayNodeBlenderOutputMaterial") {
		attrValue = exportVRayNodeBlenderOutputMaterial(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeBlenderOutputGeometry") {
		attrValue = exportVRayNodeBlenderOutputGeometry(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeOutputMaterial") {
		BL::NodeSocket materialInSock = Nodes::GetInputSocketByName(node, "Material");
		if (!materialInSock.is_linked()) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Material socket is not linked!",
			            ntree.name().c_str(), node.name().c_str());
		}
		else {
			attrValue = exportLinkedSocket(ntree, materialInSock, context);
		}
	}
	else if (nodeClass == "VRayNodeOutputTexture") {
		BL::NodeSocket textureInSock = Nodes::GetInputSocketByName(node, "Texture");
		if (!textureInSock.is_linked()) {
			PRINT_ERROR("Node tree: %s => Node name: %s => Texture socket is not linked!",
			            ntree.name().c_str(), node.name().c_str());
		}
		else {
			attrValue = exportLinkedSocket(ntree, textureInSock, context);
		}
	}

	// Selectors
	//
	else if (nodeClass == "VRayNodeSelectObject") {
		BL::Object ob = exportVRayNodeSelectObject(ntree, node, fromSocket, context);
		if (ob) {
			attrValue = AttrPlugin(Blender::GetIDName(ob));
		}
	}
	else if (nodeClass == "VRayNodeSelectGroup") {
		BL::Group group = exportVRayNodeSelectGroup(ntree, node, fromSocket, context);
		if (group) {
			attrValue = DataExporter::getObjectNameList(group);
		}
	}

	// Geometry
	//
	else if (nodeClass == "VRayNodeLightMesh") {
		attrValue = exportVRayNodeLightMesh(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeGeomDisplacedMesh") {
		attrValue = exportVRayNodeGeomDisplacedMesh(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeGeomStaticSmoothedMesh") {
		attrValue = exportVRayNodeGeomStaticSmoothedMesh(ntree, node, fromSocket, context);
	}

	// Textures
	//
	else if (nodeClass == "VRayNodeBitmapBuffer") {
		attrValue = exportVRayNodeBitmapBuffer(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeMetaImageTexture") {
		attrValue = exportVRayNodeMetaImageTexture(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexSky") {
		attrValue = exportVRayNodeTexSky(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexFalloff") {
		attrValue = exportVRayNodeTexFalloff(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexEdges") {
		attrValue = exportVRayNodeTexEdges(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexLayered") {
		attrValue = exportVRayNodeTexLayered(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexMulti") {
		attrValue = exportVRayNodeTexMulti(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexMeshVertexColorChannel") {
		attrValue = exportVRayNodeTexMeshVertexColorChannel(ntree, node, fromSocket, context);
	}

	// Material / BRDF
	//
	else if (nodeClass == "VRayNodeBRDFLayered") {
		attrValue = exportVRayNodeBRDFLayered(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeBRDFVRayMtl") {
		attrValue = exportVRayNodeBRDFVRayMtl(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeMtlMulti") {
		attrValue = exportVRayNodeMtlMulti(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeMetaStandardMaterial") {
		attrValue = exportVRayNodeMetaStandardMaterial(ntree, node, fromSocket, context);
	}

	// Math
	//
	else if (nodeClass == "VRayNodeTransform") {
		attrValue = exportVRayNodeTransform(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeMatrix") {
		attrValue = exportVRayNodeMatrix(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeVector") {
		attrValue = exportVRayNodeVector(ntree, node, fromSocket, context);
	}

	// Effects
	//
	else if (nodeClass == "VRayNodeEnvFogMeshGizmo") {
		attrValue = exportVRayNodeEnvFogMeshGizmo(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeEnvironmentFog") {
		attrValue = exportVRayNodeEnvironmentFog(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeSphereFadeGizmo") {
		attrValue = exportVRayNodeSphereFadeGizmo(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeSphereFade") {
		attrValue = exportVRayNodeSphereFade(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeVolumeVRayToon") {
		attrValue = exportVRayNodeVolumeVRayToon(ntree, node, fromSocket, context);
	}
#if 0
	else if (nodeClass == "VRayNodeTexVoxelData") {
		attrValue = exportVRayNodeTexVoxelData(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexMayaFluid") {
		attrValue = exportVRayNodeTexMayaFluid(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodePhxShaderSimVol") {
		attrValue = exportVRayNodePhxShaderSimVol(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodePhxShaderSim") {
		attrValue = exportVRayNodePhxShaderSim(ntree, node, fromSocket, context);
	}
#endif

	// UVW
	//
	else if (nodeClass == "VRayNodeUVWGenEnvironment") {
		attrValue = exportVRayNodeUVWGenEnvironment(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeUVWGenMayaPlace2dTexture") {
		attrValue = exportVRayNodeUVWGenMayaPlace2dTexture(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeUVWGenChannel") {
		attrValue = exportVRayNodeUVWGenChannel(ntree, node, fromSocket, context);
	}

	// Render channels
	//
	else if (nodeClass == "VRayNodeRenderChannelLightSelect") {
		attrValue = exportVRayNodeRenderChannelLightSelect(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeRenderChannelColor") {
		attrValue = exportVRayNodeRenderChannelColor(ntree, node, fromSocket, context);
	}

	// Blender's
	//
	else if (node.is_a(&RNA_ShaderNodeNormal)) {
		attrValue = exportBlenderNodeNormal(ntree, node, fromSocket, context);
	}

	// Automatic
	//
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


int DataExporter::isObjectVisible(BL::Object ob)
{
	int visible = true;

	if (!ob) {
		visible = false;
	}
	else if (ob.hide_render()) {
		// TODO: Check "Render Hidden" setting
		visible = false;
	}
	else {
		// TODO: Check visible layers settings
	}

	return visible;
}


std::string DataExporter::getNodeName(BL::Object ob)
{
	static boost::format obNameFormat("Node@%s");
	return boost::str(obNameFormat % ob.name());
}


std::string DataExporter::getMeshName(BL::Object ob)
{
	static boost::format meshNameFormat("Geom@%s");

	BL::ID data_id = ob.is_modified(m_scene, m_evalMode)
	                 ? ob
	                 : ob.data();

	return boost::str(meshNameFormat % data_id.name());
}


std::string DataExporter::getHairName(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSettings pset)
{
	static boost::format hairNameFormat("Hair@%s|%s|%s");

	BL::ID data_id = ob.is_modified(m_scene, m_evalMode)
	                ? ob
	                : ob.data();

	return boost::str(hairNameFormat % data_id.name() % psys.name() % pset.name());
}


std::string DataExporter::getLightName(BL::Object ob)
{
	static boost::format lampNameFormat("Lamp@%s");
	return boost::str(lampNameFormat % ob.name());
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


void DataExporter::tag_ntree(BL::NodeTree ntree, bool updated)
{
	ID *_ntree = (ID*)ntree.ptr.data;
	if (updated) {
		_ntree->flag |=  LIB_TAG_ID_RECALC_ALL;
	}
	else {
		_ntree->flag &= ~LIB_TAG_ID_RECALC_ALL;
	}
}
