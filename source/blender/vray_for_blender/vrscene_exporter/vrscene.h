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

#ifndef VRSCENE_H
#define VRSCENE_H

#include "CGR_config.h"

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "blender_includes.h"

#ifdef __cplusplus
}
#endif

#define MAX_PLUGIN_NAME  1024

#define COPY_VECTOR_3_3(a, b) \
	a[0] = b[0];\
	a[1] = b[1];\
	a[2] = b[2];

#define HEX(x) htonl(*(int*)&(x))
#define WRITE_HEX_VALUE(f, v) fprintf(f, "%08X", HEX(v));
#define WRITE_HEX_VECTOR(f, v) fprintf(f, "%08X%08X%08X", HEX(v[0]), HEX(v[1]), HEX(v[2]))

#if CGR_USE_HEX
#define WRITE_TRANSFORM(f, m) fprintf(f, "TransformHex(\"%s\")", GetTransformHex(m));
#else
#define WRITE_TRANSFORM(f, m) fprintf(f, "Transform(Matrix(Vector(%f, %f, %f),Vector(%f, %f, %f),Vector(%f, %f, %f)),Vector(%f, %f, %f))", \
	m[0][0], m[0][1], m[0][2],\
	m[1][0], m[1][1], m[1][2],\
	m[2][0], m[2][1], m[2][2],\
	m[3][0], m[3][1], m[3][2]);
#endif

#define WRITE_HEX_QUADFACE(f, face) fprintf(gfile, "%08X%08X%08X%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3), HEX(face->v3), HEX(face->v4), HEX(face->v1))
#define WRITE_HEX_TRIFACE(f, face)  fprintf(gfile, "%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3))

#define WRITE_PYOBJECT_BUF(pyObject) \
	PyObject_CallMethod(pyObject, (char*)"write", (char*)"s", buf);

#define WRITE_PYOBJECT(pyObject, ...) \
	sprintf(buf, __VA_ARGS__); \
	WRITE_PYOBJECT_BUF(pyObject);

#define WRITE_PYOBJECT_HEX_VALUE(pyObject, v) \
	sprintf(buf, "%08X", HEX(v)); \
	WRITE_PYOBJECT_BUF(pyObject);

#define WRITE_PYOBJECT_HEX_VECTOR(pyObject, v) \
	sprintf(buf, "%08X%08X%08X", HEX(v[0]), HEX(v[1]), HEX(v[2])); \
	WRITE_PYOBJECT_BUF(pyObject);

#define WRITE_PYOBJECT_HEX_QUADFACE(pyObject, face) \
	sprintf(buf, "%08X%08X%08X%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3), HEX(face->v3), HEX(face->v4), HEX(face->v1));\
	WRITE_PYOBJECT_BUF(pyObject);

#define WRITE_PYOBJECT_HEX_TRIFACE(pyObject, face) \
	sprintf(buf, "%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3)); \
	WRITE_PYOBJECT_BUF(pyObject);

#if CGR_USE_HEX
#define WRITE_PYOBJECT_TRANSFORM(pyObject, m) \
	sprintf(buf, "TransformHex(\"%s\")", GetTransformHex(m));\
	WRITE_PYOBJECT_BUF(pyObject);
#else
#define WRITE_PYOBJECT_TRANSFORM(pyObject, m) \
	sprintf(buf, "Transform(Matrix(Vector(%f, %f, %f),Vector(%f, %f, %f),Vector(%f, %f, %f)),Vector(%f, %f, %f))", \
		m[0][0], m[0][1], m[0][2],\
		m[1][0], m[1][1], m[1][2],\
		m[2][0], m[2][1], m[2][2],\
		m[3][0], m[3][1], m[3][2]); \
	WRITE_PYOBJECT_BUF(pyObject);
#endif


#ifdef __cplusplus
extern "C" {
#endif

void  GetDoubleHex(float f, char *str);
void  GetFloatHex(float f, char *buf);
char* GetTransformHex(float m[4][4]);

int   write_GeomMayaHair(PyObject *outputFile, Scene *sce, Main *main, Object *ob, ParticleSystem *psys, const char *pluginName);

void  write_Mesh(PyObject *outputFile, Scene *sce, Object *ob, Main *main, const char *pluginName, PyObject *propGroup);
void  write_GeomStaticMesh(PyObject *outputFile, Scene *sce, Object *ob, Mesh *mesh, const char *pluginName, PyObject *propGroup);

void  write_SmokeDomain(PyObject *outputFile, Scene *sce, Object *ob, SmokeModifierData *smd, const char *pluginName, const char *lights);
void  write_TexVoxelData(PyObject *outputFile, Scene *sce, Object *ob, SmokeModifierData *smd, const char *pluginName, short interp_type);

void  write_Dupli(PyObject *nodeFile, PyObject *geomFile, Scene *sce, Main *main, Object *ob);
void  write_Node(PyObject *outputFile, Scene *sce, Object *ob, const char *pluginName,
				 const char *transform,
				 const char *geometry,
				 const char *material,
				 const char *volume,
				 const int   nsamples,
				 const char *lights,
				 const char *user_attributes,
				 const int   visible,
				 const int   objectID,
				 const int   primary_visibility);

#ifdef __cplusplus
}
#endif

#endif // VRSCENE_H
