# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
#
# ***** END GPL LICENSE BLOCK *****

set(INC_SYS
	${PYTHON_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIRS}
	${BOOST_INCLUDE_DIR}
)

set(INC
	.
	./utils
	./vrscene_exporter
	./vrscene_exporter/nodes
	./vrscene_exporter/plugins
	../blenlib
	../blenkernel
	../blenloader
	../makesdna
	../makesrna
	../makesrna/intern
	../imbuf
	../windowmanager
	../editors/include
	../render/extern/include
	../render/intern/include
	../python
	${CMAKE_BINARY_DIR}/source/blender/makesrna/intern
	../../../intern/guardedalloc
	../../../intern/smoke/extern
)

set(SRC
	CGR_config.h

	# Utilities
	utils/CGR_base64.h
	utils/CGR_base64.cpp
	utils/CGR_rna.h
	utils/CGR_rna.cpp
	utils/CGR_json_plugins.h
	utils/CGR_json_plugins.cpp
	utils/CGR_blender_data.h
	utils/CGR_blender_data.cpp
	utils/CGR_string.h
	utils/CGR_string.cpp
	utils/CGR_vrscene.h
	utils/CGR_vrscene.cpp
	utils/cgr_paths.h
	utils/cgr_paths.cpp
	utils/murmur3.h
	utils/murmur3.cpp

	# vrscene export
	vrscene_exporter/exp_anim.h
	vrscene_exporter/exp_defines.h
	vrscene_exporter/exp_settings.h
	vrscene_exporter/exp_geom.cpp
	vrscene_exporter/exp_hair.cpp
	vrscene_exporter/exp_scene.cpp
	vrscene_exporter/exp_scene.h
	vrscene_exporter/exp_smoke.cpp
	vrscene_exporter/exp_types.cpp
	vrscene_exporter/exp_types.h
	vrscene_exporter/nodes/exp_nodes.cpp
	vrscene_exporter/nodes/exp_nodes.h
	vrscene_exporter/nodes/specials/BRDFLayered.cpp
	vrscene_exporter/nodes/specials/BitmapBuffer.cpp
	vrscene_exporter/nodes/specials/BlenderOutputGeometry.cpp
	vrscene_exporter/nodes/specials/BlenderOutputMaterial.cpp
	vrscene_exporter/nodes/specials/GeomDisplacedMesh.cpp
	vrscene_exporter/nodes/specials/LightMesh.cpp
	vrscene_exporter/nodes/specials/SelectGroup.cpp
	vrscene_exporter/nodes/specials/SelectNodeTree.cpp
	vrscene_exporter/nodes/specials/SelectObject.cpp
	vrscene_exporter/nodes/specials/TexGradRamp.cpp
	vrscene_exporter/nodes/specials/TexLayered.cpp
	vrscene_exporter/nodes/specials/TexRemap.cpp
	vrscene_exporter/nodes/specials/TexMulti.cpp
	vrscene_exporter/nodes/specials/MathNodes.cpp
	vrscene_exporter/nodes/specials/Blender.cpp
	vrscene_exporter/nodes/specials/UVWGen.cpp
	vrscene_exporter/nodes/specials/MtlMulti.cpp
	vrscene_exporter/plugins/GeomMayaHair.cpp
	vrscene_exporter/plugins/GeomMayaHair.h
	vrscene_exporter/plugins/GeomMeshFile.cpp
	vrscene_exporter/plugins/GeomMeshFile.h
	vrscene_exporter/plugins/GeomPlane.cpp
	vrscene_exporter/plugins/GeomPlane.h
	vrscene_exporter/plugins/GeomStaticMesh.cpp
	vrscene_exporter/plugins/GeomStaticMesh.h
	vrscene_exporter/plugins/Light.cpp
	vrscene_exporter/plugins/Light.h
	vrscene_exporter/plugins/Node.cpp
	vrscene_exporter/plugins/Node.h
	vrscene_exporter/plugins/TexVoxelData.cpp
	vrscene_exporter/plugins/TexVoxelData.h
	vrscene_exporter/plugins/LightLinker.cpp
	vrscene_exporter/plugins/LightLinker.h

	vrscene_exporter/vrscene_api.h

	python/vray_python_ext.cpp

	# C API
	CGR_vray_for_blender.h
)

blender_add_lib(vray_for_blender "${SRC}" "${INC}" "${INC_SYS}")

add_dependencies(vray_for_blender bf_rna)
