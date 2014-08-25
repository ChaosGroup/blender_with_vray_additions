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

#ifndef CGR_VRSCENE_H
#define CGR_VRSCENE_H

#include "cgr_config.h"

#include <Python.h>


void  GetDoubleHex(float f, char *buf);
void  GetFloatHex(float f, char *buf);
void  GetVectorHex(float f[3], char *buf);
void  GetTransformHex(float m[4][4], char *buf);

char* GetHex(const u_int8_t *buf, unsigned bufLen);
char* GetStringZip(const u_int8_t *buf, unsigned bufLen);
char* GetFloatArrayZip(float *data, size_t size);

int   GetPythonAttrInt(PyObject *propGroup, const char *attrName, int def=0);
float GetPythonAttrFloat(PyObject *propGroup, const char *attrName, float def=0.0f);

#endif // CGR_VRSCENE_H
