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

#include "../vray_for_blender/CGR_config.h"
#include "../vray_for_blender/utils/CGR_data.h"

#include "MurmurHash2.h"
#include "exporter_geometry_ng.h"

#include <vector>


#define GEOM_TYPE(ob) ob->type == OB_MESH || ob->type == OB_CURVE || ob->type == OB_SURF  || ob->type == OB_FONT  || ob->type == OB_MBALL


static int GetMeshHash(Mesh *mesh)
{
    unsigned int h = MurmurHash2(mesh->mvert, mesh->totvert * sizeof(MVert), 42);

    PRINT_INFO("Mesh '%s' hash is '%i'\n", mesh->id.name, h);

    return h;
}


static void ExportMeshes(Scene *sce, Main *bmain, const char *filepath, int activeLayers, int altDInstances)
{
    Base *base = (Base*)sce->base.first;
    while(base) {
        Object *ob = base->object;

        if(GEOM_TYPE(ob)) {
            Mesh *mesh = GetRenderMesh(sce, bmain, ob);
            if(mesh) {
                GetMeshHash(mesh);

                /* remove the temporary mesh */
                BKE_mesh_free(mesh, TRUE);
                BLI_remlink(&bmain->mesh, mesh);
                MEM_freeN(mesh);
            }
        }

        base = base->next;
    }
}


void ExportGeometryAnimation(Scene *sce, Main *bmain, const char *filepath, int activeLayers, int altDInstances)
{
    PRINT_INFO("ExportGeometryAnimation()");

    EvaluationContext eval_ctx = {0};
    eval_ctx.for_render = true;

    double timeMeasure = 0.0;
    char   timeMeasureBuf[32];

    // Store selected frame
    int frameCurrent = sce->r.cfra;

    int frameStart   = sce->r.sfra;
    int frameEnd     = sce->r.efra;

    int fra = 0;

    PRINT_INFO("Exporting meshes for the start frame %i...", frameStart);

    timeMeasure = PIL_check_seconds_timer();

    // Setup frame
    sce->r.cfra = frameStart;
    CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);

    // Update scene
    BKE_scene_update_for_newframe(&eval_ctx, bmain, sce, (1<<20) - 1);

    // Export stuff
    ExportMeshes(sce, bmain, filepath, activeLayers, altDInstances);

    // Setup next frame
    fra += sce->r.frame_step;

    BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
    printf(" done [%s]\n", timeMeasureBuf);

    /* Export meshes for the rest frames */
    while(fra <= frameEnd) {
        PRINT_INFO("Exporting meshes for frame %i...", fra);

        timeMeasure = PIL_check_seconds_timer();

        // Set frame
        sce->r.cfra = fra;
        CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);

        // Update scene
        BKE_scene_update_for_newframe(&eval_ctx, bmain, sce, (1<<20) - 1);

        // Export stuff
        ExportMeshes(sce, bmain, filepath, activeLayers, altDInstances);

        BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
        printf(" done [%s]\n", timeMeasureBuf);

        // Setup next frame
        fra += sce->r.frame_step;
    }

    // Restore selected frame
    sce->r.cfra = frameCurrent;
    CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);
    BKE_scene_update_for_newframe(&eval_ctx, bmain, sce, (1<<20) - 1);
}
