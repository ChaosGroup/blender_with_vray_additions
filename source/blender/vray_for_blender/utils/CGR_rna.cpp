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
#include "CGR_rna.h"

#include "DNA_ID.h"
#include "BLI_path_util.h"
#include "BKE_global.h"
#include "BKE_main.h"


std::string RNA_path_get(PointerRNA *ptr, const std::string &attrName)
{
	char filepath[FILE_MAX] = "";

	RNA_string_get(ptr, attrName.c_str(), filepath);
	BLI_path_abs(filepath, ID_BLEND_PATH(G.main, ((ID*)ptr->id.data)));

	return filepath;
}


std::string RNA_std_string_get(PointerRNA *ptr, const std::string &attrName)
{
	char buf[512] = "";

	RNA_string_get(ptr, attrName.c_str(), buf);

	return buf;
}
