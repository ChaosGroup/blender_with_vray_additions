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

#include "vfb_params_json.h"
#include "vfb_node_exporter.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_string.h"
#include "vfb_utils_nodes.h"
#include <boost/range/adaptor/reversed.hpp>

#define SPRINTF_FORMAT_INIT_IN_SCOPE()                              \
	char formatBuff[String::MAX_PLG_LEN] = {0,};                    \
	const char * const FormatFloat("%.6g");                         \
	const char * const FormatString("\"%s\"");                      \
	const char * const FormatTmHex("TransformHex(\"%s\")");         \
	const char * const FormatInt("%i");                             \
	const char * const FormatUInt("%u");                            \
	const char * const FormatColor("Color(%.6g,%.6g,%.6g)");        \
	const char * const FormatAColor("AColor(%.6g,%.6g,%.6g,%.6g)"); \
	const char * const FormatVector("Vector(%.6g,%.6g,%.6g)");      \


#define SPRINTF_FORMAT_STRING(s)  (snprintf(formatBuff, sizeof(formatBuff), FormatString , s), formatBuff)
#define SPRINTF_FORMAT_FLOAT(f)   (snprintf(formatBuff, sizeof(formatBuff), FormatFloat  , f), formatBuff)
#define SPRINTF_FORMAT_TM(tm)     (snprintf(formatBuff, sizeof(formatBuff), FormatTmHex  , tm), formatBuff)
#define SPRINTF_FORMAT_INT(i)     (snprintf(formatBuff, sizeof(formatBuff), FormatInt    , i), formatBuff)
#define SPRINTF_FORMAT_UINT(i)    (snprintf(formatBuff, sizeof(formatBuff), FormatUInt   , i), formatBuff)
#define SPRINTF_FORMAT_BOOL(i)    (snprintf(formatBuff, sizeof(formatBuff), FormatInt    , i), formatBuff)
#define SPRINTF_FORMAT_COLOR(c)   (snprintf(formatBuff, sizeof(formatBuff), FormatColor  , c[0] , c[1] , c[2]       ), formatBuff)
#define SPRINTF_FORMAT_COLOR1(c)  (snprintf(formatBuff, sizeof(formatBuff), FormatColor  , c    , c    , c          ), formatBuff)
#define SPRINTF_FORMAT_ACOLOR(c)  (snprintf(formatBuff, sizeof(formatBuff), FormatAColor , c[0] , c[1] , c[2] , c[3]), formatBuff)
#define SPRINTF_FORMAT_ACOLOR3(c) (snprintf(formatBuff, sizeof(formatBuff), FormatAColor , c[0] , c[1] , c[2] , 1.0f), formatBuff)
#define SPRINTF_FORMAT_VECTOR(v)  (snprintf(formatBuff, sizeof(formatBuff), FormatVector , v[0] , v[1] , v[2]       ), formatBuff)


using namespace VRayForBlender;
using namespace VRayForBlender::Nodes;

namespace {
/// Check of object has particle system which is hair
bool ob_has_hair(BL::Object ob) {
	BL::Object::modifiers_iterator modIt;
	for (ob.modifiers.begin(modIt); modIt != ob.modifiers.end(); ++modIt) {
		BL::Modifier mod(*modIt);
		if (mod && mod.show_render() && mod.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
			BL::ParticleSystemModifier psm(mod);
			BL::ParticleSystem psys = psm.particle_system();
			if (psys) {
				BL::ParticleSettings pset(psys.settings());
				if (pset &&
					pset.type() == BL::ParticleSettings::type_HAIR &&
					pset.render_type() == BL::ParticleSettings::render_type_PATH) {

					return true;
				}
			}
		}
	}

	return false;
}
}

int IdTrack::contains(BL::Object ob) {
	return data.find(DataExporter::getIdUniqueName(ob)) != data.end();
}

void IdTrack::clear() {
	data.clear();
}

