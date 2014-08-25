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

#ifndef CGR_PATHS_H
#define CGR_PATHS_H

#include "DNA_ID.h"

#include <string>

#define ID_BLEND_PATH_EX(b_id) (b_id ? (ID_BLEND_PATH(G.main, b_id)) : G.main->name)


namespace BlenderUtils {

std::string GetFullFilepath(const std::string &filepath, ID *holder=NULL);
std::string CopyDRAsset(const std::string &filepath);

}

#endif // CGR_PATHS_H
