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
#include "vfb_render_view.h"

#include "DNA_ID.h"
#include <map>
#include <unordered_set>
#include <stack>
#include <vector>
#include <deque>

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
	    data(PointerRNA_NULL),
	    scene(PointerRNA_NULL),
	    object(PointerRNA_NULL),
	    merge_uv(false)
	{}

	ObjectContext(BL::BlendData data, BL::Scene scene, BL::Object object):
	    data(data),
	    scene(scene),
	    object(object),
	    merge_uv(false)
	{}

	BL::BlendData  data;
	BL::Scene      scene;
	BL::Object     object;

	std::string    override_material;
	bool           merge_uv;
};


class NodeContext {
public:
	typedef std::vector<BL::NodeTree> NodeTreeVector;
	typedef std::vector<BL::Node>     NodeVector;

	NodeContext() {}
	NodeContext(BL::BlendData data, BL::Scene scene, BL::Object object)
	    : object_context(data, scene, object)
	{}

	BL::NodeTree getNodeTree() {
		BL::NodeTree ntree(PointerRNA_NULL);
		if (parent.size()) {
			ntree = parent.back();
		}
		return ntree;
	}

	void pushParentTree(BL::NodeTree nt) {
		parent.push_back(nt);
	}

	BL::NodeTree popParentTree() {
		BL::NodeTree nt(parent.back());
		parent.pop_back();
		return nt;
	}

	BL::NodeGroup getGroupNode() {
		BL::Node node(PointerRNA_NULL);
		if (group.size()) {
			node = group.back();
		}
		return BL::NodeGroup(node);
	}

	void pushGroupNode(BL::Node gr) {
		group.push_back(gr);
	}

	BL::NodeGroup popGroupNode() {
		BL::NodeGroup node(group.back());
		group.pop_back();
		return node;
	}

	ObjectContext  object_context;
	NodeTreeVector parent;
	NodeVector     group;
};


// Used to skip already exported objects
//
struct IdCache {
	int contains(BL::ID id) {
		return m_data.find(id.ptr.data) != m_data.end();
	}

	int contains(int id) {
		return m_data.find((void*)(intptr_t)id) != m_data.end();
	}

	void insert(BL::ID id) {
		m_data.insert(id.ptr.data);
	}

	void insert(int id) {
		m_data.insert((void*)(intptr_t)id);
	}

	void clear() {
		m_data.clear();
	}

private:
	std::set<void*> m_data;

};


// Used to track object deletion / creation
//
struct IdTrack {

	enum PluginType {
		NONE, CLIPPER, DUPLI_INSTACER, DUPLI_NODE, DUPLI_MODIFIER, HAIR
	};

	struct PluginInfo {
		bool used;
		PluginType type;
	};

	struct IdDep {
		IdDep(): object(PointerRNA_NULL) {}
		std::unordered_map<std::string, PluginInfo> plugins;
		BL::Object object;
		int used;
	};

	int               contains(BL::Object ob);
	void              clear();
	void              insert(BL::Object ob, const std::string &plugin, PluginType type = PluginType::NONE);
	void              reset_usage();

	typedef std::map<std::string, IdDep> TrackMap;
	TrackMap data;
};


struct DataDefaults {
	DataDefaults()
	    : default_material(AttrPlugin())
	    , override_material(AttrPlugin())
	{}

	AttrValue  default_material;
	AttrValue  override_material;
};


struct ObjectOverridesAttrs {
	ObjectOverridesAttrs()
	    : useInstancer(true)
	    , override(false)
	    , visible(true)
	    , id(0)
	    , dupliHolder(PointerRNA_NULL)
	{}

	operator bool() const {
		return override;
	}

	bool           useInstancer;
	bool           override;
	int            visible;
	AttrTransform  tm;
	int            id;
	BL::Object     dupliHolder;
	std::string    namePrefix;
};


class DataExporter {
	// one state hold all objects that were exported on a given sync
	typedef std::unordered_set<std::string> UndoStateObjects;
	typedef std::deque<UndoStateObjects> UndoStack;
public:
	enum ObjectVisibility {
		HIDE_NONE                = 0,
		HIDE_ALL                 = ~HIDE_NONE,
		HIDE_DUPLI_EMITER        = 1,
		HIDE_PARTICLE_EMITER     = 1 << 1,
		HIDE_LIST                = 1 << 2,
		HIDE_VIEWPORT            = 1 << 3,
		HIDE_RENDER              = 1 << 4,
		HIDE_LAYER               = 1 << 5,
	};

	enum ExpMode {
		ExpModeNode = 0,
		ExpModePlugin,
		ExpModePluginName,
	};

	enum UserAttributeType {
		UserAttributeInt = 0,
		UserAttributeFloat,
		UserAttributeColor,
		UserAttributeString,
	};

	typedef std::map<BL::Material, AttrValue> MaterialCache;
	typedef std::unordered_map<std::string, std::vector<BL::Object>> ObjectHideMap;

