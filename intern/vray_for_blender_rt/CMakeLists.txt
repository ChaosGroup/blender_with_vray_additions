# V-Ray For Blender Scene Exporter
#

add_definitions(
	${BOOST_DEFINITIONS}
)

set(INC_SYS
	${PYTHON_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIRS}
	${BOOST_INCLUDE_DIR}
)

set(INC
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

	../vray_for_blender

	.
	src
	src/params
	src/export
	src/export/utils

	/home/bdancer/src/appsdk_releases/428/linux/cpp/include
)

file(GLOB_RECURSE HEADERS "*.h")
file(GLOB_RECURSE SOURCES "*.cpp")

if (UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -std=c++11")
endif()

blender_add_lib(vray_for_blender_rt "${HEADERS};${SOURCES}" "${INC}" "${INC_SYS}")

add_dependencies(vray_for_blender_rt bf_rna)
