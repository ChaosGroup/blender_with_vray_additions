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

#include "cgr_config.h"

#include "GeomMayaHair.h"
#include "GeomStaticMesh.h"

#include "cgr_vrscene.h"
#include "cgr_string.h"
#include "cgr_blender_data.h"
#include "cgr_json_plugins.h"
#include "cgr_rna.h"
#include "cgr_paths.h"

#include "exp_scene.h"
#include "exp_nodes.h"
#include "exp_api.h"

#include "PIL_time.h"
#include "BLI_string.h"
#include "BKE_material.h"
#include "BKE_global.h"
#include "BLI_math_matrix.h"
#include "BLI_utildefines.h"

extern "C" {
#  include "BLI_timecode.h"
#  include "RE_engine.h"
#  include "DNA_particle_types.h"
#  include "DNA_modifier_types.h"
#  include "DNA_material_types.h"
#  include "DNA_lamp_types.h"
#  include "DNA_camera_types.h"
#  include "BKE_anim.h"
#  include "DNA_windowmanager_types.h"
}

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>

/// Cast to float [4][4].
#define _m4(x) ((float (*)[4])(x))

static bool ob_is_mesh(BL::Object ob)
{
	return (ob.type() == BL::Object::type_MESH    ||
	        ob.type() == BL::Object::type_CURVE   ||
	        ob.type() == BL::Object::type_SURFACE ||
	        ob.type() == BL::Object::type_FONT    ||
	        ob.type() == BL::Object::type_META);
}

static bool ob_is_light(BL::Object ob)
{
	return (ob.type() == BL::Object::type_LAMP);
}

static bool ob_is_mesh_light(BL::Object ob)
{
	bool is_mesh_light = false;

	BL::NodeTree ntree = VRayNodeExporter::getNodeTree(ExporterSettings::gSet.b_data, (ID*)ob.ptr.data);
	if (ntree) {
		BL::Node nodeOutput = VRayNodeExporter::getNodeByType(ntree, "VRayNodeObjectOutput");
		if (nodeOutput) {
			BL::NodeSocket geometrySocket = VRayNodeExporter::getSocketByName(nodeOutput, "Geometry");
			if (geometrySocket && geometrySocket.is_linked()) {
				VRayNodeContext ctx;
				BL::Node geomNode = VRayNodeExporter::getConnectedNode(ntree, geometrySocket, ctx);
				if (geomNode && geomNode.bl_idname() == "VRayNodeLightMesh") {
					is_mesh_light = true;
				}
			}
		}
	}

	return is_mesh_light;
}

static bool ob_is_empty(BL::Object ob)
{
	return (ob.type() == BL::Object::type_EMPTY);
}

static int ob_on_visible_layer(BL::Object ob) {
	return (((Object*)ob.ptr.data)->lay & ExporterSettings::gSet.m_activeLayers);
}

static int ob_is_visible_for_render(BL::Object ob)
{
	return !(ob.hide_render());
}

static int ob_is_visible(BL::Object ob)
{
	return ob_on_visible_layer(ob) && ob_is_visible_for_render(ob);
}

static int ob_has_dupli(BL::Object ob)
{
	return ((ob.dupli_type() != BL::Object::dupli_type_NONE) && (ob.dupli_type() != BL::Object::dupli_type_FRAMES));
}

static int ob_has_hair(BL::Object ob)
{
	BL::Object::modifiers_iterator mdIt;
	for (ob.modifiers.begin(mdIt); mdIt != ob.modifiers.end(); ++mdIt) {
		BL::Modifier mod(*mdIt);
		if (mod.show_render() && (mod.type() == BL::Modifier::type_PARTICLE_SYSTEM)) {
			BL::ParticleSystemModifier pmod(mod);
			BL::ParticleSystem psys(pmod.particle_system());
			if (psys) {
				BL::ParticleSettings pset(psys.settings());
				if (pset && (pset.type() == BL::ParticleSettings::type_HAIR) && (pset.render_type() == BL::ParticleSettings::render_type_PATH)) {
					return true;
				}
			}
		}
	}
	return false;
}

static int ob_is_smoke_domain(BL::Object ob)
{
	BL::Object::modifiers_iterator mdIt;
	for (ob.modifiers.begin(mdIt); mdIt != ob.modifiers.end(); ++mdIt) {
		BL::Modifier md(*mdIt);
		if (md.type() == BL::Modifier::type_SMOKE) {
			BL::SmokeModifier smd(md);
			if (smd.smoke_type() == BL::SmokeModifier::smoke_type_DOMAIN) {
				return true;
			}
		}
	}
	return false;
}

static int ob_is_in_parents_dupli(BL::Object ob)
{
	BL::Object parent(ob.parent());
	while (parent) {
		if (ob_has_dupli(parent)) {
			return true;
		}
		parent = parent.parent();
	}

	return false;
}

