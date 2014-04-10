include_directories(/home/bdancer/devel/globals/include)

set(INC_SYS
	${PYTHON_INCLUDE_DIRS}
)

set(INC
	.
	./utils
	./vrscene_exporter
	../blenlib
	../blenkernel
	../blenloader
	../makesdna
	../makesrna
	../imbuf
	../windowmanager
	../editors/include
	../render/extern/include
	../render/intern/include
	${CMAKE_BINARY_DIR}/source/blender/makesrna/intern
	${ZLIB_INCLUDE_DIRS}
	../../../intern/guardedalloc
	../../../intern/smoke/extern
	../python
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
	utils/murmur3.h
	utils/murmur3.cpp

	# vrscene export
	vrscene_exporter/Node.h
	vrscene_exporter/Node.cpp
	vrscene_exporter/Light.h
	vrscene_exporter/Light.cpp
	vrscene_exporter/GeomMayaHair.h
	vrscene_exporter/GeomMayaHair.cpp
	vrscene_exporter/GeomStaticMesh.h
	vrscene_exporter/GeomStaticMesh.cpp
	vrscene_exporter/GeomPlane.h
	vrscene_exporter/GeomPlane.cpp
	vrscene_exporter/GeomMeshFile.h
	vrscene_exporter/GeomMeshFile.cpp
	vrscene_exporter/TexVoxelData.h
	vrscene_exporter/TexVoxelData.cpp
	vrscene_exporter/exp_defines.h
	vrscene_exporter/exp_types.h
	vrscene_exporter/exp_types.cpp
	vrscene_exporter/exp_anim.h
	vrscene_exporter/exp_scene.h
	vrscene_exporter/exp_scene.cpp
	vrscene_exporter/exp_nodes.h
	vrscene_exporter/exp_nodes.cpp
	vrscene_exporter/exp_geom.cpp
	vrscene_exporter/exp_hair.cpp
	vrscene_exporter/exp_smoke.cpp

	vrscene_exporter/vrscene_api.h

	python/vray_python_ext.cpp

	# C API
	CGR_vray_for_blender.h
)

blender_add_lib(vray_for_blender "${SRC}" "${INC}" "${INC_SYS}")