	DataExporter()
	    : m_data(PointerRNA_NULL)
	    , m_scene(PointerRNA_NULL)
	    , m_engine(PointerRNA_NULL)
	    , m_context(PointerRNA_NULL)
	    , m_view3d(PointerRNA_NULL)
	    , m_is_local_view(false)
	    , m_active_camera(PointerRNA_NULL)
	    , m_exporter(nullptr)
	{}

	// Generate unique plugin name from node
	static std::string            GenPluginName(BL::Node node, BL::NodeTree ntree, NodeContext &context);

	// Get plugin type / id from node
	static ParamDesc::PluginType  GetNodePluginType(BL::Node node);
	static ParamDesc::PluginType  GetConnectedNodePluginType(BL::NodeSocket fromSocket);
	static std::string            GetNodePluginID(BL::Node node);
	static std::string            GetConnectedNodePluginID(BL::NodeSocket fromSocket);

	// Generate data name
	std::string       getNodeName(BL::Object ob);
	std::string       getMeshName(BL::Object ob);
	std::string       getHairName(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSettings pset);
	std::string       getLightName(BL::Object ob);
	static std::string       getIdUniqueName(BL::Pointer ob);
	static std::string       getIdUniqueName(ID * id);

	static void              tag_ntree(BL::NodeTree ntree, bool updated=true);

	std::vector<BL::Object>       getObjectList(const std::string ob_name, const std::string group_name);

	void              fillNodeVectorCurveData(BL::NodeTree ntree, BL::Node node, AttrListFloat &points, AttrListInt &types);
	void              fillRampAttributes(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context,
	                                     PluginDesc &attrs,
	                                     const std::string &texAttrName,
	                                     const std::string &colAttrName,
	                                     const std::string &posAttrName,
	                                     const std::string &typesAttrName="");
	int               fillBitmapAttributes(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context, PluginDesc &attrs);
	void              fillCameraData(BL::Object &cameraObject, ViewParams &viewParams);
	void              fillPhysicalCamera(ViewParams &viewParams, PluginDesc &physCamDesc);
	void              fillMtlMulti(BL::Object ob, PluginDesc &pluginDesc);

	void              init(PluginExporter *exporter);
	void              init(PluginExporter *exporter, ExporterSettings settings);
	void              sync();

	// will reset all state that is kept for one sync, must be called after each sync
	void              resetSyncState();

	bool              isObjectInThisSync(BL::Object ob);
	// checks if we are currently in undo sync and if yes, checks if the object passed was changed in the sync we are unding currently
	bool              shouldSyncUndoneObject(BL::Object ob);
	// saves that this object was synced on current sync
	void              saveSyncedObject(BL::Object ob);

	void              syncStart(bool isUndoSync);
	void              syncEnd();

	void              init_data(BL::BlendData data, BL::Scene scene, BL::RenderEngine engine, BL::Context context, BL::SpaceView3D view3d);
	void              init_defaults();
	void              setComputedLayers(uint32_t layers, bool is_local_view);

	void              setAttrsFromNode(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context, PluginDesc &pluginDesc, const std::string &pluginID, const ParamDesc::PluginType &pluginType);
	void              setAttrsFromNodeAuto(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context, PluginDesc &pluginDesc);
	void              setAttrFromPropGroup(PointerRNA *propGroup, ID *holder, const std::string &attrName, PluginDesc &pluginDesc);
	void              setAttrsFromPropGroupAuto(PluginDesc &pluginDesc, PointerRNA *propGroup, const std::string &pluginID);


