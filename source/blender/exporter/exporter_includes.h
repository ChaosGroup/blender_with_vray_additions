/*

  V-Ray/Blender

  http://vray.cgdo.ru

  Author: Andrey M. Izrantsev (aka bdancer)
  E-Mail: izrantsev@cgdo.ru

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  All Rights Reserved. V-Ray(R) is a registered trademark of Chaos Software.

*/

#ifndef EXPORTER_INCLUDES_H
#define EXPORTER_INCLUDES_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_voxel.h"
#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_rotation.h"
#include "BLI_fileops.h"

#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_context.h"
#include "BKE_utildefines.h"
#include "BKE_library.h"
#include "BKE_DerivedMesh.h"
#include "BKE_fcurve.h"
#include "BKE_animsys.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_curve.h"
#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_anim.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_material.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_group_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_particle_types.h"
#include "DNA_smoke_types.h"
#include "DNA_listBase.h"

#include "PIL_time.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "smoke_API.h"

#ifdef WIN32
#  ifdef htonl
#    undef htonl
#    undef htons
#    undef ntohl
#    undef ntohs
#    define correctByteOrder(x) htonl(x)
#  endif
#  include <winsock.h>
#else
#  include <netinet/in.h>
#endif

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"

#define USE_DEBUG  1
#define QTCREATOR  0

// scons -Q define=QTCREATOR
#if QTCREATOR
#   define VRAY_PROMPT "V-Ray/Blender: "
#else
#   ifdef __linux__
#      define VRAY_PROMPT "\033[0;32mV-Ray/Blender\033[0m: "
#   else
#      define VRAY_PROMPT "V-Ray/Blender: "
#   endif
#endif

#if USE_DEBUG
#  define DEBUG_OUTPUT(use_debug, ...) \
    if(use_debug) { \
        fprintf(stdout, VRAY_PROMPT); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
        fflush(stdout); \
    }
#else
#  define DEBUG_OUTPUT(use_debug, ...)
#endif

#define COPY_VECTOR_3_3(a, b) \
    a[0] = b[0];\
    a[1] = b[1];\
    a[2] = b[2];

#define HEX(x) htonl(*(int*)&(x))
#define WRITE_HEX_VALUE(f, v) fprintf(f, "%08X", HEX(v))
#define WRITE_HEX_VECTOR(f, v) fprintf(f, "%08X%08X%08X", HEX(v[0]), HEX(v[1]), HEX(v[2]))

#define WRITE_TRANSFORM(f, m) fprintf(f, "Transform(Matrix(Vector(%f, %f, %f),Vector(%f, %f, %f),Vector(%f, %f, %f)),Vector(%f, %f, %f))", \
    m[0][0], m[0][1], m[0][2],\
    m[1][0], m[1][1], m[1][2],\
    m[2][0], m[2][1], m[2][2],\
    m[3][0], m[3][1], m[3][2]);

#define WRITE_TRANSFORM_HEX(f, m) fprintf(f, "TransformHex(\"%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X\")", \
    HEX(m[0][0]), HEX(m[0][1]), HEX(m[0][2]),\
    HEX(m[1][0]), HEX(m[1][1]), HEX(m[1][2]),\
    HEX(m[2][0]), HEX(m[2][1]), HEX(m[2][2]),\
    HEX(m[3][0]), HEX(m[3][1]), HEX(m[3][2]))

#define PRINT_TRANSFORM(m) printf("Transform(Matrix(Vector(%f, %f, %f),Vector(%f, %f, %f),Vector(%f, %f, %f)),Vector(%f, %f, %f))\n", \
    m[0][0], m[0][1], m[0][2],\
    m[1][0], m[1][1], m[1][2],\
    m[2][0], m[2][1], m[2][2],\
    m[3][0], m[3][1], m[3][2]);\
    fflush(stdout)

#define PRINT_TRANSFORM_HEX(f, m) printf("TransformHex(\"%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X\")", \
    HEX(m[0][0]), HEX(m[0][1]), HEX(m[0][2]),\
    HEX(m[1][0]), HEX(m[1][1]), HEX(m[1][2]),\
    HEX(m[2][0]), HEX(m[2][1]), HEX(m[2][2]),\
    HEX(m[3][0]), HEX(m[3][1]), HEX(m[3][2]))

#endif // EXPORTER_INCLUDES_H