static int ob_is_duplicator_renderable(BL::Object ob)
{
	bool is_renderable = true;

	// Dulpi
	if (ob_has_dupli(ob)) {
		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		is_renderable = RNA_boolean_get(&vrayObject, "dupliShowEmitter");
	}

	// Particles
	// Particle system "Show / Hide Emitter" has priority over dupli
	if (ob.particle_systems.length()) {
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


VRsceneExporter::VRsceneExporter()
{
	PRINT_INFO("VRsceneExporter::VRsceneExporter()");

	exportSceneInit();
}


VRsceneExporter::~VRsceneExporter()
{
	PRINT_INFO("VRsceneExporter::~VRsceneExporter()");

	m_skipObjects.clear();
}


void VRsceneExporter::addSkipObject(void *obPtr)
{
	m_skipObjects.insert(obPtr);
}


void VRsceneExporter::addToHideFromViewList(const std::string &listKey, void *obPtr)
{
	PRINT_INFO("Adding object '%s' to hide list '%s'...",
	           ((ID*)obPtr)->name, listKey.c_str());

	if(listKey == "all")
		m_hideFromView.visibility.insert(obPtr);
	else if (listKey == "camera")
		m_hideFromView.camera_visibility.insert(obPtr);
	else if (listKey == "gi")
		m_hideFromView.gi_visibility.insert(obPtr);
	else if (listKey == "reflect")
		m_hideFromView.reflections_visibility.insert(obPtr);
	else if (listKey == "refract")
		m_hideFromView.refractions_visibility.insert(obPtr);
	else if (listKey == "shadows")
		m_hideFromView.shadows_visibility.insert(obPtr);
}


void VRsceneExporter::exportSceneInit()
{
	VRayExportable::clearCache();
	ExporterSettings::gSet.init();

	// Prepass LightLinker
	m_lightLinker.init(ExporterSettings::gSet.b_data, ExporterSettings::gSet.b_scene);
	m_lightLinker.prepass();
	m_lightLinker.setSceneSet(&m_exportedObjects);

	Node::m_lightLinker = &m_lightLinker;
	Node::m_scene_nodes = &m_exportedObjects;
}


void VRsceneExporter::exportObjectsPre()
{
	// Clear caches
	m_exportedObjects.clear();

	instancer.freeData();
}


int VRsceneExporter::exportScene(const int &exportNodes, const int &exportGeometry)
{
	PRINT_INFO("VRsceneExporter::exportScene()");

	ExporterSettings::gSet.m_exportNodes  = exportNodes;
	ExporterSettings::gSet.m_exportMeshes = exportGeometry;

	double timeMeasure = 0.0;
	char   timeMeasureBuf[32];

	PRINT_INFO_EX("Exporting data for frame %g...", ExporterSettings::gSet.m_frameCurrent);
	timeMeasure = PIL_check_seconds_timer();

	Base *base = NULL;

	ExporterSettings::gSet.b_engine.update_progress(0.0f);

	PointerRNA sceneRNA;
	RNA_id_pointer_create((ID*)ExporterSettings::gSet.m_sce, &sceneRNA);
	BL::Scene bl_sce(sceneRNA);

	size_t nObjects = bl_sce.objects.length();

	float  expProgress = 0.0f;
	float  expProgStep = 1.0f / nObjects;
	int    progUpdateCnt = nObjects > 3000 ? 1000 : 100;
	if(nObjects > 3000) {
		progUpdateCnt = 1000;
	}
	else if(nObjects < 200) {
		progUpdateCnt = 10;
	}
	else {
		progUpdateCnt = 100;
	}

	exportObjectsPre();

	VRayNodeContext nodeCtx;
	VRayNodeExporter::exportVRayEnvironment(nodeCtx);

	// Export stuff
	int exportInterrupt = false;

	base = (Base*)ExporterSettings::gSet.m_sce->base.first;
	nObjects = 0;
	while(base) {
		if(ExporterSettings::gSet.b_engine.test_break()) {
			ExporterSettings::gSet.b_engine.report(RPT_WARNING, "Export interrupted!");
			exportInterrupt = true;
			break;
		}

		Object *ob = base->object;
		base = base->next;

		// PRINT_INFO("Processing '%s'...", ob->id.name);

		// Skip object here, but not in dupli!
		// Dupli could be particles and it's better to
		// have animated 'visible' param there
		//
		if(ob->restrictflag & OB_RESTRICT_RENDER)
			continue;

		if(NOT(ob->lay & ExporterSettings::gSet.m_activeLayers))
			continue;

		if(m_skipObjects.count((void*)&ob->id)) {
			PRINT_INFO("Skipping object: %s", ob->id.name);
			continue;
		}

		PointerRNA objectRNA;
		RNA_id_pointer_create((ID*)ob, &objectRNA);
		BL::Object bl_ob(objectRNA);

		exportObjectBase(bl_ob);

		expProgress += expProgStep;
		nObjects++;
		if((nObjects % progUpdateCnt) == 0) {
			ExporterSettings::gSet.b_engine.update_progress(expProgress);
		}
	}

	if(NOT(exportInterrupt)) {
		// Export materials
		//
		BL::BlendData b_data(PointerRNA_NULL);
		BL::BlendData::materials_iterator maIt;

		if (ExporterSettings::gSet.b_engine.is_preview()) {
			RenderEngine *re = (RenderEngine*)ExporterSettings::gSet.b_engine.ptr.data;
			if(re->type->preview_main) {
				PointerRNA previewMainPtr;
				RNA_id_pointer_create((ID*)re->type->preview_main, &previewMainPtr);
				b_data = BL::BlendData(previewMainPtr);
			}
		}

		if(NOT(b_data))
			b_data = ExporterSettings::gSet.b_data;

		if(ExporterSettings::gSet.m_mtlOverride)
			VRayNodeExporter::exportMaterial(b_data, ExporterSettings::gSet.m_mtlOverride);

		// Export materials checking if we don't need to override it with global
		// override
		for(b_data.materials.begin(maIt); maIt != b_data.materials.end(); ++maIt) {
			BL::Material b_ma = *maIt;
			if(ExporterSettings::gSet.m_mtlOverride && Node::DoOverrideMaterial(b_ma))
				continue;
			VRayNodeExporter::exportMaterial(b_data, b_ma);
		}

		exportObjectsPost();
	}

	ExporterSettings::gSet.b_engine.update_progress(1.0f);

	BLI_timecode_string_from_time_simple(timeMeasureBuf, sizeof(timeMeasureBuf), PIL_check_seconds_timer()-timeMeasure);

	exportClearCaches();

	if(exportInterrupt) {
		PRINT_INFO_EX("Exporting data for frame %g is interruped! [%s]",
		              ExporterSettings::gSet.m_frameCurrent, timeMeasureBuf);
		return 1;
	}

	PRINT_INFO_EX("Exporting data for frame %g done [%s]",
	              ExporterSettings::gSet.m_frameCurrent, timeMeasureBuf);

	return 0;
}


void VRsceneExporter::exportObjectsPost()
{
	// Export subframe data
	//
	for (SubframeObjects::const_iterator sfIt = m_subframeObjects.begin(); sfIt != m_subframeObjects.end(); ++sfIt) {
		const int        subframes = sfIt->first;
		const ObjectSet &objects   = sfIt->second;

		BL::Scene scene(ExporterSettings::gSet.b_scene);

		// Store settings
		const float m_frameCurrent = ExporterSettings::gSet.m_frameCurrent;
		const float m_frameStep    = ExporterSettings::gSet.m_frameStep;
		const int   frame_current  = scene.frame_current();

		const float subframe_step = 1.0f / (subframes + 1);

		// Don't check cache
		ExporterSettings::gSet.m_anim_check_cache = false;

		// We've already exported "frame_current"
		for (float f = subframe_step; f < 1.0f; f += subframe_step) {
			const float float_frame = frame_current + f;

			// Set exporter settings
			scene.frame_set(frame_current, f);
			VRayExportable::initInterpolate(float_frame);
			ExporterSettings::gSet.m_frameCurrent = float_frame;
			ExporterSettings::gSet.m_frameStep    = subframe_step;

			BL::Scene scene(ExporterSettings::gSet.b_scene);
			for (ObjectSet::const_iterator obIt = objects.begin(); obIt != objects.end(); ++obIt) {
				BL::Object ob(*obIt);

				PRINT_INFO("Exporting sub-frame %.3f for object %s",
				           scene.frame_current_final(), ob.name().c_str());

				exportNodeEx(ob);
			}
		}

		// Restore settings
		scene.frame_set(frame_current, 0.0f);
		VRayExportable::initInterpolate(frame_current);
		ExporterSettings::gSet.m_frameCurrent = m_frameCurrent;
		ExporterSettings::gSet.m_frameStep    = m_frameStep;
	}

	// Export dupli/particle systems
	exportDupli();

	// Light linker settings only for the first frame
	if (ExporterSettings::gSet.IsFirstFrame())
		m_lightLinker.write(ExporterSettings::gSet.m_fileObject);

	m_hideFromView.clear();
}


int VRsceneExporter::is_interrupted()
{
	bool export_interrupt = false;
	if (ExporterSettings::gSet.b_engine && ExporterSettings::gSet.b_engine.test_break()) {
		export_interrupt = true;
	}
	return export_interrupt;
}


void VRsceneExporter::exportClearCaches()
{
	m_hideFromView.clear();
	m_subframeObjects.clear();

	// Clean plugin names cache
	VRayNodePluginExporter::clearNamesCache();

	// Clean Alt-D instances cache
	Node::FreeMeshCache();
}

/// Checks if object is a valid object for Instancer.
/// @param ob Blender object.
static int isValidInstancerObject(BL::Object ob)
{
	int isValidItem = true;

	if (ob_is_smoke_domain(ob)) {
		isValidItem = false;
	}
	else {
		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		if (RNA_boolean_get(&vrayObject, "overrideWithScene")) {
			isValidItem = false;
		}
		else {
			PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");
			if (RNA_boolean_get(&vrayClipper, "enabled")) {
				isValidItem = false;
			}
		}
	}

	return isValidItem;
}

void VRsceneExporter::exportObjectBase(BL::Object ob)
{
	if (!(ob_is_mesh(ob) || ob_is_light(ob) || ob_is_empty(ob)))
		return;

	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");

	PRINT_INFO("Processing object %s", ob.name().c_str());

	int data_updated = RNA_int_get(&vrayObject, "data_updated");
	if (data_updated) {
		PRINT_INFO("Base object %s (update: %i)", ob.name().c_str(), data_updated);
	}

	if (!ob_is_in_parents_dupli(ob)) {
		if (ob.is_duplicator()) {
			bool process_dupli = true;

			// If object is a dupli group holder and it's not animated -
			// export it only for the first frame
			//
			if (ExporterSettings::gSet.DoUpdateCheck()) {
				if (ob.dupli_type() != BL::Object::dupli_type_NONE) {
					if (NOT(IsObjectUpdated((Object*)ob.ptr.data) ||
					        IsObjectDataUpdated((Object*)ob.ptr.data))) {
						process_dupli = false;
					}
				}
			}

			if (process_dupli) {
				ob.dupli_list_create(ExporterSettings::gSet.b_scene, 2);

				// If need to override object ID.
				const int dupliGroupIDOverride = RNA_int_get(&vrayObject, "dupliGroupIDOverride");

				int dupliIndex = 0;
				BL::Object::dupli_list_iterator dupliIt;
				for (ob.dupli_list.begin(dupliIt); dupliIt != ob.dupli_list.end(); ++dupliIt, ++dupliIndex) {
					if (is_interrupted())
						break;

					BL::DupliObject dupliObject(*dupliIt);
					BL::Object      duplicatedObject(dupliObject.object());

					if (!dupliObject.hide() && !duplicatedObject.hide_render()) {
						const MHash particleID = VRayForBlender::getParticleID(ob, dupliObject, dupliIndex);

						const int objectID = (dupliGroupIDOverride == VRayForBlender::InstancerItem::useOriginalObjectID)
						                     ? duplicatedObject.pass_index()
						                     : dupliGroupIDOverride;

						// Export new light for every DupliObject.
						if (ob_is_light(duplicatedObject) ||
							ob_is_mesh_light(duplicatedObject))
						{
							static boost::format dupliLightFtm("DupliLight|%u|%s");

							NodeAttrs attrs;
							attrs.override = true;
							attrs.namePrefix = boost::str(dupliLightFtm % particleID % GetIDName(duplicatedObject));
							attrs.tm = dupliObject.matrix();
							attrs.objectID = objectID;

							exportLamp(duplicatedObject, attrs);
						}
						// Export Node once and add Instancer particle.
						else if (ob_is_mesh(duplicatedObject)) {
							// For Node.
							NodeAttrs attrs;
							attrs.override = true;
							attrs.objectID = objectID;

							if (isValidInstancerObject(duplicatedObject)) {
								// If DupliObject comes from a particle system then
								// show original object, hide it otherwise.
								attrs.visible = !!(dupliObject.particle_system());

								// For proper instancing with Instancer.
								attrs.dynamic_geometry = true;

								// Set original object transform
								attrs.tm = duplicatedObject.matrix_world();

								exportObject(duplicatedObject, attrs);

								// Add duplicated object particle.
								VRayForBlender::InstancerItem instancerItem(particleID);
								instancerItem.objectName = GetIDName(duplicatedObject);
								instancerItem.objectID = objectID;

								instancerItem.setTransform(dupliObject.matrix(), duplicatedObject.matrix_world());

								instancer.addParticle(instancerItem);
							}
							else {
								// Need to construct unique name for a new plugin.
								static boost::format dupliLightFtm("DupliItem|%u|%s");
								attrs.namePrefix = boost::str(dupliLightFtm % particleID % GetIDName(duplicatedObject));
								attrs.tm = dupliObject.matrix();

								exportObject(duplicatedObject, attrs);
							}

							// Duplicated object could also contain hair.
							if (ob_has_hair(duplicatedObject)) {
								// Export hair nodes and geometry.
								StrSet hairNodes;
								Node::WriteHair(duplicatedObject, attrs, &hairNodes);

								// Add hair particles.
								for (StrSet::const_iterator hIt = hairNodes.begin(); hIt != hairNodes.end(); ++hIt) {
									// Add hair particle.
									VRayForBlender::InstancerItem instancerItem(particleID);
									instancerItem.objectName = *hIt;
									instancerItem.setTransform(dupliObject.matrix(), duplicatedObject.matrix_world());

									instancer.addParticle(instancerItem);
								}
							}
						}
					}
				}

				ob.dupli_list_clear();
			}
		}

		if (!is_interrupted()) {
			Node::WriteHair(ob);

			if (ob_is_duplicator_renderable(ob) && ob_on_visible_layer(ob)) {
				// Export / expand array object.
				exportObjectOrArray(ob);
			}
		}
	}

	// Reset update flag
	RNA_int_set(&vrayObject, "data_updated", CGR_NONE);
}


void VRsceneExporter::exportObjectOrArray(BL::Object ob)
{
	/// Wrapper for BLTransform with convenient constructors.
	struct ArrayModTm {
		typedef std::vector<ArrayModTm> Vector;

		ArrayModTm(BLTransform tm)
		    : tm(tm)
		{}
		ArrayModTm(float m[4][4]) {
			::memcpy(tm.data, m, 16 * sizeof(float));
		}
		ArrayModTm(const float *m) {
			::memcpy(tm.data, m, 16 * sizeof(float));
		}

		BLTransform tm;
	};

	/// Wrapper for BL::ArrayModifier with convenient constructor.
	struct ArrayMod {
		typedef std::vector<ArrayMod> Vector;

		ArrayMod(BL::Modifier modArray=BL::Modifier(PointerRNA_NULL), int index=0)
		    : arrayModifier(modArray)
		    , index(index)
		{}

		int count() const {
			ArrayModifierData *md = getArrayModifierData();
			BLI_assert(md);
			return md->count;
		}

		/// Returns transform for the particular array item.
		/// @param index Array item.
		const float *getTm(int index) const {
			ArrayModifierData *md = getArrayModifierData();
			BLI_assert(md);
			BLI_assert(index < md->count);

			float *tm = NULL;

			// "count" represents the object itself + number of copies,
			// we store transforms only for the copies, so basically first is
			// identity transform.
			if (index == 0) {
				// Identity transform.
				static float identityTm[4][4];
				static int identityTmInit = false;
				if (!identityTmInit) {
					unit_m4(identityTm);
					identityTmInit = true;
				}

				tm = (float*)identityTm;
			}
			else {
				tm = md->dupliTms + (index-1) * 16;
			}

			BLI_assert(tm);

			return tm;
		}

		/// Set render mode for the modifier. Used to export object without
		/// any array modifier applied.
		/// @param value True to enable for render, false - otherwise.
		void showRender(int value) {
			arrayModifier.show_render(value);
		}

		/// Returns modifierd index in modifier stack.
		int getIndex() const {
			return index;
		}

	private:
		/// Returns ArrayModifierData for the modifier.
		ArrayModifierData *getArrayModifierData() const {
			return reinterpret_cast<ArrayModifierData*>(arrayModifier.ptr.data);
		}

		/// The array modifier.
		BL::ArrayModifier arrayModifier;

		/// Modifier index in modifier stack.
		/// Used to detect that modifiers are going one after another.
		int index;
	};

	ArrayMod::Vector arrayMods;

	const int numModifiers = ob.modifiers.length();

	// We support only the last sequence of Array modifiers in the modifier stack.
	for (int modIdx = numModifiers-1; modIdx >= 0; --modIdx) {
		BL::Modifier mod = ob.modifiers[modIdx];
		if (mod && mod.show_render()) {
			if (mod.type() != BL::Modifier::type_ARRAY) {
				break;
			}
			else {
				int addModifier = false;

				if (arrayMods.empty()) {
					addModifier = true;
				}
				else {
					const ArrayMod &lastAddedMod = arrayMods[arrayMods.size()-1];
					if (lastAddedMod.getIndex() - modIdx == 1) {
						addModifier = true;
					}
				}

				if (addModifier) {
					arrayMods.emplace_back(ArrayMod(mod, modIdx));
				}
			}
		}
	}

	// Export object as is.
	if (arrayMods.empty()) {
		exportObject(ob);
	}
	// Export Array modifiers as Instancer particles.
	else {
		// We want modifiers in descending order.
		std::reverse(arrayMods.begin(), arrayMods.end());

		// Disable for render so that object is exported without any Array modifier.
		for (int modIdx = 0; modIdx < arrayMods.size(); ++modIdx) {
			arrayMods[modIdx].showRender(false);
		}

		NodeAttrs nodeAttrs;
		nodeAttrs.override = true;
		nodeAttrs.objectID = ob.pass_index();
		nodeAttrs.tm = ob.matrix_world();

		// It'll be shown with Instancer.
		nodeAttrs.visible = false;

		// We could have some heavy array, better use "dynamic_geometry".
		nodeAttrs.dynamic_geometry = true;

		// Export node.
		exportObject(ob, nodeAttrs);

		// Restore Array modifier settings.
		for (int modIdx = 0; modIdx < arrayMods.size(); ++modIdx) {
			arrayMods[modIdx].showRender(true);
		}

		// Collect transforms.
		ArrayModTm::Vector finalTms;

		for (int modIdx = 0; modIdx < arrayMods.size(); ++modIdx) {
			const ArrayMod &arrayMod = arrayMods[modIdx];

			// Add the first modifier data as is.
			if (modIdx == 0) {
				for (int arrIdx = 0; arrIdx < arrayMod.count(); ++arrIdx) {
					finalTms.emplace_back(arrayMod.getTm(arrIdx));
				}
			}
			// Each next modifier is multiplying everything generated by the
			// previous ones.
			else {
				// Collect this modifier layer data.
				ArrayModTm::Vector layerTms;

				for (int finIdx = 0; finIdx < finalTms.size(); ++finIdx) {
					const ArrayModTm &finTm = finalTms[finIdx];

					// Starting with 1 because first elements are already there
					// from the previous layer.
					for (int arrIdx = 1; arrIdx < arrayMod.count(); ++arrIdx) {
						const float *itemTm = arrayMod.getTm(arrIdx);

						float tm[4][4];
						mul_m4_m4m4(tm, _m4(finTm.tm.data), _m4(itemTm));

						layerTms.emplace_back(tm);
					}
				}

				// Apped data to the main storage.
				for (int layIdx = 0; layIdx < layerTms.size(); ++layIdx) {
					finalTms.emplace_back(layerTms[layIdx].tm);
				}
			}
		}

		// Add particles for the whole array.
		for (int finIdx = 0; finIdx < finalTms.size(); ++finIdx) {
			const BLTransform &tm = finalTms[finIdx].tm;
			const BLTransform &obTm = ob.matrix_world();

			// Transform TM to world space.
			BLTransform dupliTmWorld;
			copy_m4_m4(_m4(dupliTmWorld.data), _m4(tm.data));
			mul_m4_m4m4(_m4(dupliTmWorld.data), _m4(obTm.data), _m4(dupliTmWorld.data));

			// Add array object particle.
			VRayForBlender::InstancerItem instancerItem(VRayForBlender::getParticleID(ob, finIdx));
			instancerItem.objectName = GetIDName(ob);
			instancerItem.objectID = ob.pass_index();
			instancerItem.setTransform(dupliTmWorld, obTm);

			instancer.addParticle(instancerItem);
		}
	}
}


void VRsceneExporter::exportNodeEx(BL::Object ob, const NodeAttrs &attrs)
{
	Object *b_ob = (Object*)ob.ptr.data;

	BL::NodeTree ntree = VRayNodeExporter::getNodeTree(ExporterSettings::gSet.b_data, (ID*)b_ob);
	if(ntree) {
		exportNodeFromNodeTree(ntree, b_ob, attrs);
	}
	else {
		exportNode(b_ob, attrs);
	}
}


void VRsceneExporter::exportObject(BL::Object ob, const NodeAttrs &attrs)
{
	if (ob_is_mesh(ob)) {
		PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
		PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");

		if (RNA_boolean_get(&vrayObject, "overrideWithScene")) {
			exportVRayAsset(ob, attrs);
		}
		else if (RNA_boolean_get(&vrayClipper, "enabled")) {
			exportVRayClipper(ob, attrs);
		}
		else {
			if (!ob_is_smoke_domain(ob)) {
				const std::string idName = attrs.namePrefix + GetIDName(ob);
				if (!m_exportedObjects.count(idName)) {
					m_exportedObjects.insert(idName);

					const int subframes = RNA_int_get(&vrayObject, "subframes");
					if (subframes) {
						m_subframeObjects[subframes].insert(ob);
					}

					exportNodeEx(ob, attrs);
				}
			}
		}
	}
	else if (ob_is_light(ob)) {
		exportLamp(ob, attrs);
	}
}


void VRsceneExporter::exportNode(Object *ob, const NodeAttrs &attrs)
{
	PRINT_INFO("VRsceneExporter::exportNode(%s)",
	           ob->id.name);

	Node *node = new Node(ExporterSettings::gSet.m_sce, ExporterSettings::gSet.m_main, ob);
	node->setNamePrefix(attrs.namePrefix);
	if(attrs.override) {
		node->setTransform(attrs.tm);
		node->setVisiblity(attrs.visible);
		if(attrs.objectID > -1) {
			node->setObjectID(attrs.objectID);
		}
		if(attrs.dupliHolder.ptr.data) {
			node->setDupliHolder(attrs.dupliHolder);
		}
	}
	node->init(ExporterSettings::gSet.m_mtlOverrideName);
	node->initHash();

	// This will also check if object's mesh is valid
	if(NOT(node->preInitGeometry(attrs.dynamic_geometry))) {
		delete node;
		return;
	}

	if(ExporterSettings::gSet.m_useHideFromView && m_hideFromView.hasData()) {
		RenderStats hideFromViewStats;
		hideFromViewStats.visibility             = !m_hideFromView.visibility.count(ob);
		hideFromViewStats.gi_visibility          = !m_hideFromView.gi_visibility.count(ob);

		hideFromViewStats.reflections_visibility = !m_hideFromView.reflections_visibility.count(ob);
		hideFromViewStats.refractions_visibility = !m_hideFromView.refractions_visibility.count(ob);
		hideFromViewStats.shadows_visibility     = !m_hideFromView.shadows_visibility.count(ob);
		hideFromViewStats.camera_visibility      = !m_hideFromView.camera_visibility.count(ob);

		node->setHideFromView(hideFromViewStats);
	}

	if(ExporterSettings::gSet.m_exportMeshes) {
		int writeData = true;
		if (ExporterSettings::gSet.DoUpdateCheck())
			writeData = node->isObjectDataUpdated();
		if(writeData) {
			node->initGeometry();
			node->writeGeometry(ExporterSettings::gSet.m_fileGeom, ExporterSettings::gSet.m_frameCurrent);
		}
	}

	if(ExporterSettings::gSet.m_exportNodes) {
		int writeObject = true;
		if (ExporterSettings::gSet.DoUpdateCheck())
			writeObject = node->isObjectUpdated() || node->isObjectDataUpdated();
		int toDelete = false;
		if(writeObject) {
			toDelete = node->write(ExporterSettings::gSet.m_fileObject, ExporterSettings::gSet.m_frameCurrent);
		}
		else {
			if(m_hideFromView.hasData()) {
				node->writeHideFromView();
			}
		}
		if(toDelete) {
			delete node;
		}
	}
	else {
		delete node;
	}
}


void VRsceneExporter::exportNodeFromNodeTree(BL::NodeTree ntree, Object *ob, const NodeAttrs &attrs)
{
	PRINT_INFO("VRsceneExporter::exportNodeFromNodeTree(%s)",
	           ob->id.name);

	PointerRNA objectRNA;
	RNA_id_pointer_create((ID*)ob, &objectRNA);
	BL::Object bl_ob(objectRNA);

	BL::Node nodeOutput = VRayNodeExporter::getNodeByType(ntree, "VRayNodeObjectOutput");
	if(NOT(nodeOutput)) {
		PRINT_ERROR("Object: %s Node tree: %s => Output node not found!",
		            ob->id.name, ntree.name().c_str());
		return;
	}

	BL::NodeSocket geometrySocket = VRayNodeExporter::getSocketByName(nodeOutput, "Geometry");
	if(NOT(geometrySocket && geometrySocket.is_linked())) {
		PRINT_ERROR("Object: %s Node tree: %s => Geometry node is not set!",
		            ob->id.name, ntree.name().c_str());
		return;
	}

	const std::string pluginName = attrs.namePrefix + GetIDName(bl_ob);

	std::string transform = GetTransformHex(bl_ob.matrix_world());

	int visible  = true;
	int objectID = ob->index;

	// Prepare object context
	//
	VRayNodeContext nodeCtx;
	nodeCtx.obCtx.ob   = ob;
	nodeCtx.obCtx.sce  = ExporterSettings::gSet.m_sce;
	nodeCtx.obCtx.main = ExporterSettings::gSet.m_main;
	nodeCtx.obCtx.nodeAttrs = attrs;
	nodeCtx.obCtx.mtlOverrideName = ExporterSettings::gSet.m_mtlOverrideName;

	// Export object main properties
	//
	std::string geometry = "NULL";
	bool        override_for_preview = false;

	// For preview rendering the material is copied into the separate scene, but our
	// displacement is stored on the object level thus not copied. To preview displacement
	// we'll lookup into the original scene for the object node tree
	if (ExporterSettings::gSet.b_context &&
	    ExporterSettings::gSet.b_engine &&
	    ExporterSettings::gSet.b_engine.is_preview())
	{
		// Check if currently processed object is a preview object
		if (boost::starts_with(bl_ob.name(), "preview_")) {
			// Parent scene
			BL::Scene sce(ExporterSettings::gSet.b_context.scene());
			if (sce) {
				BL::Object active_ob(sce.objects.active());
				if (active_ob && ob_is_mesh(active_ob)) {
					BL::NodeTree active_ob_ntree(VRayNodeExporter::getNodeTree(ExporterSettings::gSet.b_data, (ID*)active_ob.ptr.data));
					if (active_ob_ntree) {
						BL::Node nodeOutput(VRayNodeExporter::getNodeByType(active_ob_ntree, "VRayNodeObjectOutput"));
						if (nodeOutput) {
							BL::NodeSocket geometrySocket(VRayNodeExporter::getSocketByName(nodeOutput, "Geometry"));
							if (geometrySocket && geometrySocket.is_linked()) {
								BL::Node geometryNode(VRayNodeExporter::getConnectedNode(active_ob_ntree, geometrySocket, nodeCtx));
								if (geometryNode && (geometryNode.bl_idname() == "VRayNodeGeomDisplacedMesh")) {
									geometry = VRayNodeExporter::exportSocket(active_ob_ntree, geometrySocket, nodeCtx);
									override_for_preview = true;
								}
							}
						}
					}
				}
			}
		}
	}

	if (!override_for_preview) {
		geometry = VRayNodeExporter::exportSocket(ntree, geometrySocket, nodeCtx);
	}

	if(geometry == "NULL") {
		PRINT_ERROR("Object: %s Node tree: %s => Incorrect geometry!",
		            ob->id.name, ntree.name().c_str());
		return;
	}

	BL::Node geometryNode = VRayNodeExporter::getConnectedNode(ntree, geometrySocket, nodeCtx);
	if(geometryNode.bl_idname() == "VRayNodeLightMesh") {
		// No need to export Node - this object is LightMesh
		return;
	}

	BL::NodeSocket materialSocket = VRayNodeExporter::getSocketByName(nodeOutput, "Material");
	if(NOT(materialSocket && materialSocket.is_linked())) {
		PRINT_ERROR("Object: %s Node tree: %s => Material node is not set!",
		            ob->id.name, ntree.name().c_str());
		return;
	}

	std::string material = VRayNodeExporter::exportSocket(ntree, materialSocket, nodeCtx);
	if(material == "NULL") {
		PRINT_ERROR("Object: %s Node tree: %s => Incorrect material!",
		            ob->id.name, ntree.name().c_str());
		return;
	}

	// Add MtlRenderStats and MtlWrapper from Object level for "one click" things
	//
	PointerRNA vrayObject = RNA_pointer_get(&bl_ob.ptr, "vray");

	material = Node::WriteMtlWrapper(&vrayObject, NULL, pluginName, material);
	material = Node::WriteMtlRenderStats(&vrayObject, NULL, pluginName, material);

	// Export 'MtlRenderStats' for "Hide From View"
	//
	if(ExporterSettings::gSet.m_useHideFromView && m_hideFromView.hasData()) {
		std::string hideFromViewName = "HideFromView@" + pluginName;

		AttributeValueMap hideFromViewAttrs;
		hideFromViewAttrs["base_mtl"] = material;
		hideFromViewAttrs["visibility"]             = BOOST_FORMAT_BOOL(!m_hideFromView.visibility.count(ob));
		hideFromViewAttrs["gi_visibility"]          = BOOST_FORMAT_BOOL(!m_hideFromView.gi_visibility.count(ob));
		hideFromViewAttrs["camera_visibility"]      = BOOST_FORMAT_BOOL(!m_hideFromView.camera_visibility.count(ob));
		hideFromViewAttrs["reflections_visibility"] = BOOST_FORMAT_BOOL(!m_hideFromView.reflections_visibility.count(ob));
		hideFromViewAttrs["refractions_visibility"] = BOOST_FORMAT_BOOL(!m_hideFromView.refractions_visibility.count(ob));
		hideFromViewAttrs["shadows_visibility"]     = BOOST_FORMAT_BOOL(!m_hideFromView.shadows_visibility.count(ob));

		// It's actually a material, but we will write it along with Node
		VRayNodePluginExporter::exportPlugin("NODE", "MtlRenderStats", hideFromViewName, hideFromViewAttrs);

		material = hideFromViewName;
	}

	// Check if we need to override some stuff;
	// comes from advanced DupliGroup export.
	//
	if(attrs.override) {
		visible  = attrs.visible;
		objectID = attrs.objectID;
		transform = GetTransformHex(attrs.tm);

		if (attrs.dupliHolder.ptr.data) {
			PointerRNA vrayObject = RNA_pointer_get((PointerRNA*)&attrs.dupliHolder.ptr, "vray");
			std::string overrideBaseName = pluginName + "@" + GetIDName((ID*)attrs.dupliHolder.ptr.data);
			material = Node::WriteMtlWrapper(&vrayObject, NULL, overrideBaseName, material);
			material = Node::WriteMtlRenderStats(&vrayObject, NULL, overrideBaseName, material);
		}
	}

	PointerRNA vrayNode = RNA_pointer_get(&vrayObject, "Node");

	StrVector user_attributes;
	VRayNodeExporter::getUserAttributes(&vrayNode, user_attributes);

	AttributeValueMap pluginAttrs;
	pluginAttrs["material"]  = material;
	pluginAttrs["geometry"]  = geometry;
	pluginAttrs["objectID"]  = BOOST_FORMAT_INT(objectID);
	pluginAttrs["visible"]   = BOOST_FORMAT_INT(visible);
	pluginAttrs["transform"] = BOOST_FORMAT_TM(transform);

	if (user_attributes.size()) {
		pluginAttrs["user_attributes"] = BOOST_FORMAT_STRING(BOOST_FORMAT_LIST_JOIN_SEP(user_attributes, ";"));
	}

	VRayNodePluginExporter::exportPlugin("NODE", "Node", pluginName, pluginAttrs);
}


void VRsceneExporter::exportLamp(BL::Object ob, const NodeAttrs &attrs)
{
	BL::ID lampID = ob.data();
	if(NOT(lampID))
		return;

	BL::Lamp          lamp(lampID);
	PointerRNA        vrayLamp = RNA_pointer_get(&lamp.ptr, "vray");

	const std::string pluginName = attrs.namePrefix + GetIDName(ob);
	if(m_exportedObjects.count(pluginName))
		return;
	m_exportedObjects.insert(pluginName);

	// Find plugin ID
	std::string  pluginID;
	if(lamp.type() == BL::Lamp::type_AREA) {
		pluginID = "LightRectangle";
	}
	else if(lamp.type() == BL::Lamp::type_HEMI) {
		pluginID = "LightDome";
	}
	else if(lamp.type() == BL::Lamp::type_SPOT) {
		int spotType = RNA_enum_get(&vrayLamp, "spot_type");
		switch(spotType) {
			case 0: pluginID = "LightSpotMax"; break;
			case 1: pluginID = "LightIESMax";  break;
		}
	}
	else if(lamp.type() == BL::Lamp::type_POINT) {
		int omniType = RNA_enum_get(&vrayLamp, "omni_type");
		switch(omniType) {
			case 0: pluginID = "LightOmniMax";    break;
			case 1: pluginID = "LightAmbientMax"; break;
			case 2: pluginID = "LightSphere";     break;
		}
	}
	else if(lamp.type() == BL::Lamp::type_SUN) {
		int directType = RNA_enum_get(&vrayLamp, "direct_type");
		switch(directType) {
			case 0: pluginID = "LightDirectMax"; break;
			case 1: pluginID = "SunLight";       break;
		}
	}
	else {
		PRINT_ERROR("Lamp: %s Type: %i => Lamp type is not supported!",
		            ob.name().c_str(), lamp.type());
		return;
	}

	AttributeValueMap pluginAttrs;
	PointerRNA propGroup = RNA_pointer_get(&vrayLamp, pluginID.c_str());

	// Get all non-mappable attribute values
	StrSet pluginAttrNames;
	VRayNodeExporter::getAttributesList(pluginID, pluginAttrNames, false);

	for(StrSet::const_iterator setIt = pluginAttrNames.begin(); setIt != pluginAttrNames.end(); ++setIt) {
		const std::string &attrName = *setIt;
		const std::string &attrValue = VRayNodeExporter::getValueFromPropGroup(&propGroup, (ID*)lamp.ptr.data, attrName.c_str());
		if(attrValue != "NULL")
			pluginAttrs[attrName] = attrValue;
	}

	// Now, get all mappable attribute values
	//
	StrSet socketAttrNames;
	VRayNodeExporter::getAttributesList(pluginID, socketAttrNames, true);

	BL::Node     lightNode(PointerRNA_NULL);
	BL::NodeTree lightTree = VRayNodeExporter::getNodeTree(ExporterSettings::gSet.b_data, (ID*)lamp.ptr.data);
	if(lightTree) {
		const std::string &vrayNodeType = boost::str(boost::format("VRayNode%s") % pluginID);

		VRayNodeContext lightCtx;
		lightCtx.obCtx.ob   = (Object*)ob.ptr.data;
		lightCtx.obCtx.sce  = ExporterSettings::gSet.m_sce;
		lightCtx.obCtx.main = ExporterSettings::gSet.m_main;

		lightNode = VRayNodeExporter::getNodeByType(lightTree, vrayNodeType);
		if(lightNode) {
			for(StrSet::const_iterator setIt = socketAttrNames.begin(); setIt != socketAttrNames.end(); ++setIt) {
				const std::string &attrName = *setIt;

				BL::NodeSocket sock = VRayNodeExporter::getSocketByAttr(lightNode, attrName);
				if(sock) {
					const std::string &attrValue = VRayNodeExporter::exportSocket(lightTree, sock, lightCtx);
					if(attrValue != "NULL")
						pluginAttrs[attrName] = attrValue;
				}
			}
		}
	}

	if(pluginID == "LightRectangle") {
		BL::AreaLamp  areaLamp(lamp);

		float sizeX = areaLamp.size() / 2.0f;
		float sizeY = areaLamp.shape() == BL::AreaLamp::shape_SQUARE ? sizeX : areaLamp.size_y() / 2.0f;

		pluginAttrs["u_size"] = BOOST_FORMAT_FLOAT(sizeX);
		pluginAttrs["v_size"] = BOOST_FORMAT_FLOAT(sizeY);

		pluginAttrs["use_rect_tex"] = BOOST_FORMAT_BOOL(pluginAttrs.count("rect_tex"));

		if (attrs.override && attrs.objectID != VRayForBlender::InstancerItem::useOriginalObjectID) {
			pluginAttrs["objectID"] = BOOST_FORMAT_INT(attrs.objectID);
		}
	}
	else if(pluginID == "LightDome") {
		pluginAttrs["use_dome_tex"] = BOOST_FORMAT_BOOL(pluginAttrs.count("dome_tex"));
	}
	else if(pluginID == "LightSpotMax") {
		BL::SpotLamp spotLamp(lamp);

		pluginAttrs["fallsize"] = BOOST_FORMAT_FLOAT(spotLamp.spot_size());
	}
	else if(ELEM(pluginID, "LightRectangle", "LightSphere", "LightDome")) {
		pluginAttrs["objectID"] = BOOST_FORMAT_INT(ob.pass_index());
	}

	if (ELEM(pluginID, "LightOmniMax", "LightSpotMax", "LightDirectMax")) {
		const std::string &shadowRadius = pluginAttrs["shadowRadius"];
		if (shadowRadius != "0") {
			std::string &shadowRadius1 = pluginAttrs["shadowRadius1"];
			if (shadowRadius1 == "0") {
				shadowRadius1 = shadowRadius;
			}
			std::string &shadowRadius2 = pluginAttrs["shadowRadius2"];
			if (shadowRadius2 == "0") {
				shadowRadius2 = shadowRadius;
			}
		}
	}

	// Now, let's go through "Render Elements" and check if we have to
	// plug our light somewhere like "Light Select"
	//
	BL::NodeTree sceneTree = VRayNodeExporter::getNodeTree(ExporterSettings::gSet.b_data, (ID*)ExporterSettings::gSet.m_sce);
	if(sceneTree) {
		BL::Node chanNode = VRayNodeExporter::getNodeByType(sceneTree, "VRayNodeRenderChannels");
		if(chanNode) {
			StrSet  channels_raw;
			StrSet  channels_diffuse;
			StrSet  channels_specular;

			BL::Node::inputs_iterator inIt;
			for(chanNode.inputs.begin(inIt); inIt != chanNode.inputs.end(); ++inIt) {
				BL::NodeSocket chanSock = *inIt;
				if(chanSock && chanSock.is_linked() && chanSock.bl_idname() == "VRaySocketRenderChannel") {
					bool useChan = RNA_boolean_get(&chanSock.ptr, "use");
					if(useChan) {
						VRayNodeContext chanCtx;
						BL::Node chanNode = VRayNodeExporter::getConnectedNode(sceneTree, chanSock, chanCtx);
						if(chanNode && chanNode.bl_idname() == "VRayNodeRenderChannelLightSelect") {
							BL::NodeSocket lightsSock = VRayNodeExporter::getSocketByName(chanNode, "Lights");
							if(lightsSock) {
								VRayNodeContext lightCtx;
								BL::Node lightsConNode = VRayNodeExporter::getConnectedNode(sceneTree, lightsSock, lightCtx);
								if(lightsConNode && IS_OBJECT_SELECT_NODE(lightsConNode)) {
									ObList lampList;
									VRayNodeExporter::getNodeSelectObjects(lightsConNode, lampList);

									if(lampList.size()) {
										const int lightSelectType = RNA_enum_get(&chanNode.ptr, "type");
										const std::string &chanName = VRayNodeExporter::getPluginName(chanNode, sceneTree, lightCtx);

										ObList::const_iterator obIt;
										for(obIt = lampList.begin(); obIt != lampList.end(); ++obIt) {
											BL::Object lampOb = *obIt;
											if(lampOb.type() == BL::Object::type_LAMP && VRayNodeExporter::isObjectVisible(lampOb) && lampOb == ob) {
												switch(lightSelectType) {
													case  1: channels_diffuse.insert(chanName);  break;
													case  2: channels_specular.insert(chanName); break;
													default: channels_raw.insert(chanName);      break;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

			if(channels_raw.size())
				pluginAttrs["channels_raw"]      = BOOST_FORMAT_LIST(channels_raw);
			if(channels_diffuse.size())
				pluginAttrs["channels_diffuse"]  = BOOST_FORMAT_LIST(channels_diffuse);
			if(channels_specular.size())
				pluginAttrs["channels_specular"] = BOOST_FORMAT_LIST(channels_specular);
		}
	}

	std::string transform = GetTransformHex(ob.matrix_world());
	if (attrs.override) {
		transform = GetTransformHex(attrs.tm);
	}

	pluginAttrs["transform"] = BOOST_FORMAT_TM(transform);

	VRayNodePluginExporter::exportPlugin("LIGHT", pluginID, pluginName, pluginAttrs);
}


void VRsceneExporter::exportVRayAsset(BL::Object ob, const NodeAttrs &attrs)
{
	PointerRNA vrayObject = RNA_pointer_get(&ob.ptr, "vray");
	PointerRNA vrayAsset  = RNA_pointer_get(&vrayObject, "VRayAsset");

	const std::string &pluginName = attrs.namePrefix + "Asset@" + GetIDName(ob);
	if(m_exportedObjects.count(pluginName))
		return;
	m_exportedObjects.insert(pluginName);

	std::string transform = GetTransformHex(ob.matrix_world());
	if (attrs.override) {
		transform = GetTransformHex(attrs.tm);
	}

	AttributeValueMap pluginAttrs;

	pluginAttrs["filepath"] = BOOST_FORMAT_STRING(BlenderUtils::GetFullFilepath(RNA_std_string_get(&vrayAsset, "sceneFilepath")).c_str());

	pluginAttrs["use_transform"] = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayAsset, "sceneUseTransform"));
	pluginAttrs["transform"]     = BOOST_FORMAT_TM(transform);

	pluginAttrs["add_nodes"]       = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayAsset, "sceneAddNodes"));
	pluginAttrs["add_lights"]      = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayAsset, "sceneAddLights"));

	// pluginAttrs["add_materials"]   = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayAsset, "sceneAddMaterials"));
	// pluginAttrs["add_cameras"]     = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayAsset, "sceneAddCameras"));
	// pluginAttrs["add_environment"] = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayAsset, "sceneAddEnvironment"));

	pluginAttrs["anim_type"]   = BOOST_FORMAT_INT(RNA_enum_ext_get(&vrayAsset, "anim_type"));
	pluginAttrs["anim_speed"]  = BOOST_FORMAT_FLOAT(RNA_float_get(&vrayAsset, "anim_speed"));
	pluginAttrs["anim_offset"] = BOOST_FORMAT_FLOAT(RNA_float_get(&vrayAsset, "anim_offset"));
	pluginAttrs["anim_start"]  = BOOST_FORMAT_INT(RNA_int_get(&vrayAsset, "anim_start"));
	pluginAttrs["anim_length"] = BOOST_FORMAT_INT(RNA_int_get(&vrayAsset, "anim_length"));

	if (ExporterSettings::gSet.m_mtlOverride) {
		pluginAttrs["material_override"] = ExporterSettings::gSet.m_mtlOverrideName;
	}

	if (RNA_boolean_get(&vrayAsset, "use_hide_objects")) {
		std::string hidden_objects = RNA_std_string_get(&vrayAsset, "hidden_objects");
		if (hidden_objects.size()) {
			StrVector hidden_objects_vec;
			boost::split(hidden_objects_vec, hidden_objects, boost::is_any_of(";"), boost::token_compress_on);

			StrSet hidden_objects_set;
			for (StrVector::const_iterator sIt = hidden_objects_vec.begin(); sIt != hidden_objects_vec.end(); ++sIt) {
				hidden_objects_set.insert(BOOST_FORMAT_STRING(*sIt));
			}

			pluginAttrs["hidden_objects"] = BOOST_FORMAT_LIST(hidden_objects_set);
		}
	}

	VRayNodePluginExporter::exportPlugin("NODE", "VRayScene", pluginName, pluginAttrs);
}


void VRsceneExporter::exportVRayClipper(BL::Object ob, const NodeAttrs &attrs)
{
	PointerRNA vrayObject  = RNA_pointer_get(&ob.ptr, "vray");
	PointerRNA vrayClipper = RNA_pointer_get(&vrayObject, "VRayClipper");

	const std::string &pluginName = attrs.namePrefix + "VRayClipper@" + GetIDName(ob);
	if(m_exportedObjects.count(pluginName))
		return;
	m_exportedObjects.insert(pluginName);

	char transform[CGR_TRANSFORM_HEX_SIZE];
	GetTransformHex(((Object*)ob.ptr.data)->obmat, transform);

	const std::string &material = VRayNodeExporter::exportMtlMulti(ExporterSettings::gSet.b_data, ob);

	AttributeValueMap pluginAttrs;
	pluginAttrs["enabled"] = "1";
	pluginAttrs["affect_light"]     = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "affect_light"));
	pluginAttrs["only_camera_rays"] = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "only_camera_rays"));
	pluginAttrs["clip_lights"]      = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "clip_lights"));
	pluginAttrs["use_obj_mtl"]      = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "use_obj_mtl"));
	pluginAttrs["set_material_id"]  = BOOST_FORMAT_BOOL(RNA_boolean_get(&vrayClipper, "set_material_id"));
	pluginAttrs["material_id"]      = BOOST_FORMAT_INT(RNA_int_get(&vrayClipper, "material_id"));
	pluginAttrs["invert_inside"]    = BOOST_FORMAT_INT(RNA_enum_ext_get(&vrayClipper, "invert_inside"));
	pluginAttrs["object_id"]        = BOOST_FORMAT_INT(ob.pass_index());
	pluginAttrs["transform"]        = BOOST_FORMAT_TM(transform);

	if (RNA_boolean_get(&vrayClipper, "use_obj_mesh")) {
		const std::string &clipperGeomName = pluginName + "@Mesh";

		ExportGeomStaticMesh(ExporterSettings::gSet.m_fileGeom,
							 ExporterSettings::gSet.m_sce,
							 (Object*)ob.ptr.data,
							 ExporterSettings::gSet.m_main,
							 clipperGeomName.c_str(),
							 NULL);

		const std::string &clipperGeomNodeName = clipperGeomName + "|Node";
		AttributeValueMap clipperGeomNode;
		clipperGeomNode["geometry"]  = clipperGeomName;
		clipperGeomNode["transform"] = BOOST_FORMAT_TM(transform);
		clipperGeomNode["visible"]   = BOOST_FORMAT_BOOL(true);
		clipperGeomNode["material"]  = material;

		VRayNodePluginExporter::exportPlugin("NODE", "Node", clipperGeomNodeName, clipperGeomNode);

		pluginAttrs["clip_mesh"] = clipperGeomNodeName;
	}

	const std::string &excludeGroupName = RNA_std_string_get(&vrayClipper, "exclusion_nodes");
	if (NOT(excludeGroupName.empty())) {
		StrSet exclusion_nodes;
		BL::BlendData::groups_iterator grIt;
		for (ExporterSettings::gSet.b_data.groups.begin(grIt); grIt != ExporterSettings::gSet.b_data.groups.end(); ++grIt) {
			BL::Group gr = *grIt;
			if (gr.name() == excludeGroupName) {
				BL::Group::objects_iterator grObIt;
				for (gr.objects.begin(grObIt); grObIt != gr.objects.end(); ++grObIt) {
					BL::Object ob = *grObIt;
					exclusion_nodes.insert(GetIDName(ob));
				}
				break;
			}
		}

		pluginAttrs["exclusion_mode"] = BOOST_FORMAT_INT(RNA_enum_get(&vrayClipper, "exclusion_mode"));
		pluginAttrs["exclusion_nodes"] = BOOST_FORMAT_LIST(exclusion_nodes);
	}

	if (NOT(material.empty()) && material != "NULL")
		pluginAttrs["material"] = material;

	VRayNodePluginExporter::exportPlugin("NODE", "VRayClipper", pluginName, pluginAttrs);
}

