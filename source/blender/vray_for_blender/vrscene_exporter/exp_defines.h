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

#ifndef CGR_EXP_DEFINES_H
#define CGR_EXP_DEFINES_H

#include "CGR_config.h"

#define COPY_VECTOR_3_3(a, b) \
	a[0] = b[0];\
	a[1] = b[1];\
	a[2] = b[2];

#define PYTHON_PRINT_BUF char buf[2048]

#define PYTHON_PRINT(pyObject, str) \
	PyObject_CallMethod(pyObject, _C("write"), _C("s"), str);

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

#define HIDE_FROM_VIEW(frame, value) \
	"interpolate(" \
	<< "(" << 0 << "," << 1 << ")," \
	<< "(" << frame << "," << value << ")" \
	<< ")"

#define PYTHON_PRINT_KEYFRAME(output, attr, type, curData, curHash, prevData, prevHash) \
if(curHash != prevHash) { \
	PYTHON_PRINT(output, "\n\t"attr"=interpolate("); \
	PYTHON_PRINTF(output, "(%i,"type"(\"", prevFrame); \
	PYTHON_PRINT(output, prevData); \
	PYTHON_PRINT(output, "\")),"); \
	PYTHON_PRINTF(output, "(%i,"type"(\"", m_set->m_frameCurrent); \
	PYTHON_PRINT(output, curData); \
	PYTHON_PRINT(output, "\"))"); \
	PYTHON_PRINT(output, ");"); \
}

#define PYTHON_PRINT_FRAME(output, attr, type, curData, curHash, prevData, prevHash) \
if(curHash != prevHash) { \
	PYTHON_PRINT(output, "\n\t"attr"=interpolate("); \
	PYTHON_PRINTF(output, "(%i,"type"(\"", m_set->m_frameCurrent); \
	PYTHON_PRINT(output, curData); \
	PYTHON_PRINT(output, "\")));"); \
}

#define PYTHON_PRINT_DATA(output, attr, type, curData, curHash, prevPtr, prevData, prevHash) \
if(NOT(prevPtr)) { \
	PYTHON_PRINTF(output, "\n\t"attr"=%s"type"(\"", m_interpStart); \
	PYTHON_PRINT(output, curData); \
	PYTHON_PRINTF(output, "\")%s;", m_interpEnd); \
} \
else { \
	if(keyFrame) { \
		PYTHON_PRINT_KEYFRAME(output, attr, type, curData, curHash, prevData, prevHash); \
	} \
	else { \
		PYTHON_PRINT_FRAME(output, attr, type, curData, curHash, prevData, prevHash); \
	} \
}

#define EMPTY_HEX_DATA(ptr) \
	ptr = new char[1]; \
	ptr[0] = '\0';

#endif // CGR_EXP_DEFINES_H
