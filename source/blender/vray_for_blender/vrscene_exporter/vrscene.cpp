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

#include "vrscene.h"

#ifdef _WIN32
typedef unsigned char u_int8_t;
#endif


static char sbuf[MAX_PLUGIN_NAME];


struct TraceTransform {
	float  m[3][3];
	double v[3];
};


static char int2hexValue(char d)
{
	return d<=9 ? '0'+d : (d-10+'A');
}


static void getStringHex(const u_int8_t *buf, unsigned nBytes, char *pstr)
{
	for(unsigned i = 0; i < nBytes; ++i) {
		char val0 = int2hexValue((buf[i]>>4)&0xF);
		char val1 = int2hexValue(buf[i]&0xF);
		pstr[i*2+0] = val0;
		pstr[i*2+1] = val1;
	}
	pstr[nBytes*2] = 0;
}


void GetDoubleHex(float f, char *str)
{
	double d = double(f);
	const u_int8_t *buf = (const u_int8_t*)&d;
	getStringHex(buf, sizeof(d), str);
}


void GetFloatHex(float f, char *str)
{
	float d = f;
	const u_int8_t *buf = (const u_int8_t*)&d;
	getStringHex(buf, sizeof(d), str);
}


char* GetTransformHex(float m[4][4])
{
	TraceTransform tm;
	copy_m3_m4(tm.m, m);
	copy_v3db_v3fl(tm.v, m[3]);

	const u_int8_t *buf = (const u_int8_t*)&tm;
	getStringHex(buf, sizeof(tm), sbuf);

	return sbuf;
}
