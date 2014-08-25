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

add_definitions(
	${BOOST_DEFINITIONS}
)

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

	${CMAKE_SOURCE_DIR}/intern/guardedalloc
	${CMAKE_SOURCE_DIR}/intern/smoke/extern

	${CMAKE_SOURCE_DIR}/source/blender/blenkernel
	${CMAKE_SOURCE_DIR}/source/blender/blenlib
	${CMAKE_SOURCE_DIR}/source/blender/blenloader
	${CMAKE_SOURCE_DIR}/source/blender/editors/include
	${CMAKE_SOURCE_DIR}/source/blender/imbuf
	${CMAKE_SOURCE_DIR}/source/blender/makesdna
	${CMAKE_SOURCE_DIR}/source/blender/makesrna
	${CMAKE_SOURCE_DIR}/source/blender/python
	${CMAKE_SOURCE_DIR}/source/blender/render/extern/include
	${CMAKE_SOURCE_DIR}/source/blender/render/intern/include
	${CMAKE_SOURCE_DIR}/source/blender/windowmanager

	${CMAKE_BINARY_DIR}/source/blender/makesrna/intern
)

set(SRC
	cgr_config.h

	# Utilities
	utils/cgr_rna.h
	utils/cgr_rna.cpp
	utils/cgr_json_plugins.h
	utils/cgr_json_plugins.cpp
	utils/cgr_blender_data.h
	utils/cgr_blender_data.cpp
	utils/cgr_string.h
	utils/cgr_string.cpp
	utils/cgr_vrscene.h
	utils/cgr_vrscene.cpp
	utils/cgr_paths.h
	utils/cgr_paths.cpp
	utils/cgr_hash.h
	utils/cgr_hash.cpp

	# vrscene export
	vrscene_exporter/exp_defines.h
	vrscene_exporter/exp_anim.h

	vrscene_exporter/exp_api.h
	vrscene_exporter/exp_api.cpp
	vrscene_exporter/exp_settings.h
	vrscene_exporter/exp_settings.cpp
	vrscene_exporter/exp_scene.cpp
	vrscene_exporter/exp_scene.h
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
	vrscene_exporter/nodes/specials/Selecters.cpp
	vrscene_exporter/nodes/specials/TexGradRamp.cpp
	vrscene_exporter/nodes/specials/TexLayered.cpp
	vrscene_exporter/nodes/specials/TexRemap.cpp
	vrscene_exporter/nodes/specials/TexMulti.cpp
	vrscene_exporter/nodes/specials/MathNodes.cpp
	vrscene_exporter/nodes/specials/Blender.cpp
	vrscene_exporter/nodes/specials/UVWGen.cpp
	vrscene_exporter/nodes/specials/MtlMulti.cpp
	vrscene_exporter/nodes/specials/VRayClipper.cpp
	vrscene_exporter/nodes/specials/Effects.cpp
	vrscene_exporter/nodes/specials/Environment.cpp
	vrscene_exporter/nodes/specials/Custom.cpp
	vrscene_exporter/plugins/GeomMayaHair.cpp
	vrscene_exporter/plugins/GeomMayaHair.h
	vrscene_exporter/plugins/GeomStaticMesh.cpp
	vrscene_exporter/plugins/GeomStaticMesh.h
	vrscene_exporter/plugins/Node.cpp
	vrscene_exporter/plugins/Node.h
	vrscene_exporter/plugins/TexVoxelData.cpp
	vrscene_exporter/plugins/TexVoxelData.h
	vrscene_exporter/plugins/LightLinker.cpp
	vrscene_exporter/plugins/LightLinker.h

	python/vray_python_ext.cpp

	# C API
	cgr_vray_for_blender.h
)

blender_add_lib(vray_for_blender "${SRC}" "${INC}" "${INC_SYS}")

add_dependencies(vray_for_blender bf_rna)
