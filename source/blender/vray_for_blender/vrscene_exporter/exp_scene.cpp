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

#include "exp_scene.h"

#include "PIL_time.h"
#include "BLI_string.h"
#include "BKE_material.h"

extern "C" {
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

	m_eval_ctx.for_render = true;

	sprintf(m_interpStart, "%s", "");
	sprintf(m_interpEnd,   "%s", "");
}


VRsceneExporter::~VRsceneExporter()
{
	PRINT_INFO("VRsceneExporter::~VRsceneExporter()");

	delete m_settings;
}


void VRsceneExporter::exportScene()
{
	PRINT_INFO("VRsceneExporter::exportScene()");

	double timeMeasure = 0.0;
	char   timeMeasureBuf[32];

	PRINT_INFO_LB("VRsceneExporter: Exporting scene for frame %i...", m_settings->m_sce->r.cfra);
	timeMeasure = PIL_check_seconds_timer();

	Base *base = NULL;

	size_t nObjects = 0;
	base = (Base*)m_settings->m_sce->base.first;
	while(base) {
		nObjects++;
		base = base->next;
	}

	float  expProgress = 0.0f;
	float  expProgStep = 1.0f / nObjects;

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

		if(shouldSkip(ob))
			continue;

		// Export objects
		//
		if(m_settings->m_exportNodes) {
			if(GEOM_TYPE(ob) || EMPTY_TYPE(ob)) {
				// Free duplilist if there is some for some reason
				FreeDupliList(ob);

				ob->duplilist = object_duplilist(&m_eval_ctx, m_settings->m_sce, ob);

				for(DupliObject *dob = (DupliObject*)ob->duplilist->first; dob; dob = dob->next) {
					VRayScene::Node *node = new VRayScene::Node();
					node->init(m_settings->m_sce, m_settings->m_main, ob, dob);
					node->write(m_settings->m_fileObject, m_settings->m_sce->r.cfra);

					// TODO: Export dupli geometry checking if its already exported
				}

				FreeDupliList(ob);

				// TODO: Check particle systems for 'Render Emitter' prop

				if(NOT(EMPTY_TYPE(ob))) {
					VRayScene::Node *node = new VRayScene::Node();
					node->init(m_settings->m_sce, m_settings->m_main, ob, NULL);
					node->write(m_settings->m_fileObject, m_settings->m_sce->r.cfra);
				}
			}
		} // m_exportNodes

		// Export geometry
		//
		if(m_settings->m_exportGeometry) {
			if(NOT(m_settings->m_animation)) {
				GeomStaticMesh geomStaticMesh;
				geomStaticMesh.init(m_settings->m_sce, m_settings->m_main, ob);
				if(geomStaticMesh.getHash()) {
					geomStaticMesh.write(m_settings->m_fileGeom);
				}
			}
			else {
				if(m_settings->m_checkAnimated == ANIM_CHECK_NONE) {
					GeomStaticMesh geomStaticMesh;
					geomStaticMesh.init(m_settings->m_sce, m_settings->m_main, ob);
					if(geomStaticMesh.getHash()) {
						geomStaticMesh.write(m_settings->m_fileGeom, m_settings->m_sce->r.cfra);
					}
				}
				else if(m_settings->m_checkAnimated == ANIM_CHECK_HASH || m_settings->m_checkAnimated == ANIM_CHECK_BOTH) {
					std::string obName(ob->id.name);

					if(m_settings->m_checkAnimated == ANIM_CHECK_BOTH)
						if(NOT(IsMeshAnimated(ob)))
							continue;

					GeomStaticMesh *geomStaticMesh = new GeomStaticMesh();
					geomStaticMesh->init(m_settings->m_sce, m_settings->m_main, ob);

					MHash curHash  = geomStaticMesh->getHash();
					MHash prevHash = m_meshCache.getHash(obName);

					// TODO: add to cache for new pipeline
					//
					if(NOT(curHash == prevHash)) {
						// Write previous frame if hash is more then 'frame_step' back
						// If 'prevHash' is 0 than previous call was for the first frame
						// and no need to export
						if(prevHash) {
							int cacheFrame = m_meshCache.getFrame(obName);
							int prevFrame  = m_settings->m_sce->r.cfra - m_settings->m_sce->r.frame_step;

							if(cacheFrame < prevFrame) {
								m_meshCache.getData(obName)->write(m_settings->m_fileGeom, prevFrame);
							}
						}

						// Write current frame data
						geomStaticMesh->write(m_settings->m_fileGeom, m_settings->m_sce->r.cfra);

						// This will free previous data and store new pointer
						m_meshCache.update(obName, curHash, m_settings->m_sce->r.cfra, geomStaticMesh);
					}
				}
				else if(m_settings->m_checkAnimated == ANIM_CHECK_SIMPLE) {
					if(IsMeshAnimated(ob)) {
						GeomStaticMesh geomStaticMesh;
						geomStaticMesh.init(m_settings->m_sce, m_settings->m_main, ob);
						if(geomStaticMesh.getHash()) {
							geomStaticMesh.write(m_settings->m_fileGeom, m_settings->m_sce->r.cfra);
						}
					}
				} // ANIM_CHECK_SIMPLE
			} // animated
		} // m_exportGeometry

		expProgress += expProgStep;
		nObjects++;
		if((nObjects % 1000) == 0) {
			m_settings->m_engine.update_progress(expProgress);
		}
	} // while(base)

	m_settings->m_engine.update_progress(1.0f);

	BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
	printf(" done [%s]\n", timeMeasureBuf);
}


int VRsceneExporter::shouldSkip(Object *ob)
{
	// We should skip smoke domain object
	//
	ModifierData *mod = (ModifierData*)ob->modifiers.first;
	while(mod) {
		if(mod->type == eModifierType_Smoke)
			return 1;
		mod = mod->next;
	}

	return 0;
}
