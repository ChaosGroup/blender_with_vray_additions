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

#include "CGR_blender_data.h"
#include "CGR_vrscene.h"
#include "CGR_string.h"

#include "DNA_customdata_types.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_mesh.h"

#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_path_util.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "BKE_mball.h"
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_anim.h"
}

#include <string.h>


// Taken from: source/blender/makesrna/intern/rna_main_api.c
// with a slight modifications
//
Mesh* GetRenderMesh(Scene *sce, Main *bmain, Object *ob)
{
	Mesh        *tmpmesh = NULL;
	Curve       *tmpcu = NULL;
	Curve       *copycu = NULL;
	Object      *tmpobj = NULL;
	Object      *basis_ob = NULL;
	DerivedMesh *dm = NULL;
	ListBase     disp = {NULL, NULL};

	CustomDataMask    mask = CD_MASK_MESH;
	EvaluationContext eval_ctx = {0};

	eval_ctx.mode = DAG_EVAL_RENDER;

	switch(ob->type) {
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
			if(tmpobj->type != OB_MESH) {
				BKE_libblock_free_us(bmain, tmpobj );
				return NULL;
			}
			tmpmesh = (Mesh*)tmpobj->data;
			BKE_libblock_free_us(bmain, tmpobj );

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
	tmpmesh->id.us = 0;

	return tmpmesh;
}


void FreeRenderMesh(Main *main, Mesh *mesh)
{
	BKE_libblock_free(main, mesh);
}


void FreeDupliList(Object *ob)
{
	if(ob->duplilist) {
		free_object_duplilist(ob->duplilist);
		ob->duplilist = NULL;
	}
}


// We are checking only 'transform' and visibility here
//
int IsObjectHasActions(Object *ob)
{
	if(ob->adt)
		return (ob->adt->action != NULL);
	else if(ob->parent)
		return IsObjectHasActions(ob->parent);
	return 0;
}


int IsMeshAnimated(Object *ob)
{
	ModifierData *mod = NULL;

	switch(ob->type) {
		case OB_CURVE:
		case OB_SURF:
		case OB_FONT: {
			Curve *cu = (Curve*)ob->data;
			if(cu->adt)
				return cu->adt->action != NULL;
		}
			break;
		case OB_MBALL: {
			MetaBall *mb = (MetaBall*)ob->data;
			if(mb->adt)
				return mb->adt->action != NULL;
		}
			break;
		case OB_MESH: {
			Mesh *me = (Mesh*)ob->data;
			if(me->adt)
				return me->adt->action != NULL;
		}
			break;
		default:
			break;
	}

	mod = (ModifierData*)ob->modifiers.first;
	while(mod) {
		switch(mod->type) {
			case eModifierType_Hook: {
				HookModifierData *hMod = (HookModifierData*)mod;
				if(hMod->object)
					if(hMod->object->adt)
						return hMod->object->adt->action != NULL;
			}
			case eModifierType_Armature:
			case eModifierType_Array:
			case eModifierType_Displace:
			case eModifierType_Softbody:
			case eModifierType_Explode:
			case eModifierType_MeshDeform:
			case eModifierType_ParticleSystem:
			case eModifierType_SimpleDeform:
			case eModifierType_ShapeKey:
			case eModifierType_Screw:
			case eModifierType_Warp:
				if(ob->adt)
					return ob->adt->action != NULL;
			default:
				mod = mod->next;
		}
	}

	return 0;
}


std::string GetIDName(ID *id, const std::string prefix)
{
	char baseName[MAX_ID_NAME];
	BLI_strncpy(baseName, id->name, MAX_ID_NAME);
	StripString(baseName);

	std::string idName = prefix + baseName;

	if(id->lib) {
		char libFilename[FILE_MAX] = "";

		BLI_split_file_part(id->lib->name+2, libFilename, FILE_MAX);
		BLI_replace_extension(libFilename, FILE_MAX, "");

		StripString(libFilename);

		idName.append("LI");
		idName.append(libFilename);
	}

	return idName;
}


int IsMeshValid(Scene *sce, Main *main, Object *ob)
{
	switch(ob->type) {
		case OB_FONT: {
			Curve *cu = (Curve*)ob->data;
			if(cu->str == NULL)
				return 0;
		}
			break;
		case OB_SURF:
		case OB_CURVE: {
			Mesh *mesh = GetRenderMesh(sce, main, ob);
			if(NOT(mesh))
				return 0;
			if(NOT(mesh->totface)) {
				FreeRenderMesh(main, mesh);
				return 0;
			}
		}
			break;
		case OB_MBALL:
			break;
		case OB_MESH:
			break;
		default:
			break;
	}

	return 1;
}


int IsParentUpdated(Object *ob)
{
	if(ob->id.pad2)
		return 1;
	if(ob->parent)
		return IsParentUpdated(ob->parent);
	return 0;
}


int IsObjectDataUpdated(Object *ob)
{
	return ob->id.pad2 & CGR_UPDATED_DATA;
}
