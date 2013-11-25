include_directories(/home/bdancer/devel/globals/include)

set(INC_SYS
	${PYTHON_INCLUDE_DIRS}
)

set(INC
	.
	./common
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
	../../../intern/guardedalloc
	../../../intern/smoke/extern
)

set(SRC
	CGR_config.h

	# Common stuff
	common/blender_includes.h
	utils/CGR_rna.h
	utils/CGR_rna.cpp
	utils/CGR_data.h
	utils/CGR_data.c
	utils/CGR_string.h
	utils/CGR_string.c

	# vrscene export
	vrscene_exporter/vrscene.h
	vrscene_exporter/vrscene.cpp
	vrscene_exporter/geom.c
	vrscene_exporter/hair.c
	vrscene_exporter/node.cpp
	vrscene_exporter/smoke.cpp

	python/vray_python_ext.cpp

	# C API
	CGR_vray_for_blender.h
)

blender_add_lib(vray_for_blender "${SRC}" "${INC}" "${INC_SYS}")
