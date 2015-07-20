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

#include <Python.h>

#include <boost/asio/ip/host_name.hpp>

#include "cgr_config.h"

#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"
#include "BLI_path_util.h"


class VRsceneExporter;


struct ExporterSettings {
	enum VRayDRAssetShare {
		eTRANSFER = 0,
		eSHARE,
		eABSOLUTE,
	};

	enum VRayDRNetType {
		eWINDOWS = 0,
		eUNIX,
	};

	enum VRayDefMapping {
		eObject = 0,
		eCube,
		eChannel
	};

	enum VRayExportFormat {
		FormatZIP = 0,
		FormatHEX,
		FormatPlainText
	};

	static ExporterSettings gSet;

	ExporterSettings():
		b_context(PointerRNA_NULL),
		b_scene(PointerRNA_NULL),
		b_data(PointerRNA_NULL),
		b_engine(PointerRNA_NULL),
		m_mtlOverride(PointerRNA_NULL),
		m_anim_check_cache(true)
	{}

	void              init(BL::Context context, BL::Scene scene, BL::BlendData data, BL::RenderEngine engine);
	void              init();
	void              reset();

	bool              DoUpdateCheck();
	bool              IsFirstFrame();

	Scene            *m_sce;
	Main             *m_main;
	bContext         *m_ctx;

	BL::Context       b_context;
	BL::Scene         b_scene;
	BL::BlendData     b_data;
	BL::RenderEngine  b_engine;

	// Output files
	PyObject         *m_fileMain;
	PyObject         *m_fileObject;
	PyObject         *m_fileEnv;
	PyObject         *m_fileGeom;
	PyObject         *m_fileLights;
	PyObject         *m_fileMat;
	PyObject         *m_fileTex;

	// Export options
	int               m_exportNodes;
	int               m_exportMeshes;
	int               m_exportHair;
	int               m_exportSmoke;
	unsigned int      m_activeLayers;
	int               m_useHideFromView;
	int               m_useDisplaceSubdiv;
	int               m_useAltInstances;
	int               m_subsurfToOSD;
	VRayDefMapping    m_defaultMapping;
	VRayExportFormat  m_dataExportFormat;

	std::string       m_mtlOverrideName;
	BL::Material      m_mtlOverride;

	// Animation options
	int               m_isAnimation;
	float             m_frameCurrent;
	float             m_frameStart;
	float             m_frameStep;

	// DR
	std::string       m_drSharePath;
	std::string       m_drShareName;
	VRayDRNetType     m_drNetType;
	VRayDRAssetShare  m_drAssetShare;
	std::string       m_hostname;

	VRsceneExporter  *m_exporter;

	int               m_anim_check_cache;

};

#endif // CGR_EXP_SETTINGS_H
