/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vfb_node_exporter.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_math.h"

extern "C" {
#include "DNA_modifier_types.h"
#include "BKE_DerivedMesh.h"
#include "BKE_particle.h"
}


// Taken from "source/blender/render/intern/source/convertblender.c" and modified
//
BLI_INLINE void GetParticleUV(short from, DerivedMesh *dm, float *fuv, int layer, int num, float *uv)
{
	if (ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		if (num != DMCACHE_NOTFOUND) {
			MFace  *mface  = (MFace*)dm->getTessFaceData(dm, num, CD_MFACE);
			MTFace *mtface = (MTFace*)CustomData_get_layer_n(&dm->faceData, CD_MTFACE, layer);
			mtface += num;

			psys_interpolate_uvs(mtface, mface->v4, fuv, uv);
			uv[2] = 0.0f; // Just in case
		}
		else {
			uv[0] = 0.0f;
			uv[1] = 0.0f;
			uv[2] = 0.0f;
		}
	}
}


AttrValue DataExporter::exportGeomMayaHair(BL::Object ob, BL::ParticleSystem psys, BL::ParticleSystemModifier psm)
{
	AttrValue hair;

	const int is_preview = (m_evalMode == EvalModePreview);

	BL::ParticleSettings pset(psys.settings());

	const int draw_step = is_preview
	                      ? pset.draw_step()
	                      : pset.render_step();

	const int totparts = psys.particles.length();
	const int totchild = is_preview
	                     ? (float)psys.child_particles.length() * (float)pset.draw_percentage() / 100.0f
	                     : psys.child_particles.length();

	const bool has_child = pset.child_type() != BL::ParticleSettings::child_type_NONE;

	int totcurves = totchild;
	if (!has_child) {
		totcurves += totparts;
	}

	if (totcurves) {
		float hair_width = 0.001f;
		int   use_width_fade = false;
		int   widths_in_pixels = false;

		// UV layer index
		// NOTE: Make configurable option
		const int layer_idx = 0;

		if (RNA_struct_find_property(&pset.ptr, "vray")) {
			PointerRNA VRayParticleSettings = RNA_pointer_get(&pset.ptr, "vray");

			if (RNA_struct_find_property(&VRayParticleSettings, "VRayFur")) {
				PointerRNA VRayFur = RNA_pointer_get(&VRayParticleSettings, "VRayFur");

				hair_width       = RNA_float_get(&VRayFur, "width");
				use_width_fade   = RNA_boolean_get(&VRayFur, "make_thinner");
				widths_in_pixels = RNA_boolean_get(&VRayFur, "widths_in_pixels");
			}
		}

		int ren_step = (1 << draw_step) + 1;
		if (pset.kink() == BL::ParticleSettings::kink_SPIRAL) {
			ren_step += pset.kink_extra_steps();
		}

		BlTransform itm = Math::InvertTm(ob.matrix_world());
		float hair_itm[4][4];
		memcpy(hair_itm, &itm.data, sizeof(float[4][4]));

		AttrListInt     num_hair_vertices;
		AttrListFloat   widths;
		AttrListVector  hair_vertices;
		AttrListVector  strand_uvw;

		if (has_child) {
			// Export child particles using C API
			//
			ParticleSystem             *ps   = (ParticleSystem*)psys.ptr.data;
			ParticleSettings           *pst  = (ParticleSettings*)pset.ptr.data;
			ParticleSystemModifierData *psmd = (ParticleSystemModifierData*)psm.ptr.data;

			ParticleCacheKey **child_cache = ps->childcache;
			int                child_total = ps->totchildcache;
			int                tot_verts = 0;

			num_hair_vertices.resize(child_total);

			for (int p = 0; p < child_total; ++p) {
				const int seg_verts = child_cache[p]->segments;
				tot_verts += seg_verts;

				(*num_hair_vertices)[p] = seg_verts;
			}

			widths.resize(tot_verts);
			hair_vertices.resize(tot_verts);

			const bool has_uv = psmd->dm_final && CustomData_number_of_layers(&psmd->dm_final->faceData, CD_MTFACE);
			if (has_uv) {
				strand_uvw.resize(tot_verts);
			}

			int hair_vert_index = 0;
			for(int p = 0; p < child_total; ++p) {
				ParticleCacheKey *child_key = child_cache[p];

				const int child_steps = child_key->segments;

				float hair_fade_width = hair_width;
				float hair_fade_step  = hair_width / (child_steps+1);

				for(int s = 0; s < child_steps; ++s, ++child_key) {
					float *co = (float*)&(*hair_vertices)[hair_vert_index];
					copy_v3_v3(co, child_key->co);
					mul_m4_v3(hair_itm, co);

					(*widths)[hair_vert_index] = use_width_fade
					                             ? std::max(1e-6f, hair_fade_width)
					                             : hair_width;

					hair_fade_width -= hair_fade_step;

					hair_vert_index++;
				}

				if (has_uv) {
					float *uv = (float*)&(*strand_uvw)[p];

					ChildParticle *cpa = ps->child + p;
					if(pst->childtype == PART_CHILD_FACES) {
						GetParticleUV(PART_FROM_FACE, psmd->dm_final, cpa->fuv, layer_idx, cpa->num, uv);
					}
					else {
						ParticleData *parent = ps->particles + cpa->parent;

						int num = parent->num_dmcache;
						if (num == DMCACHE_NOTFOUND) {
							if (parent->num < psmd->dm_final->getNumTessFaces(psmd->dm_final)) {
								num = parent->num;
							}
						}

						GetParticleUV(pst->from, psmd->dm_final, parent->fuv, layer_idx, num, uv);
					}
				}
			}
		}
		else {
			// Export particles using C++ RNA API
			//
			num_hair_vertices.resize(totparts);
			widths.resize(totparts * ren_step);
			hair_vertices.resize(totparts * ren_step);

			BL::Mesh mesh(ob.data());
			const bool has_uv = mesh && mesh.tessface_uv_textures.length();
			if (has_uv) {
				strand_uvw.resize(totparts);
			}

			const float hair_fade_step  = hair_width / (ren_step + 1);

			int strand_idx = 0;

			BL::ParticleSystem::particles_iterator pIt;
			psys.particles.begin(pIt);
			for (int pa_no = 0; pa_no < totparts; ++pa_no, ++strand_idx) {
				float hair_fade_width = hair_width;

				for (int step_no = 0; step_no < ren_step; ++step_no) {
					const int strand_vertex_idx = (strand_idx * ren_step) + step_no;

					float *nco = (float*)&(*hair_vertices)[strand_vertex_idx];
					psys.co_hair(ob, pa_no, step_no, nco);
					mul_m4_v3(hair_itm, nco);

					(*widths)[strand_vertex_idx] = use_width_fade
					                               ? std::max(1e-6f, hair_fade_width)
					                               : hair_width;

					hair_fade_width -= hair_fade_step;
				}

				(*num_hair_vertices)[strand_idx] = ren_step;

				if (has_uv) {
					psys.uv_on_emitter(psm, *pIt, pa_no, layer_idx, (float*)&(*strand_uvw)[strand_idx]);
					(*strand_uvw)[strand_idx].z = 0.0f; // Just in case
				}

				if (pIt != psys.particles.end()) {
					++pIt;
				}
			}
		}

		PluginDesc hairDesc(getHairName(ob, psys, pset), "GeomMayaHair");
		hairDesc.add("num_hair_vertices", num_hair_vertices);
		hairDesc.add("hair_vertices", hair_vertices);
		hairDesc.add("widths", widths);
		if (strand_uvw.getCount()) {
			hairDesc.add("strand_uvw", strand_uvw);
		}
		hairDesc.add("widths_in_pixels", widths_in_pixels);
		hairDesc.add("geom_splines", pset.use_hair_bspline());

		hair = m_exporter->export_plugin(hairDesc);
	}

	return hair;
}
