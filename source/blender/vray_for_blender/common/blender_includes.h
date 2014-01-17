/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLENDER_INCLUDES_H
#define BLENDER_INCLUDES_H

#ifdef __cplusplus
extern "C" {
#endif

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
#include "BKE_anim.h"
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
#include "BKE_texture.h"

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

#include "render_types.h"
#include "RE_engine.h"

#include "PIL_time.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"

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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // BLENDER_INCLUDES_H