void IdTrack::insert(BL::Object ob, const std::string &plugin, PluginType type) {
	switch (type) {
	case IdTrack::CLIPPER:
		PRINT_INFO_EX("IdTrack plugin %s CLIPPER", plugin.c_str());
		break;
	case IdTrack::DUPLI_INSTACER:
		PRINT_INFO_EX("IdTrack plugin %s DUPLI_INSTACER", plugin.c_str());
		break;
	case IdTrack::DUPLI_NODE:
		PRINT_INFO_EX("IdTrack plugin %s DUPLI_NODE", plugin.c_str());
		break;
	case IdTrack::HAIR:
		PRINT_INFO_EX("IdTrack plugin %s HAIR", plugin.c_str());
		break;
	default:
		break;
	}
	IdDep &dep = data[DataExporter::getIdUniqueName(ob)];
	dep.plugins[plugin] = {true, type};
	dep.used = true;
	dep.object = ob;
}

void IdTrack::reset_usage() {
	for (auto &dIt : data) {
		IdDep &dep = dIt.second;
		for (auto &pl : dep.plugins) {
			pl.second.used = false;
		}
		dep.used = false;
	}
}

HashSet<std::string> IdTrack::getAllObjectPlugins(BL::Object ob) const {
	auto iter = data.find(DataExporter::getIdUniqueName(ob));
	if (iter == data.end()) {
		return HashSet<std::string>();
	}

	HashSet<std::string> set;
	// get all plugins for selected object
	std::transform(iter->second.plugins.begin(), iter->second.plugins.end(), std::inserter(set, set.begin()),
		std::bind(&HashMap<std::string, PluginInfo>::value_type::first, std::placeholders::_1));
	return set;
}

bool DataExporter::isObVrscene(BL::Object ob) {
	auto vray = RNA_pointer_get(&ob.ptr, "vray");
	return vray.data && RNA_boolean_get(&vray, "overrideWithScene");
}

bool DataExporter::isObMesh(BL::Object ob) {
	auto obType = ob.type();
	return (obType == BL::Object::type_MESH    ||
			obType == BL::Object::type_CURVE   ||
			obType == BL::Object::type_SURFACE ||
			obType == BL::Object::type_FONT    ||
			obType == BL::Object::type_META);
}

bool DataExporter::isObLamp(BL::Object ob) {
	return ob.type() == BL::Object::type_LAMP;
}

bool DataExporter::isObGroupInstance(BL::Object ob) {
	return ob.type() == BL::Object::type_EMPTY;
}

