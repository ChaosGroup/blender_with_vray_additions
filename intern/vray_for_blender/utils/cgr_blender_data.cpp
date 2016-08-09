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

#include "cgr_config.h"

#include "cgr_blender_data.h"
#include "cgr_vrscene.h"
#include "cgr_string.h"

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

#include "RNA_access.h"

#include <string.h>


bool CouldInstance(BL::Scene scene, BL::Object ob)
{
	bool could_instance = true;

	if (ob.type() == BL::Object::type_META) {
		could_instance = false;
	}
	else if (ob.is_modified(scene, 2)) {
		could_instance = false;
	}

	return could_instance;
}


std::string GetIDName(ID *id)
{
	std::string idName(id->name);

	if(id->lib) {
		char libFilename[FILE_MAX] = "";

		BLI_split_file_part(id->lib->name+2, libFilename, FILE_MAX);
		BLI_replace_extension(libFilename, FILE_MAX, "");

		idName.append("LI");
		idName.append(libFilename);
	}

	return StripString(idName);
}


std::string GetIDName(BL::Pointer ptr)
{
	return GetIDName((ID*)ptr.ptr.data);
}


static bool is_mesh_valid(Scene *sce, Main *main, Object *ob, bool check_length=true)
{
	PointerRNA scenePtr;
	PointerRNA dataPtr;
	PointerRNA objectPtr;

	RNA_id_pointer_create((ID*)sce,  &scenePtr);
	RNA_id_pointer_create((ID*)main, &dataPtr);
	RNA_id_pointer_create((ID*)ob,   &objectPtr);

	BL::Scene     scene = BL::Scene(scenePtr);
	BL::BlendData data  = BL::BlendData(dataPtr);
	BL::Object    obj   = BL::Object(objectPtr);

	bool valid = !!(obj.data());
	if (valid && check_length) {
		BL::Mesh mesh = data.meshes.new_from_object(scene, obj, true, 1, false, false);
		valid = mesh && mesh.polygons.length();
		if (mesh) {
			data.meshes.remove(mesh, 1);
		}
	}

	return valid;
}


int IsMeshValid(Scene *sce, Main *main, Object *ob)
{
	bool valid = true;

	switch(ob->type) {
		case OB_FONT: {
			Curve *cu = (Curve*)ob->data;
			if(cu->str == NULL)
				valid = false;
		}
			break;
		case OB_MBALL:
			valid = is_mesh_valid(sce, main, ob);
			break;
		case OB_SURF:
		case OB_CURVE:
			valid = is_mesh_valid(sce, main, ob, false);
			break;
		case OB_MESH:
			break;
		default:
			break;
	}

	return valid;
}


int IsObjectUpdated(Object *ob)
{
	PointerRNA ptr;
	RNA_pointer_create((ID*)ob, &RNA_Object, ob, &ptr);

	PointerRNA vrayObject = RNA_pointer_get(&ptr, "vray");

	int upObject = RNA_int_get(&vrayObject, "data_updated") & CGR_UPDATED_OBJECT;
	int upData   = 0;

	BL::Object bl_ob(ptr);
	if(bl_ob.is_duplicator()) {
		if(bl_ob.particle_systems.length()) {
			upData = RNA_int_get(&vrayObject, "data_updated") & CGR_UPDATED_DATA;
		}
	}

	if (!(upObject || upData) && bl_ob.parent()) {
		return IsObjectUpdated((Object*)bl_ob.parent().ptr.data);
	}

	return upObject || upData;
}


int IsObjectDataUpdated(Object *ob)
{
	PointerRNA ptr;
	RNA_pointer_create((ID*)ob, &RNA_Object, ob, &ptr);
	ptr = RNA_pointer_get(&ptr, "vray");
	return RNA_int_get(&ptr, "data_updated") & CGR_UPDATED_DATA;
}
