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

#include "vfb_node_exporter.h"
#include "vfb_utils_mesh.h"
#include "vfb_log.h"

using namespace VRayForBlender;

AttrValue DataExporter::exportGeomStaticMesh(BL::Object ob, const ObjectOverridesAttrs &oattrs)
{
	AttrValue geom;

	const std::string meshName = getMeshName(ob);

	PluginDesc geomDesc(meshName, "GeomStaticMesh");

	Mesh::ExportOptions options;
	options.merge_channel_vertices = false;
	options.mode = m_evalMode;
	options.use_subsurf_to_osd = m_settings.use_subsurf_to_osd;
	options.force_dynamic_geometry = m_settings.is_gpu && m_settings.is_viewport ||
	                                 oattrs && oattrs.useInstancer;

	const Mesh::MeshExportResult res = FillMeshData(m_data,
	                                                m_scene,
	                                                ob,
	                                                options,
	                                                geomDesc,
	                                                m_exporter->getPluginManager(),
	                                                m_exporter->get_current_frame(),
	                                                !isIPR);

	switch (res) {
		case Mesh::MeshExportResult::exported: {
			geom = m_exporter->export_plugin(geomDesc);
			break;
		}
		case Mesh::MeshExportResult::cached: {
			geom = AttrPlugin(meshName);
			break;
		}
		case Mesh::MeshExportResult::error:
		default: {
			break;
		}
	}

	return geom;
}
