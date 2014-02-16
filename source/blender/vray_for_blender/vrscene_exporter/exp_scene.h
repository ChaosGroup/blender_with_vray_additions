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
#include "vrscene_exporter/exp_types.h"

#include "BKE_depsgraph.h"
#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"

#include <Python.h>

#include <string>
#include <vector>


using namespace VRayScene;


class VRsceneExporter {
public:
	VRsceneExporter(ExpoterSettings *settings);
	~VRsceneExporter();

	void               exportScene();

private:
	void               init();

	void               exportObjectBase(Object *ob);
	void               exportObject(Object *ob, DupliObject *dOb=NULL);
	void               exportLight(Object *ob, DupliObject *dOb=NULL);

	int                checkUpdates();

	ExpoterSettings   *m_settings;

	std::string        m_mtlOverride;

	PYTHON_PRINT_BUF;

};

#endif // CGR_EXPORT_SCENE_H
