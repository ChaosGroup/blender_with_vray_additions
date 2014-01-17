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

#include "CGR_config.h"

#include "GeomMayaHair.h"
#include "CGR_blender_data.h"
#include "CGR_vrscene.h"

extern "C" {
#  include "DNA_modifier_types.h"
#  include "BKE_depsgraph.h"
#  include "BKE_DerivedMesh.h"
#  include "BKE_particle.h"
#  include "BKE_scene.h"
#  include "BLI_math.h"
#  include "MEM_guardedalloc.h"
#  include "RNA_access.h"
}


typedef struct ParticleStrandData {
    float *uvco;
    int    totuv;
} ParticleStrandData;


// Taken from "source/blender/render/intern/source/convertblender.c"
// and slightly modified
//
static void get_particle_uvco_mcol(short from, DerivedMesh *dm, float *fuv, int num, ParticleStrandData *sd)
{
    int i;

    /* get uvco */
    if (sd->uvco && ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
        for (i=0; i<sd->totuv; i++) {
            if (num != DMCACHE_NOTFOUND) {
                MFace  *mface  = (MFace*)dm->getTessFaceData(dm, num, CD_MFACE);
                MTFace *mtface = (MTFace*)CustomData_get_layer_n(&dm->faceData, CD_MTFACE, i);
                mtface += num;

                psys_interpolate_uvs(mtface, mface->v4, fuv, sd->uvco + 2 * i);
            }
            else {
                sd->uvco[2*i] = 0.0f;
                sd->uvco[2*i + 1] = 0.0f;
            }
        }
    }
}


GeomMayaHair::GeomMayaHair()
{
    hash = 0;

    hair_vertices = NULL;
    num_hair_vertices = NULL;
    widths = NULL;
    transparency = NULL;

    use_global_hair_tree = 1;

    geom_splines = 0;
    geom_tesselation_mult = 1.0;
}


void GeomMayaHair::freeData()
{
    if(hair_vertices) {
        delete [] hair_vertices;
        hair_vertices = NULL;
    }
    if(num_hair_vertices) {
        delete [] num_hair_vertices;
        num_hair_vertices = NULL;
    }
    if(widths) {
        delete [] widths;
        widths = NULL;
    }
    if(transparency) {
        delete [] transparency;
        transparency = NULL;
    }
}


