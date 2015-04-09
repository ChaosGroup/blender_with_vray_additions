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

#ifndef VRAY_FOR_BLENDER_DATA_EXPORTER_H
#define VRAY_FOR_BLENDER_DATA_EXPORTER_H

#include "vfb_plugin_exporter.h"
#include "vfb_export_settings.h"
#include "vfb_typedefs.h"
#include "vfb_params_desc.h"


using namespace VRayForBlender;


const char* const EnvironmentMappingType[] = {
    "angular",
    "cubic",
    "spherical",
    "mirror_ball",
    "screen",
    "max_spherical",
    "spherical_vray",
    "max_cylindrical",
    "max_shrink_wrap",
    NULL
};


struct ObjectContext {
	ObjectContext():
	    scene(PointerRNA_NULL),
	    data(PointerRNA_NULL),
	    object(PointerRNA_NULL)
	{}

	BL::Scene      scene;
	BL::BlendData  data;
	BL::Object     object;

	std::string    override_material;
};


class NodeContext {
public:
	NodeContext() {}

	BL::NodeTree getNodeTree() {
		if(parent.size())
			return parent.back();
		return BL::NodeTree(PointerRNA_NULL);
	}
	void pushParentTree(BL::NodeTree nt) {
		parent.push_back(nt);
	}
	BL::NodeTree popParentTree() {
		BL::NodeTree nt = parent.back();
		parent.pop_back();
		return nt;
	}

	BL::NodeGroup getGroupNode() {
		if(group.size())
			return group.back();
		return BL::NodeGroup(PointerRNA_NULL);
	}
	void pushGroupNode(BL::NodeGroup gr) {
		group.push_back(gr);
	}
	BL::NodeGroup popGroupNode() {
		BL::NodeGroup gr = group.back();
		group.pop_back();
		return gr;
	}

	ObjectContext  object_context;

private:
	// If we are exporting group node we have to treat
	// group ntree's nodes as nodes of the current tree
	// to prevent plugin overriding.
	//
	std::vector<BL::NodeTree>  parent;
	std::vector<BL::NodeGroup> group;

};


#define VRayNodeExportParam  BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, NodeContext *context


struct DataDefaults {
	DataDefaults():
	    default_material(AttrPlugin()),
	    override_material(AttrPlugin())
	{}

	AttrValue  default_material;
	AttrValue  override_material;
};


class DataExporter {
public:
	enum UserAttributeType {
		UserAttributeInt = 0,
		UserAttributeFloat,
		UserAttributeColor,
		UserAttributeString,
	};

	// Generate unique plugin name from node
	static std::string            GenPluginName(BL::Node node, BL::NodeTree ntree, NodeContext *context);

	// Get plugin type / id from node
	static ParamDesc::PluginType  GetNodePluginType(BL::Node node);
	static ParamDesc::PluginType  GetConnectedNodePluginType(BL::NodeSocket fromSocket);
	static std::string            GetNodePluginID(BL::Node node);
	static std::string            GetConnectedNodePluginID(BL::NodeSocket fromSocket);

	// Generate data name
	std::string                   getNodeName(BL::Object ob);
	std::string                   getMeshName(BL::Object ob);
	std::string                   getHairName(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSettings pset);

	void                          getNodeVectorCurveData(BL::NodeTree ntree, BL::Node node, AttrListFloat &points, AttrListInt &types);
	void                          exportRampAttribute(VRayNodeExportParam,
	                                     PluginDesc &attrs,
	                                     const std::string &texAttrName,
	                                     const std::string &colAttrName,
	                                     const std::string &posAttrName,
	                                     const std::string &typesAttrName="");
	int                           exportBitmapBuffer(VRayNodeExportParam, PluginDesc &attrs);

public:
	DataExporter():
	    m_data(PointerRNA_NULL),
	    m_scene(PointerRNA_NULL),
	    m_engine(PointerRNA_NULL),
	    m_context(PointerRNA_NULL),
	    m_exporter(nullptr)
	{}

	void              init(PluginExporter *exporter);
	void              init(PluginExporter *exporter, ExporterSettings settings);

	void              init_data(BL::BlendData data, BL::Scene scene, BL::RenderEngine engine, BL::Context context);
	void              init_defaults();

	void              setAttrsFromNode(VRayNodeExportParam, PluginDesc &pluginDesc, const std::string &pluginID, const ParamDesc::PluginType &pluginType);
	void              setAttrsFromNodeAuto(VRayNodeExportParam, PluginDesc &pluginDesc);
	void              setAttrFromPropGroup(PointerRNA *propGroup, ID *holder, const std::string &attrName, PluginDesc &pluginDesc);
	void              setAttrsFromPropGroupAuto(PluginDesc &pluginDesc, PointerRNA *propGroup, const std::string &pluginID);

	BL::Node          getConnectedNode(BL::NodeSocket fromSocket, NodeContext *context);

