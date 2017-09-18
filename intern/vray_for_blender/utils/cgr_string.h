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

#ifndef CGR_STRING_H
#define CGR_STRING_H

#include <string>

/// Replace all invalid chars in @str
/// @param str - the in/out param
void         StripString(char *str);

/// Replace all invalid chars in @str, but ignores any found in @ignore
/// @param str - the in/out param
/// @param ignore - null terminated string containing all characters to skip replacing
/// NOTE: instead of passing nullptr or empty string - call the overload
void         StripString(char *str, const char * ignore);


/// Replace all invalid chars in @str and return copy
/// @param str - the in/out param
std::string  StripString(const std::string &str);

/// Replace all invalid chars in @str and return copy, but ignores any found in @ignore
/// @param str - the in/out param
/// @param ignore - null terminated string containing all characters to skip replacing
/// NOTE: instead of passing nullptr or empty string - call the overload
std::string  StripString(const std::string &str, const char * ignore);

bool         IsStdStringDigit(const std::string &str);

#endif // CGR_STRING_H
