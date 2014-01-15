set(INC
    .
    ../blenlib
    ../blenkernel
    ../blenloader
    ../makesdna
    ../makesrna
    ../windowmanager
    ../../../intern/guardedalloc
    ../../../intern/smoke/extern
)

set(SRC
    exporter_geometry.c
    exporter_geometry.h

    exporter_geometry_ng.cpp
    exporter_geometry_ng.h

    exporter_includes.h

    exporter_ops.c

    ED_exporter.h
)

set(INC_SYS

)

blender_add_lib(vb_exporter "${SRC}" "${INC}" "${INC_SYS}")

