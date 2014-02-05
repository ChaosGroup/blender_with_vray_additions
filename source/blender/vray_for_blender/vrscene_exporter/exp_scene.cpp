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

#include "PIL_time.h"
#include "BLI_string.h"
#include "BKE_material.h"

extern "C" {
#  include "DNA_particle_types.h"
#  include "DNA_modifier_types.h"
#  include "DNA_material_types.h"
#  include "BKE_anim.h"
#  include "DNA_windowmanager_types.h"
}

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>


VRsceneExporter::VRsceneExporter(ExpoterSettings *settings):
	m_settings(settings)
{
	PRINT_INFO("VRsceneExporter::VRsceneExporter()");

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

	PRINT_INFO_LB("VRsceneExporter: Exporting scene for frame %i...", m_settings->m_sce->r.cfra);
	timeMeasure = PIL_check_seconds_timer();

	Base *base = NULL;

	m_settings->m_engine.update_progress(0.0f);

	size_t nObjects = 0;
	base = (Base*)m_settings->m_sce->base.first;
	while(base) {
		nObjects++;
		base = base->next;
	}

	float  expProgress = 0.0f;
	float  expProgStep = 1.0f / nObjects;
	int    progUpdateCnt = nObjects > 2000 ? 1000 : 100;

	// Export stuff
	base = (Base*)m_settings->m_sce->base.first;
	nObjects = 0;
	while(base) {
		if(m_settings->m_engine.test_break()) {
			m_settings->m_engine.report(RPT_WARNING, "Export interrupted!");
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

		// Smoke domain will be exported when exporting Effects
		//
		if(isSmokeDomain(ob))
			continue;

		exportObjectBase(ob);

		expProgress += expProgStep;
		nObjects++;
		if((nObjects % progUpdateCnt) == 0) {
			m_settings->m_engine.update_progress(expProgress);
		}
	}

	m_settings->m_engine.update_progress(1.0f);

	BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
	printf(" done [%s]\n", timeMeasureBuf);
}


void VRsceneExporter::exportObjectBase(Object *ob)
{
	if(GEOM_TYPE(ob) || EMPTY_TYPE(ob)) {
		if(ob->transflag & OB_DUPLI) {
			FreeDupliList(ob);

			EvaluationContext  m_eval_ctx;
			m_eval_ctx.for_render = true;

			ob->duplilist = object_duplilist(&m_eval_ctx, m_settings->m_sce, ob);

			for(DupliObject *dob = (DupliObject*)ob->duplilist->first; dob; dob = dob->next) {
				if(m_settings->m_engine.test_break())
					break;

				exportObject(ob, dob);
			}

			FreeDupliList(ob);
		}

		if(ob->particlesystem.first) {
			for(ParticleSystem *psys = (ParticleSystem*)ob->particlesystem.first; psys; psys = psys->next) {
				ParticleSettings *pset = psys->part;

				if(pset->type != PART_HAIR)
					continue;

				if(psys->part->ren_as != PART_DRAW_PATH)
					continue;

				GeomMayaHair *geomMayaHair = new GeomMayaHair(m_settings->m_sce, m_settings->m_main, ob);
				geomMayaHair->init(psys);
				if(m_settings->m_exportNodes)
					geomMayaHair->writeNode(m_settings->m_fileObject, m_settings->m_sce->r.cfra);
				if(m_settings->m_exportGeometry)
					geomMayaHair->write(m_settings->m_fileGeom, m_settings->m_sce->r.cfra);
				if(NOT(m_settings->m_animation))
					delete geomMayaHair;
			}
		}

		if(NOT(EMPTY_TYPE(ob))) {
			if(NOT(doRenderEmitter(ob)))
				return;

			if(m_settings->m_engine.test_break())
				return;

			exportObject(ob);
		}
	}
}


void VRsceneExporter::exportObject(Object *ob, DupliObject *dOb)
{
	Node *node = new Node(m_settings->m_sce, m_settings->m_main, ob);

	if(m_settings->m_animation && m_settings->m_sce->r.cfra > m_settings->m_sce->r.sfra) {
		if(NOT(node->isAnimated() || IsMeshAnimated(ob))) {
			delete node;
			return;
		}
	}

	int meshLight = isMeshLight(ob);

	node->init(dOb, m_mtlOverride);
	if(NOT(node->getHash())) {
		delete node;
		return;
	}

	int hasGeometry = 1;
	if(m_settings->m_exportGeometry) {
		hasGeometry = node->initGeometry();
		if(hasGeometry)
			node->writeGeometry(m_settings->m_fileGeom, m_settings->m_sce->r.cfra);
	}

	if(hasGeometry && m_settings->m_exportNodes && NOT(meshLight))
		node->write(m_settings->m_fileObject, m_settings->m_sce->r.cfra);

	// In animation mode pointer is stored in cache and is freed by the cache
	//
	if(NOT(m_settings->m_animation) || NOT(hasGeometry))
		delete node;
}


int VRsceneExporter::isSmokeDomain(Object *ob)
{
	ModifierData *mod = (ModifierData*)ob->modifiers.first;
	while(mod) {
		if(mod->type == eModifierType_Smoke)
			return 1;
		mod = mod->next;
	}
	return 0;
}


int VRsceneExporter::isMeshLight(Object *ob)
{
	RnaAccess::RnaValue rna(&ob->id, "vray.LightMesh");
	return rna.getBool("use");
}


int VRsceneExporter::doRenderEmitter(Object *ob)
{
	if(ob->particlesystem.first) {
		int show_emitter = 0;
		for(ParticleSystem *psys = (ParticleSystem*)ob->particlesystem.first; psys; psys = psys->next)
			show_emitter += psys->part->draw & PART_DRAW_EMITTER;
		/* if no psys has "show emitter" selected don't render emitter */
		if (show_emitter == 0)
			return 0;
	}
	return 1;
}
