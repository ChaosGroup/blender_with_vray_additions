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

#include <Python.h>

#include "exp_scene.h"

#include "cgr_string.h"
#include "cgr_rna.h"

#include <DNA_node_types.h>
#include <BLI_string.h>


#define IS_OBJECT_SELECT_NODE(n) \
( \
	n.bl_idname() == "VRayNodeSelectObject" || \
	n.bl_idname() == "VRayNodeSelectGroup" \
)


namespace VRayScene {


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


typedef std::vector<BL::Object> ObList;

struct AttrValue {
	float        frame;
	std::string  value;
	MHash        hash;
};

typedef std::map<std::string, AttrValue>  AttrCache;
typedef std::map<std::string, AttrCache>  PluginCache;


class VRayNodeCache {
public:
	bool pluginInCache(const std::string &pluginName) {
		return m_pluginCache.find(pluginName) != m_pluginCache.end();
	}

	void addToCache(const std::string &pluginName, const std::string &attrName, const float &frame, const std::string &attrValue, const MHash &hash) {
		m_pluginCache[pluginName][attrName].frame = frame;
		m_pluginCache[pluginName][attrName].value = attrValue;
		m_pluginCache[pluginName][attrName].hash  = hash;
	}

	const float getCachedFrame(const std::string &pluginName, const std::string &attrName) {
		return m_pluginCache[pluginName][attrName].frame;
	}

	const std::string getCachedValue(const std::string &pluginName, const std::string &attrName) {
		return m_pluginCache[pluginName][attrName].value;
	}

	const MHash getCachedHash(const std::string &pluginName, const std::string &attrName) {
		return m_pluginCache[pluginName][attrName].hash;
	}

	void clearCache() {
		m_pluginCache.clear();
	}

	void showCacheContents() {
		PluginCache::const_iterator cacheIt;
		for(cacheIt = m_pluginCache.begin(); cacheIt != m_pluginCache.end(); ++cacheIt) {
			const std::string  pluginName = cacheIt->first;
			const AttrCache   &attrCache  = cacheIt->second;

			std::cout << pluginName << std::endl;

			AttrCache::const_iterator attrIt;
			for(attrIt = attrCache.begin(); attrIt != attrCache.end(); ++attrIt) {
				const std::string attrName  = attrIt->first;

				const int         attrFrame = attrIt->second.frame;
				const std::string attrValue = attrIt->second.value;

				std::cout << "  " << attrName << " = " << attrValue << " [" << attrFrame << "]" << std::endl;
			}
		}
	}

private:
	PluginCache  m_pluginCache;

};


struct VRayObjectContext {
	VRayObjectContext() {
		sce  = NULL;
		main = NULL;
		ob   = NULL;

		mtlOverrideName = "";
	}

	Scene       *sce;
	Main        *main;
	Object      *ob;

	NodeAttrs    nodeAttrs;
	std::string  mtlOverrideName;
};


class VRayNodeContext {
public:
	VRayNodeContext() {}

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
			return BL::NodeGroup(group.back());
		return BL::NodeGroup(PointerRNA_NULL);
	}
	void pushGroupNode(BL::Node gr) {
		group.push_back(gr);
	}
	BL::NodeGroup popGroupNode() {
		BL::NodeGroup gr(group.back());
		group.pop_back();
		return gr;
	}

	VRayObjectContext  obCtx;

	operator bool() const {
		return true;
	}

private:
	// If we are exporting group node we have to treat
	// group ntree's nodes as nodes of the current tree
	// to prevent plugin overriding.
	//
	std::vector<BL::NodeTree>  parent;
	std::vector<BL::Node>      group;
};


#define VRayNodeExportParam  BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, VRayNodeContext &context


class VRayNodeExporter {
public:
	static void             init(BL::BlendData data);
	static void             getAttributesList(const std::string &pluginID, StrSet &attrSet, bool mappable=false);

	static std::string      getValueFromPropGroup(PointerRNA *propGroup, ID *holder, const std::string &attrName);

