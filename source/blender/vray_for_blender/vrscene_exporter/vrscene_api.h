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

#include <math.h>

#include "blender_includes.h"

#include <Python.h>

#define COPY_VECTOR_3_3(a, b) \
	a[0] = b[0];\
	a[1] = b[1];\
	a[2] = b[2];

#define HEX(x) htonl(*(int*)&(x))
#define WRITE_HEX_VALUE(f, v) fprintf(f, "%08X", HEX(v));
#define WRITE_HEX_VECTOR(f, v) fprintf(f, "%08X%08X%08X", HEX(v[0]), HEX(v[1]), HEX(v[2]))
#define WRITE_TRANSFORM(f, m) fprintf(f, "TransformHex(\"%s\")", GetTransformHex(m));

#define WRITE_HEX_QUADFACE(f, face) fprintf(gfile, "%08X%08X%08X%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3), HEX(face->v3), HEX(face->v4), HEX(face->v1))
#define WRITE_HEX_TRIFACE(f, face)  fprintf(gfile, "%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3))

#define PYTHON_PRINT_BUF char buf[2048]

#define PYTHON_PRINT(pyObject, buf) \
    PyObject_CallMethod(pyObject, (char*)"write", (char*)"s", buf);

#define WRITE_PYOBJECT_BUF(pyObject) \
	PyObject_CallMethod(pyObject, (char*)"write", (char*)"s", buf);

#define PYTHON_PRINTF(pyObject, ...) \
    sprintf(buf, __VA_ARGS__); \
    WRITE_PYOBJECT_BUF(pyObject);

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

#define WRITE_PYOBJECT_TRANSFORM(pyObject, m) \
    GetTransformHex(m, buf); \
    sprintf(buf, "TransformHex(\"%s\")", buf);\
	WRITE_PYOBJECT_BUF(pyObject);


#ifdef __cplusplus
extern "C" {
#endif

int   write_GeomMayaHair(PyObject *outputFile, Scene *sce, Main *main, Object *ob, ParticleSystem *psys, const char *pluginName);

void  write_Mesh(PyObject *outputFile, Scene *sce, Object *ob, Main *main, const char *pluginName, PyObject *propGroup);
void  write_MeshFile(FILE *outputFile, Scene *sce, Object *ob, Main *main, const char *pluginName);
void  write_GeomStaticMesh(PyObject *outputFile, Scene *sce, Object *ob, Mesh *mesh, const char *pluginName, PyObject *propGroup);

void  write_SmokeDomain(PyObject *outputFile, Scene *sce, Object *ob, SmokeModifierData *smd, const char *pluginName, const char *lights);
void  write_TexVoxelData(PyObject *outputFile, Scene *sce, Object *ob, SmokeModifierData *smd, const char *pluginName, short interp_type);

void  write_Dupli(PyObject *nodeFile, PyObject *geomFile, Scene *sce, Main *main, Object *ob);
void  write_ObjectNode(PyObject   *nodeFile,
					   PyObject   *geomFile,
					   Scene      *sce,
					   Main       *main,
					   Object     *ob,
					   float       tm[4][4],
					   const char *pluginName);
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