void GeomMayaHair::init(Scene *sce, Main *main, Object *ob, ParticleSystem *psys)
{
#if 0
    int    i, c, p, s;
    float  f;
    float  t;

    EvaluationContext eval_ctx = {0};

    ParticleSettings           *pset = NULL;
    ParticleSystemModifierData *psmd = NULL;

    ParticleData       *pa   = NULL;
    HairKey            *hkey = NULL;
    ParticleStrandData  sd;

    ParticleCacheKey **child_cache = NULL;
    ParticleCacheKey  *child_key   = NULL;
    ChildParticle     *cpa         = NULL;
    int                child_total = 0;
    int                child_steps = 0;
    float              child_key_co[3];

    float     hairmat[4][4];
    float     segment[3];
    float     color[3] = {0.5f,0.5f,0.5f};
    float     width = 0.001f;
    float     cone_width = 0.001f;
    int       num = -1;

    int       spline_init_flag;
    int       interp_points_count;
    float     interp_points_step;
    int       data_points_count;
    float     data_points_step;
    float     data_points_ordinates[3][64];
    float     data_points_abscissas[64];

    float     s_b[3][16];
    float     s_c[3][16];
    float     s_d[3][16];

    int       spline_last[3];

    short     use_cone    = false;
    short     use_child   = 0;
    short     free_edit   = 0;
    short     need_recalc = 0;

    PointerRNA  rna_pset;
    PointerRNA  VRayParticleSettings;
    PointerRNA  VRayFur;

    int  display_percentage;
    int  display_percentage_child;

    int  debug = false;

    eval_ctx.for_render = true;

    need_recalc = 0;

    pset = psys->part;
    if(pset->type != PART_HAIR)
        return;
    if(psys->part->ren_as != PART_DRAW_PATH)
        return;

    psmd = psys_get_modifier(ob, psys);
    if(NOT(psmd))
        return;
    if(NOT(psmd->modifier.mode & eModifierMode_Render))
        return;

    RNA_id_pointer_create(&pset->id, &rna_pset);

    if(RNA_struct_find_property(&rna_pset, "vray")) {
        VRayParticleSettings= RNA_pointer_get(&rna_pset, "vray");

        if(RNA_struct_find_property(&VRayParticleSettings, "VRayFur")) {
            VRayFur = RNA_pointer_get(&VRayParticleSettings, "VRayFur");

            // Get hair width
            width = RNA_float_get(&VRayFur, "width");
        }
    }

    child_cache = psys->childcache;
    child_total = psys->totchildcache;
    use_child   = (pset->childtype && child_cache);

    // Store "Display percentage" setting
    display_percentage       = pset->disp;
    display_percentage_child = pset->child_nbr;

    // Check if particles are edited
    free_edit = psys_check_edited(psys);

    // Recalc parent hair only if they are not
    // manually edited
    if(!free_edit) {
        need_recalc = 1;
        pset->disp = 100;
        psys->recalc |= PSYS_RECALC;
    }

    if(use_child) {
        need_recalc = 1;
        pset->child_nbr = pset->ren_child_nbr;
        psys->recalc |= PSYS_RECALC_CHILD;
    }

    if(psys->flag & PSYS_HAIR_DYNAMICS)
        need_recalc = 0;

    // Recalc hair with render settings
    if(need_recalc) {
        ob->recalc |= OB_RECALC_ALL;
        BKE_scene_update_tagged(&eval_ctx, main, sce);
    }

    // Get new child data pointers
    if(use_child) {
        child_cache = psys->childcache;
        child_total = psys->totchildcache;
    }

    // Store the number or vertices per hair
    //
    if(use_child) {
        for(p = 0; p < child_total; ++p) {
            child_cache[p]->steps;
        }
    }
    else {
        LOOP_PARTICLES {
            pa->totkey;
        }
    }

    WRITE_PYOBJECT(outputFile, "\n\thair_vertices=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
    if(use_child) {
        for(p = 0; p < child_total; ++p) {
            child_key   = child_cache[p];
            child_steps = child_key->steps;

            // Spline interpolation
            data_points_count = child_steps;
            data_points_step  = 1.0f / (child_steps - 1);

            // Store control points
            for(s = 0, f = 0.0f; s < child_steps; ++s, ++child_key, f += data_points_step) {
                data_points_abscissas[s] = f;

                // Child particles are stored in world space,
                // but we need them in object space
                copy_v3_v3(child_key_co, child_key->co);

                // Remove transform by applying inverse matrix
                mul_m4_v3(ob->imat, child_key_co);

                for(c = 0; c < 3; ++c) {
                    data_points_ordinates[c][s] = child_key_co[c];
                }
            }

            // Init spline coefficients
            for(c = 0; c < 3; ++c) {
                c_spline_init(data_points_count, 0, 0, 0.0f, 0.0f,
                              data_points_abscissas, data_points_ordinates[c],
                              s_b[c], s_c[c], s_d[c], &spline_init_flag);
            }

            // Write interpolated child points
            for(c = 0; c < 3; ++c)
                spline_last[c] = 0;

            for(t = 0.0f; t <= 1.0; t += interp_points_step) {
                // Calculate interpolated coordinate
                for(c = 0; c < 3; ++c) {
                    segment[c] = c_spline_eval(data_points_count, t, data_points_abscissas, data_points_ordinates[c],
                                               s_b[c], s_c[c], s_d[c], &spline_last[c]);
                }

                WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
            }
        }
    }
    else {
        LOOP_PARTICLES {
            DEBUG_PRINT(debug, "\033[0;32mV-Ray/Blender:\033[0m Particle system: %s => Hair: %i\n", psys->name, p + 1);

            psys_mat_hair_to_object(NULL, psmd->dm, psmd->psys->part->from, pa, hairmat);

            // Spline interpolation
            data_points_count = pa->totkey;
            data_points_step  = 1.0f / (data_points_count - 1);

            DEBUG_PRINT(debug, "data_points_count = %i", data_points_count);
            DEBUG_PRINT(debug, "data_points_step = %.3f", data_points_step);

            for(i = 0, f = 0.0f; i < data_points_count; ++i, f += data_points_step) {
                data_points_abscissas[i] = f;
            }

            // Store control points
            for(s = 0, hkey = pa->hair; s < pa->totkey; ++s, ++hkey) {
                copy_v3_v3(segment, hkey->co);
                mul_m4_v3(hairmat, segment);

                for(c = 0; c < 3; ++c) {
                    data_points_ordinates[c][s] = segment[c];
                }
            }

            // Init spline coefficients
            for(c = 0; c < 3; ++c) {
                c_spline_init(data_points_count, 0, 0, 0.0f, 0.0f,
                              data_points_abscissas, data_points_ordinates[c],
                              s_b[c], s_c[c], s_d[c], &spline_init_flag);
            }

            // Write interpolated points
            for(c = 0; c < 3; ++c)
                spline_last[c] = 0;
            for(t = 0.0f; t <= 1.0; t += interp_points_step) {
                // Calculate interpolated coordinates
                for(c = 0; c < 3; ++c) {
                    segment[c] = c_spline_eval(data_points_count, t, data_points_abscissas, data_points_ordinates[c],
                                               s_b[c], s_c[c], s_d[c], &spline_last[c]);
                }
                WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
            }
        }
    }
    WRITE_PYOBJECT(outputFile, "\")));");

    memset(&sd, 0, sizeof(ParticleStrandData));

    // DEBUG_PRINT(TRUE, "psmd->dm = 0x%X", psmd->dm);

    if(psmd->dm) {
        if(use_child) {
            sd.totuv = CustomData_number_of_layers(&psmd->dm->faceData, CD_MTFACE);

            if(sd.totuv) {
                sd.uvco = (float*)MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");
            }
            else {
                sd.uvco = NULL;
            }

            if(sd.uvco) {
                WRITE_PYOBJECT(outputFile, "\n\tstrand_uvw=interpolate((%d,ListVectorHex(\"", sce->r.cfra);

                for(p = 0; p < child_total; ++p) {
                    cpa = psys->child + p;

                    /* get uvco & mcol */
                    if(pset->childtype==PART_CHILD_FACES) {
                        get_particle_uvco_mcol(PART_FROM_FACE, psmd->dm, cpa->fuv, cpa->num, &sd);
                    }
                    else {
                        ParticleData *parent = psys->particles + cpa->parent;
                        num = parent->num_dmcache;

                        if (num == DMCACHE_NOTFOUND)
                            if (parent->num < psmd->dm->getNumTessFaces(psmd->dm))
                                num = parent->num;

                        get_particle_uvco_mcol(pset->from, psmd->dm, parent->fuv, num, &sd);
                    }

                    segment[0] = sd.uvco[0];
                    segment[1] = sd.uvco[1];
                    segment[2] = 0.0f;

                    WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
                }
                WRITE_PYOBJECT(outputFile, "\")));");

                MEM_freeN(sd.uvco);
            }
        }
        else {
            sd.totuv = CustomData_number_of_layers(&psmd->dm->faceData, CD_MTFACE);

            if(sd.totuv) {
                sd.uvco = (float*)MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");
            }
            else {
                sd.uvco = NULL;
            }

            if(sd.uvco) {
                WRITE_PYOBJECT(outputFile, "\n\tstrand_uvw=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
                LOOP_PARTICLES {
                    /* get uvco & mcol */
                    num = pa->num_dmcache;

                    if(num == DMCACHE_NOTFOUND) {
                        if(pa->num < psmd->dm->getNumTessFaces(psmd->dm)) {
                            num = pa->num;
                        }
                    }

                    get_particle_uvco_mcol(pset->from, psmd->dm, pa->fuv, num, &sd);

                    // DEBUG_PRINT(TRUE, "Pa.uv = %.3f, %.3f", sd.uvco[0], sd.uvco[1]);

                    segment[0] = sd.uvco[0];
                    segment[1] = sd.uvco[1];
                    segment[2] = 0.0f;

                    WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
                }
                WRITE_PYOBJECT(outputFile, "\")));");

                MEM_freeN(sd.uvco);
            }
        }
    }

    WRITE_PYOBJECT(outputFile, "\n\twidths=interpolate((%d,ListFloatHex(\"", sce->r.cfra);
    if(use_child) {
        for(p = 0; p < child_total; ++p) {
            cone_width = width;
            for(s = 0; s < interp_points_count; ++s) {
                if(use_cone && s > 0) {
                    cone_width = width / s;
                }
                WRITE_PYOBJECT_HEX_VALUE(outputFile, cone_width);
            }
        }
    }
    else {
        for(p = 0; p < psys->totpart; ++p) {
            for(s = 0; s < interp_points_count; ++s) {
                cone_width = width;
                if(use_cone && s > 0) {
                    cone_width = width / s;
                }
                WRITE_PYOBJECT_HEX_VALUE(outputFile, cone_width);
            }
        }
    }
    WRITE_PYOBJECT(outputFile, "\")));");

    WRITE_PYOBJECT(outputFile, "\n\tcolors=interpolate((%d,ListColorHex(\"", sce->r.cfra);
    if(use_child) {
        for(p = 0; p < child_total; ++p) {
            for(s = 0; s < interp_points_count; ++s) {
                WRITE_PYOBJECT_HEX_VECTOR(outputFile, color);
            }
        }
    }
    else {
        for(p = 0; p < psys->totpart; ++p) {
            for(s = 0; s < interp_points_count; ++s) {
                WRITE_PYOBJECT_HEX_VECTOR(outputFile, color);
            }
        }
    }
    WRITE_PYOBJECT(outputFile, "\")));");

    WRITE_PYOBJECT(outputFile, "\n\topacity=1.0;");
    if(psys->part->flag & PART_HAIR_BSPLINE) {
        WRITE_PYOBJECT(outputFile, "\n\tgeom_splines=1;");
        WRITE_PYOBJECT(outputFile, "\n\tgeom_tesselation_mult=1.0;");
    }
    WRITE_PYOBJECT(outputFile, "\n}\n\n");

    // Restore "Display percentage" setting
    pset->disp      = display_percentage;
    pset->child_nbr = display_percentage_child;

    if(!free_edit) {
        psys->recalc |= PSYS_RECALC;
    }
    if(use_child) {
        psys->recalc |= PSYS_RECALC_CHILD;
    }

    // Recalc hair back with viewport settings
    if(need_recalc) {
        ob->recalc |= OB_RECALC_ALL;
        BKE_scene_update_tagged(&eval_ctx, main, sce);
    }
#endif
}