void VRsceneExporter::exportDupli()
{
	PyObject *out = ExporterSettings::gSet.m_fileObject;

	VRayExportable::initInterpolate(ExporterSettings::gSet.m_frameCurrent);

	const VRayForBlender::InstancerItems &particles = instancer.getParticles();

	PYTHON_PRINTF(out, "\nInstancer instancer {");
	PYTHON_PRINTF(out, "\n\tinstances=%sList(%g", VRayExportable::m_interpStart, ExporterSettings::gSet.m_isAnimation ? ExporterSettings::gSet.m_frameCurrent : 0);
	if (particles.size()) {
		PYTHON_PRINT(out, ",");

		for(VRayForBlender::InstancerItems::const_iterator paIt = particles.begin(); paIt != particles.end(); ++paIt) {
			const VRayForBlender::InstancerItem &pa = *paIt;

			PYTHON_PRINTF(out, "\n\t\tList(%u,"
			                   "TransformHex(\"%s\"),"
			                   "TransformHex(\"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\"),"
			                   "%i,"
			                   "%i,"
			                   "%s)",
			              pa.particleID,
			              GetTransformHex(pa.getTransform()).c_str(),
			              pa.flags,
			              pa.objectID,
			              pa.objectName.c_str());

			if(paIt != --particles.end()) {
				PYTHON_PRINT(out, ",");
			}
		}
	}
	PYTHON_PRINTF(out, "\n\t)%s;", VRayExportable::m_interpEnd);
	PYTHON_PRINT(out, "\n\tuse_additional_params=1;");
	PYTHON_PRINT(out, "\n\tuse_time_instancing=0;");
	PYTHON_PRINTF(ExporterSettings::gSet.m_fileObject, "\n}\n");
}
