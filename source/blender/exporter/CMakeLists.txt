set(INC
	.
	../blenlib
	../blenkernel
	../blenloader
	../makesdna
	../makesrna
	../windowmanager
	../render/extern/include
	../render/intern/include
	../../../intern/guardedalloc
	../../../intern/smoke/extern
	../vray_for_blender
)

set(SRC
	cgr_export_nodes.h
	cgr_export_nodes.cpp

	cgr_export_geom.h
	cgr_export_geom.cpp

	cgr_export_op.h
	cgr_export_op.c

	ED_exporter.h
	ED_exporter.c
)

set(INC_SYS

)

blender_add_lib(vb_exporter "${SRC}" "${INC}" "${INC_SYS}")
