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
	./vrscene_exporter/instancer

	${CMAKE_SOURCE_DIR}/intern/guardedalloc
	${CMAKE_SOURCE_DIR}/intern/smoke/extern

	${CMAKE_SOURCE_DIR}/source/blender/blenkernel
	${CMAKE_SOURCE_DIR}/source/blender/blenlib
	${CMAKE_SOURCE_DIR}/source/blender/blenloader
	${CMAKE_SOURCE_DIR}/source/blender/editors/include
	${CMAKE_SOURCE_DIR}/source/blender/imbuf
	${CMAKE_SOURCE_DIR}/source/blender/makesdna
	${CMAKE_SOURCE_DIR}/source/blender/makesrna
	${CMAKE_SOURCE_DIR}/source/blender/makesrna/intern
	${CMAKE_SOURCE_DIR}/source/blender/python
	${CMAKE_SOURCE_DIR}/source/blender/render/extern/include
	${CMAKE_SOURCE_DIR}/source/blender/render/intern/include
	${CMAKE_SOURCE_DIR}/source/blender/windowmanager

	${CMAKE_BINARY_DIR}/source/blender/makesrna/intern
)

file(GLOB_RECURSE SRC "*.*")

blender_add_lib(vray_for_blender "${SRC}" "${INC}" "${INC_SYS}")

# avoid link failure with clang 3.4 debug
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT ${CMAKE_C_COMPILER_VERSION} VERSION_LESS '3.4')
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gline-tables-only")
endif()

add_dependencies(vray_for_blender bf_rna)
