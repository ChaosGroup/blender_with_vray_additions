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


using namespace VRayScene;


// Taken from "source/blender/render/intern/source/convertblender.c"
// and slightly modified
//

typedef struct ParticleStrandData {
	float *uvco;
	int    totuv;
} ParticleStrandData;


BLI_INLINE void get_particle_uvco_mcol(short from, DerivedMesh *dm, float *fuv, int num, ParticleStrandData *sd)
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

	use_width_fade = 0;

	hair_vertices     = NULL;
	num_hair_vertices = NULL;
	widths            = NULL;
	transparency      = NULL;
	strand_uvw        = NULL;

	use_global_hair_tree = 1;

	geom_splines = 0;
	geom_tesselation_mult = 1.0;
}


void GeomMayaHair::freeData()
{
	if(num_hair_vertices) {
		delete [] num_hair_vertices;
		num_hair_vertices = NULL;
	}
	if(hair_vertices) {
		delete [] hair_vertices;
		hair_vertices = NULL;
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


void GeomMayaHair::buildHash()
{
}


Material *GeomMayaHair::getHairMaterial() const
{
}


void GeomMayaHair::init(Scene *sce, Main *main, Object *ob, ParticleSystem *psys)
{
	m_sce  = sce;
	m_ob   = ob;
	m_main = main;
	m_psys = psys;

	initData();
	initAttributes();

	buildHash();
}


void GeomMayaHair::initData()
{
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
	float     hair_width = 0.001f;

	int       use_child    = 0;
	int       need_recalc  = 0;
	int       is_free_edit = 0;

	PointerRNA  rna_pset;
	PointerRNA  VRayParticleSettings;
	PointerRNA  VRayFur;

	int  display_percentage;
	int  display_percentage_child;

	EvaluationContext eval_ctx;
	eval_ctx.for_render = true;

	// For standart macroses to work
	int p;
	int s;
	ParticleSystem *psys = m_psys;

	pset = psys->part;

	if(pset->type != PART_HAIR)
		return;

	if(psys->part->ren_as != PART_DRAW_PATH)
		return;

	psmd = psys_get_modifier(m_ob, psys);

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
			hair_width = RNA_float_get(&VRayFur, "width");
		}
	}

	child_cache = psys->childcache;
	child_total = psys->totchildcache;
	use_child   = (pset->childtype && child_cache);

	// Store "Display percentage" setting
	display_percentage       = pset->disp;
	display_percentage_child = pset->child_nbr;

	// Recalc parent hair only if they are not
	// manually edited
	is_free_edit = psys_check_edited(psys);
	if(NOT(is_free_edit)) {
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
		m_ob->recalc |= OB_RECALC_ALL;
		BKE_scene_update_tagged(&eval_ctx, m_main, m_sce);
	}

	// Get new child data pointers
	if(use_child) {
		child_cache = psys->childcache;
		child_total = psys->totchildcache;
	}

	// Store the number or vertices per hair
	//
	int     vertices_total_count = 0;

	int    *num_hair_vertices_arr = NULL;
	size_t  num_hair_vertices_arrSize = 0;

	if(use_child) {
		num_hair_vertices_arrSize = child_total;
		num_hair_vertices_arr = new int[child_total];
		for(p = 0; p < child_total; ++p) {
			num_hair_vertices_arr[p] = child_cache[p]->steps;
			vertices_total_count += child_cache[p]->steps;
		}
	}
	else {
		num_hair_vertices_arrSize = psys->totpart;
		num_hair_vertices_arr = new int[psys->totpart];
		LOOP_PARTICLES {
			num_hair_vertices_arr[p] = pa->totkey;
			vertices_total_count += pa->totkey;
		}
	}

	num_hair_vertices = GetStringZip((u_int8_t*)num_hair_vertices_arr, num_hair_vertices_arrSize * sizeof(int));

	delete [] num_hair_vertices_arr;

	// Store hair vertices
	//
	int    hair_vert_index    = 0;
	int    hair_vert_co_index = 0;

	float *hair_vertices_arr  = new float[3 * vertices_total_count];
	float *widths_arr         = new float[vertices_total_count];

	if(use_child) {
		for(p = 0; p < child_total; ++p) {
			child_key   = child_cache[p];
			child_steps = child_key->steps;

			for(s = 0; s < child_steps; ++s, ++child_key) {
				// Child particles are stored in world space,
				// but we need them in object space
				copy_v3_v3(child_key_co, child_key->co);

				// Remove transform by applying inverse matrix
				mul_m4_v3(m_ob->imat, child_key_co);

				// Store coordinates
				COPY_VECTOR(hair_vertices_arr, hair_vert_co_index, child_key_co);

				widths_arr[hair_vert_index++] = hair_width;
			}
		}
	}
	else {
		LOOP_PARTICLES {
			psys_mat_hair_to_object(NULL, psmd->dm, psmd->psys->part->from, pa, hairmat);

			for(s = 0, hkey = pa->hair; s < pa->totkey; ++s, ++hkey) {
				copy_v3_v3(segment, hkey->co);
				mul_m4_v3(hairmat, segment);

				// Store coordinates
				COPY_VECTOR(hair_vertices_arr, hair_vert_co_index, segment);

				widths_arr[hair_vert_index++] = hair_width;
			}
		}
	}

	hair_vertices = GetStringZip((u_int8_t*)hair_vertices_arr, hair_vert_co_index   * sizeof(float));
	widths        = GetStringZip((u_int8_t*)widths_arr,        vertices_total_count * sizeof(float));

	delete [] hair_vertices_arr;
	delete [] widths_arr;

	// Store UV per hair
	//
	memset(&sd, 0, sizeof(ParticleStrandData));

	if(psmd->dm) {
		sd.totuv = CustomData_number_of_layers(&psmd->dm->faceData, CD_MTFACE);
		sd.uvco  = NULL;

		if(sd.totuv) {
			sd.uvco = (float*)MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");
			if(sd.uvco) {
				int    uv_vert_co_index = 0;
				float *uv_vertices_arr  = new float[3 * num_hair_vertices_arrSize];

				if(use_child) {
					for(p = 0; p < child_total; ++p) {
						cpa = psys->child + p;

						/* get uvco & mcol */
						if(pset->childtype==PART_CHILD_FACES) {
							get_particle_uvco_mcol(PART_FROM_FACE, psmd->dm, cpa->fuv, cpa->num, &sd);
						}
						else {
							ParticleData *parent = psys->particles + cpa->parent;
							int num = parent->num_dmcache;

							if (num == DMCACHE_NOTFOUND)
								if (parent->num < psmd->dm->getNumTessFaces(psmd->dm))
									num = parent->num;

							get_particle_uvco_mcol(pset->from, psmd->dm, parent->fuv, num, &sd);
						}

						segment[0] = sd.uvco[0];
						segment[1] = sd.uvco[1];
						segment[2] = 0.0f;

						// Store coordinates
						COPY_VECTOR(uv_vertices_arr, uv_vert_co_index, segment);
					}
				} // use_child
				else {
					LOOP_PARTICLES {
						/* get uvco & mcol */
						int num = pa->num_dmcache;
						if(num == DMCACHE_NOTFOUND)
							if(pa->num < psmd->dm->getNumTessFaces(psmd->dm))
								num = pa->num;

						get_particle_uvco_mcol(pset->from, psmd->dm, pa->fuv, num, &sd);

						segment[0] = sd.uvco[0];
						segment[1] = sd.uvco[1];
						segment[2] = 0.0f;

						// Store coordinates
						COPY_VECTOR(uv_vertices_arr, uv_vert_co_index, segment);
					}
				}

				MEM_freeN(sd.uvco);

				strand_uvw = GetStringZip((u_int8_t*)uv_vertices_arr, uv_vert_co_index * sizeof(float));

				delete [] uv_vertices_arr;
			}
		}
	}

	// Restore "Display percentage" setting
	pset->disp      = display_percentage;
	pset->child_nbr = display_percentage_child;

	if(NOT(is_free_edit))
		psys->recalc |= PSYS_RECALC;

	if(use_child)
		psys->recalc |= PSYS_RECALC_CHILD;

	// Recalc hair back with viewport settings
	if(need_recalc) {
		m_ob->recalc |= OB_RECALC_ALL;
		BKE_scene_update_tagged(&eval_ctx, m_main, m_sce);
	}
}


void GeomMayaHair::initAttributes()
{
	opacity = 1.0;

	geom_splines = (m_psys->part->flag & PART_HAIR_BSPLINE);
}
