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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "CGR_config.h"

#include "utils/CGR_vrscene.h"
#include "utils/CGR_string.h"
#include "utils/CGR_blender_data.h"
#include "utils/CGR_json_plugins.h"
#include "utils/CGR_rna.h"

#include "exp_scene.h"

#include "vrscene_exporter/GeomMayaHair.h"
#include "vrscene_exporter/GeomStaticMesh.h"
#include "vrscene_exporter/Light.h"

#include "PIL_time.h"
#include "BLI_string.h"
#include "BKE_material.h"
#include "BKE_global.h"

extern "C" {
#  include "DNA_particle_types.h"
#  include "DNA_modifier_types.h"
#  include "DNA_material_types.h"
#  include "BKE_anim.h"
#  include "DNA_windowmanager_types.h"
}

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>


const char* MyParticle::velocity = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

ExpoterSettings* VRsceneExporter::m_settings = NULL;
std::string      VRsceneExporter::m_mtlOverride;


VRsceneExporter::VRsceneExporter(ExpoterSettings *settings)
{
	PRINT_INFO("VRsceneExporter::VRsceneExporter()");

	m_settings = settings;

	init();
}


VRsceneExporter::~VRsceneExporter()
{
	PRINT_INFO("VRsceneExporter::~VRsceneExporter()");

	delete m_settings;
}


void VRsceneExporter::init()
{
	VRayExportable::clearCache();

	m_mtlOverride = "";

	RnaAccess::RnaValue rna(&m_settings->m_sce->id, "vray.SettingsOptions");
	if(rna.getBool("mtl_override_on"))
		m_mtlOverride = "MA" + rna.getString("mtl_override");
}