std::string DataExporter::GenPluginName(BL::Node node, BL::NodeTree ntree, NodeContext &context) {
	char basePlugiName[String::MAX_PLG_LEN] = {0, };
	snprintf(basePlugiName, sizeof(basePlugiName), "%s|N%s", getIdUniqueName(ntree).c_str(), node.name().c_str());
	std::string pluginName(basePlugiName);

	for (NodeContext::NodeTreeVector::iterator ntIt = context.parent.begin(); ntIt != context.parent.end(); ++ntIt) {
		BL::NodeTree &parent = *ntIt;
		if (parent) {
			pluginName += "|NT" + getIdUniqueName(parent);
		}
	}

	for (NodeContext::NodeVector::iterator gnIt = context.group.begin(); gnIt != context.group.end(); ++gnIt) {
		BL::Node &group = *gnIt;
		if (group) {
			pluginName += "|GR" + group.name();
		}
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


void DataExporter::init(std::shared_ptr<PluginExporter> exporter)
{
	m_exporter = exporter;
	m_evalMode = exporter->get_is_viewport() ? EvalModePreview : EvalModeRender;
}


void DataExporter::sync()
{
	auto lock = raiiLock();
	for (auto dIt = m_id_track.data.begin(); dIt != m_id_track.data.end(); ++dIt) {
		auto ob = dIt->second.object;
		auto &dep = dIt->second;



		for (auto plIter = dep.plugins.cbegin(), end = dep.plugins.cend(); plIter != end; /*nop*/) {
			bool should_remove = false;
			const char * type = nullptr;

			if (!dep.used) {
				// object not used at all - remove all plugins
				should_remove = true;
			} else if (!plIter->second.used) {
				// object used, but not this plugin - check if object still has it

				const int base_visibility = ObjectVisibility::HIDE_VIEWPORT | ObjectVisibility::HIDE_RENDER | ObjectVisibility::HIDE_LAYER;
				const bool skip_export = !isObjectVisible(ob, DataExporter::ObjectVisibility(base_visibility));

				switch (plIter->second.type) {
				case IdTrack::CLIPPER: {
					PointerRNA vrayObject = PointerRNA_NULL;
					vrayObject = RNA_pointer_get(&ob.ptr, "vray");
					PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
					should_remove = !RNA_boolean_get(&vrayClipper, "enabled");
					type = "CLIPPER";
					break;
				}
				case IdTrack::DUPLI_NODE:
					// we had dupli *without* instancer, now we dont have dupli, or its via instancer
					should_remove = skip_export || !ob.is_duplicator();
					type = "DUPLI_NODE";
					break;
				case IdTrack::DUPLI_LIGHT:
					// we had dupli *without* instancer, now we dont have dupli, or its via instancer
					should_remove = skip_export || !ob.is_duplicator();
					type = "DUPLI_LIGHT";
					break;
				case IdTrack::DUPLI_INSTACER:
					// we had dupli *with* instancer, now we have node based dupli or not at all
					should_remove = !ob.is_duplicator();
					type = "DUPLI_INSTACER";
					break;
				case IdTrack::DUPLI_MODIFIER:
					// we had dupli with array modifier, now it changed (hidden or removed mod)
					if (ob.modifiers.length()) {
						BL::Modifier mod = ob.modifiers[ob.modifiers.length() - 1];
						should_remove = !mod || !mod.show_render() || mod.type() != BL::Modifier::type_ARRAY;
					} else {
						should_remove = true;
					}
					type = "DUPLI_MODIFIER";
					break;
				case IdTrack::HAIR:
					// we had hair, check if we still have hair
					should_remove = !ob_has_hair(ob);
					type = "HAIR";
				default:
					break;
				}
			}

			if (should_remove) {
				PRINT_INFO_EX("Removing plugin: %s, with type: %s, for ob [%s]", plIter->first.c_str(), type, dIt->first.c_str());
				m_exporter->remove_plugin(plIter->first);
				plIter = dep.plugins.erase(plIter);
			} else {
				++plIter;
			}
		}
	}
}


void DataExporter::init_data(BL::BlendData data, BL::Scene scene, BL::RenderEngine engine, BL::Context context, BL::SpaceView3D view3d)
{
	m_data = data;
	m_scene = scene;
	m_engine = engine;
	m_context = context;
	m_view3d = view3d;
	BL::Object cameraOB = m_view3d ? m_view3d.camera() : scene.camera();
	if (cameraOB.type() == BL::Object::type_CAMERA) {
		m_active_camera = cameraOB;
	}
	m_is_preview = engine.is_preview();
}

void DataExporter::setComputedLayers(uint32_t layers, bool is_local_view)
{
	m_layer_changed = m_computedLayers != layers || m_is_local_view != is_local_view;
	m_is_local_view = is_local_view;
	m_computedLayers = layers;
}


void DataExporter::resetSyncState()
{
	auto lock = raiiLock();
	m_id_cache.clear();
	m_id_track.reset_usage();
	clearMaterialCache();
	// all hidden objects will be checked agains current settings
	refreshHideLists();
	// layer did not change since last set
	m_layer_changed = false;
	m_scene_layers = to_int_layer(m_scene.layers());
}

void DataExporter::reset()
{
	auto lock = raiiLock();
	m_defaults.default_material = AttrPlugin();
	m_id_cache.clear();
	m_id_track.clear();
	clearMaterialCache();
	// all hidden objects will be checked agains current settings
	refreshHideLists();
	// layer did not change since last set
	m_layer_changed = true;
	m_scene_layers = to_int_layer(m_scene.layers());
}

AttrValue DataExporter::exportDefaultSocket(BL::NodeTree &ntree, BL::NodeSocket &socket)
{
	AttrValue attrValue;

	const VRayNodeSocketType socketVRayType = getVRayNodeSocketType(socket);
	const std::string socketTypeName = getVRayNodeSocketTypeName(socket);

	if (socketVRayType == VRayNodeSocketType::vrayNodeSocketColor ||
	    socketVRayType == VRayNodeSocketType::vrayNodeSocketEnvironment) {
		float color[3];
		RNA_float_get_array(&socket.ptr, "value", color);
		attrValue = AttrColor(color);
	}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketEnvironmentOverride) {
		if (RNA_boolean_get(&socket.ptr, "use")) {
			float color[3];
			RNA_float_get_array(&socket.ptr, "value", color);
			attrValue = AttrColor(color);
		}
	}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketFloatColor ||
	         socketVRayType == VRayNodeSocketType::vrayNodeSocketFloat) {
		attrValue = RNA_float_get(&socket.ptr, "value");
	}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketInt) {
		attrValue = RNA_int_get(&socket.ptr, "value");
	}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketVector) {
		float vector[3];
		RNA_float_get_array(&socket.ptr, "value", vector);
		attrValue = AttrVector(vector);
	}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketBRDF) {
		PRINT_ERROR("Node tree: %s => Node name: %s => Mandatory socket of type '%s' is not linked!",
		            ntree.name().c_str(), socket.node().name().c_str(), socketTypeName.c_str());
	}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketPlugin) {
		attrValue = AttrSimpleType<std::string>(RNA_std_string_get(&socket.ptr, "value"));
	}
	// These sockets do not have default value, they must be linked or skipped otherwise.
	//
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketTransform) {}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketFloatNoValue) {}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketColorNoValue) {}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketCoords) {}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketObject) {}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketEffect) {}
	else if (socketVRayType == VRayNodeSocketType::vrayNodeSocketMtl) {}
	else {
		PRINT_ERROR("Node tree: %s => Node name: %s => Unsupported socket type: %s",
		            ntree.name().c_str(), socket.node().name().c_str(), socketTypeName.c_str());
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
#include "DNA_node_types.h"

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
			attrValue = AttrPlugin(getNodeName(ob));
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
	else if (nodeClass == "VRayNodeTexLayered") {
		attrValue = exportVRayNodeTexLayered(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexMulti") {
		attrValue = exportVRayNodeTexMulti(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexMeshVertexColorChannel") {
		attrValue = exportVRayNodeTexMeshVertexColorChannel(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexOSL") {
		attrValue = exportVRayNodeShaderScript(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexSoftbox") {
		attrValue = exportVRayNodeTexSoftbox(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexRemap") {
		attrValue = exportVRayNodeTexRemap(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeTexDistance") {
		attrValue = exportVRayNodeTexDistance(ntree, node, fromSocket, context);
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
	else if (nodeClass == "VRayNodeBRDFBump") {
		attrValue = exportVRayNodeBRDFBumpMtl(ntree, node, fromSocket, context);
	}
	else if (nodeClass == "VRayNodeMtlOSL") {
		attrValue = exportVRayNodeShaderScript(ntree, node, fromSocket, context);
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
	else if (nodeClass == "VRayNodeTexVoxelData") {
		attrValue = exportVRayNodeTexVoxelData(ntree, node, fromSocket, context);
	}
#if 0
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
	SPRINTF_FORMAT_INIT_IN_SCOPE();
	RNA_BEGIN(ptr, itemptr, "user_attributes") {
		bool useAttr = RNA_boolean_get(&itemptr, "use");
		if (useAttr) {
			const std::string &attrName = RNA_std_string_get(&itemptr, "name");;
			std::string attrValue       = "0";

			UserAttributeType attrType = (UserAttributeType)RNA_enum_get(&itemptr, "value_type");
			switch (attrType) {
				case UserAttributeInt:
					attrValue = SPRINTF_FORMAT_INT(RNA_int_get(&itemptr, "value_int"));
					break;
				case UserAttributeFloat:
					attrValue = SPRINTF_FORMAT_FLOAT(RNA_float_get(&itemptr, "value_float"));
					break;
				case UserAttributeColor:
					float color[3];
					RNA_float_get_array(&itemptr, "value_color", color);
					attrValue = SPRINTF_FORMAT_COLOR(color);
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

bool DataExporter::hasDupli(BL::Object ob) {
	const auto dupliType = ob.dupli_type();
	return dupliType != BL::Object::dupli_type_NONE && dupliType != BL::Object::dupli_type_FRAMES;
}

bool DataExporter::isDupliVisible(BL::Object ob, ObjectVisibility ignore) {
	bool is_renderable = true;

	// Dulpi
	if ((ignore & HIDE_DUPLI_EMITER) && hasDupli(ob)) {
		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		is_renderable = RNA_boolean_get(&vrayObject, "dupliShowEmitter");
	}

	// Particles
	// Particle system "Show / Hide Emitter" has priority over dupli
	if ((ignore & HIDE_PARTICLE_EMITER) && ob.particle_systems.length()) {
		is_renderable = true;

		BL::Object::modifiers_iterator mdIt;
		for (ob.modifiers.begin(mdIt); mdIt != ob.modifiers.end(); ++mdIt) {
			BL::Modifier md(*mdIt);
			if (md.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
				BL::ParticleSystemModifier pmod(md);
				BL::ParticleSystem psys(pmod.particle_system());
				if (psys) {
					BL::ParticleSettings pset(psys.settings());
					if (pset) {
						if (!pset.use_render_emitter()) {
							is_renderable = false;
							break;
						}
					}
				}
			}
		}
	}

	return is_renderable;
}

bool DataExporter::isObjectVisible(BL::Object ob, ObjectVisibility ignore)
{
#define has(flag) ((ignore & flag) != 0)

	if (!ob) {
		return false;
	}

	// object is duplicator and is hidden from dupli options/psys
	if (!isDupliVisible(ob, ignore)) {
		return false;
	}

	// hidden from viewport rendering
	if (has(HIDE_VIEWPORT) && m_exporter->get_is_viewport() && ob.hide()) {
		return false;
	}

	// hidden from rendering
	if (has(HIDE_RENDER) && !m_exporter->get_is_viewport() && ob.hide_render()) {
		return false;
	}

	// object is on another layer
	if (has(HIDE_LAYER) && !(m_computedLayers & ::get_layer(ob, m_is_local_view, ::to_int_layer(m_scene.layers()))) ) {
		return false;
	}

#undef has

	return true;
}

std::string DataExporter::getAssetName(BL::Object ob)
{
	return "Asset@" + getNodeName(ob);
}

std::string DataExporter::getNodeName(BL::Object ob)
{
	return "Node@" + getIdUniqueName(ob);
}

std::string DataExporter::getMeshName(BL::Object ob)
{
	BL::ID data_id = ob.is_modified(m_scene, m_evalMode)
	                 ? ob
	                 : ob.data();

	return "Geom@" + getIdUniqueName(data_id);
}


std::string DataExporter::getHairName(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSettings pset)
{
	BL::ID data_id = ob.is_modified(m_scene, m_evalMode)
	                ? ob
	                : ob.data();

	char hairName[String::MAX_PLG_LEN];
	snprintf(hairName, sizeof(hairName), "Hair@%s|%s|%s", getIdUniqueName(data_id).c_str(), psys.name().c_str(), pset.name().c_str());
	return hairName;
}


std::string DataExporter::getLightName(BL::Object ob)
{
	return "Lamp@" + getIdUniqueName(ob);
}

std::string DataExporter::getClipperName(BL::Object ob)
{
	return "Clipper@" + getIdUniqueName(ob);
}


std::string DataExporter::getIdUniqueName(ID * id) {
	std::string name(id->name);
	const std::string libPrefix("|L");
	Library * lib = id->lib;

	while (lib) {
		name += libPrefix;
		name += lib->name;
		lib = lib->parent;
	}

	return name;
}

std::string DataExporter::cryptomatteName(BL::Object ob) {
	return ob.name();
}

std::string DataExporter::cryptomatteNameHierarchy(BL::Object ob) {
	std::vector<std::string> nameParts;
	const char * base = "object";
	if (isObLamp(ob)) {
		base = "lamp";
	}
	int size = strlen(base);
	nameParts.push_back(base);

	BL::Object iter = ob.parent();
	while (iter) {
		const auto name = iter.name();
		nameParts.push_back(name);
		size += name.length();
		iter = iter.parent();
	}

	BL::Library lib = ob.library();
	while (lib) {
		const auto name = lib.name();
		nameParts.push_back(name);
		size += name.length();
		lib = lib.library();
	}

	const std::string prefix = "scene";
	nameParts.push_back(prefix);
	size += prefix.length();

	std::string result;
	// size for all strings + size for separators
	result.reserve(size + nameParts.size() + 2);

	for (const auto & item : boost::adaptors::reverse(nameParts)) {
		result += item;
		result += "/";
	}
	result.pop_back(); // remove trailing separator

	return result;
}

std::vector<std::string> DataExporter::cryptomatteAllNames(BL::Object ob) {
	std::vector<std::string> names;

	names.push_back(cryptomatteName(ob));
	names.push_back(cryptomatteNameHierarchy(ob));

	return names;
}

std::string DataExporter::getIdUniqueName(BL::Pointer ob) {
	return getIdUniqueName(reinterpret_cast<ID*>(ob.ptr.data));
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
		_ntree->tag |=  ID_RECALC_ALL;
	}
	else {
		_ntree->tag &= ~ID_RECALC_ALL;
	}
}

bool DataExporter::shouldSyncUndoneObject(BL::Object ob)
{
	auto lock = raiiLock();
	// we are no undo-ing
	if (!m_is_undo_sync) {
		return false;
	}

	if (m_undo_stack.size() < 2) {
		BLI_assert(!"Trying to do undo/redo but stack did not have enought states");
		return false;
	}

	// check if object is the state we are currently undoing
	const auto & checkState = m_undo_stack[1];
	return checkState.find(getIdUniqueName(ob)) != checkState.end();
}

bool DataExporter::isObjectInThisSync(BL::Object ob)
{
	auto lock = raiiLock();
	return m_undo_stack.front().find(getIdUniqueName(ob)) != m_undo_stack.front().end();
}

void DataExporter::saveSyncedObject(BL::Object ob)
{
	auto lock = raiiLock();
	const auto key = getIdUniqueName(ob);
	m_undo_stack.front().insert(key);
}

void DataExporter::syncStart(bool isUndoSync)
{
	auto lock = raiiLock();
	m_is_undo_sync = isUndoSync;
	m_undo_stack.push_front(UndoStateObjects());
}

void DataExporter::syncEnd()
{
	auto lock = raiiLock();
	if (m_is_undo_sync) {
		if (m_undo_stack.size() < 2) {
			BLI_assert(!"Trying to do undo/redo but stack did not have enought states");
			m_undo_stack.clear();
		} else {
			m_undo_stack.pop_back(); // pop the state then we were undo-ing stuff
			m_undo_stack.pop_back(); // pop the original state where we were do-ing stuff
		}
	}

	// discard oldest states until we reach limit
	while (m_undo_stack.size() > 32) {
		m_undo_stack.pop_back();
	}

	m_is_undo_sync = false;
}

