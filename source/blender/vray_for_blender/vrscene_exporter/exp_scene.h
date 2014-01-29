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

#ifndef CGR_EXPORT_SCENE_H
#define CGR_EXPORT_SCENE_H

#include "CGR_config.h"

#include "vrscene_exporter/exp_defines.h"
#include "vrscene_exporter/exp_anim.h"
#include "vrscene_exporter/GeomMayaHair.h"
#include "vrscene_exporter/GeomStaticMesh.h"
#include "vrscene_exporter/Node.h"

#include "BKE_depsgraph.h"
#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"

#include <Python.h>

#include <string>
#include <vector>


using namespace VRayScene;


typedef AnimationCache<VRayScene::Node>  NodesCache;


struct ExpoterSettings {
	ExpoterSettings(BL::RenderEngine engine):m_engine(engine) {
		m_sce  = NULL;
		m_main = NULL;

		m_fileObject = NULL;
		m_fileGeom   = NULL;
		m_fileLights = NULL;

		m_exportNodes    = true;
		m_exportGeometry = true;

		m_animation = false;
		m_checkAnimated = ANIM_CHECK_BOTH;

		m_activeLayers = true;
		m_altDInstances = false;
	}

	Scene            *m_sce;
	Main             *m_main;
	BL::RenderEngine  m_engine;

	PyObject         *m_fileObject;
	PyObject         *m_fileGeom;
	PyObject         *m_fileLights;

	int               m_exportNodes;
	int               m_exportGeometry;

	int               m_animation;
	int               m_checkAnimated;

	int               m_activeLayers;
	int               m_altDInstances;
};


class VRsceneExporter {
public:
	VRsceneExporter(ExpoterSettings *settings);
	~VRsceneExporter();

	void               exportScene();

private:
	void               exportObjectBase(Object *ob);
	void               exportObject(Object *ob, DupliObject *dOb=NULL);

	int                isSmokeDomain(Object *ob);
	int                doRenderEmitter(Object *ob);

	NodesCache         m_nodeCache;
	EvaluationContext  m_eval_ctx;

	ExpoterSettings   *m_settings;

	char               m_interpStart[32];
	char               m_interpEnd[3];

	PYTHON_PRINT_BUF;

};

#endif // CGR_EXPORT_SCENE_H
