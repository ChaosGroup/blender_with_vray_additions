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
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * * ***** END GPL LICENSE BLOCK *****
 */

#include "CGR_config.h"

#include "exp_nodes.h"

#include "GeomMayaHair.h"
#include "Node.h"

#include "CGR_blender_data.h"
#include "CGR_vrscene.h"
#include "CGR_string.h"

#include "BKE_material.h"
#include "BKE_depsgraph.h"
#include "BKE_scene.h"
#include "BKE_object.h"
#include "MEM_guardedalloc.h"
#include "RNA_access.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_math.h"

extern "C" {
#  include "DNA_material_types.h"
#  include "DNA_modifier_types.h"
#  include "BKE_DerivedMesh.h"
#  include "BKE_particle.h"
}

#include <cmath>


using namespace VRayScene;


#define USE_MANUAL_RECALC  0


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


GeomMayaHair::GeomMayaHair(Scene *scene, Main *main, Object *ob):
	VRayExportable(scene, main, ob)
{
	m_hash = 1;

	hair_vertices     = NULL;
	num_hair_vertices = NULL;
	widths            = NULL;
	transparency      = NULL;
	strand_uvw        = NULL;

	use_global_hair_tree = 1;
	geom_splines = 0;
	geom_tesselation_mult = 1.0;
	widths_in_pixels = false;

	m_hashHairVertices = 1;
	m_hashNumHairVertices = 1;
	m_hashWidths = 1;
	m_hashTransparency = 1;
	m_hashStrandUVW = 1;

	m_lightLinker = NULL;
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
	if(strand_uvw) {
		delete [] strand_uvw;
		strand_uvw = NULL;
	}
	if(transparency) {
		delete [] transparency;
		transparency = NULL;
	}
}


void GeomMayaHair::init()
{
	initData();
	initHash();
}


void GeomMayaHair::setLightLinker(LightLinker *lightLinker)
{
	m_lightLinker = lightLinker;
}


void GeomMayaHair::preInit(ParticleSystem *psys)
{
	m_psys = psys;

	GetTransformHex(m_ob->obmat, m_nodeTm);

	initAttributes();
	initName();
}


