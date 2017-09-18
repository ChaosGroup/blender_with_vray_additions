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
#include "cgr_string.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>


struct ReplaceTable
{
	enum {
		size = 256,
	};
	char data[size];


	ReplaceTable() {
		for (int c = 0; c < ReplaceTable::size; c++) {
			data[c] = '_';
		}
		// lower case
		for (int c = 'a'; c <= 'z'; c++) {
			data[c] = c;
		}
		// uppser case
		for (int c = 'A'; c <= 'Z'; c++) {
			data[c] = c;
		}
		// digits
		for (int c = '0'; c <= '9'; c++) {
			data[c] = c;
		}
		// specials
		data['@'] = '@';
		data['|'] = '|';
		data['+'] = 'p';
		data['-'] = 'm';
	}

	void update(const char * ignore) {
		while (*ignore) {
			data[*ignore] = *ignore;
			ignore++;
		}
	}
};

static const ReplaceTable default_replace{};

namespace
{

/// Copy string, but no more than @max_size
/// @param dest - the memory destination
/// @param source - the source string - must be null terminated
/// @param max_size - the max number of characters copied (the size of @dest)
/// @return remaining size, or -1 if max_size was exceeded
int safe_str_ncpy_replace(char * __restrict dest, const char * __restrict source, int max_size, const ReplaceTable * table) {
	while (*source && max_size) {
		*dest++ = table->data[*source++];
		--max_size;
	}

	if (max_size) {
		*dest++ = 0;
	} else {
		*(dest - 1) = 0;
	}
	--max_size;

	return max_size;
}

/// Replaces characters in @string
/// @string - the in/out string
/// @table - pointer to the replace table
void replace_in_place(char * string, const ReplaceTable * table)
{
	while (*string) {
		*string = table->data[*string];
		string++;
	}
}
} // namespace


void StripString(char *str, const char * ignore)
{
	ReplaceTable replace_table = default_replace;
	replace_table.update(ignore);
	replace_in_place(str, &replace_table);
}

void StripString(char *str)
{
	replace_in_place(str, &default_replace);
}

std::string StripString(const std::string &str)
{
	static char buf[CGR_MAX_PLUGIN_NAME];
	if (safe_str_ncpy_replace(buf, str.c_str(), CGR_MAX_PLUGIN_NAME, &default_replace) < 0) {
		PRINT_ERROR("Trying to write string longer than [%d]: \"%.200s...\"", CGR_MAX_PLUGIN_NAME, str.c_str());
	}
	return buf;
}

std::string StripString(const std::string &str, const char * ignore)
{
	ReplaceTable replace_table = default_replace;
	replace_table.update(ignore);

	static char buf[CGR_MAX_PLUGIN_NAME];
	if (safe_str_ncpy_replace(buf, str.c_str(), CGR_MAX_PLUGIN_NAME, &replace_table) < 0) {
		PRINT_ERROR("Trying to write string longer than [%d]: \"%.200s...\"", CGR_MAX_PLUGIN_NAME, str.c_str());
	}
	return buf;
}


bool IsStdStringDigit(const std::string &str)
{
	return std::count_if(str.begin(), str.end(), ::isdigit) == str.size();
}
