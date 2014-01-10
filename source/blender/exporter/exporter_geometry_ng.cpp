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


// Taken from: source/blender/makesrna/intern/rna_object_api.c
// with a slight modifications
//
static Mesh *GetRenderMesh(Scene *sce, Main *bmain, Object *ob)
{
    Mesh *tmpmesh;
    Curve *tmpcu = NULL, *copycu;
    Object *tmpobj = NULL;
    Object *basis_ob = NULL;
    ListBase disp = {NULL, NULL};
    EvaluationContext eval_ctx = {0};

    /* Make a dummy mesh, saves copying */
    DerivedMesh *dm;

    CustomDataMask mask = CD_MASK_MESH;

    eval_ctx.for_render = true;

    /* perform the mesh extraction based on type */
    switch (ob->type) {
    case OB_FONT:
    case OB_CURVE:
    case OB_SURF:
        /* copies object and modifiers (but not the data) */
        tmpobj = BKE_object_copy(ob);
        tmpcu = (Curve *)tmpobj->data;
        tmpcu->id.us--;

        /* copies the data */
        tmpobj->data = BKE_curve_copy( (Curve *) ob->data );
        copycu = (Curve *)tmpobj->data;

        /* temporarily set edit so we get updates from edit mode, but
         * also because for text datablocks copying it while in edit
         * mode gives invalid data structures */
        copycu->editfont = tmpcu->editfont;
        copycu->editnurb = tmpcu->editnurb;

        /* get updated display list, and convert to a mesh */
        BKE_displist_make_curveTypes( sce, tmpobj, 0 );

        copycu->editfont = NULL;
        copycu->editnurb = NULL;

        BKE_mesh_from_nurbs( tmpobj );

        /* nurbs_to_mesh changes the type to a mesh, check it worked */
        if (tmpobj->type != OB_MESH) {
            BKE_libblock_free_us( &(G.main->object), tmpobj );
            return NULL;
        }
        tmpmesh = (Mesh*)tmpobj->data;
        BKE_libblock_free_us( &G.main->object, tmpobj );

        break;

    case OB_MBALL:
        /* metaballs don't have modifiers, so just convert to mesh */
        basis_ob = BKE_mball_basis_find(sce, ob);

        if (ob != basis_ob)
            return NULL; /* only do basis metaball */

        tmpmesh = BKE_mesh_add(bmain, "Mesh");

        BKE_displist_make_mball_forRender(&eval_ctx, sce, ob, &disp);
        BKE_mesh_from_metaball(&disp, tmpmesh);
        BKE_displist_free(&disp);

        break;

    case OB_MESH:
        /* Write the render mesh into the dummy mesh */
        dm = mesh_create_derived_render(sce, ob, mask);

        tmpmesh = BKE_mesh_add(bmain, "Mesh");
        DM_to_mesh(dm, tmpmesh, ob, mask);
        dm->release(dm);

        break;

    default:
        return NULL;
    }

    /* cycles and exporters rely on this still */
    BKE_mesh_tessface_ensure(tmpmesh);

    /* we don't assign it to anything */
    tmpmesh->id.us--;

    return tmpmesh;
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