void GeomMayaHair::initData()
{
	ParticleSettings           *pset = NULL;
	ParticleSystemModifierData *psmd = NULL;

	ParticleData       *pa   = NULL;
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

	int       use_child   = 0;
	int       need_recalc = 1;

	PointerRNA  rna_pset;
	PointerRNA  VRayParticleSettings;
	PointerRNA  VRayFur;

#if USE_MANUAL_RECALC
	int  display_percentage;
	int  display_percentage_child;

	EvaluationContext eval_ctx;
	eval_ctx.for_render = true;
#endif

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

	// Experimental option to make hair thinner to the end
	int use_width_fade = false;

	if(RNA_struct_find_property(&rna_pset, "vray")) {
		VRayParticleSettings = RNA_pointer_get(&rna_pset, "vray");

		if(RNA_struct_find_property(&VRayParticleSettings, "VRayFur")) {
			VRayFur = RNA_pointer_get(&VRayParticleSettings, "VRayFur");

			hair_width = RNA_float_get(&VRayFur, "width");
			use_width_fade = RNA_boolean_get(&VRayFur, "make_thinner");
			widths_in_pixels = RNA_boolean_get(&VRayFur, "widths_in_pixels");
		}
	}

	PointerRNA psmdRNA;
	PointerRNA obRNA;
	PointerRNA sceRNA;

	RNA_id_pointer_create((ID*)psmd,  &psmdRNA);
	RNA_id_pointer_create((ID*)m_sce, &sceRNA);
	RNA_id_pointer_create((ID*)m_ob,  &obRNA);

	BL::ParticleSystemModifier b_psmd(psmdRNA);
	BL::Scene                  b_sce(sceRNA);
	BL::Object                 b_ob(obRNA);

	BL::ParticleSystem   b_psys(b_psmd.particle_system().ptr);
	BL::ParticleSettings b_part(b_psys.settings().ptr);

	b_psys.set_resolution(b_sce, b_ob, 2);

	child_cache = psys->childcache;
	child_total = psys->totchildcache;
	use_child   = (pset->childtype && child_cache);

	// Recalc parent hair only if they are not
	// manually edited
	if(psys_check_edited(psys))
		need_recalc = 0;

	if(psys->flag & PSYS_HAIR_DYNAMICS)
		need_recalc = 0;

	if(need_recalc) {
#if USE_MANUAL_RECALC
		// Store "Display percentage" setting
		display_percentage       = pset->disp;
		display_percentage_child = pset->child_nbr;

		// Set render settings
		pset->disp = 100;
		if(use_child)
			pset->child_nbr = pset->ren_child_nbr;

		// Recalc hair with render settings
		psys->recalc |= PSYS_RECALC;
		m_ob->recalc |= OB_RECALC_ALL;

		BKE_object_handle_update_ex(&eval_ctx, m_sce, m_ob, m_sce->rigidbody_world, false);

		// Get new child data pointers
		if(use_child) {
			child_cache = psys->childcache;
			child_total = psys->totchildcache;
		}
#endif
	}

	int draw_step = b_part.render_step();
	int ren_step = (int)powf(2.0f, (float)draw_step);

	int totparts = b_psys.particles.length();

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
		num_hair_vertices_arrSize = totparts;
		num_hair_vertices_arr = new int[totparts];

		for(int pa_no = 0; pa_no < totparts; pa_no++) {
			num_hair_vertices_arr[pa_no]  = ren_step+1;
			vertices_total_count         += ren_step+1;
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

			float hair_fade_width = hair_width;
			float hair_fade_step  = hair_width / (child_steps+1);

			for(s = 0; s < child_steps; ++s, ++child_key) {
				// Child particles are stored in world space,
				// but we need them in object space
				copy_v3_v3(child_key_co, child_key->co);

				// Remove transform by applying inverse matrix
				float obImat[4][4];
				copy_m4_m4(obImat, m_ob->obmat);
				invert_m4(obImat);
				mul_m4_v3(obImat, child_key_co);

				// Store coordinates
				COPY_VECTOR(hair_vertices_arr, hair_vert_co_index, child_key_co);

				widths_arr[hair_vert_index++] = use_width_fade ? std::max(1e-6f, hair_fade_width) : hair_width;

				hair_fade_width -= hair_fade_step;
			}
		}
	}
	else {
		float nco[3];
		invert_m4_m4(hairmat, m_ob->obmat);
		for(int pa_no = 0; pa_no < totparts; pa_no++) {
			float hair_fade_width = hair_width;
			float hair_fade_step  = hair_width / (ren_step+1);

			for(int step_no = 0; step_no <= ren_step; step_no++) {
				b_psys.co_hair(b_ob, pa_no, step_no, nco);
				mul_m4_v3(hairmat, nco);

				COPY_VECTOR(hair_vertices_arr, hair_vert_co_index, nco);

				widths_arr[hair_vert_index++] = use_width_fade ? std::max(1e-6f, hair_fade_width) : hair_width;

				hair_fade_width -= hair_fade_step;
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

	if(need_recalc) {
#if USE_MANUAL_RECALC
		// Restore "Display percentage" setting
		pset->disp      = display_percentage;
		pset->child_nbr = display_percentage_child;

		// Recalc hair back with viewport settings
		psys->recalc |= PSYS_RECALC;
		m_ob->recalc |= OB_RECALC_ALL;
		BKE_object_handle_update_ex(&eval_ctx, m_sce, m_ob, m_sce->rigidbody_world, false);
#endif
	}

	psys_render_restore(m_ob, m_psys);
}


void GeomMayaHair::initAttributes()
{
	opacity      = 1.0;
	geom_splines = (m_psys->part->flag & PART_HAIR_BSPLINE);
}


void GeomMayaHair::initName(const std::string &name)
{
	if(NOT(name.empty())) {
		m_name = name;
	}
	else {
		char nameBuf[MAX_ID_NAME] = "";

		m_name.clear();
		m_name.append("HAIR");

		BLI_strncpy(nameBuf, m_psys->name, MAX_ID_NAME);
		StripString(nameBuf);
		m_name.append(nameBuf);

		BLI_strncpy(nameBuf, m_psys->part->id.name, MAX_ID_NAME);
		StripString(nameBuf);
		m_name.append(nameBuf);
	}

	m_nodeName = "Node" + m_name;
}


void GeomMayaHair::initHash()
{
	if(NOT(hair_vertices))
		return;

	m_hash = 1;

	// If not animation don't waste time calculating hashes
	if(ExporterSettings::gSet.m_isAnimation) {
		if(hair_vertices)
			m_hashHairVertices    = HashCode(hair_vertices);
		if(num_hair_vertices)
			m_hashNumHairVertices = HashCode(num_hair_vertices);
		if(widths)
			m_hashWidths          = HashCode(widths);
		if(strand_uvw)
			m_hashStrandUVW       = HashCode(strand_uvw);
		if(transparency)
			m_hashTransparency    = HashCode(transparency);

		m_hash = m_hashHairVertices    ^
				 m_hashNumHairVertices ^
				 m_hashWidths          ^
				 m_hashStrandUVW       ^
				 m_hashTransparency;
	}
}


void GeomMayaHair::writeNode(PyObject *output, int frame, const NodeAttrs &attrs)
{
	// Have to manually setup frame here
	// because this is not called from write().
	initInterpolate(frame); // XXX: Get rig of this for nodes?

	PointerRNA vrayObject = RNA_pointer_get(&m_bl_ob.ptr, "vray");

	int          visible  = 1;
	int          objectID = m_ob->index;
	std::string  material = getHairMaterialName();

	material = Node::WriteMtlWrapper(&vrayObject, NULL, m_nodeName, material);
	material = Node::WriteMtlRenderStats(&vrayObject, NULL, m_nodeName, material);

	if(attrs.override) {
		std::string overrideBaseName = m_nodeName + "@" + GetIDName((ID*)attrs.dupliHolder.ptr.data);

		visible  = attrs.visible;
		objectID = attrs.objectID;

		material = Node::WriteMtlWrapper(&vrayObject, NULL, overrideBaseName, material);
		material = Node::WriteMtlRenderStats(&vrayObject, NULL, overrideBaseName, material);
	}

	AttributeValueMap pluginAttrs;
	pluginAttrs["material"]  = material;
	pluginAttrs["geometry"]  = m_name;
	pluginAttrs["objectID"]  = BOOST_FORMAT_INT(objectID);
	pluginAttrs["visible"]   = BOOST_FORMAT_INT(visible);
	pluginAttrs["transform"] = BOOST_FORMAT_TM(m_nodeTm);

	if(m_lightLinker) {
		const std::string &baseObjectName = GetIDName(&m_ob->id);

		m_scene_nodes->insert(m_nodeName);
		m_lightLinker->excludePlugin(baseObjectName, m_nodeName);
	}

	VRayNodePluginExporter::exportPlugin("NODE", "Node", m_nodeName, pluginAttrs);
}


void GeomMayaHair::writeData(PyObject *output, VRayExportable *prevState, bool keyFrame)
{
	GeomMayaHair *prevHair  = (GeomMayaHair*)prevState;
	int           prevFrame = ExporterSettings::gSet.m_frameCurrent - ExporterSettings::gSet.m_frameStep;

	PYTHON_PRINTF(output, "\nGeomMayaHair %s {", m_name.c_str());

	PYTHON_PRINT_DATA(output, "num_hair_vertices", "ListIntHex",
					  num_hair_vertices, m_hashNumHairVertices,
					  prevHair,
					  prevHair->getNumHairVertices(), prevHair->getNumHairVerticesHash());

	PYTHON_PRINT_DATA(output, "hair_vertices", "ListVectorHex",
					  hair_vertices, m_hashHairVertices,
					  prevHair,
					  prevHair->getHairVertices(), prevHair->getHairVerticesHash());

	PYTHON_PRINT_DATA(output, "widths", "ListFloatHex",
					  widths, m_hashWidths,
					  prevHair,
					  prevHair->getWidths(), prevHair->getWidthsHash());

	if(strand_uvw) {
		PYTHON_PRINT_DATA(output, "strand_uvw", "ListVectorHex",
						  strand_uvw, m_hashStrandUVW,
						  prevHair,
						  prevHair->getStrandUVW(), prevHair->getStrandUvwHash());
	}

	if(NOT(prevHair)) {
		PYTHON_PRINTF(output, "\n\topacity=%.3f;", opacity);
		PYTHON_PRINTF(output, "\n\tgeom_splines=%i;", geom_splines);
		PYTHON_PRINTF(output, "\n\tgeom_tesselation_mult=%.3f;",  geom_tesselation_mult);
		PYTHON_PRINTF(output, "\n\tuse_global_hair_tree=%i;",  use_global_hair_tree);
		PYTHON_PRINTF(output, "\n\twidths_in_pixels=%i;",  widths_in_pixels);
	}

	PYTHON_PRINT(output, "\n}\n");
}


std::string GeomMayaHair::getHairMaterialName() const
{
	Material *ma = give_current_material(m_ob, m_psys->part->omat);
	if(NOT(ma))
		return "MANOMATERIALISSET";
	return Node::GetMaterialName(ma, ExporterSettings::gSet.m_mtlOverride);
}
