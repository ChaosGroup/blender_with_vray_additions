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

#ifndef CGR_EXP_DEFINES_H
#define CGR_EXP_DEFINES_H

#include "CGR_config.h"

#define COPY_VECTOR_3_3(a, b) \
	a[0] = b[0];\
	a[1] = b[1];\
	a[2] = b[2];

#define PYTHON_PRINT_BUF char buf[2048]

#define PYTHON_PRINT(pyObject, buf) \
	PyObject_CallMethod(pyObject, (char*)"write", (char*)"s", buf);

#define PYTHON_PRINTF(pyObject, ...) \
	sprintf(buf, __VA_ARGS__); \
	PYTHON_PRINT(pyObject, buf);

#define PYTHON_PRINT_TRANSFORM(o, m) \
	char tmBuf[129]; \
	GetTransformHex(m, tmBuf); \
	sprintf(buf, "TransformHex(\"%s\")", tmBuf);\
	PYTHON_PRINT(o, buf);

#define WRITE_PYOBJECT_HEX_VALUE(o, v) \
	char vBuf[129]; \
	GetFloatHex(v, vBuf); \
	PYTHON_PRINTF(o, "%s", vBuf);

#define WRITE_PYOBJECT_HEX_VECTOR(o, v) \
	char vBuf[129]; \
	GetVectorHex(v, vBuf); \
	PYTHON_PRINTF(o, "%s", vBuf);

#endif // CGR_EXP_DEFINES_H
