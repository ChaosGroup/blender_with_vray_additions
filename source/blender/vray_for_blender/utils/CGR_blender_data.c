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

#include "CGR_data.h"

#include "DNA_curve_types.h"
#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_global.h"
#include "BKE_mball.h"
#include "BKE_library.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_curve.h"
#include "BKE_mesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"

#include <string.h>


Mesh* GetRenderMesh(Scene *sce, Main *bmain, Object *ob)
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