	AttrValue         exportGeomStaticMesh(BL::Object ob);
	AttrValue         exportGeomMayaHair(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSystemModifier psm);
	AttrValue         exportObject(BL::Object ob, bool check_updated=false);
	AttrValue         exportLight(BL::Object ob, bool check_updated=false);

	void              exportVRayEnvironment(NodeContext *context);

	AttrValue         exportMtlMulti(BL::BlendData bl_data, BL::Object bl_ob);
	AttrValue         exportMaterial(BL::Material b_ma, bool dont_export=false);

	void              getNodeSelectObjects(BL::Node node, ObList &obList);
	void              getUserAttributes(PointerRNA *ptr, StrVector &user_attributes);
	AttrValue         getObjectNameList(BL::Group group);
	AttrValue         getObjectName(BL::Object group);

	AttrValue         exportVRayNode(VRayNodeExportParam);
	AttrValue         exportVRayNodeAuto(VRayNodeExportParam, PluginDesc &pluginDesc);

	AttrValue         exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket socket, NodeContext *context, bool dont_export=false);
	AttrValue         exportDefaultSocket(BL::NodeTree ntree, BL::NodeSocket socket);
	AttrValue         exportSocket(BL::NodeTree ntree, BL::NodeSocket socket, NodeContext *context=NULL);
	AttrValue         exportSocket(BL::NodeTree ntree, BL::Node node, const std::string &socketName, NodeContext *context=NULL);

private:
	BL::Object        exportVRayNodeSelectObject(VRayNodeExportParam);
	BL::Group         exportVRayNodeSelectGroup(VRayNodeExportParam);

	AttrValue         exportVRayNodeBitmapBuffer(VRayNodeExportParam);
	AttrValue         exportVRayNodeBlenderOutputGeometry(VRayNodeExportParam);
	AttrValue         exportVRayNodeBlenderOutputMaterial(VRayNodeExportParam);
	AttrValue         exportVRayNodeBRDFLayered(VRayNodeExportParam);
	AttrValue         exportVRayNodeBRDFVRayMtl(VRayNodeExportParam);
	AttrValue         exportVRayNodeEnvFogMeshGizmo(VRayNodeExportParam);
	AttrValue         exportVRayNodeEnvironmentFog(VRayNodeExportParam);
	AttrValue         exportVRayNodeGeomDisplacedMesh(VRayNodeExportParam);
	AttrValue         exportVRayNodeGeomStaticSmoothedMesh(VRayNodeExportParam);
	AttrValue         exportVRayNodeLightMesh(VRayNodeExportParam);
	AttrValue         exportVRayNodeMatrix(VRayNodeExportParam);
	AttrValue         exportVRayNodeMetaImageTexture(VRayNodeExportParam);
	AttrValue         exportVRayNodeMtlMulti(VRayNodeExportParam);
	AttrValue         exportVRayNodePhxShaderSim(VRayNodeExportParam);
	AttrValue         exportVRayNodePhxShaderSimVol(VRayNodeExportParam);
	AttrValue         exportVRayNodeRenderChannelColor(VRayNodeExportParam);
	AttrValue         exportVRayNodeRenderChannelLightSelect(VRayNodeExportParam);
	AttrValue         exportVRayNodeSphereFade(VRayNodeExportParam);
	AttrValue         exportVRayNodeSphereFadeGizmo(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexEdges(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexFalloff(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexGradRamp(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexLayered(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexMayaFluid(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexMulti(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexRemap(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexSky(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexSoftbox(VRayNodeExportParam);
	AttrValue         exportVRayNodeTexVoxelData(VRayNodeExportParam);
	AttrValue         exportVRayNodeTransform(VRayNodeExportParam);
	AttrValue         exportVRayNodeUVWGenChannel(VRayNodeExportParam);
	AttrValue         exportVRayNodeUVWGenEnvironment(VRayNodeExportParam);
	AttrValue         exportVRayNodeUVWGenMayaPlace2dTexture(VRayNodeExportParam);
	AttrValue         exportVRayNodeVector(VRayNodeExportParam);
	AttrValue         exportVRayNodeVolumeVRayToon(VRayNodeExportParam);

	AttrValue         exportBlenderNodeNormal(VRayNodeExportParam);

private:
	BL::NodeTree      getNodeGroupTree(BL::Node node);
	BL::NodeSocket    getNodeGroupSocketReal(BL::Node node, BL::NodeSocket fromSocket);
	BL::Texture       getTextureFromProperty(PointerRNA *ptr, const std::string &propName);

public:
	StrSet            RenderChannelNames;

private:
	BL::BlendData     m_data;
	BL::Scene         m_scene;
	BL::RenderEngine  m_engine;
	BL::Context       m_context;

	PluginExporter   *m_exporter;
	ExporterSettings  m_settings;
	DataDefaults      m_defaults;

};

#endif // VRAY_FOR_BLENDER_DATA_EXPORTER_H
