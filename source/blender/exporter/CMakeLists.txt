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
    exporter_nodes.c
    exporter_socket.c
    exporter_ops.c

    exporter_includes.h
    exporter_geometry.h
    exporter_socket.h
    exporter_nodes.h

    ED_exporter.h
)

set(INC_SYS

)

blender_add_lib(vb_exporter "${SRC}" "${INC}" "${INC_SYS}")