	static std::string      getPluginName(BL::Node node, BL::NodeTree ntree, VRayNodeContext &context);
	static std::string      getPluginType(BL::Node node);
	static std::string      getPluginID(BL::Node node);
	
	static BL::NodeTree     getNodeTree(BL::BlendData b_data, ID *id);

	static BL::Node         getNodeByType(BL::NodeTree nodeTree, const std::string &nodeType);

	static BL::NodeSocket   getSocketByName(BL::Node node, const std::string &socketName);
	static BL::NodeSocket   getSocketByAttr(BL::Node node, const std::string &attrName);

	static BL::NodeSocket   getOutputSocketByName(BL::Node node, const std::string &socketName);

	static BL::Node         getConnectedNode(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext &context);
	static BL::NodeSocket   getConnectedSocket(BL::NodeSocket socket);

	static std::string      getConnectedNodePluginName(BL::NodeTree ntree, BL::NodeSocket socket, VRayNodeContext &context);

	static void             getVRayNodeAttributes(AttributeValueMap &pluginAttrs,
												  VRayNodeExportParam,
												  const AttributeValueMap &customAttrs=AttributeValueMap(),
												  const std::string &customID="",
												  const std::string &customType="");

	static std::string      exportVRayNode(VRayNodeExportParam, const AttributeValueMap &manualAttrs=AttributeValueMap());

	static std::string      exportVRayNodeAttributes(VRayNodeExportParam,
													 const AttributeValueMap &customAttrs=AttributeValueMap(),
													 const std::string &customName="",
													 const std::string &customID="",
													 const std::string &customType="");

	static void             exportVRayEnvironment(VRayNodeContext &context);
	static std::string      exportMtlMulti(BL::BlendData bl_data, BL::Object bl_ob);

	static std::string      exportSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayNodeContext &context);
	static std::string      exportSocket(BL::NodeTree ntree, BL::Node node, const std::string &socketName, VRayNodeContext &context);

	static std::string      exportMaterial(BL::BlendData b_data, BL::Material b_ma);

	static void             getUserAttributes(PointerRNA *ptr, StrVector &user_attributes);
	static std::string      getObjectNameList(BL::Group group);
	static std::string      getObjectName(BL::Object group);
	static void             getNodeSelectObjects(BL::Node node, ObList &obList);
	static void             getNodeSelectLightsNames(BL::Node node, StrSet &obNames);

	static int              isObjectVisible(BL::Object b_ob);

	static BL::Object       getObjectByName(const std::string &name);

