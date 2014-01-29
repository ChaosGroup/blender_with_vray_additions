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

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "smoke_API.h"
#include "BKE_main.h"

#ifdef __cplusplus
}
#endif

#include <Python.h>


#ifdef __cplusplus
extern "C" {
#endif

void  ExportScene(FILE *file, Scene *sce, Main *main);

int   write_GeomMayaHairInterpolate(PyObject *outputFile, Scene *sce, Main *main, Object *ob, ParticleSystem *psys, const char *pluginName);
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