	BL::NodeSocket    getSocketByAttr(BL::Node node, const std::string &attrName);
	BL::Node          getConnectedNode(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         getConnectedNodePluginName(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context);

	void              getSelectorObjectNames(BL::Node node, AttrListPlugin & names);
	void              getSelectorObjectList(BL::Node node, ObList &obList);
	void              getUserAttributes(PointerRNA *ptr, StrVector &user_attributes);
	AttrValue         getObjectNameList(BL::Group group);

	AttrValue         exportMaterial(BL::Material ma, BL::Object ob);
	AttrValue         exportVRayClipper(BL::Object ob, bool check_updated = false, const ObjectOverridesAttrs & = ObjectOverridesAttrs());
	AttrValue         exportObject(BL::Object ob, bool check_updated = false, const ObjectOverridesAttrs & = ObjectOverridesAttrs());
	AttrValue         exportLight(BL::Object ob, bool check_updated = false, const ObjectOverridesAttrs & = ObjectOverridesAttrs());
	void              exportHair(BL::Object ob, BL::ParticleSystemModifier psm, BL::ParticleSystem psys, bool check_updated = false);
	AttrValue         exportVrayInstacer2(BL::Object ob, AttrInstancer & instacer, IdTrack::PluginType dupliType, bool exportObTm = false);
	void              exportEnvironment(NodeContext &context);

	AttrValue         exportSingleMaterial(BL::Object &ob);
	AttrValue         exportMtlMulti(BL::Object ob);
	AttrValue         exportGeomStaticMesh(BL::Object ob, const ObjectOverridesAttrs &);
	AttrValue         exportGeomMayaHair(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSystemModifier psm);
	AttrPlugin        exportRenderView(ViewParams &viewParams);
	AttrPlugin        exportSettingsCameraDof(ViewParams &viewParams);
	AttrPlugin        exportCameraPhysical(ViewParams &viewParams);
	AttrPlugin        exportCameraDefault(const ViewParams &viewParams);
	AttrPlugin        exportCameraSettings(ViewParams &viewParams);

	BL::Node          getNtreeSelectedNode(BL::NodeTree &ntree);

	void              exportLinkedSocketEx2(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context,
	                                        ExpMode expMode, BL::Node &outNode, AttrValue &outPlugin, BL::Node toNode);
	void              exportLinkedSocketEx(BL::NodeTree &ntree, BL::NodeSocket &fromSocket, NodeContext &context,
	                                       ExpMode expMode, BL::Node &outNode, AttrValue &outPlugin);
	AttrValue         exportLinkedSocket(BL::NodeTree &ntree, BL::NodeSocket &socket, NodeContext &context);
	AttrValue         exportDefaultSocket(BL::NodeTree &ntree, BL::NodeSocket &socket);
	AttrValue         exportSocket(BL::NodeTree &ntree, BL::NodeSocket &socket, NodeContext &context);
	AttrValue         exportSocket(BL::NodeTree &ntree, BL::Node &node, const std::string &socketName, NodeContext &context);

	AttrValue         exportVRayNode(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeAuto(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context, PluginDesc &pluginDesc);

	AttrValue         getDefaultMaterial();

	bool              hasLayerChanged() const { return m_layer_changed; }
	bool              hasDupli(BL::Object ob);
	// bits set in ObjectVisibility are used to check only some of the criteria
	bool              isDupliVisible(BL::Object ob, ObjectVisibility ignore = HIDE_ALL);
	// bits set in ObjectVisibility are used to check only some of the criteria
	bool              isObjectVisible(BL::Object ob, ObjectVisibility ignore = HIDE_ALL);

	void              clearMaterialCache();

	void              setActiveCamera(BL::Camera camera);
	void              refreshHideLists();
	bool              isObjectInHideList(BL::Object ob, const std::string listName) const;

private:
	BL::Object        exportVRayNodeSelectObject(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	BL::Group         exportVRayNodeSelectGroup(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);

	AttrValue         exportVRayNodeBitmapBuffer(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeBlenderOutputGeometry(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeBlenderOutputMaterial(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeBRDFLayered(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeBRDFVRayMtl(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeEnvFogMeshGizmo(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeEnvironmentFog(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeGeomDisplacedMesh(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeGeomStaticSmoothedMesh(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeLightMesh(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeMatrix(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeMetaImageTexture(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeMetaStandardMaterial(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeMtlMulti(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodePhxShaderSim(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodePhxShaderSimVol(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeRenderChannelColor(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeRenderChannelLightSelect(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeSphereFade(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeSphereFadeGizmo(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeSmokeDomain(BL::NodeTree ntree, BL::Node node, BL::Object domainOb, NodeContext &context);
	AttrValue         exportVRayNodeTexEdges(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTexFalloff(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTexLayered(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTexMayaFluid(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTexMulti(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTexSky(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTexVoxelData(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTexMeshVertexColorChannel(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeTransform(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeUVWGenChannel(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeUVWGenEnvironment(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeUVWGenMayaPlace2dTexture(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeVector(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);
	AttrValue         exportVRayNodeVolumeVRayToon(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);

	AttrValue         exportBlenderNodeNormal(BL::NodeTree &ntree, BL::Node &node, BL::NodeSocket &fromSocket, NodeContext &context);

public:
	StrSet            RenderChannelNames;
	IdCache           m_id_cache;
	IdTrack           m_id_track;

	bool              m_is_undo_sync;
	UndoStack         m_undo_stack;


private:
	BL::BlendData     m_data;
	BL::Scene         m_scene;
	BL::RenderEngine  m_engine;
	BL::Context       m_context;
	BL::SpaceView3D   m_view3d;

	bool              m_is_local_view;
	bool              m_layer_changed;
	bool              m_is_preview;

	BL::Camera        m_active_camera;
	// should be set on each sync with setComputedLayers
	uint32_t          m_computedLayers;
	uint32_t          m_scene_layers;
	ObjectHideMap     m_hide_lists;

	PluginExporter   *m_exporter;
	ExporterSettings  m_settings;
	DataDefaults      m_defaults;

	EvalMode          m_evalMode;
	MaterialCache     m_exported_materials;

};

// implemented in vfb_export_object.cpp
uint32_t to_int_layer(const BlLayers & layers);
uint32_t get_layer(BL::Object ob, bool use_local, uint32_t scene_layers);
#endif // VRAY_FOR_BLENDER_DATA_EXPORTER_H