void VRsceneExporter::exportScene()
{
	PRINT_INFO("VRsceneExporter::exportScene()");

	double timeMeasure = 0.0;
	char   timeMeasureBuf[32];

	PRINT_INFO_LB("Exporting scene for frame %i...%s", m_settings->m_sce->r.cfra, G.debug ? "\n" : "");
	timeMeasure = PIL_check_seconds_timer();

	Base *base = NULL;

	m_settings->b_engine.update_progress(0.0f);

	PointerRNA sceneRNA;
	RNA_id_pointer_create((ID*)m_settings->m_sce, &sceneRNA);
	BL::Scene bl_sce(sceneRNA);

	size_t nObjects = bl_sce.objects.length();

	float  expProgress = 0.0f;
	float  expProgStep = 1.0f / nObjects;
	int    progUpdateCnt = nObjects > 3000 ? 1000 : 100;

	// Clear caches
	m_exportedObject.clear();
	m_psys.clear();

	// Create particle system data
	// Needed for the correct first frame
	//
	if(m_settings->m_sce->r.cfra == m_settings->m_sce->r.sfra) {
		BL::Scene::objects_iterator bl_obIt;
		for(bl_sce.objects.begin(bl_obIt); bl_obIt != bl_sce.objects.end(); ++bl_obIt) {
			BL::Object bl_ob = *bl_obIt;
			if(bl_ob.type() == BL::Object::type_META)
				continue;
			if(bl_ob.is_duplicator()) {
				if(bl_ob.particle_systems.length()) {
					BL::Object::particle_systems_iterator bl_psysIt;
					for(bl_ob.particle_systems.begin(bl_psysIt); bl_psysIt != bl_ob.particle_systems.end(); ++bl_psysIt) {
						BL::ParticleSystem bl_psys = *bl_psysIt;
						BL::ParticleSettings bl_pset = bl_psys.settings();

						if(bl_pset.type() == BL::ParticleSettings::type_HAIR && bl_pset.render_type() == BL::ParticleSettings::render_type_PATH)
							continue;

						m_psys.get(bl_pset.name());
					}
				}
				if(bl_ob.dupli_type() != BL::Object::dupli_type_NONE)
					m_psys.get(bl_ob.name());
			}
		}
	}

	// Export stuff
	base = (Base*)m_settings->m_sce->base.first;
	nObjects = 0;
	while(base) {
		if(m_settings->b_engine.test_break()) {
			m_settings->b_engine.report(RPT_WARNING, "Export interrupted!");
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

		if(m_settings->m_activeLayers)
			if(NOT(ob->lay & m_settings->m_sce->lay))
				continue;

		exportObjectBase(ob);

		expProgress += expProgStep;
		nObjects++;
		if((nObjects % progUpdateCnt) == 0) {
			m_settings->b_engine.update_progress(expProgress);
		}
	}

	exportDupli();

	m_settings->b_engine.update_progress(1.0f);

	BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
	if(G.debug) {
		PRINT_INFO_LB("Frame %i export",  m_settings->m_sce->r.cfra);
	}
	printf(" done [%s]\n", timeMeasureBuf);
}


void VRsceneExporter::exportObjectBase(Object *ob)
{
	if(NOT(GEOM_TYPE(ob) || EMPTY_TYPE(ob) || LIGHT_TYPE(ob)))
		return;

	PointerRNA objectRNA;
	RNA_id_pointer_create((ID*)ob, &objectRNA);
	BL::Object bl_ob(objectRNA);

	if(ob->id.pad2)
		PRINT_INFO("Base object %s (update: %i)", ob->id.name, ob->id.pad2);

	if(bl_ob.is_duplicator()) {
		bl_ob.dupli_list_create(m_settings->b_scene, 2);

		BL::Object::dupli_list_iterator b_dup;
		for(bl_ob.dupli_list.begin(b_dup); b_dup != bl_ob.dupli_list.end(); ++b_dup) {
			if(m_settings->b_engine.test_break())
				break;

			BL::DupliObject bl_dupliOb      = *b_dup;
			BL::Object      bl_duplicatedOb = bl_dupliOb.object();

			DupliObject *dupliOb = (DupliObject*)bl_dupliOb.ptr.data;

			if(NOT(GEOM_TYPE(dupliOb->ob) || LIGHT_TYPE(dupliOb->ob)))
				continue;

			std::string dupliBaseName;

			BL::ParticleSystem bl_psys = bl_dupliOb.particle_system();
			if(bl_psys) {
				BL::ParticleSettings bl_pset = bl_psys.settings();
				dupliBaseName = bl_pset.name();
			}
			else {
				dupliBaseName = bl_ob.name();
			}

			MyPartSystem *mySys = m_psys.get(dupliBaseName);

			MyParticle *myPa = new MyParticle();
			myPa->nodeName = GetIDName(&dupliOb->ob->id);
			myPa->particleId = dupliOb->persistent_id[0];
			GetTransformHex(dupliOb->mat, myPa->transform);

			mySys->append(myPa);

			if(bl_duplicatedOb.type() == BL::Object::type_LAMP)
				exportLight(ob, dupliOb);
			else
				exportObject(dupliOb->ob, false, false);
		}

		bl_ob.dupli_list_clear();

		// If dupli were not from particles (eg DupliGroup) skip base object
		if(NOT(ob->transflag & OB_DUPLIPARTS))
			return;

		// If there is fur we will check for "Render Emitter" later
		if(NOT(Node::HasHair(ob)))
			if(NOT(Node::DoRenderEmitter(ob)))
				return;
	}

	if(GEOM_TYPE(ob)) {
		if(m_settings->b_engine.test_break())
			return;

		// Smoke domain will be exported from Effects
		if(Node::IsSmokeDomain(ob))
			return;

		exportObject(ob);
	}
}


#if CGR_USE_CPP_API
void VRsceneExporter::exportObject(BL::Object ob, BLTm tm, bool visible)
{
	BLNode *node = new BLNode(m_settings->m_sce, ob, tm);
	node->setVisible(visible);
	node->initName();
	node->initHash();
	node->write(m_settings->m_fileObject, m_settings->m_sce->r.cfra);
}
#endif


void VRsceneExporter::exportObject(Object *ob, const int &visible, const int &checkUpdated)
{
	const std::string idName = GetIDName(&ob->id);

	if(m_exportedObject.count(idName))
		return;
	m_exportedObject.insert(idName);

	Node *node = new Node(m_settings->m_sce, m_settings->m_main, ob);
	node->init(m_mtlOverride);
	node->setVisiblity(visible);
	node->initHash();

	// This will check if object's mesh is valid
	if(NOT(node->preInitGeometry())) {
		delete node;
		return;
	}

	exportNode(node, checkUpdated);

	if(NOT(m_settings->m_animation))
		delete node;
}


void VRsceneExporter::exportNode(Node *node, const int &checkUpdated)
{
	if(node->hasHair()) {
		node->writeHair(m_settings);
		if(NOT(node->doRenderEmitter()))
			return;
	}

	if(m_settings->m_exportGeometry) {
		int writeData = true;
		if(checkUpdated && m_settings->checkUpdates())
			writeData = node->isObjectDataUpdated();
		if(writeData) {
			node->initGeometry();
			node->writeGeometry(m_settings->m_fileGeom, m_settings->m_sce->r.cfra);
		}
	}

	if(m_settings->m_exportNodes && NOT(node->isMeshLight())) {
		int writeObject = true;
		if(checkUpdated && m_settings->checkUpdates())
			writeObject = node->isObjectUpdated();
		if(writeObject)
			node->write(m_settings->m_fileObject, m_settings->m_sce->r.cfra);
	}
}


void VRsceneExporter::exportLight(Object *ob, DupliObject *dOb)
{
	Light *light = new Light(m_settings->m_sce, m_settings->m_main, ob, dOb);

	if(m_settings->m_exportNodes)
		light->write(m_settings->m_fileLights, m_settings->m_sce->r.cfra);

	if(NOT(m_settings->m_animation))
		delete light;
}


void VRsceneExporter::exportDupli()
{
	PyObject *out = m_settings->m_fileObject;
	Scene    *sce = m_settings->m_sce;

	for(MyPartSystems::const_iterator sysIt = m_psys.m_systems.begin(); sysIt != m_psys.m_systems.end(); ++sysIt) {
		const std::string   psysName = sysIt->first;
		const MyPartSystem *parts    = sysIt->second;

		std::cout << psysName << std::endl;

		PYTHON_PRINTF(out, "\nInstancer Dupli%s {", StripString(psysName).c_str());
		PYTHON_PRINTF(out, "\n\tinstances=%sList(%i", VRayExportable::m_interpStart, m_settings->m_animation ? sce->r.cfra : 0);
		if(parts->size()) {
			PYTHON_PRINT(out, ",");
			for(Particles::const_iterator paIt = parts->m_particles.begin(); paIt != parts->m_particles.end(); ++paIt) {
				MyParticle *pa = *paIt;

				PYTHON_PRINTF(out, "List(%zd,TransformHex(\"%s\"),TransformHex(\"%s\"),%s)", pa->particleId, pa->transform, pa->velocity, pa->nodeName.c_str());

				if(paIt != --parts->m_particles.end()) {
					PYTHON_PRINT(out, ",");
				}
			}
		}
		PYTHON_PRINTF(out, ")%s;", VRayExportable::m_interpEnd);
		PYTHON_PRINTF(m_settings->m_fileObject, "\n}\n");
	}

	m_psys.clear();
}
