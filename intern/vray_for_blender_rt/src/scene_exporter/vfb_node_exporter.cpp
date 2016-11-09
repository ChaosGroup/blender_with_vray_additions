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

int IdTrack::contains(BL::Object ob) {
	return data.find(DataExporter::getObjectUniqueKey(ob)) != data.end();
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
	IdDep &dep = data[DataExporter::getObjectUniqueKey(ob)];
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


std::string DataExporter::GenPluginName(BL::Node node, BL::NodeTree ntree, NodeContext &context)
{
	std::string pluginName;
	pluginName.reserve(512);

	pluginName = "N" + node.name() + "|" + ntree.name();

	// If we are exporting nodes from group node tree we have to resolve full path to node
	// to prevent plugin name override.
	//
	for (NodeContext::NodeTreeVector::iterator ntIt = context.parent.begin(); ntIt != context.parent.end(); ++ntIt) {
		BL::NodeTree &parent = *ntIt;
		if (parent) {
			pluginName += "|" + parent.name();
		}
	}
	for (NodeContext::NodeVector::iterator gnIt = context.group.begin(); gnIt != context.group.end(); ++gnIt) {
		BL::Node &group = *gnIt;
		if (group) {
			pluginName += "@" + group.name();
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


bool ob_has_hair(BL::Object ob)
{
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

void DataExporter::sync()
{
	for (auto dIt = m_id_track.data.begin(); dIt != m_id_track.data.end(); ++dIt) {
		auto ob = dIt->second.object;
		auto &dep = dIt->second;

		const auto & obName = ob.name();

		PointerRNA vrayObject = PointerRNA_NULL;
		PointerRNA vrayClipper = PointerRNA_NULL;
		bool dupli_use_instancer = false;

		if (dep.used) {
			vrayObject = RNA_pointer_get(&ob.ptr, "vray");
			vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
			dupli_use_instancer = RNA_boolean_get(&vrayObject, "use_instancer");
		}

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
					type = "CLIPPER";
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
	m_active_camera = BL::Camera(scene.camera() ? scene.camera() : view3d.camera());
	m_is_preview = engine.is_preview();
}

void DataExporter::setComputedLayers(uint32_t layers, bool is_local_view)
{
	m_layer_changed = m_computedLayers != layers || m_is_local_view != is_local_view;
	m_is_local_view = is_local_view;
	m_computedLayers = layers;
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

void DataExporter::resetSyncState()
{
	m_id_cache.clear();
	m_id_track.reset_usage();
	clearMaterialCache();
	// all hidden objects will be checked agains current settings
	refreshHideLists();
	// layer did not change since last set
	m_layer_changed = false;
	m_scene_layers = to_int_layer(m_scene.layers());
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

bool DataExporter::hasDupli(BL::Object ob) {
	return ((ob.dupli_type() != BL::Object::dupli_type_NONE) && (ob.dupli_type() != BL::Object::dupli_type_FRAMES));
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

	// hidden for current camra
	if (has(HIDE_LIST) && isObjectInHideList(ob, "camera")) {
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

#undef has;

	return true;
}


std::string DataExporter::getNodeName(BL::Object ob)
{
	// TODO: check if ob is from library and append it's name
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

std::string DataExporter::getObjectUniqueKey(BL::Object ob) {
	ID * id = reinterpret_cast<ID*>(ob.ptr.data);
	std::string name(id->name);

	Library * lib = id->lib;

	while (lib) {
		name += lib->name;
		lib = lib->parent;
	}

	return name;
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
		_ntree->tag |=  LIB_TAG_ID_RECALC_ALL;
	}
	else {
		_ntree->tag &= ~LIB_TAG_ID_RECALC_ALL;
	}
}

bool DataExporter::shouldSyncUndoneObject(BL::Object ob)
{
	// we are no undo-ing
	if (!m_is_undo_sync) {
		return false;
	}

	if (m_undo_stack.size() < 2) {
		BLI_assert(!"Trying to do undo/redo but stach did not have enought states");
		return false;
	}

	// check if object is the state we are currently undoing
	const auto & checkState = m_undo_stack[1];
	return checkState.find(getObjectUniqueKey(ob)) != checkState.end();
}

bool DataExporter::isObjectInThisSync(BL::Object ob)
{
	return m_undo_stack.front().find(getObjectUniqueKey(ob)) != m_undo_stack.front().end();
}

void DataExporter::saveSyncedObject(BL::Object ob)
{
	const auto key = getObjectUniqueKey(ob);
	m_undo_stack.front().insert(key);
}

void DataExporter::syncStart(bool isUndoSync)
{
	m_is_undo_sync = isUndoSync;
	m_undo_stack.push_front(UndoStateObjects());
}

void DataExporter::syncEnd() {
	if (m_is_undo_sync) {
		if (m_undo_stack.size() < 2) {
			BLI_assert(!"Trying to do undo/redo but stach did not have enought states");
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