private:
	enum ExpMode {
		ExpModeNode = 0,
		ExpModePlugin,
		ExpModePluginName,
	};
	static void             exportLinkedSocketEx(BL::NodeTree ntree, BL::NodeSocket fromSocket, VRayNodeContext &context,
	                                             ExpMode expMode, BL::Node &outNode, std::string &outPlugin);
	static std::string      exportLinkedSocket(BL::NodeTree ntree, BL::NodeSocket socket, VRayNodeContext &context);
	static std::string      exportDefaultSocket(BL::NodeTree ntree, BL::NodeSocket socket);

	static std::string      exportVRayNodeBlenderOutputMaterial(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
																VRayNodeContext &context);
	static std::string      exportVRayNodeBlenderOutputGeometry(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
																VRayNodeContext &context);

	static std::string      exportVRayNodeLightMesh(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
													VRayNodeContext &context);
	static std::string      exportVRayNodeGeomDisplacedMesh(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
															VRayNodeContext &context);
	static std::string      exportVRayNodeGeomStaticSmoothedMesh(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
																 VRayNodeContext &context);

	static BL::Object       exportVRayNodeSelectObject(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, 
													   VRayNodeContext &context);
	static BL::Group        exportVRayNodeSelectGroup(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
													  VRayNodeContext &context);

	static std::string      exportVRayNodeBRDFLayered(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, 
													  VRayNodeContext &context);
	static std::string      exportVRayNodeBRDFVRayMtl(VRayNodeExportParam);

	static std::string      exportVRayNodeTexLayered(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
													 VRayNodeContext &context);
	static std::string      exportVRayNodeTexMulti(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
													 VRayNodeContext &context);
	static std::string      exportVRayNodeTexGradRamp(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, 
													  VRayNodeContext &context);
	static std::string      exportVRayNodeTexRemap(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
												   VRayNodeContext &context);
	static std::string      exportVRayNodeTexSoftbox(VRayNodeExportParam);

	static void             exportRampAttribute(VRayNodeExportParam,
												AttributeValueMap &attrs,
												const std::string &texAttrName,
												const std::string &colAttrName,
												const std::string &posAttrName,
												const std::string &typesAttrName="");

	static void             exportBitmapBuffer(VRayNodeExportParam, AttributeValueMap &attrs);
	static std::string      exportVRayNodeBitmapBuffer(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
											           VRayNodeContext &context);
	static std::string      exportVRayNodeTexVoxelData(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
													   VRayNodeContext &context);
	static std::string      exportVRayNodeTexSky(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
												 VRayNodeContext &context);
	static std::string      exportVRayNodeTexFalloff(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
													 VRayNodeContext &context);
	static std::string      exportVRayNodeTexEdges(VRayNodeExportParam);
	static std::string      exportVRayNodeTexMayaFluid(VRayNodeExportParam);

	static std::string      exportVRayNodeMtlMulti(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
												   VRayNodeContext &context);

	static std::string      exportVRayNodeTransform(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, 
													VRayNodeContext &context);
	static std::string      exportVRayNodeMatrix(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, 
												 VRayNodeContext &context);
	static std::string      exportVRayNodeVector(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket, 
												 VRayNodeContext &context);

	static std::string      exportBlenderNodeNormal(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
													VRayNodeContext &context);
	static std::string      exportVRayNodeEnvFogMeshGizmo(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
												          VRayNodeContext &context);
	static std::string      exportVRayNodeEnvironmentFog(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
														 VRayNodeContext &context);
	static std::string      exportVRayNodeUVWGenEnvironment(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
															VRayNodeContext &context);
	static std::string      exportVRayNodeUVWGenMayaPlace2dTexture(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
																   VRayNodeContext &context);
	static std::string      exportVRayNodeUVWGenChannel(VRayNodeExportParam);

	static std::string      exportVRayNodeRenderChannelLightSelect(BL::NodeTree ntree, BL::Node node, BL::NodeSocket fromSocket,
																   VRayNodeContext &context);

	static std::string      exportVRayNodeRenderChannelColor(VRayNodeExportParam);

	static std::string      exportVRayNodePhxShaderSimVol(VRayNodeExportParam);
	static std::string      exportVRayNodePhxShaderSim(VRayNodeExportParam);

	static std::string      exportVRayNodeSphereFade(VRayNodeExportParam);
	static std::string      exportVRayNodeSphereFadeGizmo(VRayNodeExportParam);
	static std::string      exportVRayNodeVolumeVRayToon(VRayNodeExportParam);

	static std::string      exportVRayNodeMetaImageTexture(VRayNodeExportParam);
	static std::string      exportVRayNodeMetaStandardMaterial(VRayNodeExportParam);

	static std::string      fromNodePluginID(BL::NodeSocket fromSocket);
	static std::string      fromNodePluginType(BL::NodeSocket fromSocket);

private:
	static BL::NodeTree     getNodeGroupTree(BL::Node node);
	static BL::NodeSocket   getNodeGroupSocketReal(BL::Node node, BL::NodeSocket fromSocket);

	static BL::Texture      getTextureFromIDRef(PointerRNA *ptr, const std::string &propName);

	static void             getNodeVectorCurveData(BL::NodeTree ntree, BL::Node node, StrVector &points, StrVector &types);

public:
	static StrSet           RenderChannelNames;

};


class VRayNodePluginExporter {
public:
	static int   exportPlugin(const std::string &pluginType, const std::string &pluginID, const std::string &pluginName, const AttributeValueMap &pluginAttrs);
	static void  clearNamesCache();
	static void  clearNodesCache();

private:
	static VRayNodeCache    m_nodeCache;
	static StrSet           m_namesCache;

};

} // namespace VRayScene

#endif // CGR_EXPORT_NODES_H
