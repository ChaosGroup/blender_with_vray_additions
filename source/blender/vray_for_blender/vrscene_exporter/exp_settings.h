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

#ifndef CGR_EXP_SETTINGS_H
#define CGR_EXP_SETTINGS_H

#include "CGR_config.h"

#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"
#include "BLI_path_util.h"

#include <Python.h>


struct ExpoterSettings {
	ExpoterSettings(BL::Scene scene, BL::BlendData data, BL::RenderEngine engine):
		b_scene(scene),
		b_data(data),
		b_engine(engine)
	{
		m_sce  = NULL;
		m_main = NULL;

		m_fileObject = NULL;
		m_fileGeom   = NULL;
		m_fileLights = NULL;
		m_fileMat    = NULL;
		m_fileTex    = NULL;

		m_activeLayers  = 0;
		m_useNodeTrees  = false;
		m_useInstancerForGroup = false;
		m_useInstancerForParticles = true;

		m_isAnimation  = false;
		m_frameCurrent = 0;
		m_frameStart   = 1;
		m_frameStep    = 1;

		m_mtlOverride = "";
		m_drSharePath = "";
	}

	bool              DoUpdateCheck() { return m_isAnimation && (m_frameCurrent > m_frameStart); }

	Scene            *m_sce;
	Main             *m_main;

	BL::Scene         b_scene;
	BL::BlendData     b_data;
	BL::RenderEngine  b_engine;

	// Output files
	PyObject         *m_fileObject;
	PyObject         *m_fileGeom;
	PyObject         *m_fileLights;
	PyObject         *m_fileMat;
	PyObject         *m_fileTex;

	// Export options
	int               m_exportNodes;
	int               m_exportMeshes;
	unsigned int      m_activeLayers;
	int               m_useNodeTrees;
	int               m_useHideFromView;

	int               m_useDisplaceSubdiv;
	int               m_useInstancerForGroup;
	int               m_useInstancerForParticles;
	std::string       m_mtlOverride;

	// Animation options
	int               m_isAnimation;
	int               m_frameCurrent;
	int               m_frameStart;
	int               m_frameStep;

	// DR
	std::string       m_drSharePath;

};

#endif // CGR_EXP_SETTINGS_H
